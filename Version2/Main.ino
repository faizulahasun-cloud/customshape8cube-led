#include <Arduino.h>
#include <AltSoftSerial.h>

#include "FunctionConversion/FunctionConversionEngine.h"
#include "Animations/AnimationEngine.h"
#include "Multiplexing/DisplayEngine.h"

// V2 Main firmware
// ----------------
// Complete runtime path:
//
// HM-10 Bluetooth
//      |
//      v
// AltSoftSerial receiver
//      |
//      v
// V2FunctionConversion::receiveCharacter()
//      |
//      | newline received
//      v
// V2FunctionConversion::compileFunction()
//      |
//      v
// V2Animation::start()
//      |
//      v
// V2Animation::generateNextFrame()
//      |
//      v
// FrameXXX.h -> FrameEngine -> DisplayEngine -> 8x8x8 multiplexing
//
// Bluetooth protocol:
//   '@' starts/restarts a function.
//   '\n' terminates the function.
//   '\r' is ignored so CRLF is also accepted.
//
// Example:
//   @((F/2)%4==0&&X==0)||((F/2)%4==1&&Y==7)||((F/2)%4==2&&X==7)||((F/2)%4==3&&Y==0)\n
// The V2 DisplayEngine already owns the physical cube pins:
//   DATA  = D11
//   LATCH = D12
//   CLOCK = D13
// AltSoftSerial on Arduino Uno uses its fixed pins:
//   RX = D8, TX = D9

AltSoftSerial HM10;

// Generate the next animation frame outside the display ISR.
// This keeps the Timer2 multiplexing ISR short and deterministic.
static const uint16_t FRAME_INTERVAL_MS = 100;
static uint32_t nextFrameTime = 0;
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

static void processBluetoothCharacter(char c) {
  // FunctionConversion owns the protocol state. Main only transports
  // received characters into the V2 pipeline.
  if (!V2FunctionConversion::receiveCharacter(c)) return;

  // A complete '@...\\n' function has arrived.
  // Compile once; do not compile while the animation is running frame-by-frame.
  if (!V2FunctionConversion::compileFunction()) {
    animationRunning = false;
    return;
  }

  // Start always generates frame F=0 through Frame001.h and the rest of V2.
  animationRunning = V2Animation::start();
  nextFrameTime = millis() + FRAME_INTERVAL_MS;
}

void setup() {
  // DisplayEngine uses PORTB directly, so explicitly configure its three
  // physical shift-register pins as outputs before enabling multiplexing.
  pinMode(11, OUTPUT); // DATA  / PB3
  pinMode(12, OUTPUT); // LATCH / PB4
  pinMode(13, OUTPUT); // CLOCK / PB5

  digitalWrite(11, LOW);
  digitalWrite(12, LOW);
  digitalWrite(13, LOW);

  // HM-10 communication. The V2 pipeline accepts bytes; it does not depend
  // on a command prompt or fixed packet size.
  HM10.begin(9600);

  // Start with the display ISR running, but no animation until a valid
  // Bluetooth function is received and compiled.
  V2DisplayEngine::setBrightness(4);
  setupDisplayTimer2();
}

void loop() {
  // Drain all currently available HM-10 bytes so Bluetooth reception does
  // not unnecessarily block animation generation.
  while (HM10.available() > 0) {
    processBluetoothCharacter((char)HM10.read());
  }

  // Animation generation is deliberately outside the Timer2 ISR.
  // Only one complete frame is generated at a time by AnimationEngine.
  if (animationRunning) {
    const uint32_t now = millis();
    if ((int32_t)(now - nextFrameTime) >= 0) {
      if (!V2Animation::generateNextFrame()) {
        animationRunning = false;
      } else {
        nextFrameTime = now + FRAME_INTERVAL_MS;
      }
    }
  }
}
