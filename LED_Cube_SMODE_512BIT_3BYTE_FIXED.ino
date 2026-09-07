#include <Arduino.h>
#include <avr/interrupt.h>
#include <AltSoftSerial.h>
#include <math.h>

// COORDINATE CONTRACT (physical cube):
// X = columns 1..8 from left to right on the FRONT face (x=0..7).
// Y = depth, with the FRONT face at y=0 and the rear face at y=7.
// Z = vertical layers, bottom z=0 to top z=7.
// Every built-in and custom animation uses this same coordinate system.
const byte DATA_PIN = 11, CLOCK_PIN = 13, LATCH_PIN = 12, TOUCH_PIN = 10, POT_PIN = A0, BLE_STATE_PIN = 2;
AltSoftSerial bluetooth;
volatile byte currentCubeMode = 0;
byte globalBrightness = 4;
unsigned int animationIndex = 0;
byte frameCounter = 0;
const unsigned int TOTAL_ANIMATIONS = 27, FRAME_TIME = 200;
const unsigned long AUTO_MODE_CAROUSEL_TIME = 10000UL;
const unsigned long STARTUP_DELAY_TIME = 3000UL;
unsigned long lastFrameTime = 0, animationStart = 0;
volatile byte displayBuffer[8][8];
byte parseMode = 0;
volatile byte brightnessAccumulator[8] = {0,0,0,0,0,0,0,0};
bool lastBluetoothConnected = false;
unsigned long bluetoothStateChangedAt = 0;
const unsigned long BLE_STATE_DEBOUNCE_TIME = 3000UL;

struct ColumnMap { byte reg; byte bit; };
// First 8 entries are physical front-face columns 1..8.
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
inline byte columnIndex(byte x,byte y){ return y*8+x; }

void copyFrameToDisplay(const byte source[8][8]){
  noInterrupts();
  memcpy((void*)displayBuffer, source, 64);
  interrupts();
}
void clearDisplayBuffer(){
  byte blank[8][8] = {};
  copyFrameToDisplay(blank);
}
void showSolidAcknowledgment(byte value, unsigned int holdMs){
  byte ack[8][8];
  for(byte z=0; z<8; z++) for(byte r=0; r<8; r++) ack[z][r]=value;
  copyFrameToDisplay(ack);
  delay(holdMs);
  clearDisplayBuffer();
}
void triggerModeBlinkAcknowledgment(){
  showSolidAcknowledgment(0xFF,80);
  delay(80);
}

// A=auto, M=manual, N=next animation, C=begin custom upload, X=start custom,
// S=stop custom, B=brightness + one byte value.
void sendAck(const __FlashStringHelper *msg){ bluetooth.println(msg); }

enum ExprOp : byte {
  E_END=0, E_CONST, E_X, E_Y, E_Z, E_F,
  E_ADD, E_SUB, E_MUL, E_DIV, E_MOD, E_NEG,
  E_SIN, E_COS, E_SQRT, E_ABS, E_NOT,
  E_LT, E_LE, E_GT, E_GE, E_EQ, E_NE, E_AND, E_OR
};

struct ExprInstr { uint32_t code; };
ExprInstr customProgram[96];
byte customProgramLength=0;
bool customProgramValid=false;
char rxBuffer[512];
unsigned int rxLength=0;

inline uint32_t exprFloatBits(float value){ union { float f; uint32_t u; } v; v.f=value; return v.u; }
inline float exprBitsFloat(uint32_t value){ union { float f; uint32_t u; } v; v.u=value; return v.f; }
inline bool isPackedExprOp(uint32_t code){ return (code & 0x7FC00000UL)==0x7FC00000UL; }

