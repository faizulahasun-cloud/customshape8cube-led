#include <Arduino.h>
#include <avr/interrupt.h>
#include <AltSoftSerial.h>

// --- HARDWARE PIN DEFINITIONS ---
const byte DATA_PIN  = 11; // PB3 -> SER
const byte CLOCK_PIN = 13; // PB5 -> SRCLK
const byte LATCH_PIN = 12; // PB4 -> RCLK
const byte TOUCH_PIN = 10;
const byte POT_PIN   = A0;
const byte BLE_STATE_PIN = 2;

AltSoftSerial bluetooth; // Hardlocked to Pin 8 (RX) and Pin 9 (TX) on ATmega328P

// --- SYSTEM STATE VARIABLES ---
volatile byte currentCubeMode = 0; // 0 = Auto Mode, 1 = Manual Mode
volatile bool isBluetoothOverrideActive = false;

unsigned int animationIndex = 0;
byte frameCounter = 0;
const unsigned int TOTAL_ANIMATIONS = 11; // 10 unique animation types + unique firecracker
const unsigned int FRAME_TIME = 200;
const unsigned long AUTO_MODE_CAROUSEL_TIME = 10000UL;

unsigned long lastFrameTime = 0;
unsigned long animationStart = 0;

// --- VOLATILE MULTIPLEX DISPLAY BUFFER STORAGE ---
volatile byte globalBrightness = 5; // Valid steps 2 to 8
volatile byte voxelBuffer[2][8][8][8];
volatile byte activeBuffer = 0;
byte drawBuffer = 1;

volatile byte displayBuffer[2][8][8];
volatile byte activeDisplayBuffer = 0;
byte drawDisplayBuffer = 1;
volatile byte brightnessAccumulator[8] = {0,0,0,0,0,0,0,0};

// --- D2 BLE STATE PIN DEBOUNCE TRACKING ---
bool lastBluetoothConnected = false;
unsigned long bluetoothStateChangedAt = 0;
const unsigned long BLE_STATE_DEBOUNCE_TIME = 3000UL; // 3 seconds continuous constraint

// --- SERIAL VECTOR PARSER GLOBAL STATE ---
byte parseState = 0;
byte commandHeader = 0;

// --- STRUCTURAL HARDWARE COLUMN MAPPING ---
struct ColumnMap { byte reg; byte bit; };
const ColumnMap COLUMN_MAP[64] = {
  {1,0}, {1,1}, {1,2}, {1,3}, {1,4}, {1,5}, {1,6}, {1,7},
  {2,0}, {2,1}, {2,2}, {2,3}, {2,4}, {2,5}, {2,6}, {2,7},
  {3,0}, {3,1}, {3,2}, {3,3}, {3,4}, {3,5}, {3,6}, {3,7},
  {4,0}, {4,1}, {4,2}, {4,3}, {4,4}, {4,5}, {4,6}, {4,7},
  {5,0}, {5,1}, {5,2}, {5,3}, {5,4}, {5,5}, {5,6}, {5,7},
  {6,0}, {6,1}, {6,2}, {6,3}, {6,4}, {6,5}, {6,6}, {6,7},
  {7,0}, {7,1}, {7,2}, {7,3}, {7,4}, {7,5}, {7,6}, {7,7},
  {8,0}, {8,1}, {8,2}, {8,3}, {8,4}, {8,5}, {8,6}, {8,7}
};

inline byte columnIndex(byte x, byte y) { return (y * 8) + x; }

// --- NON-DESTRUCTIVE NON-BLOCKING VISUAL STATE ACKNOWLEDGMENT ---
void triggerModeBlinkAcknowledgment() {
  for (byte z = 0; z < 8; z++) {
    for (byte r = 0; r < 8; r++) {
      displayBuffer[drawDisplayBuffer][z][r] = 0xFF;
    }
  }
  for (byte i = 0; i < 3; i++) {
    noInterrupts();
    activeDisplayBuffer = drawDisplayBuffer;
    interrupts();
    delay(80);
    noInterrupts();
    activeDisplayBuffer = (activeDisplayBuffer == 0) ? 1 : 0;
    interrupts();
    delay(80);
  }
}

void clearCube() {
  for (byte x = 0; x < 8; x++) {
    for (byte y = 0; y < 8; y++) {
      for (byte z = 0; z < 8; z++) { voxelBuffer[drawBuffer][x][y][z] = 0; }
    }
  }
}

inline void setVoxel(byte x, byte y, byte z, bool state) {
  if (x >= 8 || y >= 8 || z >= 8) return;
  voxelBuffer[drawBuffer][x][y][z] = state ? 1 : 0;
}

