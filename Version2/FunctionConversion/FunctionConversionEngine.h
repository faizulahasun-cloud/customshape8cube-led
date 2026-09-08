#pragma once

#include <Arduino.h>
#include <math.h>

// V2 Function Conversion Engine
//
// Responsibility:
//   Bluetooth/text stream -> function character buffer -> validation -> compact bytecode
//
// It does NOT generate frames and does NOT drive the display.
// The Animation Engine will consume the compiled function later.
//
// Coordinate variables:
//   X, Y, Z = 0..7 voxel coordinates
//   F       = animation frame index
//
// Input completion:
//   '\n' completes a function.
//   '\r' is ignored so CRLF input is also accepted.
//
// The input vocabulary is 96 characters: printable ASCII 0x20..0x7E (95
// keyboard characters) plus '\n' as the function terminator.
// Only characters actually received are stored in the input buffer.

namespace V2FunctionConversion {

static const uint16_t MAX_FUNCTION_LENGTH = 255;
static const uint8_t MAX_BYTECODE_LENGTH = 96;

// ---------------------------------------------------------------------------
// 96-character input vocabulary
// ---------------------------------------------------------------------------
inline bool isAllowedCharacter(char c) {
  return ((uint8_t)c >= 0x20 && (uint8_t)c <= 0x7E) || c == '\n';
}

// ---------------------------------------------------------------------------
// Compact bytecode
// ---------------------------------------------------------------------------
enum OpCode : uint8_t {
  OP_END = 0,
  OP_CONST,
  OP_X,
  OP_Y,
  OP_Z,
  OP_F,
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_MOD,
  OP_NEG,
  OP_SIN,
  OP_COS,
  OP_SQRT,
  OP_ABS,
  OP_NOT,
  OP_LT,
  OP_LE,
  OP_GT,
  OP_GE,
  OP_EQ,
  OP_NE,
  OP_AND,
  OP_OR
};

struct Instruction {
  uint8_t op;
  int16_t value;
};

// ---------------------------------------------------------------------------
// Engine state
// ---------------------------------------------------------------------------
static char functionBuffer[MAX_FUNCTION_LENGTH + 1];
static uint16_t functionLength = 0;
static bool functionComplete = false;
static bool functionValid = false;
static Instruction bytecode[MAX_BYTECODE_LENGTH];
static uint8_t bytecodeLength = 0;
static uint16_t parsePosition = 0;
static bool parseError = false;

inline void clearFunction() {
  functionLength = 0;
  functionComplete = false;
  functionValid = false;
  bytecodeLength = 0;
  parsePosition = 0;
  parseError = false;
  functionBuffer[0] = '\0';
  bytecode[0].op = OP_END;
  bytecode[0].value = 0;
}

// ---------------------------------------------------------------------------
// Receive one character at a time.
// Returns true only when '\n' completes the function.
// ---------------------------------------------------------------------------
inline bool receiveCharacter(char c) {
  if (!isAllowedCharacter(c)) {
    parseError = true;
    return false;
  }

  if (c == '\r') {
    return false;
  }

  if (c == '\n') {
    functionBuffer[functionLength] = '\0';
    functionComplete = true;
    return true;
  }

  if (functionLength >= MAX_FUNCTION_LENGTH) {
    parseError = true;
    return false;
  }

  functionBuffer[functionLength++] = c;
  functionBuffer[functionLength] = '\0';
  return false;
}

inline const char *receivedFunction() {
  return functionBuffer;
}

inline uint16_t receivedLength() {
  return functionLength;
}

inline bool isFunctionComplete() {
  return functionComplete;
}

inline bool isFunctionValid() {
  return functionValid;
}

inline const Instruction *compiledFunction() {
  return bytecode;
}

inline uint8_t compiledLength() {
  return bytecodeLength;
}

// ---------------------------------------------------------------------------
// Parser helpers
// ---------------------------------------------------------------------------
inline void skipSpaces() {
  while (parsePosition < functionLength && functionBuffer[parsePosition] == ' ') {
    parsePosition++;
  }
}

inline bool matchChar(char c) {
  skipSpaces();
  if (parsePosition < functionLength && functionBuffer[parsePosition] == c) {
    parsePosition++;
    return true;
  }
  return false;
}

inline bool emit(uint8_t op, int16_t value = 0) {
  if (bytecodeLength >= MAX_BYTECODE_LENGTH - 1) {
    parseError = true;
    return false;
  }
  bytecode[bytecodeLength].op = op;
  bytecode[bytecodeLength].value = value;
  bytecodeLength++;
  return true;
}

inline bool parseExpression();

inline bool parseNumber() {
  skipSpaces();
  if (parsePosition >= functionLength || functionBuffer[parsePosition] < '0' || functionBuffer[parsePosition] > '9') {
    return false;
  }

  int16_t value = 0;
  while (parsePosition < functionLength) {
    char c = functionBuffer[parsePosition];
    if (c < '0' || c > '9') break;
    value = (int16_t)(value * 10 + (c - '0'));
    if (value > 32767) {
      parseError = true;
      return false;
    }
    parsePosition++;
  }
  return emit(OP_CONST, value);
}

inline bool parseIdentifier() {
  skipSpaces();
  if (parsePosition >= functionLength) return false;

  uint16_t start = parsePosition;
  while (parsePosition < functionLength) {
    char c = functionBuffer[parsePosition];
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) break;
    parsePosition++;
  }
  if (start == parsePosition) return false;

