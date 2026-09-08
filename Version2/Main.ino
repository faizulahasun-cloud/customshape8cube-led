#include <Arduino.h>
#include <avr/interrupt.h>
#include <AltSoftSerial.h>

#include "FunctionConversion/FunctionConversionEngine.h"
#include "Animations/AnimationEngine.h"
#include "Frames/FrameEngine.h"
#include "Multiplexing/DisplayEngine.h"

// V2 Runtime Controller
// ---------------------
// Main.ino only defines the execution order between the engines.
//
// Runtime flow:
//   ANIMATION RUNNING
//        '@'
//         -> FUNCTION RECEIVING
//         -> '\n'
//         -> COMPILE
//         -> valid: START ANIMATION at F=0
//         -> invalid: remain stopped
//
// Function Conversion does not call Animation Engine directly.
// Main.ino is the controller between the engines.

const byte DATA_PIN = 11;
const byte CLOCK_PIN = 13;
const byte LATCH_PIN = 12;

AltSoftSerial bluetooth;

static const unsigned long FRAME_TIME = 200UL;
static unsigned long lastFrameTime = 0;

static bool animationRunning = false;

// Keep the display blank while a new function is being received or when
// compilation fails. This does not change the Display Engine logic.
inline void showBlankFrame() {
  V2FrameEngine::clear();
  V2FrameEngine::submit();
}

// Timer2 is dedicated to the existing V2 multiplexing engine.
// 16 MHz / 64 / (249 + 1) = 1000 Hz layer refresh interrupt.
ISR(TIMER2_COMPA_vect) {
  V2DisplayEngine::onTimer2Compare();
}

void setupTimer2() {
  noInterrupts();

  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2 = 0;

  // CTC mode, 1 kHz compare interrupt.
  OCR2A = 249;
  TCCR2A |= _BV(WGM21);
  TCCR2B |= _BV(CS22);              // prescaler 64
  TIMSK2 |= _BV(OCIE2A);

  interrupts();
}

void setup() {
  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);

  digitalWrite(DATA_PIN, LOW);
  digitalWrite(CLOCK_PIN, LOW);
  digitalWrite(LATCH_PIN, LOW);

  bluetooth.begin(9600);

  V2DisplayEngine::setBrightness(4);
  V2Animation::reset();
  showBlankFrame();
  setupTimer2();

  lastFrameTime = millis();
}

void loop() {
  // Bluetooth input is the only event source for changing the function.
  while (bluetooth.available() > 0) {
    const char c = (char)bluetooth.read();

    // '@' starts a new function reception. A new '@' also restarts the
    // Function Conversion Engine, so the current animation is interrupted.
    if (c == '@') {
      V2FunctionConversion::receiveCharacter(c);
      animationRunning = false;
      showBlankFrame();
      continue;
    }

    const bool functionEnded = V2FunctionConversion::receiveCharacter(c);

    if (functionEnded) {
      // Only a successfully compiled function may start a new animation.
      if (V2FunctionConversion::compileFunction()) {
        animationRunning = V2Animation::start();
        lastFrameTime = millis();
      } else {
        animationRunning = false;
        showBlankFrame();
      }
    }
  }

  // Do not run animation frames while a function is being received.
  if (!animationRunning || V2FunctionConversion::isFunctionStarted()) return;

  const unsigned long now = millis();
  if ((unsigned long)(now - lastFrameTime) >= FRAME_TIME) {
    lastFrameTime = now;
    if (!V2Animation::generateNextFrame()) {
      animationRunning = false;
      showBlankFrame();
    }
  }
}