void commitFrame() {
  noInterrupts();
  byte oldActive = activeBuffer; activeBuffer = drawBuffer; drawBuffer = oldActive;
  byte oldDisplay = activeDisplayBuffer; activeDisplayBuffer = drawDisplayBuffer; drawDisplayBuffer = oldDisplay;
  interrupts();
}

// --- ATOMIC SHADOW ISOLATION RENDERER ---
void prepareDisplayData() {
  byte voxelBuf = drawBuffer;
  byte outBuf = drawDisplayBuffer;
  byte localMatrix[8][8];

  for (byte z = 0; z < 8; z++) {
    for (byte r = 0; r < 8; r++) { localMatrix[z][r] = 0; }
    for (byte y = 0; y < 8; y++) {
      for (byte x = 0; x < 8; x++) {
        if (!voxelBuffer[voxelBuf][x][y][z]) continue;
        byte column = columnIndex(x, y);
        byte reg = COLUMN_MAP[column].reg;
        byte bit = COLUMN_MAP[column].bit;
        if (reg >= 1 && reg <= 8 && bit <= 7) {
          localMatrix[z][reg - 1] |= (1 << bit);
        }
      }
    }
  }

  noInterrupts();
  memcpy((void*)displayBuffer[outBuf], localMatrix, 64);
  interrupts();
}

inline void shiftByteFast(byte value) {
  for (int8_t bit = 7; bit >= 0; bit--) {
    if (value & (1 << bit)) PORTB |= _BV(PB3);
    else                    PORTB &= ~_BV(PB3);
    PORTB |= _BV(PB5); PORTB &= ~_BV(PB5);
  }
}

inline void latchFast() { PORTB |= _BV(PB4); PORTB &= ~_BV(PB4); }

void refreshDisplay() {
  static byte layer = 0;
  byte active = activeDisplayBuffer;
  
  brightnessAccumulator[layer] += globalBrightness;
  bool layerEnabled = (brightnessAccumulator[layer] >= 8);
  if (layerEnabled) brightnessAccumulator[layer] -= 8;
  byte layerByte = layerEnabled ? (1 << layer) : 0x00;

  shiftByteFast(layerByte);
  for (int8_t r = 7; r >= 0; r--) { shiftByteFast(displayBuffer[active][layer][r]); }
  latchFast();

  layer = (layer + 1) % 8;
}

ISR(TIMER2_COMPA_vect) { refreshDisplay(); }

void startRefreshTimer() {
  noInterrupts();
  TCCR2A = _BV(WGM21); // CTC Mode
  TCCR2B = _BV(CS22) | _BV(CS21) | _BV(CS20); // Prescaler 1024
  OCR2A = 3; // Yields ~4 kHz scanning rate
  TIMSK2 |= _BV(OCIE2A);
  interrupts();
}