int exprPrecedence(byte op){
  if(op==E_OR) return 1;
  if(op==E_AND) return 2;
  if(op==E_LT || op==E_LE || op==E_GT || op==E_GE || op==E_EQ || op==E_NE) return 3;
  if(op==E_ADD || op==E_SUB) return 4;
  if(op==E_MUL || op==E_DIV || op==E_MOD) return 5;
  if(op==E_NEG || op==E_NOT || op==E_SIN || op==E_COS || op==E_SQRT || op==E_ABS) return 6;
  return 0;
}
bool exprRightAssociative(byte op){ return op==E_NEG || op==E_NOT; }
bool exprEmit(byte op, float value=0.0f){
  if(customProgramLength>=95) return false;
  customProgram[customProgramLength].code=(op==E_CONST)?exprFloatBits(value):(0x7FC00000UL|(uint32_t)op);
  customProgramLength++;
  return true;
}
bool exprPopOperator(byte stack[], byte &top){ if(top==0) return false; return exprEmit(stack[--top]); }

bool compileCustomExpression(const char *src){
  customProgramLength=0; customProgramValid=false;
  byte ops[96]; byte top=0; bool expectValue=true;
  for(unsigned int i=0; src[i]!='\0'; ){
    char c=src[i];
    if(c==' ' || c=='\t'){ i++; continue; }
    if((c>='0' && c<='9') || c=='.'){
      if(!expectValue) return false;
      char *endPtr; float value=strtod(src+i,&endPtr);
      if(endPtr==src+i || !exprEmit(E_CONST,value)) return false;
      i=(unsigned int)(endPtr-src); expectValue=false; continue;
    }
    if((c>='A' && c<='Z') || (c>='a' && c<='z') || c=='_'){
      if(!expectValue) return false;
      char name[20]; byte n=0;
      while(((src[i]>='A' && src[i]<='Z') || (src[i]>='a' && src[i]<='z') || (src[i]>='0' && src[i]<='9') || src[i]=='_') && n<19) name[n++]=src[i++];
      name[n]='\0'; for(byte k=0;k<n;k++) if(name[k]>='A' && name[k]<='Z') name[k]+=('a'-'A');
      if(!strcmp(name,"x")){ if(!exprEmit(E_X)) return false; expectValue=false; continue; }
      if(!strcmp(name,"y")){ if(!exprEmit(E_Y)) return false; expectValue=false; continue; }
      if(!strcmp(name,"z")){ if(!exprEmit(E_Z)) return false; expectValue=false; continue; }
      if(!strcmp(name,"f") || !strcmp(name,"t")){ if(!exprEmit(E_F)) return false; expectValue=false; continue; }
      byte fn=0;
      if(!strcmp(name,"sin")) fn=E_SIN; else if(!strcmp(name,"cos")) fn=E_COS; else if(!strcmp(name,"sqrt")) fn=E_SQRT; else if(!strcmp(name,"abs")) fn=E_ABS; else return false;
      if(top>=95) return false; ops[top++]=fn; expectValue=true; continue;
    }
    if(c=='('){ if(!expectValue || top>=95) return false; ops[top++]=0xFF; i++; expectValue=true; continue; }
    if(c==')'){
      if(expectValue) return false;
      while(top && ops[top-1]!=0xFF) if(!exprPopOperator(ops,top)) return false;
      if(!top) return false; top--;
      if(top && (ops[top-1]==E_SIN || ops[top-1]==E_COS || ops[top-1]==E_SQRT || ops[top-1]==E_ABS)) if(!exprPopOperator(ops,top)) return false;
      i++; expectValue=false; continue;
    }
    byte op=E_END, tokenLen=1;
    if(c=='|' && src[i+1]=='|'){ op=E_OR; tokenLen=2; }
    else if(c=='&' && src[i+1]=='&'){ op=E_AND; tokenLen=2; }
    else if(c=='<' && src[i+1]=='='){ op=E_LE; tokenLen=2; }
    else if(c=='>' && src[i+1]=='='){ op=E_GE; tokenLen=2; }
    else if(c=='=' && src[i+1]=='='){ op=E_EQ; tokenLen=2; }
    else if(c=='!' && src[i+1]=='='){ op=E_NE; tokenLen=2; }
    else if(c=='!') op=E_NOT;
    else if(c=='<') op=E_LT;
    else if(c=='>') op=E_GT;
    else if(c=='+') op=E_ADD;
    else if(c=='-') op=expectValue ? E_NEG : E_SUB;
    else if(c=='*') op=E_MUL;
    else if(c=='/') op=E_DIV;
    else if(c=='%') op=E_MOD;
    else return false;
    if(expectValue && op!=E_NEG && op!=E_NOT) return false;
    while(top && ops[top-1]!=0xFF){
      byte previous=ops[top-1];
      bool popIt=(!exprRightAssociative(op) && exprPrecedence(op)<=exprPrecedence(previous)) || (exprRightAssociative(op) && exprPrecedence(op)<exprPrecedence(previous));
      if(!popIt) break; if(!exprPopOperator(ops,top)) return false;
    }
    if(top>=95) return false; ops[top++]=op; i+=tokenLen;
    expectValue=(op==E_NEG || op==E_NOT || op==E_ADD || op==E_SUB || op==E_MUL || op==E_DIV || op==E_MOD || op==E_LT || op==E_LE || op==E_GT || op==E_GE || op==E_EQ || op==E_NE || op==E_AND || op==E_OR);
  }
  while(top){ if(ops[top-1]==0xFF) return false; if(!exprPopOperator(ops,top)) return false; }
  if(expectValue || customProgramLength==0) return false;
  if(!exprEmit(E_END)) return false; customProgramValid=true; return true;
}

