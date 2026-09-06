#include <Arduino.h>
#include <avr/interrupt.h>
#include <AltSoftSerial.h>
#include <math.h>

const byte DATA_PIN = 11, CLOCK_PIN = 13, LATCH_PIN = 12, TOUCH_PIN = 10, POT_PIN = A0, BLE_STATE_PIN = 2;
AltSoftSerial bluetooth;

volatile byte currentCubeMode = 0; // 0 Auto, 1 Manual, 3 Math Mode, 4 Custom Engine (Mode 2 is Reserved)
byte globalBrightness = 4;
unsigned int animationIndex = 0; 
byte frameCounter = 0;
const unsigned int TOTAL_ANIMATIONS = 24, FRAME_TIME = 200;
const unsigned long AUTO_MODE_CAROUSEL_TIME = 10000UL;
unsigned long lastFrameTime = 0, animationStart = 0;

volatile byte displayBuffer[8][8]; 
byte parseMode = 0; 
byte backBuffer[8][8]; 
volatile byte brightnessAccumulator[8] = {0,0,0,0,0,0,0,0};

bool lastBluetoothConnected = false; 
unsigned long bluetoothStateChangedAt = 0;
const unsigned long BLE_STATE_DEBOUNCE_TIME = 3000UL;

struct ColumnMap { 
  byte reg; 
  byte bit; 
};

const ColumnMap COLUMN_MAP[64] = {
  {1,0},{1,1},{1,2},{1,3},{1,4},{1,5},{1,6},{1,7},
  {2,0},{2,1},{2,2},{2,3},{2,4},{2,5},{2,6},{2,7},
  {3,0},{3,1},{3,2},{3,3},{3,4},{3,5},{3,6},{3,7},
  {4,0},{4,1},{4,2},{4,3},{4,4},{4,5},{4,6},{4,7},
  {5,0},{5,1},{5,2},{5,3},{5,4},{5,5},{5,6},{5,7},
  {6,0},{6,1},{6,2},{6,3},{6,4},{6,5},{6,6},{6,7},
  {7,0},{7,1},{7,2},{7,3},{7,4},{7,5},{7,6},{7,7},
  {8,0},{8,1},{8,2},{8,3},{8,4},{8,5},{8,6},{8,7}
};

inline byte columnIndex(byte x, byte y){ return y*8+x; }

void triggerModeBlinkAcknowledgment(){
  for(byte z = 0; z < 8; z++){
    for(byte r = 0; r < 8; r++){
      displayBuffer[z][r] = 0xFF;
    }
  }
  delay(80);
  for(byte z = 0; z < 8; z++){
    for(byte r = 0; r < 8; r++){
      displayBuffer[z][r] = 0x00;
    }
  }
  delay(80);
}

enum OpCode {
  OP_NONE = 0,
  OP_CALC_H,
  OP_CHECK_H_GE_8,
  OP_CALC_RZ,
  OP_CHECK_Z_MATCH
};

struct CompiledInstruction {
  OpCode op;
};

CompiledInstruction compiledProgram[16];
byte programLength = 4;
char customRxBuf[32];
byte customRxIdx = 0;

void handleScriptControl(byte cmd){
  if(cmd == 0x41 || cmd == 0x51 || cmd == 'A' || cmd == 'Q'){ 
    currentCubeMode = 0;
    parseMode = 0;
    animationStart = millis();
    lastFrameTime = animationStart;
    triggerModeBlinkAcknowledgment();
  } else if(cmd == 0x4D || cmd == 'M'){ 
    currentCubeMode = 1;
    parseMode = 0;
    animationStart = millis();
    lastFrameTime = animationStart;
    triggerModeBlinkAcknowledgment();
  } else if(cmd == 0x46 || cmd == 'F'){
    currentCubeMode = 3;
    memset((void*)displayBuffer, 0, 64);
    parseMode = 0;
    animationStart = millis();
    lastFrameTime = millis();
    triggerModeBlinkAcknowledgment();
  } else if(cmd == 0x58 || cmd == 'X'){
    currentCubeMode = 4;
    memset((void*)displayBuffer, 0, 64);
    parseMode = 0;
    animationStart = millis();
    lastFrameTime = millis();
    triggerModeBlinkAcknowledgment();
  }
}

void setup(){
  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(TOUCH_PIN, INPUT);
  pinMode(BLE_STATE_PIN, INPUT);
  PORTB &= ~(_BV(PB3) | _BV(PB4) | _BV(PB5));
  bluetooth.begin(9600);
  initDefaultCustomProgram();
  startRefreshTimer();
  animationStart = millis();
  lastFrameTime = millis();
}

