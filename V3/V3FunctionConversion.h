#pragma once
#include <Arduino.h>
#include <math.h>

// V3 Bluetooth Function Conversion Engine.
// Receives one ASCII boolean expression after '@' and compiles it to postfix bytecode.
// X,Y,Z are voxel coordinates 0..7; F is the animation frame 0..49.
// The engine never drives the cube or allocates LED frame buffers.
namespace V3FunctionConversion {

static const uint16_t MAX_FUNCTION_LENGTH = 92;
static const uint8_t MAX_BYTECODE_LENGTH = 56;
static const char FUNCTION_START='@';
static const char FUNCTION_END='\n';

enum OpCode:uint8_t{OP_END=0,OP_CONST,OP_X,OP_Y,OP_Z,OP_F,OP_ADD,OP_SUB,OP_MUL,OP_DIV,OP_MOD,OP_NEG,OP_SIN,OP_COS,OP_SQRT,OP_ABS,OP_NOT,OP_LT,OP_LE,OP_GT,OP_GE,OP_EQ,OP_NE,OP_AND,OP_OR};
struct Instruction{uint8_t op;float value;};

static char functionBuffer[MAX_FUNCTION_LENGTH+1];
static uint16_t functionLength=0;
static bool functionStarted=false;
static bool functionComplete=false;
static bool functionValid=false;
static bool receiveError=false;
static Instruction bytecode[MAX_BYTECODE_LENGTH];