bool evaluateCustomExpression(byte x, byte y, byte z, byte f){
  if(!customProgramValid) return false;
  float stack[32]; byte sp=0;
  for(byte i=0;i<customProgramLength;i++){
    uint32_t code=customProgram[i].code; byte op=isPackedExprOp(code)?(byte)(code&0xFF):E_CONST;
    if(op==E_END) break;
    if(op==E_CONST){ if(sp>=32) return false; stack[sp++]=exprBitsFloat(code); continue; }
    if(op==E_X || op==E_Y || op==E_Z || op==E_F){ if(sp>=32) return false; stack[sp++]=(op==E_X)?x:(op==E_Y)?y:(op==E_Z)?z:f; continue; }
    if(op==E_NEG || op==E_NOT || op==E_SIN || op==E_COS || op==E_SQRT || op==E_ABS){
      if(sp==0) return false; float a=stack[sp-1];
      if(op==E_NEG) stack[sp-1]=-a; else if(op==E_NOT) stack[sp-1]=(a==0.0f); else if(op==E_SIN) stack[sp-1]=sin(a); else if(op==E_COS) stack[sp-1]=cos(a); else if(op==E_SQRT) stack[sp-1]=sqrt(max(0.0f,a)); else stack[sp-1]=fabs(a); continue;
    }
    if(sp<2) return false; float b=stack[--sp],a=stack[sp-1];
    if(op==E_ADD) stack[sp-1]=a+b; else if(op==E_SUB) stack[sp-1]=a-b; else if(op==E_MUL) stack[sp-1]=a*b; else if(op==E_DIV) stack[sp-1]=(fabs(b)<0.000001f)?0.0f:a/b; else if(op==E_MOD) stack[sp-1]=(fabs(b)<0.000001f)?0.0f:fmod(a,b); else if(op==E_LT) stack[sp-1]=a<b; else if(op==E_LE) stack[sp-1]=a<=b; else if(op==E_GT) stack[sp-1]=a>b; else if(op==E_GE) stack[sp-1]=a>=b; else if(op==E_EQ) stack[sp-1]=fabs(a-b)<0.0001f; else if(op==E_NE) stack[sp-1]=fabs(a-b)>=0.0001f; else if(op==E_AND) stack[sp-1]=(a!=0.0f && b!=0.0f); else if(op==E_OR) stack[sp-1]=(a!=0.0f || b!=0.0f); else return false;
  }
  return sp==1 && stack[0]!=0.0f;
}

