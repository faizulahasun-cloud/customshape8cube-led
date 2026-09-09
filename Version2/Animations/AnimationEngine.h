#pragma once

#include <Arduino.h>
#include "../FunctionConversion/FunctionConversionEngine.h"
#include "../LEDs/LEDDefinitions.h"

// Every animation frame has its own FrameXXX.h pipeline stage.
#include "../Frames/Frame001.h"
#include "../Frames/Frame002.h"
#include "../Frames/Frame003.h"
#include "../Frames/Frame004.h"
#include "../Frames/Frame005.h"
#include "../Frames/Frame006.h"
#include "../Frames/Frame007.h"
#include "../Frames/Frame008.h"
#include "../Frames/Frame009.h"
#include "../Frames/Frame010.h"
#include "../Frames/Frame011.h"
#include "../Frames/Frame012.h"
#include "../Frames/Frame013.h"
#include "../Frames/Frame014.h"
#include "../Frames/Frame015.h"
#include "../Frames/Frame016.h"
#include "../Frames/Frame017.h"
#include "../Frames/Frame018.h"
#include "../Frames/Frame019.h"
#include "../Frames/Frame020.h"
#include "../Frames/Frame021.h"
#include "../Frames/Frame022.h"
#include "../Frames/Frame023.h"
#include "../Frames/Frame024.h"
#include "../Frames/Frame025.h"
#include "../Frames/Frame026.h"
#include "../Frames/Frame027.h"
#include "../Frames/Frame028.h"
#include "../Frames/Frame029.h"
#include "../Frames/Frame030.h"
#include "../Frames/Frame031.h"
#include "../Frames/Frame032.h"
#include "../Frames/Frame033.h"
#include "../Frames/Frame034.h"
#include "../Frames/Frame035.h"
#include "../Frames/Frame036.h"
#include "../Frames/Frame037.h"
#include "../Frames/Frame038.h"
#include "../Frames/Frame039.h"
#include "../Frames/Frame040.h"
#include "../Frames/Frame041.h"
#include "../Frames/Frame042.h"
#include "../Frames/Frame043.h"
#include "../Frames/Frame044.h"
#include "../Frames/Frame045.h"
#include "../Frames/Frame046.h"
#include "../Frames/Frame047.h"
#include "../Frames/Frame048.h"
#include "../Frames/Frame049.h"
#include "../Frames/Frame050.h"