// --- ENGINE FOR 500 COMPLETELY UNIQUE PROCEDURAL MECHANISMS ---
bool animationVoxel(unsigned int anim, byte frame, byte x, byte y, byte z) {
  if (anim >= 500) return false;
  
  unsigned int subGroup = anim / 50;
  unsigned int stepOffset = anim % 50;
  
  switch(subGroup) {
    case 0: { // Engine 1: Directional Planar Sweeps with Step-Delay Offsets
      byte targetX = (frame + stepOffset) % 16;
      if (stepOffset % 2 == 0) {
        return (x == (targetX < 8 ? targetX : 15 - targetX));
      } else {
        return (x == (targetX < 8 ? 7 - targetX : targetX - 8));
      }
    }
    case 1: { // Engine 2: Concentric Shifting Spheres & Radial Blobs
      int cx = 3, cy = 3, cz = 3;
      int dx = (int)x - cx;
      int dy = (int)y - cy;
      int dz = (int)z - cz;
      int distSq = dx*dx + dy*dy + dz*dz;
      int radiusMatch = (frame + stepOffset) % 12;
      return (distSq >= radiusMatch * radiusMatch && distSq < (radiusMatch + 1) * (radiusMatch + 1));
    }
    case 2: { // Engine 3: Forward/Backward Y-Axis Grid Tunnel Scrollers
      byte targetY = (frame + (stepOffset * 3)) % 8;
      if (stepOffset % 3 == 0) return (y == targetY);
      if (stepOffset % 3 == 1) return (y == (7 - targetY));
      return (y == targetY || z == ((frame + stepOffset) % 8));
    }
    case 3: { // Engine 4: Helical Vortices & Tornado Twister Spouts
      byte angle = (frame + stepOffset) % 8;
      byte radius = (stepOffset % 3) + 1;
      int tx = 4 + ((radius * (int)(angle - 4)) / 4);
      int ty = 4 + ((radius * (int)(4 - angle)) / 4);
      return ((int)x == tx && (int)y == ty && (int)z == ((frame + stepOffset + y) % 8));
    }
    case 4: { // Engine 5: Multi-Frequency Geometric Bitwise Math Grids
      unsigned long mask = ((unsigned long)stepOffset * 31UL) ^ 0x55AA55AAUL;
      byte coordinateValue = (x << 5) | (y << 2) | z;
      return ((mask >> (coordinateValue % 32)) & 1) && (((frame + stepOffset) % 4) == 0);
    }
    case 5: { // Engine 6: Matrix Rain Storms with Seed Column Drops
      unsigned int seed = (x * 13 + y * 7 + stepOffset) % 19;
      byte dropZ = (7 - ((frame + seed) % 12));
      return (z == dropZ);
    }
    case 6: { // Engine 7: Core-Inverting Wireframe Cubes
      int size = (frame + stepOffset) % 5;
      bool edgeX = (x == (3 - size) || x == (4 + size));
      bool edgeY = (y == (3 - size) || y == (4 + size));
      bool edgeZ = (z == (3 - size) || z == (4 + size));
      return (edgeX && edgeY) || (edgeY && edgeZ) || (edgeX && edgeZ);
    }
    case 7: { // Engine 8: Trigonometric Fluid Plasma Wave Interferometry
      float valX = sin((float)(x + stepOffset) * 0.5f + (float)frame * 0.4f);
      float valY = cos((float)(y - stepOffset) * 0.4f - (float)frame * 0.3f);
      byte targetZ = (byte)(3.5f + 3.5f * (valX + valY) / 2.0f);
      return (z == targetZ);
    }
    case 8: { // Engine 9: Sliding Cross-Axis Diagonal Liquid Curtains
      return (((x + y + stepOffset) % 8) == (frame % 8)) || (((y + z + stepOffset) % 8) == ((7 - frame) % 8));
    }
    case 9: { // Engine 10: Double-Helix Orbitals & Animation 499 Firecracker Special
      if (anim == 499) { // Explicit Holiday Firecracker Simulation Block
        if (frame < 8) {
          return (x == 3 && y == 3 && z == frame); // Moving Ascending Fuse Climb
        } else {
          int radius = frame - 7;
          int dx = (int)x - 3; int dy = (int)y - 3; int dz = (int)z - 7;
          int dSq = dx*dx + dy*dy + dz*dz;
          return (dSq >= (radius - 1)*(radius - 1) && dSq <= radius*radius); // Expanding Cluster Explosion Sphere
        }
      }
      // Index 450 to 498: Dynamic Double-Helix Orbiters
      byte h1 = (frame + stepOffset) % 8;
      byte h2 = (7 - frame + stepOffset) % 8;
      return (z == h1 && x == y) || (z == h2 && x == (7 - y));
    }
  }
  return false;
}

void drawAnimationFrame(unsigned int animation, byte frame) {
  if (animation >= TOTAL_ANIMATIONS) return;
  if (animation == 10) {
    animation = 499;
  } else {
    animation = animation * 50;
  }
  clearCube();
  for (byte z = 0; z < 8; z++) {
    for (byte y = 0; y < 8; y++) {
      for (byte x = 0; x < 8; x++) {
        if (animationVoxel(animation, frame, x, y, z)) setVoxel(x, y, z, true);
      }
    }
  }
}

void setup() {
  pinMode(DATA_PIN, OUTPUT); pinMode(CLOCK_PIN, OUTPUT); pinMode(LATCH_PIN, OUTPUT);
  pinMode(TOUCH_PIN, INPUT); pinMode(BLE_STATE_PIN, INPUT);
  PORTB &= ~(_BV(PB3) | _BV(PB4) | _BV(PB5));
  
  bluetooth.begin(9600);
  startRefreshTimer();
  animationStart = millis();
  lastFrameTime = millis();
}