bool customReady=false;
String getBufferLine(unsigned int start,unsigned int end){ String line=""; for(unsigned int i=start;i<end;i++) line+=rxBuffer[i]; line.trim(); line.toUpperCase(); return line; }
String getCustomVariableExpression(const String &name){
  unsigned int pos=0;
  while(pos<rxLength){ unsigned int start=pos; while(pos<rxLength && rxBuffer[pos]!='\n') pos++; String line=getBufferLine(start,pos); if(pos<rxLength) pos++; int eq=line.indexOf('='); if(eq>0){ String lhs=line.substring(0,eq); lhs.trim(); if(lhs==name){ String rhs=line.substring(eq+1); rhs.trim(); return rhs; } } }
  return String("");
}
String expandCustomExpression(String expr,byte depth=0){
  if(depth>12) return String("__CYCLE__"); String out="";
  for(unsigned int i=0;i<expr.length();){
    char c=expr[i];
    if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_'){
      String name=""; while(i<expr.length()){ char q=expr[i]; if((q>='A'&&q<='Z')||(q>='a'&&q<='z')||(q>='0'&&q<='9')||q=='_'){name+=q;i++;} else break; }
      name.toUpperCase();
      if(name=="X"||name=="Y"||name=="Z"||name=="F"||name=="T"||name=="SIN"||name=="COS"||name=="SQRT"||name=="ABS") out+=name;
      else{ String rhs=getCustomVariableExpression(name); if(rhs.length()) out+="("+expandCustomExpression(rhs,depth+1)+")"; else out+=name; }
    }else{out+=c;i++;}
    if(out.length()>=580) return String("__TOO_LONG__");
  }
  return out;
}
bool appendCustomCondition(String &finalExpr,const String &condition){
  String expanded=expandCustomExpression(condition);
  if(expanded.indexOf("__CYCLE__")>=0||expanded.indexOf("__TOO_LONG__")>=0) return false;
  if(finalExpr.length()) finalExpr+="&&";
  finalExpr+="("+expanded+")";
  return true;
}
bool appendCustomZCondition(String &zExpr,const String &rhs){
  String expanded=expandCustomExpression(rhs);
  if(expanded.indexOf("__CYCLE__")>=0||expanded.indexOf("__TOO_LONG__")>=0) return false;
  if(zExpr.length()) zExpr+="||";
  zExpr+="(z==("+expanded+"))";
  return true;
}

bool compileCustomSource(){
  String finalExpr=""; String zExpr=""; bool haveGeometry=false; bool parseError=false; unsigned int pos=0;
  while(pos<rxLength){
    unsigned int start=pos; while(pos<rxLength&&rxBuffer[pos]!='\n')pos++; String line=getBufferLine(start,pos); if(pos<rxLength)pos++;
    if(!line.length()||line=="CUSTOM"||line=="CF_BEGIN"||line=="START CUSTOM"||line=="END"||line=="CF_END")continue;
    if(line.startsWith("RETURN ")){finalExpr=expandCustomExpression(line.substring(7));haveGeometry=true;break;}
    if(line.startsWith("SHOW ")){finalExpr=expandCustomExpression(line.substring(5));haveGeometry=true;break;}
    if(line.startsWith("VOXEL ")){finalExpr=expandCustomExpression(line.substring(6));haveGeometry=true;break;}
    if(line.startsWith("IF ")){String condition=line.substring(3);condition.trim();if(condition.endsWith(" OFF")){condition.remove(condition.length()-4);condition.trim();if(!appendCustomCondition(finalExpr,"!("+condition+")"))parseError=true;}else if(condition.endsWith(" ON")){condition.remove(condition.length()-3);condition.trim();if(!appendCustomCondition(finalExpr,condition))parseError=true;}else if(!appendCustomCondition(finalExpr,condition))parseError=true;haveGeometry=true;continue;}
    if(line.startsWith("ON IF ")){if(!appendCustomCondition(finalExpr,line.substring(6)))parseError=true;haveGeometry=true;continue;}
    if(line.startsWith("OFF IF ")){if(!appendCustomCondition(finalExpr,"!("+line.substring(7)+")"))parseError=true;haveGeometry=true;continue;}
    int eq=line.indexOf('=');
    if(eq>0){
      String lhs=line.substring(0,eq); lhs.trim();
      bool validName=lhs.length()>0 && ((lhs[0]>='A'&&lhs[0]<='Z')||(lhs[0]>='a'&&lhs[0]<='z')||lhs[0]=='_');
      for(byte k=1;k<lhs.length()&&validName;k++){char q=lhs[k];if(!((q>='A'&&q<='Z')||(q>='a'&&q<='z')||(q>='0'&&q<='9')||q=='_'))validName=false;}
      if(!validName){parseError=true;continue;}
      String rhs=line.substring(eq+1);rhs.trim();
      if(lhs=="Z"){
        int orPos=rhs.indexOf(" OR ");
        while(orPos>=0){String part=rhs.substring(0,orPos);part.trim();if(part.startsWith("Z="))part=part.substring(2);part.trim();if(!appendCustomZCondition(zExpr,part))parseError=true;rhs=rhs.substring(orPos+4);rhs.trim();orPos=rhs.indexOf(" OR ");}
        if(rhs.startsWith("Z="))rhs=rhs.substring(2);rhs.trim();if(!rhs.length()||!appendCustomZCondition(zExpr,rhs))parseError=true;
        haveGeometry=true;
      }
      continue;
    }
    if(!appendCustomCondition(finalExpr,line))parseError=true; else haveGeometry=true;
  }
  if(parseError||!haveGeometry)return false;
  if(zExpr.length()){if(finalExpr.length())finalExpr+="&&";finalExpr+="("+zExpr+")";}
  if(finalExpr.indexOf("__CYCLE__")>=0||finalExpr.indexOf("__TOO_LONG__")>=0)return false;
  if(finalExpr.length()>580)return false;
  return compileCustomExpression(finalExpr.c_str());
}