// V2 Animation Engine
// Data path for every frame:
// Function Conversion -> LED001..LED512 definitions -> FrameXXX.h -> FrameEngine -> DisplayEngine.
// Only one 64-byte frame buffer is used at a time.
namespace V2Animation {

static const uint8_t TOTAL_FRAMES = 50;
static uint8_t currentFrame = 0;

inline bool evaluateCurrentVoxel(uint8_t X, uint8_t Y, uint8_t Z) {
  return V2FunctionConversion::evaluate(X, Y, Z, currentFrame);
}

inline void frameBegin(uint8_t frameIndex) {
  switch (frameIndex) {
    case 0: frame001Begin(); break; case 1: frame002Begin(); break;
    case 2: frame003Begin(); break; case 3: frame004Begin(); break;
    case 4: frame005Begin(); break; case 5: frame006Begin(); break;
    case 6: frame007Begin(); break; case 7: frame008Begin(); break;
    case 8: frame009Begin(); break; case 9: frame010Begin(); break;
    case 10: frame011Begin(); break; case 11: frame012Begin(); break;
    case 12: frame013Begin(); break; case 13: frame014Begin(); break;
    case 14: frame015Begin(); break; case 15: frame016Begin(); break;
    case 16: frame017Begin(); break; case 17: frame018Begin(); break;
    case 18: frame019Begin(); break; case 19: frame020Begin(); break;
    case 20: frame021Begin(); break; case 21: frame022Begin(); break;
    case 22: frame023Begin(); break; case 23: frame024Begin(); break;
    case 24: frame025Begin(); break; case 25: frame026Begin(); break;
    case 26: frame027Begin(); break; case 27: frame028Begin(); break;
    case 28: frame029Begin(); break; case 29: frame030Begin(); break;
    case 30: frame031Begin(); break; case 31: frame032Begin(); break;
    case 32: frame033Begin(); break; case 33: frame034Begin(); break;
    case 34: frame035Begin(); break; case 35: frame036Begin(); break;
    case 36: frame037Begin(); break; case 37: frame038Begin(); break;
    case 38: frame039Begin(); break; case 39: frame040Begin(); break;
    case 40: frame041Begin(); break; case 41: frame042Begin(); break;
    case 42: frame043Begin(); break; case 43: frame044Begin(); break;
    case 44: frame045Begin(); break; case 45: frame046Begin(); break;
    case 46: frame047Begin(); break; case 47: frame048Begin(); break;
    case 48: frame049Begin(); break; default: frame050Begin(); break;
  }
}

inline void frameSetLED(uint8_t frameIndex, uint16_t ledNumber) {
  switch (frameIndex) {
    case 0: frame001SetLED(ledNumber); break; case 1: frame002SetLED(ledNumber); break;
    case 2: frame003SetLED(ledNumber); break; case 3: frame004SetLED(ledNumber); break;
    case 4: frame005SetLED(ledNumber); break; case 5: frame006SetLED(ledNumber); break;
    case 6: frame007SetLED(ledNumber); break; case 7: frame008SetLED(ledNumber); break;
    case 8: frame009SetLED(ledNumber); break; case 9: frame010SetLED(ledNumber); break;
    case 10: frame011SetLED(ledNumber); break; case 11: frame012SetLED(ledNumber); break;
    case 12: frame013SetLED(ledNumber); break; case 13: frame014SetLED(ledNumber); break;
    case 14: frame015SetLED(ledNumber); break; case 15: frame016SetLED(ledNumber); break;
    case 16: frame017SetLED(ledNumber); break; case 17: frame018SetLED(ledNumber); break;
    case 18: frame019SetLED(ledNumber); break; case 19: frame020SetLED(ledNumber); break;
    case 20: frame021SetLED(ledNumber); break; case 21: frame022SetLED(ledNumber); break;
    case 22: frame023SetLED(ledNumber); break; case 23: frame024SetLED(ledNumber); break;
    case 24: frame025SetLED(ledNumber); break; case 25: frame026SetLED(ledNumber); break;
    case 26: frame027SetLED(ledNumber); break; case 27: frame028SetLED(ledNumber); break;
    case 28: frame029SetLED(ledNumber); break; case 29: frame030SetLED(ledNumber); break;
    case 30: frame031SetLED(ledNumber); break; case 31: frame032SetLED(ledNumber); break;
    case 32: frame033SetLED(ledNumber); break; case 33: frame034SetLED(ledNumber); break;
    case 34: frame035SetLED(ledNumber); break; case 35: frame036SetLED(ledNumber); break;
    case 36: frame037SetLED(ledNumber); break; case 37: frame038SetLED(ledNumber); break;
    case 38: frame039SetLED(ledNumber); break; case 39: frame040SetLED(ledNumber); break;
    case 40: frame041SetLED(ledNumber); break; case 41: frame042SetLED(ledNumber); break;
    case 42: frame043SetLED(ledNumber); break; case 43: frame044SetLED(ledNumber); break;
    case 44: frame045SetLED(ledNumber); break; case 45: frame046SetLED(ledNumber); break;
    case 46: frame047SetLED(ledNumber); break; case 47: frame048SetLED(ledNumber); break;
    case 48: frame049SetLED(ledNumber); break; default: frame050SetLED(ledNumber); break;
  }
}

inline void frameSubmit(uint8_t frameIndex) {
  switch (frameIndex) {
    case 0: frame001Submit(); break; case 1: frame002Submit(); break;
    case 2: frame003Submit(); break; case 3: frame004Submit(); break;
    case 4: frame005Submit(); break; case 5: frame006Submit(); break;
    case 6: frame007Submit(); break; case 7: frame008Submit(); break;
    case 8: frame009Submit(); break; case 9: frame010Submit(); break;
    case 10: frame011Submit(); break; case 11: frame012Submit(); break;
    case 12: frame013Submit(); break; case 13: frame014Submit(); break;
    case 14: frame015Submit(); break; case 15: frame016Submit(); break;
    case 16: frame017Submit(); break; case 17: frame018Submit(); break;
    case 18: frame019Submit(); break; case 19: frame020Submit(); break;
    case 20: frame021Submit(); break; case 21: frame022Submit(); break;
    case 22: frame023Submit(); break; case 23: frame024Submit(); break;
    case 24: frame025Submit(); break; case 25: frame026Submit(); break;
    case 26: frame027Submit(); break; case 27: frame028Submit(); break;
    case 28: frame029Submit(); break; case 29: frame030Submit(); break;
    case 30: frame031Submit(); break; case 31: frame032Submit(); break;
    case 32: frame033Submit(); break; case 33: frame034Submit(); break;
    case 34: frame035Submit(); break; case 35: frame036Submit(); break;
    case 36: frame037Submit(); break; case 37: frame038Submit(); break;
    case 38: frame039Submit(); break; case 39: frame040Submit(); break;
    case 40: frame041Submit(); break; case 41: frame042Submit(); break;
    case 42: frame043Submit(); break; case 43: frame044Submit(); break;
    case 44: frame045Submit(); break; case 45: frame046Submit(); break;
    case 46: frame047Submit(); break; case 47: frame048Submit(); break;
    case 48: frame049Submit(); break; default: frame050Submit(); break;
  }
}

inline bool generateFrame(uint8_t frameIndex) {
  if (!V2FunctionConversion::isFunctionValid()) return false;

  currentFrame = frameIndex % TOTAL_FRAMES;
  frameBegin(currentFrame);

  // Walk LED001..LED512 definitions. Each matching physical LED is handed to
  // the selected FrameXXX.h file before FrameEngine receives the completed frame.
  for (uint16_t ledIndex = 0; ledIndex < 512; ++ledIndex) {
    const V2LEDDefinitions::Definition &led = V2LEDDefinitions::DEFINITIONS[ledIndex];
    if (evaluateCurrentVoxel(led.x, led.y, led.z)) {
      frameSetLED(currentFrame, ledIndex + 1);
    }
  }

  // The selected FrameXXX.h then passes the same complete frame to FrameEngine.
  frameSubmit(currentFrame);
  return true;
}

inline void reset() { currentFrame = 0; }
inline bool start() { reset(); return generateFrame(0); }
inline bool generateNextFrame() { return generateFrame((currentFrame + 1) % TOTAL_FRAMES); }
inline uint8_t frameIndex() { return currentFrame; }

} // namespace V2Animation