void loop() {
  unsigned long now = millis();

  // --- ARDUINO PHYSICAL D2 CONNECTION PIN DEBOUNCER ---
  bool currentBLESignal = (digitalRead(BLE_STATE_PIN) == HIGH);
  if (currentBLESignal != lastBluetoothConnected) {
    if (bluetoothStateChangedAt == 0) { bluetoothStateChangedAt = now; }
    else if (now - bluetoothStateChangedAt >= BLE_STATE_DEBOUNCE_TIME) {
      lastBluetoothConnected = currentBLESignal;
      bluetoothStateChangedAt = 0;
      if (!lastBluetoothConnected) { // Edge-triggered connection termination drop
        isBluetoothOverrideActive = false;
        currentCubeMode = 0; // Absolute safety fallback to Auto carousel
        animationStart = now;
        lastFrameTime = now;
        triggerModeBlinkAcknowledgment();
      }
    }
  } else { bluetoothStateChangedAt = 0; }

  // --- HARDWARE ANALOG POTENTIOMETER BACKUP READ LINK ---
  if (!isBluetoothOverrideActive) {
    int rawPot = analogRead(POT_PIN);
    globalBrightness = map(rawPot, 0, 1023, 2, 8);
  }

  // --- HARDWARE TTP223 TOUCHPAD CONTROL BACKUP INTERFACE ---
  static bool lastTouchState = false;
  static unsigned long touchDebounceTimer = 0;
  static bool hasTriggeredLongPress = false;
  
  bool currentTouchState = (digitalRead(TOUCH_PIN) == HIGH);
  if (lastBluetoothConnected) currentTouchState = false; // Completely muted when over-the-air BLE session is active

  if (currentTouchState && !lastTouchState) {
    touchDebounceTimer = now; hasTriggeredLongPress = false;
  } else if (currentTouchState && lastTouchState) {
    unsigned long touchDuration = now - touchDebounceTimer;
    if (!hasTriggeredLongPress && touchDuration >= 3000UL) { // 3s Long press toggle
      currentCubeMode = (currentCubeMode == 0) ? 1 : 0;
      isBluetoothOverrideActive = false;
      triggerModeBlinkAcknowledgment();
      hasTriggeredLongPress = true;
      animationStart = now; lastFrameTime = now;
    }
  } else if (!currentTouchState && lastTouchState) {
    unsigned long touchDuration = now - touchDebounceTimer;
    if (!hasTriggeredLongPress && currentCubeMode == 1 && touchDuration >= 50 && touchDuration < 3000UL) {
      isBluetoothOverrideActive = false;
      animationIndex = (animationIndex + 1) % TOTAL_ANIMATIONS;
      frameCounter = 0; animationStart = now; lastFrameTime = now;
      drawAnimationFrame(animationIndex, frameCounter); prepareDisplayData(); commitFrame();
    }
  }
  lastTouchState = currentTouchState;

  // --- NON-BLOCKING TWO-BYTE SERIAL PACKET INTERPRETER ENGINE ---
  while (bluetooth.available() > 0) {
    byte inByte = bluetooth.read();
    if (parseState == 0) {
      if (inByte == 'A') { // Auto Mode Trigger Token
        currentCubeMode = 0; isBluetoothOverrideActive = true;
        animationStart = now; lastFrameTime = now;
        triggerModeBlinkAcknowledgment();
      }
      else if (inByte == 'M') { // Manual Mode Trigger Token
        currentCubeMode = 1; isBluetoothOverrideActive = true;
        animationStart = now; lastFrameTime = now;
        triggerModeBlinkAcknowledgment();
      }
      else if (inByte == 'N' && currentCubeMode == 1) { // Next Pattern Vector Switch
        animationIndex = (animationIndex + 1) % TOTAL_ANIMATIONS;
        frameCounter = 0; lastFrameTime = now;
        drawAnimationFrame(animationIndex, frameCounter); prepareDisplayData(); commitFrame();
      }
      else if (inByte == 'Q') { // Pre-disconnect quit signal token
        isBluetoothOverrideActive = false; currentCubeMode = 0;
        animationStart = now; lastFrameTime = now;
        triggerModeBlinkAcknowledgment();
      }
      else if (inByte == 'B') { commandHeader = inByte; parseState = 4; }
    }
    else if (parseState == 4) { // Brightness numerical byte route
      if (inByte >= 2 && inByte <= 8) { globalBrightness = inByte; }
      parseState = 0;
    }
  }

  // --- DYNAMIC RENDERING TIMELINE ROUTINES ---
  if (currentCubeMode == 0) { // Auto Carousel Playback loop
    if (now - animationStart >= AUTO_MODE_CAROUSEL_TIME) {
      animationIndex = (animationIndex + 1) % TOTAL_ANIMATIONS;
      frameCounter = 0; animationStart = now; lastFrameTime = now;
    }
    if (now - lastFrameTime >= FRAME_TIME) {
      lastFrameTime = now;
      drawAnimationFrame(animationIndex, frameCounter); prepareDisplayData(); commitFrame();
      frameCounter = (frameCounter + 1) % 50;
    }
  }
  else if (currentCubeMode == 1) { // Manual Mode Continuous Playback loop
    if (now - lastFrameTime >= FRAME_TIME) {
      lastFrameTime = now;
      drawAnimationFrame(animationIndex, frameCounter); prepareDisplayData(); commitFrame();
      frameCounter = (frameCounter + 1) % 50;
    }
  }
}