void enterCustomWaitingState(){
  currentCubeMode=5;
  animationStart=millis();
  lastFrameTime=animationStart;
  frameCounter=0;
  clearDisplayBuffer();
}

void resetCustomReceive(){rxLength=0;rxBuffer[0]='\0';customReady=false;customProgramValid=false;customProgramLength=0;}
void parseCustomFunctionStream(char c){
  if(rxLength>=sizeof(rxBuffer)-1){customReady=false;customProgramValid=false;parseMode=0;enterCustomWaitingState();sendAck(F("CUSTOM_ERROR"));return;}
  rxBuffer[rxLength++]=c;rxBuffer[rxLength]='\0';
  if(rxLength>=7&&rxBuffer[rxLength-7]=='C'&&rxBuffer[rxLength-6]=='F'&&rxBuffer[rxLength-5]=='_'&&rxBuffer[rxLength-4]=='E'&&rxBuffer[rxLength-3]=='N'&&rxBuffer[rxLength-2]=='D'&&rxBuffer[rxLength-1]=='\n'){
    if(compileCustomSource()){
      customReady=true;
      parseMode=0;
      enterCustomWaitingState();
      sendAck(F("CUSTOM_OK"));
    }else{
      customReady=false;
      parseMode=0;
      customProgramValid=false;
      enterCustomWaitingState();
      sendAck(F("CUSTOM_ERROR"));
    }
  }
}

void drawCustomFunctionFrame(byte f){
  byte localMatrix[8][8];
  for(byte z=0;z<8;z++){
    for(byte r=0;r<8;r++)localMatrix[z][r]=0;
    for(byte y=0;y<8;y++)for(byte x=0;x<8;x++)if(evaluateCustomExpression(x,y,z,f)){
      byte c=columnIndex(x,y),reg=COLUMN_MAP[c].reg,bit=COLUMN_MAP[c].bit;
      if(reg>=1&&reg<=8&&bit<=7)localMatrix[z][reg-1]|=(1<<bit);
    }
  }
  copyFrameToDisplay(localMatrix);
}

