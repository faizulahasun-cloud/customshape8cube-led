#include <Arduino.h>
#include <AltSoftSerial.h>

#include "FunctionConversion/FunctionConversionEngine.h"
#include "Animations/AnimationEngine.h"
#include "Multiplexing/DisplayEngine.h"

// V2 Main firmware
// ----------------
// Complete runtime path:
//
// Built-in animations OR HM-10 function
//      |
//      v
// V2Animation
//      |
//      v
// FrameXXX.h -> FrameEngine -> DisplayEngine -> 8x8x8 multiplexing
//
// Bluetooth protocol retained for V2 functions:
//   '@' starts/restarts a function.
//   '\n' terminates the function.
//   '\r' is ignored so CRLF is also accepted.
//
// Old-firmware control characters restored:
//   'A' = Auto mode
//   'M' = Manual mode
//   'N' = next built-in animation in Manual mode
//
// Hardware controls restored from the old firmware:
//   POT A0 = brightness (2..8)
//   TOUCH D10 = long press Auto/Manual, short press next animation in Manual

AltSoftSerial HM10;

enum V2RuntimeMode : uint8_t {
  MODE_AUTO = 0,
  MODE_MANUAL = 1,
  MODE_CUSTOM = 2
};

static V2RuntimeMode runtimeMode = MODE_AUTO;
static const uint8_t TOUCH_PIN = 10;
static const uint8_t POT_PIN = A0;
static const uint8_t TOTAL_BUILT_IN_ANIMATIONS = 27;
static const uint16_t BUILT_IN_FRAME_INTERVAL_MS = 200;
static const uint16_t CUSTOM_FRAME_INTERVAL_MS = 100;
static const uint32_t AUTO_MODE_CAROUSEL_TIME = 10000UL;
static const uint32_t STARTUP_DELAY_TIME = 1000UL;

static uint32_t nextFrameTime = 0;
static uint32_t animationStart = 0;
static bool animationRunning = false;

ISR(TIMER2_COMPA_vect) {
  V2DisplayEngine::onTimer2Compare();
}

static void setupDisplayTimer2() {
  // Exact same Timer2 CTC configuration as the old firmware.
  // 16 MHz / 1024 / (3 + 1) = 3906.25 Hz ISR.
  // Each interrupt advances one Z layer, giving 488.28125 complete cube scans/s.
  noInterrupts();

  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2 = 0;
  OCR2A = 3;

  // CTC mode, prescaler /1024.
  TCCR2A |= _BV(WGM21);
  TCCR2B |= _BV(CS22) | _BV(CS21) | _BV(CS20);
  TIMSK2 |= _BV(OCIE2A);

  interrupts();
}

static void startBuiltInAnimation(uint8_t animationIndex) {
  animationIndex %= TOTAL_BUILT_IN_ANIMATIONS;
  animationRunning = V2Animation::startBuiltIn(animationIndex);
  animationStart = millis();
  nextFrameTime = animationStart + BUILT_IN_FRAME_INTERVAL_MS;
}

static void nextBuiltInAnimation() {
  V2Animation::nextBuiltIn();
  animationRunning = V2Animation::generateBuiltInFrame(0);
  animationStart = millis();
  nextFrameTime = animationStart + BUILT_IN_FRAME_INTERVAL_MS;
}

static void processBluetoothCharacter(char c) {
  // Single-character controls are only commands when no '@...\n'
  // function is currently being received. Function contents are untouched.
  if (!V2FunctionConversion::isFunctionStarted()) {
    if (c == 'A') {
      runtimeMode = MODE_AUTO;
      startBuiltInAnimation(V2Animation::builtInAnimationIndex());
      return;
    }

    if (c == 'M') {
      runtimeMode = MODE_MANUAL;
      startBuiltInAnimation(V2Animation::builtInAnimationIndex());
      return;
    }

    if (c == 'N' && runtimeMode == MODE_MANUAL) {
      nextBuiltInAnimation();
      return;
    }
  }

  // FunctionConversion owns the function protocol state. Main only
  // transports received characters into the existing V2 pipeline.
  if (!V2FunctionConversion::receiveCharacter(c)) return;

  // A complete '@...\n' function has arrived.
  // Compile once; do not compile while the animation is running frame-by-frame.
  if (!V2FunctionConversion::compileFunction()) {
    animationRunning = false;
    return;
  }

  runtimeMode = MODE_CUSTOM;
  animationRunning = V2Animation::start();
  nextFrameTime = millis() + CUSTOM_FRAME_INTERVAL_MS;
}

static void updateTouchControls(uint32_t now) {
  static bool lastTouch = false;
  static uint32_t touchTimer = 0;
  static bool longPress = false;

  bool touch = digitalRead(TOUCH_PIN) == HIGH;

  if (touch && !lastTouch) {
    touchTimer = now;
    longPress = false;
  } else if (touch && lastTouch) {
    if (!longPress && (now - touchTimer >= 3000UL)) {
      // Same old-firmware behavior: long press toggles Auto/Manual.
      runtimeMode = (runtimeMode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO;
      startBuiltInAnimation(V2Animation::builtInAnimationIndex());
      longPress = true;
    }
  } else if (!touch && lastTouch) {
    uint32_t duration = now - touchTimer;
    if (!longPress && runtimeMode == MODE_MANUAL && duration >= 50UL && duration < 3000UL) {
      nextBuiltInAnimation();
    }
  }

  lastTouch = touch;
}

void setup() {
  pinMode(11, OUTPUT); // DATA  / PB3
  pinMode(12, OUTPUT); // LATCH / PB4
  pinMode(13, OUTPUT); // CLOCK / PB5
  pinMode(TOUCH_PIN, INPUT);
  pinMode(POT_PIN, INPUT);

  digitalWrite(11, LOW);
  digitalWrite(12, LOW);
  digitalWrite(13, LOW);

  HM10.begin(9600);

  // Start with the display ISR running and the old firmware's Auto mode.
  V2DisplayEngine::setBrightness(4);
  setupDisplayTimer2();

  delay(STARTUP_DELAY_TIME);
  startBuiltInAnimation(0);
}

void loop() {
  const uint32_t now = millis();

  // Restore the old B10K brightness control. The V2 DisplayEngine keeps the
  // same 0..8 brightness scale and the same Timer2 refresh path.
  const uint8_t brightness = (uint8_t)map(analogRead(POT_PIN), 0, 1023, 2, 8);
  V2DisplayEngine::setBrightness(brightness);

  updateTouchControls(now);

  while (HM10.available() > 0) {
    processBluetoothCharacter((char)HM10.read());
  }

  if (!animationRunning) return;

  if ((int32_t)(now - nextFrameTime) >= 0) {
    if (runtimeMode == MODE_CUSTOM) {
      if (!V2Animation::generateNextFrame()) {
        animationRunning = false;
      } else {
        nextFrameTime = now + CUSTOM_FRAME_INTERVAL_MS;
      }
      return;
    }

    // Auto and Manual both use the exact same 27 built-in animation
    // pipeline. Auto additionally changes animation every 10 seconds.
    if (!V2Animation::generateNextBuiltInFrame()) {
      animationRunning = false;
      return;
    }

    nextFrameTime = now + BUILT_IN_FRAME_INTERVAL_MS;

    if (runtimeMode == MODE_AUTO && (now - animationStart >= AUTO_MODE_CAROUSEL_TIME)) {
      nextBuiltInAnimation();
    }
  }
}
