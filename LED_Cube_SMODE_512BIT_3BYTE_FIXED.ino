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

// ---------------- TRANSMITTED EXPRESSION ENGINE ----------------
// Both Math mode and Custom mode use this bytecode evaluator.  The browser
// sends the function once; the Arduino evaluates all 512 voxels locally.
enum MathOp : byte {
  M_END=0,M_CONST,M_X,M_Y,M_Z,M_T,
  M_ADD,M_SUB,M_MUL,M_DIV,M_MOD,M_NEG,
  M_SIN,M_COS,M_SQRT,M_ABS,M_NOT,
  M_LT,M_LE,M_GT,M_GE,M_EQ,M_NE,M_AND,M_OR
};
struct MathInstr { byte op; float value; };
MathInstr mathProgram[64];
byte mathProgramLength=0;
char mathRxBuf[192];
byte mathRxIdx=0;

int mathPrecedence(byte op){
  if(op==M_OR)return 1;
  if(op==M_AND)return 2;
  if(op==M_LT||op==M_LE||op==M_GT||op==M_GE||op==M_EQ||op==M_NE)return 3;
  if(op==M_ADD||op==M_SUB)return 4;
  if(op==M_MUL||op==M_DIV||op==M_MOD)return 5;
  if(op==M_NEG||op==M_NOT||op==M_SIN||op==M_COS||op==M_SQRT||op==M_ABS)return 6;
  return 0;
}
bool mathRightAssociative(byte op){ return op==M_NEG||op==M_NOT; }
void mathEmit(byte op,float v=0){ if(mathProgramLength<63){mathProgram[mathProgramLength].op=op;mathProgram[mathProgramLength].value=v;mathProgramLength++;} }
void mathPopOperator(byte opStack[],byte &top){ if(top>0){byte op=opStack[--top];mathEmit(op);}}

bool compileMathExpression(const char* src){
  mathProgramLength=0;
  byte opStack[64],top=0;
  bool expectValue=true;
  for(unsigned int i=0;src[i]!='\0';){
    char c=src[i];
    if(c==' '||c=='\t'){i++;continue;}
    if((c>='0'&&c<='9')||c=='.'){
      char *endp; float v=strtod(src+i,&endp); if(endp==src+i)return false;
      mathEmit(M_CONST,v); i=(unsigned int)(endp-src); expectValue=false; continue;
    }
    if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_'){
      char name[12];byte n=0;
      while(((src[i]>='A'&&src[i]<='Z')||(src[i]>='a'&&src[i]<='z')||(src[i]>='0'&&src[i]<='9')||src[i]=='_')&&n<11)name[n++]=src[i++];
      name[n]='\0';
      for(byte k=0;k<n;k++)if(name[k]>='A'&&name[k]<='Z')name[k]+=32;
      if(!strcmp(name,"x")){mathEmit(M_X);expectValue=false;continue;}
      if(!strcmp(name,"y")){mathEmit(M_Y);expectValue=false;continue;}
      if(!strcmp(name,"z")){mathEmit(M_Z);expectValue=false;continue;}
      if(!strcmp(name,"t")||!strcmp(name,"f")){mathEmit(M_T);expectValue=false;continue;}
      byte fn=M_END;
      if(!strcmp(name,"sin"))fn=M_SIN;
      else if(!strcmp(name,"cos"))fn=M_COS;
      else if(!strcmp(name,"sqrt"))fn=M_SQRT;
      else if(!strcmp(name,"abs"))fn=M_ABS;
      else return false;
      if(top>=63)return false; opStack[top++]=fn; expectValue=true; continue;
    }
    if(c=='('){if(top>=63)return false;opStack[top++]=0xFF;i++;expectValue=true;continue;}
    if(c==')'){
      while(top>0&&opStack[top-1]!=0xFF)mathPopOperator(opStack,top);
      if(top==0)return false;
      top--;
      if(top>0&&(opStack[top-1]==M_SIN||opStack[top-1]==M_COS||opStack[top-1]==M_SQRT||opStack[top-1]==M_ABS))mathPopOperator(opStack,top);
      i++;expectValue=false;continue;
    }
    byte op=M_END;byte len=1;
    if(c=='|'&&src[i+1]=='|'){op=M_OR;len=2;}
    else if(c=='&'&&src[i+1]=='&'){op=M_AND;len=2;}
    else if(c=='<'&&src[i+1]=='='){op=M_LE;len=2;}
    else if(c=='>'&&src[i+1]=='='){op=M_GE;len=2;}
    else if(c=='='&&src[i+1]=='='){op=M_EQ;len=2;}
    else if(c=='!'&&src[i+1]=='='){op=M_NE;len=2;}
    else if(c=='!'){op=M_NOT;}
    else if(c=='<')op=M_LT;
    else if(c=='>')op=M_GT;
    else if(c=='+')op=M_ADD;
    else if(c=='-')op=expectValue?M_NEG:M_SUB;
    else if(c=='*')op=M_MUL;
    else if(c=='/')op=M_DIV;
    else if(c=='%')op=M_MOD;
    else return false;
    while(top>0&&opStack[top-1]!=0xFF){
      byte p=opStack[top-1];
      if((!mathRightAssociative(op)&&mathPrecedence(op)<=mathPrecedence(p))||(mathRightAssociative(op)&&mathPrecedence(op)<mathPrecedence(p)))mathPopOperator(opStack,top);else break;
    }
    if(top>=63)return false;
    opStack[top++]=op;i+=len;
    expectValue=(op==M_NEG||op==M_NOT||op==M_ADD||op==M_SUB||op==M_MUL||op==M_DIV||op==M_MOD||op==M_LT||op==M_LE||op==M_GT||op==M_GE||op==M_EQ||op==M_NE||op==M_AND||op==M_OR);
  }
  while(top>0){if(opStack[top-1]==0xFF)return false;mathPopOperator(opStack,top);}
  if(mathProgramLength==0)return false;
  mathEmit(M_END);
  return mathProgramLength<64;
}