  uint16_t n = parsePosition - start;
  const char *name = &functionBuffer[start];

  if (n == 1 && name[0] == 'X') return emit(OP_X);
  if (n == 1 && name[0] == 'Y') return emit(OP_Y);
  if (n == 1 && name[0] == 'Z') return emit(OP_Z);
  if (n == 1 && name[0] == 'F') return emit(OP_F);

  // Function names.
  if (matchChar('(')) {
    bool ok = parseExpression() && matchChar(')');
    if (!ok) return false;

    if (n == 3 && name[0] == 'S' && name[1] == 'I' && name[2] == 'N') return emit(OP_SIN);
    if (n == 3 && name[0] == 'C' && name[1] == 'O' && name[2] == 'S') return emit(OP_COS);
    if (n == 4 && name[0] == 'S' && name[1] == 'Q' && name[2] == 'R' && name[3] == 'T') return emit(OP_SQRT);
    if (n == 3 && name[0] == 'A' && name[1] == 'B' && name[2] == 'S') return emit(OP_ABS);
  }

  parseError = true;
  return false;
}

inline bool parsePrimary() {
  skipSpaces();
  if (matchChar('(')) {
    if (!parseExpression()) return false;
    return matchChar(')');
  }

  if (parseNumber()) return true;
  return parseIdentifier();
}

inline bool parseUnary() {
  skipSpaces();
  if (matchChar('-')) {
    if (!parseUnary()) return false;
    return emit(OP_NEG);
  }
  if (matchChar('!')) {
    if (!parseUnary()) return false;
    return emit(OP_NOT);
  }
  return parsePrimary();
}

inline bool parseMultiplication() {
  if (!parseUnary()) return false;
  while (true) {
    if (matchChar('*')) {
      if (!parseUnary() || !emit(OP_MUL)) return false;
    } else if (matchChar('/')) {
      if (!parseUnary() || !emit(OP_DIV)) return false;
    } else if (matchChar('%')) {
      if (!parseUnary() || !emit(OP_MOD)) return false;
    } else {
      return true;
    }
  }
}

inline bool parseAddition() {
  if (!parseMultiplication()) return false;
  while (true) {
    if (matchChar('+')) {
      if (!parseMultiplication() || !emit(OP_ADD)) return false;
    } else if (matchChar('-')) {
      if (!parseMultiplication() || !emit(OP_SUB)) return false;
    } else {
      return true;
    }
  }
}

inline bool parseComparison() {
  if (!parseAddition()) return false;

  skipSpaces();
  if (parsePosition + 1 < functionLength) {
    char a = functionBuffer[parsePosition];
    char b = functionBuffer[parsePosition + 1];
    uint8_t op = OP_END;
    if (a == '<' && b == '=') op = OP_LE;
    else if (a == '>' && b == '=') op = OP_GE;
    else if (a == '=' && b == '=') op = OP_EQ;
    else if (a == '!' && b == '=') op = OP_NE;
    if (op != OP_END) {
      parsePosition += 2;
      if (!parseAddition() || !emit(op)) return false;
    } else if (a == '<' || a == '>') {
      parsePosition++;
      if (!parseAddition() || !emit(a == '<' ? OP_LT : OP_GT)) return false;
    }
  }
  return true;
}

inline bool parseLogicalAnd() {
  if (!parseComparison()) return false;
  while (true) {
    skipSpaces();
    if (parsePosition + 1 < functionLength && functionBuffer[parsePosition] == '&' && functionBuffer[parsePosition + 1] == '&') {
      parsePosition += 2;
      if (!parseComparison() || !emit(OP_AND)) return false;
    } else {
      return true;
    }
  }
}

