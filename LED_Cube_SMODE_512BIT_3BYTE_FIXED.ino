#include <Arduino.h>
#include <avr/interrupt.h>
#include <AltSoftSerial.h>
#include <math.h>

const byte DATA_PIN = 11, CLOCK_PIN = 13, LATCH_PIN = 12, TOUCH_PIN = 10, POT_PIN = A0, BLE_STATE_PIN = 2;
AltSoftSerial bluetooth;

volatile byte currentCubeMode = 0;
byte globalBrightness = 5;
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

struct ColumnMap { byte reg; byte bit; };
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
  for(byte z=0;z<8;z++) for(byte r=0;r<8;r++) displayBuffer[z][r]=0xFF;
  delay(80);
  for(byte z=0;z<8;z++) for(byte r=0;r<8;r++) displayBuffer[z][r]=0x00;
  delay(80);
}

// ---------------- CUSTOM FUNCTION ENGINE ----------------
enum OpCode { OP_NONE=0, OP_CALC_H, OP_CHECK_H_GE_8, OP_CALC_RZ, OP_CHECK_Z_MATCH };
struct CompiledInstruction { OpCode op; };
CompiledInstruction compiledProgram[16];
byte programLength=4;
char customRxBuf[32];
byte customRxIdx=0;
void initDefaultCustomProgram(){
  programLength=4;
  compiledProgram[0].op=OP_CALC_H;
  compiledProgram[1].op=OP_CHECK_H_GE_8;
  compiledProgram[2].op=OP_CALC_RZ;
  compiledProgram[3].op=OP_CHECK_Z_MATCH;
}
bool evaluateCustomFunction(byte x,byte y,byte z,byte f){
  int H=0,RZ=0;
  for(byte i=0;i<programLength;i++){
    switch(compiledProgram[i].op){
      case OP_CALC_H:H=((int)x*3+(int)y*5+(int)f)%16;break;
      case OP_CHECK_H_GE_8:if(H>=8)return false;break;
      case OP_CALC_RZ:RZ=7-H;break;
      case OP_CHECK_Z_MATCH:return (z==RZ)||((RZ<7)&&(z==RZ+1));
      default:break;
    }
  }
  return false;
}
void drawCustomFunctionFrame(byte f){
  byte localMatrix[8][8];
  for(byte z=0;z<8;z++){
    for(byte r=0;r<8;r++) localMatrix[z][r]=0;
    for(byte y=0;y<8;y++) for(byte x=0;x<8;x++) if(evaluateCustomFunction(x,y,z,f)){
      byte c=columnIndex(x,y),reg=COLUMN_MAP[c].reg,bit=COLUMN_MAP[c].bit;
      if(reg>=1&&reg<=8&&bit<=7)localMatrix[z][reg-1]|=(1<<bit);
    }
  }
  noInterrupts();memcpy((void*)displayBuffer,localMatrix,64);interrupts();
}
void parseCustomFunctionStream(char c){
  if(c=='\n'||c=='\r'){
    if(customRxIdx>0){
      customRxBuf[customRxIdx]='\0'; String line=String(customRxBuf); line.trim(); line.toUpperCase();
      if(line=="CUSTOM"||line=="CF_BEGIN"){programLength=0;currentCubeMode=4;memset((void*)displayBuffer,0,64);}
      else if(line=="START CUSTOM"){currentCubeMode=4;memset((void*)displayBuffer,0,64);}
      else if(line=="END"||line=="CF_END"){parseMode=0;currentCubeMode=0;triggerModeBlinkAcknowledgment();}
      else{
        if(line.indexOf("H=")!=-1||line.indexOf("%16")!=-1)if(programLength<16)compiledProgram[programLength++].op=OP_CALC_H;
        if(line.indexOf("IF")!=-1&&line.indexOf(">=8")!=-1)if(programLength<16)compiledProgram[programLength++].op=OP_CHECK_H_GE_8;
        if(line.indexOf("RZ=")!=-1||line.indexOf("7-H")!=-1)if(programLength<16)compiledProgram[programLength++].op=OP_CALC_RZ;
        if(line.indexOf("Z=")!=-1||line.indexOf("OR")!=-1||line.indexOf("RZ+1")!=-1)if(programLength<16)compiledProgram[programLength++].op=OP_CHECK_Z_MATCH;
      }
      customRxIdx=0;
    }
  }else if(customRxIdx<31)customRxBuf[customRxIdx++]=c;
}