bool evaluateMathExpression(byte x,byte y,byte z,byte f){
  float st[24];byte sp=0;
  for(byte i=0;i<mathProgramLength;i++){
    byte op=mathProgram[i].op;
    if(op==M_END)break;
    if(op==M_CONST){if(sp>=24)return false;st[sp++]=mathProgram[i].value;continue;}
    if(op==M_X||op==M_Y||op==M_Z||op==M_T){if(sp>=24)return false;st[sp++]=(op==M_X?x:op==M_Y?y:op==M_Z?z:f);continue;}
    if(op==M_NEG){if(!sp)return false;st[sp-1]=-st[sp-1];continue;}
    if(op==M_NOT){if(!sp)return false;st[sp-1]=(st[sp-1]==0)?1:0;continue;}
    if(op==M_SIN||op==M_COS||op==M_SQRT||op==M_ABS){
      if(!sp)return false;float a=st[sp-1];
      if(op==M_SIN)st[sp-1]=sin(a);else if(op==M_COS)st[sp-1]=cos(a);else if(op==M_SQRT)st[sp-1]=sqrt(max(0.0f,a));else st[sp-1]=fabs(a);
      continue;
    }
    if(sp<2)return false;
    float b=st[--sp],a=st[sp-1];
    if(op==M_ADD)st[sp-1]=a+b;
    else if(op==M_SUB)st[sp-1]=a-b;
    else if(op==M_MUL)st[sp-1]=a*b;
    else if(op==M_DIV)st[sp-1]=(fabs(b)<0.000001f?0:a/b);
    else if(op==M_MOD)st[sp-1]=(fabs(b)<0.000001f?0:fmod(a,b));
    else if(op==M_LT)st[sp-1]=a<b;
    else if(op==M_LE)st[sp-1]=a<=b;
    else if(op==M_GT)st[sp-1]=a>b;
    else if(op==M_GE)st[sp-1]=a>=b;
    else if(op==M_EQ)st[sp-1]=fabs(a-b)<0.0001f;
    else if(op==M_NE)st[sp-1]=fabs(a-b)>=0.0001f;
    else if(op==M_AND)st[sp-1]=(a!=0&&b!=0);
    else if(op==M_OR)st[sp-1]=(a!=0||b!=0);
    else return false;
  }
  return sp>0&&st[0]!=0;
}
void parseMathFunctionStream(char c){
  if(c=='\n'||c=='\r'){
    if(mathRxIdx>0){
      mathRxBuf[mathRxIdx]='\0';String line=String(mathRxBuf);line.trim();
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

// ---------------- CUSTOM FUNCTION ENGINE ----------------
// Custom is deliberately a small voxel-description language, not a second
// full programming language.  It supports calculated variables, ON/OFF
// conditions, and Z rules, then compiles the resulting boolean expression to
// the same local bytecode engine above.
char customRxBuf[96];
byte customRxIdx=0;
String customVarName[4];
String customVarExpr[4];
byte customVarCount=0;
String customAndExpr="";
String customZExpr="";
bool customProgramValid=false;

void resetCustomBuilder(){
  customRxIdx=0;customVarCount=0;customAndExpr="";customZExpr="";
  for(byte i=0;i<4;i++){customVarName[i]="";customVarExpr[i]="";}
}
String expandCustomExpr(String expr,byte depth){
  if(depth>4)return expr;
  String out="";
  for(unsigned int i=0;i<expr.length();){
    char c=expr[i];
    if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_'){
      String name="";
      while(i<expr.length()){
        char q=expr[i];
        if((q>='A'&&q<='Z')||(q>='a'&&q<='z')||(q>='0'&&q<='9')||q=='_'){name+=q;i++;}else break;
      }
      String repl="";bool found=false;
      for(byte k=0;k<customVarCount;k++)if(name.equalsIgnoreCase(customVarName[k])){repl="("+expandCustomExpr(customVarExpr[k],depth+1)+")";found=true;break;}
      out+=found?repl:name;
    }else{out+=c;i++;}
  }
  return out;
}
void addCustomAnd(String expr){
  expr.trim();if(!expr.length())return;
  expr=expandCustomExpr(expr,0);
  if(customAndExpr.length())customAndExpr+="&&";
  customAndExpr+="("+expr+")";
}
void addCustomZRule(String rhs){
  rhs.trim();if(!rhs.length())return;
  rhs=expandCustomExpr(rhs,0);
  String rule="(z==("+rhs+"))";
  if(customZExpr.length())customZExpr+="||";
  customZExpr+=rule;
}
void compileCustomDefinition(){
  String finalExpr="";
  if(customAndExpr.length())finalExpr=customAndExpr;
  if(customZExpr.length()){if(finalExpr.length())finalExpr+="&&";finalExpr+="("+customZExpr+")";}
  if(!finalExpr.length())finalExpr="1";
  if(compileMathExpression(finalExpr.c_str())){
    customProgramValid=true;
    currentCubeMode=4;frameCounter=0;animationStart=millis();lastFrameTime=millis();
    memset((void*)displayBuffer,0,64);
    bluetooth.println("CUSTOM_OK");
  }else{
    customProgramValid=false;currentCubeMode=0;parseMode=0;
    bluetooth.println("CUSTOM_ERROR");
  }
}
void parseCustomFunctionLine(String line){
  line.trim();line.toUpperCase();
  if(!line.length())return;
  if(line=="CUSTOM"||line=="CF_BEGIN"||line=="START CUSTOM")return;
  if(line=="END"||line=="CF_END"){compileCustomDefinition();return;}

  if(line.startsWith("IF ")){
    String cond=line.substring(3);cond.trim();
    if(cond.endsWith(" OFF")){cond=cond.substring(0,cond.length()-4);addCustomAnd("!("+cond+")");}
    else if(cond.endsWith(" ON")){cond=cond.substring(0,cond.length()-3);addCustomAnd(cond);}
    else addCustomAnd(cond);
    return;
  }
  if(line.startsWith("ON IF ")){addCustomAnd(line.substring(6));return;}
  if(line.startsWith("OFF IF ")){addCustomAnd("!("+line.substring(7)+")");return;}

  int eq=line.indexOf('=');
  if(eq>0){
    String lhs=line.substring(0,eq);lhs.trim();
    String rhs=line.substring(eq+1);rhs.trim();
    int orPos=rhs.indexOf(" OR ");
    if(lhs=="Z"){
      if(orPos>=0){
        String first=rhs.substring(0,orPos);String second=rhs.substring(orPos+4);
        int eq2=second.indexOf('=');
        if(eq2>0&&second.substring(0,eq2).equals("Z")){addCustomZRule(first);addCustomZRule(second.substring(eq2+1));}
        else {addCustomZRule(first);addCustomZRule(second);}
      }else addCustomZRule(rhs);
      return;
    }
    if(customVarCount<4){customVarName[customVarCount]=lhs;customVarExpr[customVarCount]=rhs;customVarCount++;}
    return;
  }
  // A plain line is treated as an ON condition.
  addCustomAnd(line);
}
void parseCustomFunctionStream(char c){
  if(c=='\n'||c=='\r'){
    if(customRxIdx>0){customRxBuf[customRxIdx]='\0';String line=String(customRxBuf);customRxIdx=0;parseCustomFunctionLine(line);}
  }else if(customRxIdx<95)customRxBuf[customRxIdx++]=c;
}
void initDefaultCustomProgram(){
  resetCustomBuilder();
  customAndExpr="(((x*3+y*5+f)%16)<8)";
  customZExpr="(z==(7-((x*3+y*5+f)%16)))||(z==(8-((x*3+y*5+f)%16)))";
  String expr=customAndExpr+"&&("+customZExpr+")";
  customProgramValid=compileMathExpression(expr.c_str());
}
void drawCustomFunctionFrame(byte f){
  if(!customProgramValid)return;
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
      else if(in=='C'){parseMode=5;resetCustomBuilder();}
      else if(in=='Y'){parseMode=6;mathRxIdx=0;}
      else if(in=='H'){bluetooth.println("CONNECTED");triggerModeBlinkAcknowledgment();}
      else if(in=='N'&&currentCubeMode==1){animationIndex=(animationIndex+1)%TOTAL_ANIMATIONS;frameCounter=0;lastFrameTime=now;drawAnimationFrame(animationIndex,frameCounter);}
      else if(in=='Q'){currentCubeMode=0;animationStart=now;lastFrameTime=now;triggerModeBlinkAcknowledgment();}
      else if(in=='B'){parseMode=4;}
      else if(in=='E'){currentCubeMode=0;animationStart=now;lastFrameTime=now;triggerModeBlinkAcknowledgment();}
    }else if(parseMode==4){if(in>=2&&in<=8)globalBrightness=in;parseMode=0;}
  }
  if(currentCubeMode==0){if(now-animationStart>=AUTO_MODE_CAROUSEL_TIME){animationIndex=(animationIndex+1)%TOTAL_ANIMATIONS;frameCounter=0;animationStart=now;lastFrameTime=now;}if(now-lastFrameTime>=FRAME_TIME){lastFrameTime=now;drawAnimationFrame(animationIndex,frameCounter);frameCounter=(frameCounter+1)%50;}}
  else if(currentCubeMode==1){if(now-lastFrameTime>=FRAME_TIME){lastFrameTime=now;drawAnimationFrame(animationIndex,frameCounter);frameCounter=(frameCounter+1)%50;}}
  else if(currentCubeMode==3){if(now-lastFrameTime>=FRAME_TIME){lastFrameTime=now;drawMathFrame(frameCounter);frameCounter=(frameCounter+1)%50;}}
  else if(currentCubeMode==4){if(now-lastFrameTime>=FRAME_TIME){lastFrameTime=now;drawCustomFunctionFrame(frameCounter);frameCounter=(frameCounter+1)%50;}}
}
