#pragma once
#include <Arduino.h>
#include <math.h>

// V3 Bluetooth Function Conversion Engine.
// Receives function characters supplied by the Arduino receiver and stores them
// until the receiver explicitly ends reception. The engine knows nothing about
// the Bluetooth control characters @, E, C, A, M, N, or R.
// The expression is compiled only when the firmware receives the R command.
// X,Y,Z are voxel coordinates 0..7; F is the animation frame 0..49.
// The engine never drives the cube or allocates LED frame buffers.
namespace V3FunctionConversion {

static const uint16_t MAX_FUNCTION_LENGTH = 192;
static const uint8_t MAX_BYTECODE_LENGTH = 80;
static const uint8_t EVALUATOR_STACK_SIZE = 16;
static const uint8_t FOURIER_MAX_HARMONICS = 8;
static const uint8_t FOURIER_N = 50;
static const uint8_t FOURIER_Q_SHIFT = 12;
static const int32_t FOURIER_Q_ONE = (1L << FOURIER_Q_SHIFT);
static const int32_t FOURIER_Q_LIMIT = 32767;

enum Representation:uint8_t {REP_BYTECODE=0,REP_FOURIER=1};
struct FourierRecord{
  int16_t dc;
  int16_t cosine[FOURIER_MAX_HARMONICS];
  int16_t sine[FOURIER_MAX_HARMONICS];
  uint8_t harmonics;
  bool valid;
};

enum OpCode:uint8_t{OP_END=0,OP_CONST,OP_X,OP_Y,OP_Z,OP_F,OP_ADD,OP_SUB,OP_MUL,OP_DIV,OP_MOD,OP_NEG,OP_SIN,OP_COS,OP_SQRT,OP_ABS,OP_NOT,OP_LT,OP_LE,OP_GT,OP_GE,OP_EQ,OP_NE,OP_AND,OP_OR};
struct Instruction{uint8_t op;float value;};

static char functionBuffer[MAX_FUNCTION_LENGTH+1];
static uint16_t functionLength=0;
static bool functionStarted=false;
static bool functionComplete=false;
static bool functionValid=false;
static bool receiveError=false;
static Instruction bytecode[MAX_BYTECODE_LENGTH];
static uint8_t bytecodeLength=0;
static uint16_t parsePosition=0;
static bool parseError=false;
static Representation representation=REP_BYTECODE;
static FourierRecord fourier={0,{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},0,false};
static const int16_t FOURIER_SIN_LUT[FOURIER_N] PROGMEM={
0,4107,8149,12062,15786,19260,22431,25247,27666,29648,31163,32187,32702,32702,32187,31163,29648,27666,25247,22431,
19260,15786,12062,8149,4107,0,-4107,-8149,-12062,-15786,-19260,-22431,-25247,-27666,-29648,-31163,-32187,-32702,-32702,
-32187,-31163,-29648,-27666,-25247,-22431,-19260,-15786,-12062,-8149,-4107};


inline void resetReception(){
  functionLength=0;functionStarted=true;functionComplete=false;functionValid=false;receiveError=false;parsePosition=0;parseError=false;
  representation=REP_BYTECODE;fourier.valid=false;fourier.harmonics=0;
  functionBuffer[0]='\0';
}
inline void startReception(){resetReception();}
inline void stopReception(){
  if(!functionStarted)return;
  functionBuffer[functionLength]='\0';
  functionComplete=(functionLength>0&&!receiveError);
  functionStarted=false;
}
inline bool receiveCharacter(char c){
  if(!functionStarted)return false;
  if(c=='\r')return false;
  if((uint8_t)c<0x20||(uint8_t)c>0x7E){receiveError=true;return false;}
  if(functionLength>=MAX_FUNCTION_LENGTH){receiveError=true;return false;}
  functionBuffer[functionLength++]=c;functionBuffer[functionLength]='\0';
  return false;
}
inline bool isFunctionStarted(){return functionStarted;}
inline bool isFunctionComplete(){return functionComplete;}
inline bool isFunctionValid(){return functionValid;}
inline uint16_t receivedLength(){return functionLength;}
inline void skipSpaces(){while(parsePosition<functionLength&&functionBuffer[parsePosition]==' ')parsePosition++;}
inline bool matchChar(char c){skipSpaces();if(parsePosition<functionLength&&functionBuffer[parsePosition]==c){parsePosition++;return true;}return false;}
inline bool emit(uint8_t op,float value=0.0f){if(bytecodeLength>=MAX_BYTECODE_LENGTH-1){parseError=true;return false;}bytecode[bytecodeLength].op=op;bytecode[bytecodeLength].value=value;bytecodeLength++;return true;}
inline bool parseExpression();

inline bool parseNumber(){
  skipSpaces();if(parsePosition>=functionLength||functionBuffer[parsePosition]<'0'||functionBuffer[parsePosition]>'9')return false;
  float value=0.0f;
  while(parsePosition<functionLength){char c=functionBuffer[parsePosition];if(c<'0'||c>'9')break;value=value*10.0f+(float)(c-'0');if(value>32767.0f){parseError=true;return false;}parsePosition++;}
  if(parsePosition<functionLength&&functionBuffer[parsePosition]=='.'){
    parsePosition++;if(parsePosition>=functionLength||functionBuffer[parsePosition]<'0'||functionBuffer[parsePosition]>'9'){parseError=true;return false;}
    float place=0.1f;while(parsePosition<functionLength){char c=functionBuffer[parsePosition];if(c<'0'||c>'9')break;value+=(float)(c-'0')*place;place*=0.1f;parsePosition++;}
  }
  return emit(OP_CONST,value);
}
inline char upperAscii(char c){return(c>='a'&&c<='z')?(char)(c-'a'+'A'):c;}
inline bool isReservedCommandToken(char c){
  c=upperAscii(c);
  return c=='A'||c=='M'||c=='N'||c=='C'||c=='E'||c=='R';
}
inline bool parseIdentifier(){
  skipSpaces();if(parsePosition>=functionLength)return false;uint16_t start=parsePosition;
  while(parsePosition<functionLength){char c=functionBuffer[parsePosition];if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')))break;parsePosition++;}
  if(start==parsePosition)return false;uint16_t n=parsePosition-start;
  // Only X,Y,Z,F are legal single-letter formula variables. All single-letter
  // Bluetooth command tokens are explicitly rejected here, never aliased.
  if(n==1){char q=upperAscii(functionBuffer[start]);if(q=='X')return emit(OP_X);if(q=='Y')return emit(OP_Y);if(q=='Z')return emit(OP_Z);if(q=='F')return emit(OP_F);if(isReservedCommandToken(q)){parseError=true;return false;}parseError=true;return false;}
  if(!matchChar('(')){parseError=true;return false;}if(!parseExpression()||!matchChar(')'))return false;
  char n0=n>0?upperAscii(functionBuffer[start]):0,n1=n>1?upperAscii(functionBuffer[start+1]):0,n2=n>2?upperAscii(functionBuffer[start+2]):0,n3=n>3?upperAscii(functionBuffer[start+3]):0;
  if(n==3&&n0=='S'&&n1=='I'&&n2=='N')return emit(OP_SIN);
  if(n==3&&n0=='C'&&n1=='O'&&n2=='S')return emit(OP_COS);
  if(n==4&&n0=='S'&&n1=='Q'&&n2=='R'&&n3=='T')return emit(OP_SQRT);
  if(n==3&&n0=='A'&&n1=='B'&&n2=='S')return emit(OP_ABS);
  parseError=true;return false;
}
inline bool parsePrimary(){skipSpaces();if(matchChar('(')){if(!parseExpression())return false;return matchChar(')');}if(parseNumber())return true;return parseIdentifier();}
inline bool parseUnary(){skipSpaces();if(matchChar('-')){if(!parseUnary())return false;return emit(OP_NEG);}if(matchChar('!')){if(!parseUnary())return false;return emit(OP_NOT);}return parsePrimary();}
inline bool parseMultiplication(){if(!parseUnary())return false;while(true){if(matchChar('*')){if(!parseUnary()||!emit(OP_MUL))return false;}else if(matchChar('/')){if(!parseUnary()||!emit(OP_DIV))return false;}else if(matchChar('%')){if(!parseUnary()||!emit(OP_MOD))return false;}else return true;}}
inline bool parseAddition(){if(!parseMultiplication())return false;while(true){if(matchChar('+')){if(!parseMultiplication()||!emit(OP_ADD))return false;}else if(matchChar('-')){if(!parseMultiplication()||!emit(OP_SUB))return false;}else return true;}}
inline bool parseComparison(){
  if(!parseAddition())return false;skipSpaces();if(parsePosition>=functionLength)return true;char a=functionBuffer[parsePosition],b=(parsePosition+1<functionLength)?functionBuffer[parsePosition+1]:'\0';uint8_t op=OP_END;
  if(a=='<'&&b=='=')op=OP_LE;else if(a=='>'&&b=='=')op=OP_GE;else if(a=='='&&b=='=')op=OP_EQ;else if(a=='!'&&b=='=')op=OP_NE;else if(a=='<')op=OP_LT;else if(a=='>')op=OP_GT;else return true;
  parsePosition+=(b=='='?2:1);if(!parseAddition()||!emit(op))return false;return true;
}
inline bool parseLogicalAnd(){if(!parseComparison())return false;while(true){skipSpaces();if(parsePosition+1<functionLength&&functionBuffer[parsePosition]=='&'&&functionBuffer[parsePosition+1]=='&'){parsePosition+=2;if(!parseComparison()||!emit(OP_AND))return false;}else return true;}}
inline bool parseExpression(){if(!parseLogicalAnd())return false;while(true){skipSpaces();if(parsePosition+1<functionLength&&functionBuffer[parsePosition]=='|'&&functionBuffer[parsePosition+1]=='|'){parsePosition+=2;if(!parseLogicalAnd()||!emit(OP_OR))return false;}else return true;}}

inline bool evaluateBytecode(uint8_t X,uint8_t Y,uint8_t Z,uint8_t F){
  if(!functionValid)return false;float stack[EVALUATOR_STACK_SIZE];uint8_t sp=0;
  for(uint8_t i=0;i<bytecodeLength;i++){const Instruction& ins=bytecode[i];switch(ins.op){
    case OP_END:return sp?(stack[sp-1]!=0.0f):false;
    case OP_CONST:if(sp>=EVALUATOR_STACK_SIZE)return false;stack[sp++]=ins.value;break;
    case OP_X:if(sp>=EVALUATOR_STACK_SIZE)return false;stack[sp++]=X;break;
    case OP_Y:if(sp>=EVALUATOR_STACK_SIZE)return false;stack[sp++]=Y;break;
    case OP_Z:if(sp>=EVALUATOR_STACK_SIZE)return false;stack[sp++]=Z;break;
    case OP_F:if(sp>=EVALUATOR_STACK_SIZE)return false;stack[sp++]=F;break;
    case OP_NEG:if(!sp)return false;stack[sp-1]=-stack[sp-1];break;
    case OP_NOT:if(!sp)return false;stack[sp-1]=(stack[sp-1]==0.0f);break;
    case OP_SIN:if(!sp)return false;stack[sp-1]=sin(stack[sp-1]);break;
    case OP_COS:if(!sp)return false;stack[sp-1]=cos(stack[sp-1]);break;
    case OP_SQRT:if(!sp)return false;stack[sp-1]=sqrt(max(0.0f,stack[sp-1]));break;
    case OP_ABS:if(!sp)return false;stack[sp-1]=fabs(stack[sp-1]);break;
    case OP_ADD:if(sp<2)return false;stack[sp-2]+=stack[--sp];break;
    case OP_SUB:if(sp<2)return false;stack[sp-2]-=stack[--sp];break;
    case OP_MUL:if(sp<2)return false;stack[sp-2]*=stack[--sp];break;
    case OP_DIV:if(sp<2||stack[sp-1]==0.0f)return false;stack[sp-2]/=stack[--sp];break;
    case OP_MOD:if(sp<2||stack[sp-1]==0.0f)return false;stack[sp-2]=fmod(stack[sp-2],stack[--sp]);break;
    case OP_LT:if(sp<2)return false;stack[sp-2]=stack[sp-2]<stack[--sp];break;
    case OP_LE:if(sp<2)return false;stack[sp-2]=stack[sp-2]<=stack[--sp];break;
    case OP_GT:if(sp<2)return false;stack[sp-2]=stack[sp-2]>stack[--sp];break;
    case OP_GE:if(sp<2)return false;stack[sp-2]=stack[sp-2]>=stack[--sp];break;
    case OP_EQ:if(sp<2)return false;stack[sp-2]=stack[sp-2]==stack[--sp];break;
    case OP_NE:if(sp<2)return false;stack[sp-2]=stack[sp-2]!=stack[--sp];break;
    case OP_AND:if(sp<2)return false;stack[sp-2]=(stack[sp-2]!=0.0f)&&(stack[--sp]!=0.0f);break;
    case OP_OR:if(sp<2)return false;stack[sp-2]=(stack[sp-2]!=0.0f)||(stack[--sp]!=0.0f);break;
    default:return false;
  }}return false;
}

inline bool fourierCandidateIsFOnly(){
  for(uint8_t i=0;i<bytecodeLength;i++){uint8_t op=bytecode[i].op;if(op==OP_X||op==OP_Y||op==OP_Z)return false;}
  return true;
}
inline bool fourierQuantize(float value,int16_t& out){
  if(value < -7.999f || value > 7.999f)return false;
  long q=lround(value*(float)FOURIER_Q_ONE);
  if(q < -FOURIER_Q_LIMIT || q > FOURIER_Q_LIMIT)return false;
  out=(int16_t)q;return true;
}
inline int32_t fourierEvaluateQ(uint8_t F){
  int32_t sum=(int32_t)fourier.dc;
  for(uint8_t k=1;k<=fourier.harmonics;k++){
    uint8_t idx=(uint8_t)((uint16_t)k*F%FOURIER_N);
    int16_t s=pgm_read_word(&FOURIER_SIN_LUT[idx]);
    int16_t co=pgm_read_word(&FOURIER_SIN_LUT[(idx+FOURIER_N/4)%FOURIER_N]);
    sum+=((int32_t)fourier.cosine[k-1]*co)>>15;
    sum+=((int32_t)fourier.sine[k-1]*s)>>15;
  }
  return sum;
}
inline bool tryCompileFourier(){
  fourier.valid=false;fourier.harmonics=0;
  if(!fourierCandidateIsFOnly())return false;
  float samples[FOURIER_N];
  for(uint8_t f=0;f<FOURIER_N;f++){
    float stack[EVALUATOR_STACK_SIZE];uint8_t sp=0;float result=0.0f;
    for(uint8_t i=0;i<bytecodeLength;i++){
      const Instruction& ins=bytecode[i];
      switch(ins.op){
        case OP_END:result=sp?stack[sp-1]:0.0f;i=bytecodeLength;break;
        case OP_CONST:if(sp>=EVALUATOR_STACK_SIZE)return false;stack[sp++]=ins.value;break;
        case OP_F:if(sp>=EVALUATOR_STACK_SIZE)return false;stack[sp++]=f;break;
        case OP_NEG:if(!sp)return false;stack[sp-1]=-stack[sp-1];break;
        case OP_NOT:if(!sp)return false;stack[sp-1]=(stack[sp-1]==0.0f);break;
        case OP_SIN:if(!sp)return false;stack[sp-1]=sin(stack[sp-1]);break;
        case OP_COS:if(!sp)return false;stack[sp-1]=cos(stack[sp-1]);break;
        case OP_SQRT:if(!sp)return false;stack[sp-1]=sqrt(max(0.0f,stack[sp-1]));break;
        case OP_ABS:if(!sp)return false;stack[sp-1]=fabs(stack[sp-1]);break;
        case OP_ADD:if(sp<2)return false;stack[sp-2]+=stack[--sp];break;
        case OP_SUB:if(sp<2)return false;stack[sp-2]-=stack[--sp];break;
        case OP_MUL:if(sp<2)return false;stack[sp-2]*=stack[--sp];break;
        case OP_DIV:if(sp<2||stack[sp-1]==0.0f)return false;stack[sp-2]/=stack[--sp];break;
        case OP_MOD:if(sp<2||stack[sp-1]==0.0f)return false;stack[sp-2]=fmod(stack[sp-2],stack[--sp]);break;
        case OP_LT:if(sp<2)return false;stack[sp-2]=stack[sp-2]<stack[--sp];break;
        case OP_LE:if(sp<2)return false;stack[sp-2]=stack[sp-2]<=stack[--sp];break;
        case OP_GT:if(sp<2)return false;stack[sp-2]=stack[sp-2]>stack[--sp];break;
        case OP_GE:if(sp<2)return false;stack[sp-2]=stack[sp-2]>=stack[--sp];break;
        case OP_EQ:if(sp<2)return false;stack[sp-2]=stack[sp-2]==stack[--sp];break;
        case OP_NE:if(sp<2)return false;stack[sp-2]=stack[sp-2]!=stack[--sp];break;
        case OP_AND:if(sp<2)return false;stack[sp-2]=(stack[sp-2]!=0.0f)&&(stack[--sp]!=0.0f);break;
        case OP_OR:if(sp<2)return false;stack[sp-2]=(stack[sp-2]!=0.0f)||(stack[--sp]!=0.0f);break;
        default:return false;
      }
    }
    samples[f]=result;
  }
  for(uint8_t H=1;H<=FOURIER_MAX_HARMONICS;H++){
    float dc=0.0f;for(uint8_t f=0;f<FOURIER_N;f++)dc+=samples[f];dc/=FOURIER_N;
    FourierRecord candidate;candidate.dc=0;candidate.harmonics=H;candidate.valid=false;
    for(uint8_t k=0;k<FOURIER_MAX_HARMONICS;k++){candidate.cosine[k]=0;candidate.sine[k]=0;}
    if(!fourierQuantize(dc,candidate.dc))continue;
    bool ok=true;
    for(uint8_t k=1;k<=H;k++){
      float ak=0.0f,bk=0.0f;
      for(uint8_t f=0;f<FOURIER_N;f++){
        float phase=6.28318530718f*(float)k*(float)f/(float)FOURIER_N;
        ak+=samples[f]*cos(phase);bk+=samples[f]*sin(phase);
      }
      ak*=2.0f/(float)FOURIER_N;bk*=2.0f/(float)FOURIER_N;
      if(!fourierQuantize(ak,candidate.cosine[k-1])||!fourierQuantize(bk,candidate.sine[k-1])){ok=false;break;}
    }
    if(!ok)continue;
    fourier=candidate;bool exact=true;
    for(uint8_t f=0;f<FOURIER_N;f++){
      if((samples[f]!=0.0f)!=(fourierEvaluateQ(f)!=0)){exact=false;break;}
    }
    if(exact){fourier.valid=true;return true;}
  }
  fourier.valid=false;return false;
}

inline bool compileFunction(){
  functionValid=false;bytecodeLength=0;parsePosition=0;parseError=receiveError;
  representation=REP_BYTECODE;fourier.valid=false;fourier.harmonics=0;
  if(!functionComplete||functionLength==0||receiveError)return false;
  if(!parseExpression())return false;
  skipSpaces();if(parsePosition!=functionLength||!emit(OP_END)){bytecodeLength=0;return false;}
  functionValid=true;
  if(tryCompileFourier())representation=REP_FOURIER;
  return true;
}
inline bool evaluate(uint8_t X,uint8_t Y,uint8_t Z,uint8_t F){
  if(!functionValid)return false;
  if(representation==REP_FOURIER)return fourierEvaluateQ(F)!=0;
  return evaluateBytecode(X,Y,Z,F);
}
inline Representation getRepresentation(){return representation;}
inline uint8_t getFourierHarmonics(){return fourier.harmonics;}
} // namespace V3FunctionConversion