inline bool parseExpression() {
  if (!parseLogicalAnd()) return false;
  while (true) {
    skipSpaces();
    if (parsePosition + 1 < functionLength && functionBuffer[parsePosition] == '|' && functionBuffer[parsePosition + 1] == '|') {
      parsePosition += 2;
      if (!parseLogicalAnd() || !emit(OP_OR)) return false;
    } else {
      return true;
    }
  }
}

// ---------------------------------------------------------------------------
// Compile the received text into compact bytecode.
// ---------------------------------------------------------------------------
inline bool compileFunction() {
  functionValid = false;
  bytecodeLength = 0;
  parsePosition = 0;
  parseError = false;

  if (!functionComplete || functionLength == 0) {
    parseError = true;
    return false;
  }

  if (!parseExpression()) return false;
  skipSpaces();
  if (parsePosition != functionLength) {
    parseError = true;
    return false;
  }
  if (!emit(OP_END)) return false;

  functionValid = true;
  return true;
}

// ---------------------------------------------------------------------------
// Evaluate compiled function for one voxel/frame.
// This is the hand-off point the Animation Engine can call later.
// ---------------------------------------------------------------------------
inline bool evaluate(uint8_t X, uint8_t Y, uint8_t Z, uint8_t F) {
  if (!functionValid) return false;

  float stack[24];
  uint8_t sp = 0;

  for (uint8_t i = 0; i < bytecodeLength; i++) {
    const Instruction &ins = bytecode[i];
    switch (ins.op) {
      case OP_END:
        return sp ? (stack[sp - 1] != 0.0f) : false;
      case OP_CONST: if (sp >= 24) return false; stack[sp++] = ins.value; break;
      case OP_X: if (sp >= 24) return false; stack[sp++] = X; break;
      case OP_Y: if (sp >= 24) return false; stack[sp++] = Y; break;
      case OP_Z: if (sp >= 24) return false; stack[sp++] = Z; break;
      case OP_F: if (sp >= 24) return false; stack[sp++] = F; break;
      case OP_NEG: if (!sp) return false; stack[sp-1] = -stack[sp-1]; break;
      case OP_NOT: if (!sp) return false; stack[sp-1] = (stack[sp-1] == 0.0f); break;
      case OP_SIN: if (!sp) return false; stack[sp-1] = sin(stack[sp-1]); break;
      case OP_COS: if (!sp) return false; stack[sp-1] = cos(stack[sp-1]); break;
      case OP_SQRT: if (!sp) return false; stack[sp-1] = sqrt(max(0.0f, stack[sp-1])); break;
      case OP_ABS: if (!sp) return false; stack[sp-1] = fabs(stack[sp-1]); break;
      case OP_ADD: if (sp < 2) return false; stack[sp-2] += stack[--sp]; break;
      case OP_SUB: if (sp < 2) return false; stack[sp-2] -= stack[--sp]; break;
      case OP_MUL: if (sp < 2) return false; stack[sp-2] *= stack[--sp]; break;
      case OP_DIV: if (sp < 2 || stack[sp-1] == 0.0f) return false; stack[sp-2] /= stack[--sp]; break;
      case OP_MOD: if (sp < 2 || stack[sp-1] == 0.0f) return false; stack[sp-2] = fmod(stack[sp-2], stack[--sp]); break;
      case OP_LT: if (sp < 2) return false; stack[sp-2] = stack[sp-2] < stack[--sp]; break;
      case OP_LE: if (sp < 2) return false; stack[sp-2] = stack[sp-2] <= stack[--sp]; break;
      case OP_GT: if (sp < 2) return false; stack[sp-2] = stack[sp-2] > stack[--sp]; break;
      case OP_GE: if (sp < 2) return false; stack[sp-2] = stack[sp-2] >= stack[--sp]; break;
      case OP_EQ: if (sp < 2) return false; stack[sp-2] = stack[sp-2] == stack[--sp]; break;
      case OP_NE: if (sp < 2) return false; stack[sp-2] = stack[sp-2] != stack[--sp]; break;
      case OP_AND: if (sp < 2) return false; stack[sp-2] = (stack[sp-2] != 0.0f) && (stack[--sp] != 0.0f); break;
      case OP_OR: if (sp < 2) return false; stack[sp-2] = (stack[sp-2] != 0.0f) || (stack[--sp] != 0.0f); break;
      default: return false;
    }
  }
  return false;
}

} // namespace V2FunctionConversion