// ---------------- TRANSMITTED MATH ENGINE ----------------
// HTML sends the expression once. Arduino compiles it to bytecode and evaluates
// that bytecode locally for all 512 voxels on every frame.
enum MathOp : byte { M_END=0,M_CONST,M_X,M_Y,M_Z,M_T,M_ADD,M_SUB,M_MUL,M_DIV,M_NEG,M_SIN,M_COS,M_SQRT,M_ABS,M_LT,M_LE,M_GT,M_GE,M_EQ,M_NE };
struct MathInstr { byte op; float value; };
MathInstr mathProgram[48];
byte mathProgramLength=0;
char mathRxBuf[192];
byte mathRxIdx=0;

int mathPrecedence(byte op){
  if(op==M_LT||op==M_LE||op==M_GT||op==M_GE||op==M_EQ||op==M_NE)return 1;
  if(op==M_ADD||op==M_SUB)return 2;
  if(op==M_MUL||op==M_DIV)return 3;
  if(op==M_NEG||op==M_SIN||op==M_COS||op==M_SQRT||op==M_ABS)return 4;
  return 0;
}
bool mathRightAssociative(byte op){ return op==M_NEG; }
void mathEmit(byte op,float v=0){ if(mathProgramLength<47){mathProgram[mathProgramLength].op=op;mathProgram[mathProgramLength].value=v;mathProgramLength++;} }
void mathPopOperator(byte opStack[],byte &top){ if(top>0){byte op=opStack[--top];mathEmit(op);}}

bool compileMathExpression(const char* src){
  mathProgramLength=0;
  byte opStack[48],top=0;
  bool expectValue=true;
  for(unsigned int i=0;src[i]!='\0';){
    char c=src[i];
    if(c==' '||c=='\t'){i++;continue;}
    if((c>='0'&&c<='9')||c=='.'){
      char *endp; float v=strtod(src+i,&endp); if(endp==src+i)return false;
      mathEmit(M_CONST,v); i=(unsigned int)(endp-src); expectValue=false; continue;
    }
    if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_'){
      char name[8];byte n=0;while(((src[i]>='A'&&src[i]<='Z')||(src[i]>='a'&&src[i]<='z')||(src[i]>='0'&&src[i]<='9')||src[i]=='_')&&n<7)name[n++]=src[i++];name[n]='\0';
      for(byte k=0;k<n;k++)if(name[k]>='A'&&name[k]<='Z')name[k]+=32;
      if(!strcmp(name,"x")){mathEmit(M_X);expectValue=false;continue;}
      if(!strcmp(name,"y")){mathEmit(M_Y);expectValue=false;continue;}
      if(!strcmp(name,"z")){mathEmit(M_Z);expectValue=false;continue;}
      if(!strcmp(name,"t")||!strcmp(name,"f")){mathEmit(M_T);expectValue=false;continue;}
      byte fn=M_END;if(!strcmp(name,"sin"))fn=M_SIN;else if(!strcmp(name,"cos"))fn=M_COS;else if(!strcmp(name,"sqrt"))fn=M_SQRT;else if(!strcmp(name,"abs"))fn=M_ABS;else return false;
      if(top>=47)return false;opStack[top++]=fn;expectValue=true;continue;
    }
    if(c=='('){if(top>=47)return false;opStack[top++]=0xFF;i++;expectValue=true;continue;}
    if(c==')'){
      while(top>0&&opStack[top-1]!=0xFF)mathPopOperator(opStack,top);
      if(top==0)return false;top--;
      if(top>0&&(opStack[top-1]==M_SIN||opStack[top-1]==M_COS||opStack[top-1]==M_SQRT||opStack[top-1]==M_ABS))mathPopOperator(opStack,top);
      i++;expectValue=false;continue;
    }
    byte op=M_END;byte len=1;
    if(c=='<'&&src[i+1]=='='){op=M_LE;len=2;}else if(c=='>'&&src[i+1]=='='){op=M_GE;len=2;}else if(c=='='&&src[i+1]=='='){op=M_EQ;len=2;}else if(c=='!'&&src[i+1]=='='){op=M_NE;len=2;}else if(c=='<')op=M_LT;else if(c=='>')op=M_GT;else if(c=='+')op=M_ADD;else if(c=='-')op=expectValue?M_NEG:M_SUB;else if(c=='*')op=M_MUL;else if(c=='/')op=M_DIV;else return false;
    while(top>0&&opStack[top-1]!=0xFF){byte p=opStack[top-1];if((!mathRightAssociative(op)&&mathPrecedence(op)<=mathPrecedence(p))||(mathRightAssociative(op)&&mathPrecedence(op)<mathPrecedence(p)))mathPopOperator(opStack,top);else break;}
    if(top>=47)return false;opStack[top++]=op;i+=len;expectValue=(op==M_NEG||op==M_ADD||op==M_SUB||op==M_MUL||op==M_DIV||op==M_LT||op==M_LE||op==M_GT||op==M_GE||op==M_EQ||op==M_NE);
  }
  while(top>0){if(opStack[top-1]==0xFF)return false;mathPopOperator(opStack,top);}
  if(mathProgramLength==0)return false;mathEmit(M_END);return mathProgramLength<48;
}