inline void shiftByteFast(byte value){for(int8_t bit=7;bit>=0;bit--){if(value&(1<<bit))PORTB|=_BV(PB3);else PORTB&=~_BV(PB3);PORTB|=_BV(PB5);PORTB&=~_BV(PB5);}}
inline void latchFast(){PORTB|=_BV(PB4);PORTB&=~_BV(PB4);}
void refreshDisplay(){static byte layer=0;shiftByteFast(0);for(int8_t r=7;r>=0;r--)shiftByteFast(0);latchFast();brightnessAccumulator[layer]+=globalBrightness;bool en=brightnessAccumulator[layer]>=8;if(en)brightnessAccumulator[layer]-=8;shiftByteFast(en?(1<<layer):0);for(int8_t r=7;r>=0;r--)shiftByteFast(displayBuffer[layer][r]);latchFast();layer=(layer+1)%8;}
ISR(TIMER2_COMPA_vect){refreshDisplay();}
void startRefreshTimer(){noInterrupts();TCCR2A=_BV(WGM21);TCCR2B=_BV(CS22)|_BV(CS21)|_BV(CS20);OCR2A=3;TIMSK2|=_BV(OCIE2A);interrupts();}
inline bool isOuterRing(byte x,byte y){return x==0||x==7||y==0||y==7;}
byte perimeterIndex(byte x,byte y){if(y==0)return x;if(x==7)return 7+y;if(y==7)return 21-x;return 21+(7-y);}

bool firecrackerVoxel(byte f,byte x,byte y,byte z){
  if(f<16){byte launchZ=f/2;if((x==3||x==4)&&(y==3||y==4)){if(z==launchZ)return true;if(f>1&&z+1==launchZ)return true;}return false;}
  byte burstF=f-16,d=burstF/3;if(d>3)d=3;if(z!=7)return false;int vx=(int)x-3,vy=(int)y-3;if(vx==0&&vy==0)return d==0;if(!(vx==0||vy==0||abs(vx)==abs(vy)))return false;return max(abs(vx),abs(vy))==(int)d;
}

const byte SNAKE_DIRS[49] PROGMEM={0,5,1,1,5,1,2,4,2,0,0,2,5,2,5,1,4,1,1,5,3,3,0,0,3,1,3,4,4,2,4,0,0,2,4,3,4,4,2,5,5,3,3,1,2,2,0,3,5};
void snakePosition(byte step,byte &sx,byte &sy,byte &sz){int8_t px=3,py=3,pz=3;for(byte s=0;s<step;s++){byte d=pgm_read_byte(&SNAKE_DIRS[s%49]);if(d==0)px++;else if(d==1)px--;else if(d==2)py++;else if(d==3)py--;else if(d==4)pz++;else pz--;}sx=(byte)px;sy=(byte)py;sz=(byte)pz;}
bool snakeVoxel(byte f,byte x,byte y,byte z){for(byte k=0;k<8;k++){byte step=(byte)((f+50-k)%50);byte sx,sy,sz;snakePosition(step,sx,sy,sz);if(x==sx&&y==sy&&z==sz)return true;}return false;}

const byte HEART_MASK[8]={0x66,0xFF,0xFF,0x7E,0x3C,0x18,0x18,0x00};
bool rotatingHeartVoxel(byte f,byte x,byte y,byte z){if(y!=0&&y!=1)return false;byte r=(f/4)%4,u,v;if(r==0){u=x;v=z;}else if(r==1){u=z;v=7-x;}else if(r==2){u=7-x;v=7-z;}else{u=7-z;v=x;}return (HEART_MASK[v]&(1<<u))!=0;}