void loop(){
  unsigned long now = millis();
  bool ble = digitalRead(BLE_STATE_PIN) == HIGH;
  if(ble != lastBluetoothConnected){
    if(bluetoothStateChangedAt == 0) bluetoothStateChangedAt = now;
    else if(now - bluetoothStateChangedAt >= BLE_STATE_DEBOUNCE_TIME){
      lastBluetoothConnected = ble;
      bluetoothStateChangedAt = 0;
      if(!lastBluetoothConnected){
        currentCubeMode = 0;
        parseMode = 0;
        animationStart = now;
        lastFrameTime = now;
        triggerModeBlinkAcknowledgment();
      }
    }
  } else {
    bluetoothStateChangedAt = 0;
  }

  if(!lastBluetoothConnected) {
    int raw = analogRead(POT_PIN);
    globalBrightness = map(raw, 0, 1023, 2, 8);
  }

  static bool lastTouch = false; 
  static unsigned long touchTimer = 0; 
  static bool longPress = false;
  bool touch = digitalRead(TOUCH_PIN) == HIGH;
  if(lastBluetoothConnected) touch = false;
  
  if(touch && !lastTouch){
    touchTimer = now; 
    longPress = false;
  } else if(touch && lastTouch){
    unsigned long d = now - touchTimer;
    if(!longPress && d >= 3000UL){
      currentCubeMode = (currentCubeMode == 0) ? 1 : 0;
      longPress = true;
      animationStart = now;
      lastFrameTime = now;
      triggerModeBlinkAcknowledgment();
    }
  }
  lastTouch = touch;
}

inline void shiftByteFast(byte value){
  for(int8_t bit = 7; bit >= 0; bit--){
    if(value & (1 << bit)) PORTB |= _BV(PB3); 
    else PORTB &= ~_BV(PB3);
    PORTB |= _BV(PB5); 
    PORTB &= ~_BV(PB5);
  }
}

inline void latchFast(){
  PORTB |= _BV(PB4); 
  PORTB &= ~_BV(PB4);
}

void refreshDisplay(){
  static byte layer = 0;
  brightnessAccumulator[layer] += globalBrightness;
  bool en = brightnessAccumulator[layer] >= 8;
  if(en) brightnessAccumulator[layer] -= 8;
  
  shiftByteFast(en ? (1 << layer) : 0);
  for(int8_t r = 7; r >= 0; r--){
    shiftByteFast(displayBuffer[layer][r]);
  }
  latchFast();
  layer = (layer + 1) % 8;
}

ISR(TIMER2_COMPA_vect){ refreshDisplay(); }
void startRefreshTimer(){
  noInterrupts();
  TCCR2A = _BV(WGM21);
  TCCR2B = _BV(CS22) | _BV(CS21) | _BV(CS20);
  OCR2A = 3;
  TIMSK2 |= _BV(OCIE2A);
  interrupts();
}

inline bool isOuterRing(byte x, byte y){ return x == 0 || x == 7 || y == 0 || y == 7; }
byte perimeterIndex(byte x, byte y){ if(y == 0) return x; if(x == 7) return 7 + y; if(y == 7) return 21 - x; return 21 + (7 - y); }

bool mathFunctionVoxel(byte f, byte x, byte y, byte z){
  float fx = (float)x - 3.5f;
  float fy = (float)y - 3.5f;
  float dist = sqrt(fx * fx + fy * fy);
  float waveZ = 3.5f + 2.5f * sin(dist * 0.8f - (float)f * 0.2f);
  int targetZ = (int)(waveZ + 0.5f);
  return z == targetZ;
}

void drawMathFrame(byte f){
  byte localMatrix[8][8];
  for(byte z = 0; z < 8; z++){
    for(byte r = 0; r < 8; r++) localMatrix[z][r] = 0;
    for(byte y = 0; y < 8; y++){
      for(byte x = 0; x < 8; x++){
        if(mathFunctionVoxel(f, x, y, z)){
          byte c = columnIndex(x, y), reg = COLUMN_MAP[c].reg, bit = COLUMN_MAP[c].bit;
          if(reg >= 1 && reg <= 8 && bit <= 7) localMatrix[z][reg - 1] |= (1 << bit);
        }
      }
    }
  }
  noInterrupts();
  memcpy((void*)displayBuffer, localMatrix, 64);
  interrupts();
}

void drawAnimationFrame(byte a, byte f){
  byte localMatrix[8][8];
  for(byte z = 0; z < 8; z++){
    for(byte r = 0; r < 8; r++) localMatrix[z][r] = 0;
    for(byte y = 0; y < 8; y++){
      for(byte x = 0; x < 8; x++){
        // Animation voxel logic from the working sketch continues here.
        // This placeholder preserves the uploaded sketch's structure.
        bool on = false;
        if(a == 23){
          if(!((x == 0 || x == 7) && (y == 0 || y == 7) && (z == 0 || z == 7))) on = false;
          else {
            byte c = ((z == 7) ? 4 : 0) + ((y == 7) ? 2 : 0) + ((x == 7) ? 1 : 0);
            on = c == (f % 8);
          }
        }
        if(on){
          byte c = columnIndex(x, y), reg = COLUMN_MAP[c].reg, bit = COLUMN_MAP[c].bit;
          if(reg >= 1 && reg <= 8 && bit <= 7) localMatrix[z][reg - 1] |= (1 << bit);
        }
      }
    }
  }
  noInterrupts();
  memcpy((void*)displayBuffer, localMatrix, 64);
  interrupts();
}

void initDefaultCustomProgram(){
  programLength = 4;
  compiledProgram[0].op = OP_CALC_H;
  compiledProgram[1].op = OP_CHECK_H_GE_8;
  compiledProgram[2].op = OP_CALC_RZ;
  compiledProgram[3].op = OP_CHECK_Z_MATCH;
}