bool evaluateMathExpression(byte x,byte y,byte z,byte f){
  float st[24];byte sp=0;
  for(byte i=0;i<mathProgramLength;i++){
    byte op=mathProgram[i].op;
    if(op==M_END)break;
    if(op==M_CONST){if(sp>=24)return false;st[sp++]=mathProgram[i].value;continue;}
    if(op==M_X||op==M_Y||op==M_Z||op==M_T){if(sp>=24)return false;st[sp++]=(op==M_X?x:op==M_Y?y:op==M_Z?z:f);continue;}
    if(op==M_NEG){if(!sp)return false;st[sp-1]=-st[sp-1];continue;}
    if(op==M_SIN||op==M_COS||op==M_SQRT||op==M_ABS){if(!sp)return false;float a=st[sp-1];if(op==M_SIN)st[sp-1]=sin(a);else if(op==M_COS)st[sp-1]=cos(a);else if(op==M_SQRT)st[sp-1]=sqrt(max(0.0f,a));else st[sp-1]=fabs(a);continue;}
    if(sp<2)return false;float b=st[--sp],a=st[sp-1];
    if(op==M_ADD)st[sp-1]=a+b;else if(op==M_SUB)st[sp-1]=a-b;else if(op==M_MUL)st[sp-1]=a*b;else if(op==M_DIV)st[sp-1]=(fabs(b)<0.000001f?0:a/b);else if(op==M_LT)st[sp-1]=a<b;else if(op==M_LE)st[sp-1]=a<=b;else if(op==M_GT)st[sp-1]=a>b;else if(op==M_GE)st[sp-1]=a>=b;else if(op==M_EQ)st[sp-1]=fabs(a-b)<0.0001f;else if(op==M_NE)st[sp-1]=fabs(a-b)>=0.0001f;else return false;
  }
  return sp>0&&st[0]!=0;
}
void parseMathFunctionStream(char c){
  if(c=='\n'||c=='\r'){
    if(mathRxIdx>0){mathRxBuf[mathRxIdx]='\0';String line=String(mathRxBuf);line.trim();
      if(line.length()>0&&compileMathExpression(line.c_str())){currentCubeMode=3;frameCounter=0;animationStart=millis();lastFrameTime=millis();memset((void*)displayBuffer,0,64);}
      mathRxIdx=0;parseMode=0;
    }
  }else if(mathRxIdx<191)mathRxBuf[mathRxIdx++]=c;
}
void drawMathFrame(byte f){
  byte localMatrix[8][8];
  for(byte z=0;z<8;z++){
    for(byte r=0;r<8;r++)localMatrix[z][r]=0;
    for(byte y=0;y<8;y++)for(byte x=0;x<8;x++)if(evaluateMathExpression(x,y,z,f)){
      byte c=columnIndex(x,y),reg=COLUMN_MAP[c].reg,bit=COLUMN_MAP[c].bit;
      if(reg>=1&&reg<=8&&bit<=7)localMatrix[z][reg-1]|=(1<<bit);
    }
  }
  noInterrupts();memcpy((void*)displayBuffer,localMatrix,64);interrupts();
}

inline void shiftByteFast(byte value){for(int8_t bit=7;bit>=0;bit--){if(value&(1<<bit))PORTB|=_BV(PB3);else PORTB&=~_BV(PB3);PORTB|=_BV(PB5);PORTB&=~_BV(PB5);}}
inline void latchFast(){PORTB|=_BV(PB4);PORTB&=~_BV(PB4);}
void refreshDisplay(){static byte layer=0;brightnessAccumulator[layer]+=globalBrightness;bool en=brightnessAccumulator[layer]>=8;if(en)brightnessAccumulator[layer]-=8;shiftByteFast(en?(1<<layer):0);for(int8_t r=7;r>=0;r--)shiftByteFast(displayBuffer[layer][r]);latchFast();layer=(layer+1)%8;}
ISR(TIMER2_COMPA_vect){refreshDisplay();}
void startRefreshTimer(){noInterrupts();TCCR2A=_BV(WGM21);TCCR2B=_BV(CS22)|_BV(CS21)|_BV(CS20);OCR2A=3;TIMSK2|=_BV(OCIE2A);interrupts();}