bool animationVoxel(byte a,byte f,byte x,byte y,byte z){if(a==0)return z==(f%8);if(a==1)return z==(7-(f%8));if(a==2)return x==(f%8);if(a==3)return y==(f%8);if(a==4)return x==y&&y==z&&x==(f%8);if(a==5)return x==y&&z==(7-x)&&x==(f%8);if(a==6)return ((x+y+z+f)&1)==0;if(a==7){byte r=f%5;int d=max(abs((int)x-3),max(abs((int)y-3),abs((int)z-3)));return d==r;}if(a==8){byte r=4-(f%5);int d=max(abs((int)x-3),max(abs((int)y-3),abs((int)z-3)));return d==r;}if(a==9){if(!(x==3||x==4||y==3||y==4||z==3||z==4))return false;return ((x+y+z+f)&1)==0;}if(a==10){byte w=(x+y+f)%8;return z==w||z==((w+1)%8);}if(a==11){byte ss=(f/2)%8;if(ss==0)return x==0;if(ss==1)return y==7;if(ss==2)return x==7;return y==0;}if(a==12){if(!isOuterRing(x,y))return false;byte p=perimeterIndex(x,y);return ((p+f)%28)<3;}if(a==13){if(!isOuterRing(x,y))return false;byte p=perimeterIndex(x,y);return z==((p+f)%8);}if(a==14){byte h=(x*3+y*5+f)%16;if(h>=8)return false;byte rz=7-h;return z==rz||(rz<7&&z==rz+1);}if(a==15){int dx=abs((int)x-3),dy=abs((int)y-3);if(dx<=1&&dy<=1){if(z>((f/2)%8))return false;return ((x+y+f)&1)!=0;}return false;}if(a==16){if(!isOuterRing(x,y))return false;byte p=perimeterIndex(x,y),o=(p+f)%28;return z==(o%8)||z==((o+1)%8);}if(a==17){if(!isOuterRing(x,y))return false;byte p=perimeterIndex(x,y);return z==((p+f)%8);}if(a==18){byte r=f%8;int d=abs((int)x-3)+abs((int)y-3)+abs((int)z-3);return d==r||d==r+1;}if(a==19){byte r=f%10;int d=min(abs((int)x-3),abs((int)x-4))+min(abs((int)y-3),abs((int)y-4))+min(abs((int)z-3),abs((int)z-4));return d==r||d==r+1;}if(a==20){byte r=9-(f%10);int d=min(abs((int)x-3),abs((int)x-4))+min(abs((int)y-3),abs((int)y-4))+min(abs((int)z-3),abs((int)z-4));return d==r||d==r+1;}if(a==21){int d=abs((int)x-3)+abs((int)y-3)+abs((int)z-3);return ((d+f)%4)<2;}if(a==22)return ((x+y+z+f)%8)==0;if(a==23){if(!((x==0||x==7)&&(y==0||y==7)&&(z==0||z==7)))return false;byte c=((z==7)?4:0)+((y==7)?2:0)+((x==7)?1:0);return c==(f%8);}if(a==24)return firecrackerVoxel(f,x,y,z);if(a==25)return snakeVoxel(f,x,y,z);if(a==26)return rotatingHeartVoxel(f,x,y,z);return false;}
void drawAnimationFrame(byte a,byte f){byte localMatrix[8][8];for(byte z=0;z<8;z++){for(byte r=0;r<8;r++)localMatrix[z][r]=0;for(byte y=0;y<8;y++)for(byte x=0;x<8;x++)if(animationVoxel(a,f,x,y,z)){byte c=columnIndex(x,y),reg=COLUMN_MAP[c].reg,bit=COLUMN_MAP[c].bit;if(reg>=1&&reg<=8&&bit<=7)localMatrix[z][reg-1]|=(1<<bit);}}copyFrameToDisplay(localMatrix);}
void setup(){pinMode(DATA_PIN,OUTPUT);pinMode(CLOCK_PIN,OUTPUT);pinMode(LATCH_PIN,OUTPUT);pinMode(TOUCH_PIN,INPUT);pinMode(BLE_STATE_PIN,INPUT);PORTB&=~(_BV(PB3)|_BV(PB4)|_BV(PB5));bluetooth.begin(9600);customProgramValid=false;customReady=false;clearDisplayBuffer();startRefreshTimer();delay(STARTUP_DELAY_TIME);while(bluetooth.available()>0)bluetooth.read();lastBluetoothConnected=digitalRead(BLE_STATE_PIN)==HIGH;bluetoothStateChangedAt=0;animationStart=millis();lastFrameTime=animationStart;frameCounter=0;drawAnimationFrame(animationIndex,frameCounter);}
void loop(){unsigned long now=millis();bool ble=digitalRead(BLE_STATE_PIN)==HIGH;if(ble!=lastBluetoothConnected){if(bluetoothStateChangedAt==0)bluetoothStateChangedAt=now;else if(now-bluetoothStateChangedAt>=BLE_STATE_DEBOUNCE_TIME){lastBluetoothConnected=ble;bluetoothStateChangedAt=0;if(!lastBluetoothConnected){currentCubeMode=0;parseMode=0;rxLength=0;customProgramValid=false;customReady=false;animationStart=now;lastFrameTime=now;triggerModeBlinkAcknowledgment();}}}else bluetoothStateChangedAt=0;if(!lastBluetoothConnected)globalBrightness=map(analogRead(POT_PIN),0,1023,2,8);static bool lastTouch=false;static unsigned long touchTimer=0;static bool longPress=false;bool touch=digitalRead(TOUCH_PIN)==HIGH;if(lastBluetoothConnected)touch=false;if(touch&&!lastTouch){touchTimer=now;longPress=false;}else if(touch&&lastTouch){unsigned long d=now-touchTimer;if(!longPress&&d>=3000UL){currentCubeMode=(currentCubeMode==0)?1:0;triggerModeBlinkAcknowledgment();longPress=true;animationStart=now;lastFrameTime=now;}}else if(!touch&&lastTouch){unsigned long d=now-touchTimer;if(!longPress&&currentCubeMode==1&&d>=50&&d<3000UL){animationIndex=(animationIndex+1)%TOTAL_ANIMATIONS;frameCounter=0;animationStart=now;lastFrameTime=now;drawAnimationFrame(animationIndex,frameCounter);}}lastTouch=touch;
  while(bluetooth.available()>0){byte in=bluetooth.read();if(!lastBluetoothConnected){parseMode=0;rxLength=0;continue;}if(parseMode==5){parseCustomFunctionStream((char)in);continue;}if(parseMode==4){globalBrightness=constrain(in,(byte)2,(byte)8);parseMode=0;sendAck(F("BRIGHTNESS_OK"));continue;}if(parseMode==0){if(in=='A'){currentCubeMode=0;animationStart=now;lastFrameTime=now;triggerModeBlinkAcknowledgment();sendAck(F("MODE_AUTO"));}else if(in=='M'){currentCubeMode=1;animationStart=now;lastFrameTime=now;triggerModeBlinkAcknowledgment();sendAck(F("MODE_MANUAL"));}else if(in=='C'){parseMode=5;resetCustomReceive();enterCustomWaitingState();sendAck(F("CUSTOM_UPLOAD_READY"));}else if(in=='X'){if(customReady){currentCubeMode=4;animationStart=now;lastFrameTime=now;frameCounter=0;triggerModeBlinkAcknowledgment();sendAck(F("CUSTOM_STARTED"));}else sendAck(F("CUSTOM_NOT_READY"));}else if(in=='S'){if(currentCubeMode==4||customReady){currentCubeMode=0;animationStart=now;lastFrameTime=now;frameCounter=0;sendAck(F("CUSTOM_STOPPED"));}else sendAck(F("CUSTOM_NOT_ACTIVE"));}else if(in=='N'&&currentCubeMode==1){animationIndex=(animationIndex+1)%TOTAL_ANIMATIONS;frameCounter=0;lastFrameTime=now;drawAnimationFrame(animationIndex,frameCounter);sendAck(F("ANIMATION_NEXT"));}else if(in=='B')parseMode=4;}
  }if(currentCubeMode==0){if(now-animationStart>=AUTO_MODE_CAROUSEL_TIME){animationIndex=(animationIndex+1)%TOTAL_ANIMATIONS;frameCounter=0;animationStart=now;lastFrameTime=now;}if(now-lastFrameTime>=FRAME_TIME){lastFrameTime=now;drawAnimationFrame(animationIndex,frameCounter);frameCounter=(frameCounter+1)%50;}}else if(currentCubeMode==1){if(now-lastFrameTime>=FRAME_TIME){lastFrameTime=now;drawAnimationFrame(animationIndex,frameCounter);frameCounter=(frameCounter+1)%50;}}else if(currentCubeMode==4){if(now-lastFrameTime>=FRAME_TIME){lastFrameTime=now;drawCustomFunctionFrame(frameCounter);frameCounter=(frameCounter+1)%50;}}}