inline bool isOuterRing(byte x,byte y){return x==0||x==7||y==0||y==7;}
byte perimeterIndex(byte x,byte y){if(y==0)return x;if(x==7)return 7+y;if(y==7)return 21-x;return 21+(7-y);}

bool animationVoxel(byte a,byte f,byte x,byte y,byte z){
 if(a==0)return z==(f%8);if(a==1)return z==(7-(f%8));if(a==2)return x==(f%8);if(a==3)return y==(f%8);if(a==4)return x==y&&y==z&&x==(f%8);if(a==5)return x==y&&z==(7-x)&&x==(f%8);if(a==6)return ((x+y+z+f)&1)==0;if(a==7){byte r=f%5;int d=max(abs((int)x-3),max(abs((int)y-3),abs((int)z-3)));return d==r;}if(a==8){byte r=4-(f%5);int d=max(abs((int)x-3),max(abs((int)y-3),abs((int)z-3)));return d==r;}if(a==9){if(!(x==3||x==4||y==3||y==4||z==3||z==4))return false;return ((x+y+z+f)&1)==0;}if(a==10){byte w=(x+y+f)%8;return z==w||z==((w+1)%8);}if(a==11){byte s=(f/2)%8;if(s==0)return x==0;if(s==1)return y==7;if(s==2)return x==7;return y==0;}if(a==12){if(!isOuterRing(x,y))return false;byte p=perimeterIndex(x,y);return ((p+f)%28)<3;}if(a==13){if(!isOuterRing(x,y))return false;byte p=perimeterIndex(x,y);return z==((p+f)%8);}if(a==14){byte h=(x*3+y*5+f)%16;if(h>=8)return false;byte rz=7-h;return z==rz||(rz<7&&z==rz+1);}if(a==15){int dx=abs((int)x-3),dy=abs((int)y-3);if(dx<=1&&dy<=1){if(z>((f/2)%8))return false;return ((x+y+f)&1)!=0;}return false;}if(a==16){if(!isOuterRing(x,y))return false;byte p=perimeterIndex(x,y),o=(p+f)%28;return z==(o%8)||z==((o+1)%8);}if(a==17){if(!isOuterRing(x,y))return false;byte p=perimeterIndex(x,y);return z==((p+f)%8);}if(a==18){byte r=f%8;int d=abs((int)x-3)+abs((int)y-3)+abs((int)z-3);return d==r||d==r+1;}if(a==19){byte r=f%10;int d=min(abs((int)x-3),abs((int)x-4))+min(abs((int)y-3),abs((int)y-4))+min(abs((int)z-3),abs((int)z-4));return d==r||d==r+1;}if(a==20){byte r=9-(f%10);int d=min(abs((int)x-3),abs((int)x-4))+min(abs((int)y-3),abs((int)y-4))+min(abs((int)z-3),abs((int)z-4));return d==r||d==r+1;}if(a==21){int d=abs((int)x-3)+abs((int)y-3)+abs((int)z-3);return ((d+f)%4)<2;}if(a==22)return ((x+y+z+f)%8)==0;if(a==23){if(!((x==0||x==7)&&(y==0||y==7)&&(z==0||z==7)))return false;byte c=((z==7)?4:0)+((y==7)?2:0)+((x==7)?1:0);return c==(f%8);}return false;
}
void drawAnimationFrame(byte a,byte f){byte localMatrix[8][8];for(byte z=0;z<8;z++){for(byte r=0;r<8;r++)localMatrix[z][r]=0;for(byte y=0;y<8;y++)for(byte x=0;x<8;x++)if(animationVoxel(a,f,x,y,z)){byte c=columnIndex(x,y),reg=COLUMN_MAP[c].reg,bit=COLUMN_MAP[c].bit;if(reg>=1&&reg<=8&&bit<=7)localMatrix[z][reg-1]|=(1<<bit);}}noInterrupts();memcpy((void*)displayBuffer,localMatrix,64);interrupts();}

void setup(){
  pinMode(DATA_PIN,OUTPUT);pinMode(CLOCK_PIN,OUTPUT);pinMode(LATCH_PIN,OUTPUT);pinMode(TOUCH_PIN,INPUT);pinMode(BLE_STATE_PIN,INPUT);PORTB&=~(_BV(PB3)|_BV(PB4)|_BV(PB5));bluetooth.begin(9600);initDefaultCustomProgram();startRefreshTimer();animationStart=millis();lastFrameTime=millis();
}
void loop(){
  unsigned long now=millis();bool ble=digitalRead(BLE_STATE_PIN)==HIGH;
  if(ble!=lastBluetoothConnected){if(bluetoothStateChangedAt==0)bluetoothStateChangedAt=now;else if(now-bluetoothStateChangedAt>=BLE_STATE_DEBOUNCE_TIME){lastBluetoothConnected=ble;bluetoothStateChangedAt=0;if(!lastBluetoothConnected){currentCubeMode=0;parseMode=0;animationStart=now;lastFrameTime=now;triggerModeBlinkAcknowledgment();}}}else bluetoothStateChangedAt=0;
  if(!lastBluetoothConnected){int raw=analogRead(POT_PIN);globalBrightness=map(raw,0,1023,2,8);}
  static bool lastTouch=false;static unsigned long touchTimer=0;static bool longPress=false;bool touch=digitalRead(TOUCH_PIN)==HIGH;if(lastBluetoothConnected)touch=false;
  if(touch&&!lastTouch){touchTimer=now;longPress=false;}else if(touch&&lastTouch){unsigned long d=now-touchTimer;if(!longPress&&d>=3000UL){currentCubeMode=(currentCubeMode==0)?1:0;triggerModeBlinkAcknowledgment();longPress=true;animationStart=now;lastFrameTime=now;}}else if(!touch&&lastTouch){unsigned long d=now-touchTimer;if(!longPress&&currentCubeMode==1&&d>=50&&d<3000UL){animationIndex=(animationIndex+1)%TOTAL_ANIMATIONS;frameCounter=0;animationStart=now;lastFrameTime=now;drawAnimationFrame(animationIndex,frameCounter);}}lastTouch=touch;
  while(bluetooth.available()>0){byte in=bluetooth.read();
    if(parseMode==5){parseCustomFunctionStream((char)in);continue;}
    if(parseMode==6){parseMathFunctionStream((char)in);continue;}
    if(parseMode==0){
      if(in=='A'||in==0x41||in==0x51){currentCubeMode=0;animationStart=now;lastFrameTime=now;triggerModeBlinkAcknowledgment();}
      else if(in=='M'||in==0x4D){currentCubeMode=1;animationStart=now;lastFrameTime=now;triggerModeBlinkAcknowledgment();}
      else if(in=='F'||in==0x46){currentCubeMode=3;memset((void*)displayBuffer,0,64);animationStart=now;lastFrameTime=now;triggerModeBlinkAcknowledgment();}
      else if(in=='X'||in==0x58){currentCubeMode=4;memset((void*)displayBuffer,0,64);animationStart=now;lastFrameTime=now;triggerModeBlinkAcknowledgment();}
      else if(in=='C'){parseMode=5;customRxIdx=0;}
      else if(in=='Y'){parseMode=6;mathRxIdx=0;}
      else if(in=='H'){bluetooth.println("CONNECTED");triggerModeBlinkAcknowledgment();}
      else if(in=='N'&&currentCubeMode==1){animationIndex=(animationIndex+1)%TOTAL_ANIMATIONS;frameCounter=0;lastFrameTime=now;drawAnimationFrame(animationIndex,frameCounter);}
      else if(in=='Q'){currentCubeMode=0;animationStart=now;lastFrameTime=now;triggerModeBlinkAcknowledgment();}
      else if(in=='B'){parseMode=4;}
    }else if(parseMode==4){if(in>=2&&in<=8)globalBrightness=in;parseMode=0;}
  }
  if(currentCubeMode==0){if(now-animationStart>=AUTO_MODE_CAROUSEL_TIME){animationIndex=(animationIndex+1)%TOTAL_ANIMATIONS;frameCounter=0;animationStart=now;lastFrameTime=now;}if(now-lastFrameTime>=FRAME_TIME){lastFrameTime=now;drawAnimationFrame(animationIndex,frameCounter);frameCounter=(frameCounter+1)%50;}}
  else if(currentCubeMode==1){if(now-lastFrameTime>=FRAME_TIME){lastFrameTime=now;drawAnimationFrame(animationIndex,frameCounter);frameCounter=(frameCounter+1)%50;}}
  else if(currentCubeMode==3){if(now-lastFrameTime>=FRAME_TIME){lastFrameTime=now;drawMathFrame(frameCounter);frameCounter=(frameCounter+1)%50;}}
  else if(currentCubeMode==4){if(now-lastFrameTime>=FRAME_TIME){lastFrameTime=now;drawCustomFunctionFrame(frameCounter);frameCounter=(frameCounter+1)%50;}}
}
