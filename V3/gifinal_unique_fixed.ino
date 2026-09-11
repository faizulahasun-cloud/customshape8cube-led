#include <Arduino.h>
#include <avr/interrupt.h>
#include <AltSoftSerial.h>
#include "V3FunctionConversion.h"

const byte DATA_PIN=11; const byte CLOCK_PIN=13; const byte LATCH_PIN=12; const byte TOUCH_PIN=10; const byte POT_PIN=A0;
AltSoftSerial bluetooth;
volatile byte currentCubeMode=0;
unsigned int animationIndex=0; byte frameCounter=0;
const unsigned int TOTAL_ANIMATIONS=38;
const unsigned int BUILTIN_ANIMATIONS=37;
const unsigned int BLUETOOTH_FUNCTION_ANIMATION=37;
const unsigned int FRAME_TIME=200;
const unsigned long AUTO_MODE_CAROUSEL_TIME=10000UL;
unsigned long lastFrameTime=0; unsigned long animationStart=0;
bool bluetoothFunctionValid=false;

volatile byte globalBrightness=5;
volatile byte voxelBuffer[2][8][8]; volatile byte activeBuffer=0; byte drawBuffer=1;
volatile byte displayBuffer[2][8][8]; volatile byte activeDisplayBuffer=0; byte drawDisplayBuffer=1;
volatile byte brightnessAccumulator[8]={0,0,0,0,0,0,0,0};

// Non-blocking visible confirmation state. This touches only displayBuffer;
// functionBuffer, bytecode, voxelBuffer, and animation state are preserved.
const unsigned long CONFIRMATION_PHASE_MS=250UL;
const byte CONFIRMATION_NONE=0;
const byte CONFIRMATION_RECEIVED=1;
const byte CONFIRMATION_COMPILED=2;
bool confirmationActive=false;
byte confirmationType=CONFIRMATION_NONE;
byte confirmationPhase=0;
unsigned long confirmationPhaseStart=0;
bool pendingCompiledConfirmation=false;

struct ColumnMap { byte reg; byte bit; };
const ColumnMap COLUMN_MAP[64]={
 {1,0},{1,1},{1,2},{1,3},{1,4},{1,5},{1,6},{1,7},
 {2,0},{2,1},{2,2},{2,3},{2,4},{2,5},{2,6},{2,7},
 {3,0},{3,1},{3,2},{3,3},{3,4},{3,5},{3,6},{3,7},
 {4,0},{4,1},{4,2},{4,3},{4,4},{4,5},{4,6},{4,7},
 {5,0},{5,1},{5,2},{5,3},{5,4},{5,5},{5,6},{5,7},
 {6,0},{6,1},{6,2},{6,3},{6,4},{6,5},{6,6},{6,7},
 {7,0},{7,1},{7,2},{7,3},{7,4},{7,5},{7,6},{7,7},
 {8,0},{8,1},{8,2},{8,3},{8,4},{8,5},{8,6},{8,7}
};
inline byte columnIndex(byte x,byte y){return (y*8)+x;}
void clearCube(){for(byte x=0;x<8;x++)for(byte y=0;y<8;y++)voxelBuffer[drawBuffer][x][y]=0;}
inline void setVoxel(byte x,byte y,byte z,bool state){if(x>=8||y>=8||z>=8)return;if(state)voxelBuffer[drawBuffer][x][y]|=(1<<z);else voxelBuffer[drawBuffer][x][y]&=~(1<<z);}
void commitFrame(){noInterrupts();byte oldActive=activeBuffer;activeBuffer=drawBuffer;drawBuffer=oldActive;byte oldDisplay=activeDisplayBuffer;activeDisplayBuffer=drawDisplayBuffer;drawDisplayBuffer=oldDisplay;interrupts();}
void prepareDisplayData(){byte voxelBuf=drawBuffer;byte outBuf=drawDisplayBuffer;byte localMatrix[8][8];for(byte z=0;z<8;z++){for(byte r=0;r<8;r++)localMatrix[z][r]=0;for(byte y=0;y<8;y++)for(byte x=0;x<8;x++){if(!(voxelBuffer[voxelBuf][x][y]&(1<<z)))continue;byte column=columnIndex(x,y);byte reg=COLUMN_MAP[column].reg;byte bit=COLUMN_MAP[column].bit;if(reg>=1&&reg<=8&&bit<=7)localMatrix[z][reg-1]|=(1<<bit);}}memcpy((void*)displayBuffer[outBuf],localMatrix,64);}
inline void shiftByteFast(byte value){for(int8_t bit=7;bit>=0;bit--){if(value&(1<<bit))PORTB|=_BV(PB3);else PORTB&=~_BV(PB3);PORTB|=_BV(PB5);PORTB&=~_BV(PB5);}}
inline void latchFast(){PORTB|=_BV(PB4);PORTB&=~_BV(PB4);}
void refreshDisplay(){static byte layer=0;byte active=activeDisplayBuffer;brightnessAccumulator[layer]+=globalBrightness;bool layerEnabled=(brightnessAccumulator[layer]>=8);if(layerEnabled)brightnessAccumulator[layer]-=8;byte layerByte=layerEnabled?(1<<layer):0x00;shiftByteFast(layerByte);for(int8_t r=7;r>=0;r--)shiftByteFast(displayBuffer[active][layer][r]);latchFast();layer=(layer+1)%8;}
ISR(TIMER2_COMPA_vect){refreshDisplay();}
void startRefreshTimer(){noInterrupts();TCCR2A=_BV(WGM21);TCCR2B=_BV(CS22)|_BV(CS21)|_BV(CS20);OCR2A=3;TIMSK2|=_BV(OCIE2A);interrupts();}
void stopRefreshTimer(){noInterrupts();TIMSK2&=~_BV(OCIE2A);interrupts();}

// Full cube clear: stop multiplexing first, clear the complete 512-voxel state
// and both frame/display buffers, then shift nine zero bytes (8 column registers
// plus the layer register) and latch. No multiplexing restarts until a new
// operation explicitly starts a mode.
void blankCubeAndStop(){
 stopRefreshTimer();
 noInterrupts();
 memset((void*)voxelBuffer,0,sizeof(voxelBuffer));
 memset((void*)displayBuffer,0,sizeof(displayBuffer));
 for(byte i=0;i<8;i++)brightnessAccumulator[i]=0;
 interrupts();
 shiftByteFast(0x00);
 for(byte r=0;r<8;r++)shiftByteFast(0x00);
 latchFast();
}

// Confirmation display uses the existing multiplexing ISR and only replaces
// displayBuffer contents temporarily. It never calls delay() and never edits
// the stored function or compiled bytecode.
void setConfirmationDisplay(bool on){
 noInterrupts();
 for(byte b=0;b<2;b++)for(byte layer=0;layer<8;layer++)for(byte r=0;r<8;r++)displayBuffer[b][layer][r]=on?0xFF:0x00;
 interrupts();
}

void startConfirmation(byte type){
 confirmationActive=true;
 confirmationType=type;
 confirmationPhase=0;
 confirmationPhaseStart=millis();
 pendingCompiledConfirmation=false;
 // @/R may have stopped multiplexing, so restart the existing Timer2 refresh
 // without changing its configuration or ISR frequency.
 startRefreshTimer();
 setConfirmationDisplay(true);
}

void finishConfirmation(){
 byte finishedType=confirmationType;
 confirmationActive=false;
 confirmationType=CONFIRMATION_NONE;
 confirmationPhase=0;
 setConfirmationDisplay(false);
 if(finishedType==CONFIRMATION_RECEIVED && pendingCompiledConfirmation){
   pendingCompiledConfirmation=false;
   startConfirmation(CONFIRMATION_COMPILED);
   return;
 }
 if(finishedType==CONFIRMATION_COMPILED){
   currentCubeMode=1;
   animationIndex=BLUETOOTH_FUNCTION_ANIMATION;
   frameCounter=0;
   animationStart=millis();
   lastFrameTime=millis();
   drawAnimationFrame(animationIndex,frameCounter);
   prepareDisplayData();
   commitFrame();
 }
}

void serviceConfirmation(unsigned long now){
 if(!confirmationActive)return;
 if(now-confirmationPhaseStart<CONFIRMATION_PHASE_MS)return;
 confirmationPhaseStart=now;
 confirmationPhase++;
 if(confirmationPhase==1)setConfirmationDisplay(false);
 else if(confirmationPhase==2)setConfirmationDisplay(true);
 else if(confirmationPhase==3)setConfirmationDisplay(false);
 else finishConfirmation();
}

inline bool isOuterRing(byte x,byte y){return x==0||x==7||y==0||y==7;}
byte perimeterIndex(byte x,byte y){if(y==0)return x;if(x==7)return 7+y;if(y==7)return 21-x;return 21+(7-y);}
bool firecrackerVoxel(byte f,byte x,byte y,byte z){if(f<16){byte lZ=f/2;if((x==3||x==4)&&(y==3||y==4)){if(z==lZ)return true;if(f>1&&z+1==lZ)return true;}return false;}byte bF=f-16,d=bF/3;if(d>3)d=3;if(z!=7)return false;int vx=(int)x-3,vy=(int)y-3;if(vx==0&&vy==0)return d==0;if(!(vx==0||vy==0||abs(vx)==abs(vy)))return false;return max(abs(vx),abs(vy))==(int)d;}
const byte SNAKE_DIRS[49] PROGMEM={0,5,1,1,5,1,2,4,2,0,0,2,5,2,5,1,4,1,1,5,3,3,0,0,3,1,3,4,4,2,4,0,0,2,4,3,4,4,2,5,5,3,3,1,2,2,0,3,5};
void snakePosition(byte step,byte& sx,byte& sy,byte& sz){int8_t px=3,py=3,pz=3;for(byte s=0;s<step;s++){byte d=pgm_read_byte(&SNAKE_DIRS[s%49]);if(d==0)px++;else if(d==1)px--;else if(d==2)py++;else if(d==3)py--;else if(d==4)pz++;else pz--;}sx=(byte)px;sy=(byte)py;sz=(byte)pz;}
bool snakeVoxel(byte f,byte x,byte y,byte z){for(byte k=0;k<8;k++){byte step=(byte)((f+50-k)%50);byte sx,sy,sz;snakePosition(step,sx,sy,sz);if(x==sx&&y==sy&&z==sz)return true;}return false;}
const byte HEART_MASK[8]={0x66,0xFF,0xFF,0x7E,0x3C,0x18,0x18,0x00};
bool rotatingHeartVoxel(byte f,byte x,byte y,byte z){if(y!=0&&y!=1)return false;byte r=(f/4)%4,u,v;if(r==0){u=x;v=z;}else if(r==1){u=z;v=7-x;}else if(r==2){u=7-x;v=7-z;}else{u=7-z;v=x;}return(HEART_MASK[v]&(1<<u))!=0;}
bool v3DirectionalSweepVoxel(byte f,byte x,byte y,byte z){byte targetX=(f+0)%16;if(0%2==0)return(x==(targetX<8?targetX:15-targetX));return(x==(targetX<8?7-targetX:targetX-8));}
bool v3SphereVoxel(byte f,byte x,byte y,byte z){int cx=3,cy=3,cz=3;int dx=(int)x-cx,dy=(int)y-cy,dz=(int)z-cz;int distSq=dx*dx+dy*dy+dz*dz;int radiusMatch=1+(f%5);return(distSq>=radiusMatch*radiusMatch&&distSq<(radiusMatch+1)*(radiusMatch+1));}
bool v3TunnelVoxel(byte f,byte x,byte y,byte z){byte targetY=(f+(0*3))%8;if(0%3==0)return(y==targetY);if(0%3==1)return(y==(7-targetY));return(y==targetY||z==((f+0)%8));}
bool v3HelixVoxel(byte f,byte x,byte y,byte z){byte angle=(f+0)%8;byte radius=(0%3)+1;int tx=4+((radius*(int)(angle-4))/4);int ty=4+((radius*(int)(4-angle))/4);return((int)x==tx&&(int)y==ty&&(int)z==((f+0+y)%8));}
bool v3MathGridVoxel(byte f,byte x,byte y,byte z){byte coordinateValue=(byte)((x*5+y*3+z*7)%32);byte phase=f%4;byte pattern=(byte)((coordinateValue+phase*8)%32);return((0x55AA55AAUL>>pattern)&1UL)!=0;}
bool v3MatrixRainVoxel(byte f,byte x,byte y,byte z){unsigned int seed=(x*13+y*7+0)%19;byte dropZ=(7-((f+seed)%12));return(z==dropZ);}
bool v3WireCubeVoxel(byte f,byte x,byte y,byte z){byte size=f%4;byte lo=3-size;byte hi=4+size;bool edgeX=(x==lo||x==hi);bool edgeY=(y==lo||y==hi);bool edgeZ=(z==lo||z==hi);return(edgeX&&edgeY)||(edgeY&&edgeZ)||(edgeX&&edgeZ);}
bool v3PlasmaVoxel(byte f,byte x,byte y,byte z){float valX=sin((float)(x+0)*0.5f+(float)f*0.4f);float valY=cos((float)(y-0)*0.4f-(float)f*0.3f);byte targetZ=(byte)(3.5f+3.5f*(valX+valY)/2.0f);return(z==targetZ);}
bool v3CurtainVoxel(byte f,byte x,byte y,byte z){return(((x+y+0)%8)==(f%8))||(((y+z+0)%8)==((7-f)%8));}
bool v3HelixOrbitalVoxel(byte f,byte x,byte y,byte z){byte h1=(f+0)%8,h2=(7-f+0)%8;return(z==h1&&x==y)||(z==h2&&x==(7-y));}
inline bool bluetoothFunctionVoxel(byte f,byte x,byte y,byte z){if(!bluetoothFunctionValid)return false;return V3FunctionConversion::evaluate(x,y,z,f);}

bool animationVoxel(byte a,byte f,byte x,byte y,byte z){
 if(a==0)return z==(f%8); if(a==1)return z==(7-(f%8)); if(a==2)return x==(f%8); if(a==3)return y==(f%8); if(a==4)return x==y&&y==z&&x==(f%8); if(a==5)return x==y&&z==(7-x)&&x==(f%8); if(a==6)return((x+y+z+f)&1)==0;
 if(a==7){byte r=f%5;int d=max(abs((int)x-3),max(abs((int)y-3),abs((int)z-3)));return d==r;}
 if(a==8){byte r=4-(f%5);int d=max(abs((int)x-3),max(abs((int)y-3),abs((int)z-3)));return d==r;}
 if(a==9){if(!(x==3||x==4||y==3||y==4||z==3||z==4))return false;return((x+y+z+f)&1)==0;}
 if(a==10){byte w=(x+y+f)%8;return z==w||z==((w+1)%8);} if(a==11){byte ss=(f/2)%8;if(ss==0)return x==0;if(ss==1)return y==7;if(ss==2)return x==7;return y==0;}
 if(a==12){if(!isOuterRing(x,y))return false;byte p=perimeterIndex(x,y);return((p+f)%28)<3;} if(a==13){if(!isOuterRing(x,y))return false;byte p=perimeterIndex(x,y);return z==((p+f)%8);}
 if(a==14){byte h=(x*3+y*5+f)%16;if(h>=8)return false;byte rz=7-h;return z==rz||(rz<7&&z==rz+1);} if(a==15){int dx=abs((int)x-3),dy=abs((int)y-3);if(dx<=1&&dy<=1){if(z>((f/2)%8))return false;return((x+y+f)&1)!=0;}return false;}
 if(a==16){if(!isOuterRing(x,y))return false;byte p=perimeterIndex(x,y),o=(p+f)%28;return z==(o%8)||z==((o+1)%8);} if(a==17){if(!isOuterRing(x,y))return false;byte p=perimeterIndex(x,y);return z==((p+f)%8);}
 if(a==18){byte r=f%8;int d=abs((int)x-3)+abs((int)y-3)+abs((int)z-3);return d==r||d==r+1;} if(a==19){byte r=f%10;int d=min(abs((int)x-3),abs((int)x-4))+min(abs((int)y-3),abs((int)y-4))+min(abs((int)z-3),abs((int)z-4));return d==r||d==r+1;}
 if(a==20){byte r=9-(f%10);int d=min(abs((int)x-3),abs((int)x-4))+min(abs((int)y-3),abs((int)y-4))+min(abs((int)z-3),abs((int)z-4));return d==r||d==r+1;} if(a==21){int d=abs((int)x-3)+abs((int)y-3)+abs((int)z-3);return((d+f)%4)<2;} if(a==22)return((x+y+z+f)%8)==0;
 if(a==23){if(!((x==0||x==7)&&(y==0||y==7)&&(z==0||z==7)))return false;byte c=((z==7)?4:0)+((y==7)?2:0)+((x==7)?1:0);return c==(f%8);} if(a==24)return firecrackerVoxel(f,x,y,z); if(a==25)return snakeVoxel(f,x,y,z); if(a==26)return rotatingHeartVoxel(f,x,y,z); if(a==27)return v3DirectionalSweepVoxel(f,x,y,z); if(a==28)return v3SphereVoxel(f,x,y,z); if(a==29)return v3TunnelVoxel(f,x,y,z); if(a==30)return v3HelixVoxel(f,x,y,z); if(a==31)return v3MathGridVoxel(f,x,y,z); if(a==32)return v3MatrixRainVoxel(f,x,y,z); if(a==33)return v3WireCubeVoxel(f,x,y,z); if(a==34)return v3PlasmaVoxel(f,x,y,z); if(a==35)return v3CurtainVoxel(f,x,y,z); if(a==36)return v3HelixOrbitalVoxel(f,x,y,z); if(a==37)return bluetoothFunctionVoxel(f,x,y,z); return false;
}
void drawAnimationFrame(unsigned int animation,byte frame){if(animation>=TOTAL_ANIMATIONS)return;clearCube();for(byte z=0;z<8;z++)for(byte y=0;y<8;y++)for(byte x=0;x<8;x++)if(animationVoxel(animation,frame,x,y,z))setVoxel(x,y,z,true);}

void setMode(byte targetMode,unsigned int targetAnimation){
 blankCubeAndStop();
 currentCubeMode=targetMode;
 animationIndex=targetAnimation;
 frameCounter=0;
 animationStart=millis();
 lastFrameTime=animationStart;
 drawAnimationFrame(animationIndex,frameCounter);
 prepareDisplayData();
 commitFrame();
 startRefreshTimer();
}

void setup(){pinMode(DATA_PIN,OUTPUT);pinMode(CLOCK_PIN,OUTPUT);pinMode(LATCH_PIN,OUTPUT);pinMode(TOUCH_PIN,INPUT);PORTB&=~(_BV(PB3)|_BV(PB4)|_BV(PB5));bluetooth.begin(9600);startRefreshTimer();animationStart=millis();lastFrameTime=millis();}

void loop(){
 unsigned long now=millis();
 int rawPot=analogRead(POT_PIN);
 globalBrightness=map(rawPot,0,1023,2,8);
 static bool lastTouchState=false;
 static unsigned long touchDebounceTimer=0;
 static bool hasTriggeredLongPress=false;
 bool currentTouchState=(digitalRead(TOUCH_PIN)==HIGH);
 if(currentTouchState&&!lastTouchState){touchDebounceTimer=now;hasTriggeredLongPress=false;}
 else if(currentTouchState&&lastTouchState){unsigned long touchDuration=now-touchDebounceTimer;if(!hasTriggeredLongPress&&touchDuration>=3000UL){byte targetMode=(currentCubeMode==0)?1:0;setMode(targetMode,animationIndex%BUILTIN_ANIMATIONS);hasTriggeredLongPress=true;}}
 else if(!currentTouchState&&lastTouchState){unsigned long touchDuration=now-touchDebounceTimer;if(!hasTriggeredLongPress&&currentCubeMode==1&&touchDuration>=50&&touchDuration<3000UL){byte nextAnimation=(animationIndex+1)%BUILTIN_ANIMATIONS;setMode(1,nextAnimation);}}
 lastTouchState=currentTouchState;

 while(bluetooth.available()>0){
   char inChar=(char)bluetooth.read();

   // STRICT PROTOCOL/FUNCTION BOUNDARY:
   // Once a function transfer is active, command characters A/M/N/C/R and the
   // protocol marker @ are data for the function receiver, never commands.
   // E alone is the transfer terminator and is never function data.
   // This prevents a formula character from accidentally changing cube mode.
   if(V3FunctionConversion::isFunctionStarted()){
     if(inChar=='E'){
       V3FunctionConversion::stopReception();
       // Confirmation #1 starts only after stopReception() has completed and
       // the engine confirms the complete function is stored successfully.
       if(V3FunctionConversion::isFunctionComplete())startConfirmation(CONFIRMATION_RECEIVED);
     }else{
       V3FunctionConversion::receiveCharacter(inChar);
     }
   }else if(inChar=='@'){
     // @ is a command-layer START marker only when no function transfer is active.
     blankCubeAndStop();
     currentCubeMode=1;
     animationIndex=BLUETOOTH_FUNCTION_ANIMATION;
     frameCounter=0;
     animationStart=millis();
     lastFrameTime=millis();
     V3FunctionConversion::startReception();
   }else if(inChar=='A'){
     if(!confirmationActive)setMode(0,animationIndex%BUILTIN_ANIMATIONS);
   }else if(inChar=='M'){
     if(!confirmationActive)setMode(1,animationIndex%BUILTIN_ANIMATIONS);
   }else if(inChar=='N'&&currentCubeMode==1){
     if(!confirmationActive){byte nextAnimation=(animationIndex+1)%BUILTIN_ANIMATIONS;setMode(1,nextAnimation);}
   }else if(inChar=='C'){
     // Custom selection: clear the full voxel state and WAIT with multiplexing stopped.
     if(!confirmationActive){bluetoothFunctionValid=false;blankCubeAndStop();currentCubeMode=1;animationIndex=BLUETOOTH_FUNCTION_ANIMATION;frameCounter=0;animationStart=millis();lastFrameTime=millis();}
   }else if(inChar=='R'){
     // R is the only command that compiles and starts the stored function.
     // If confirmation #1 is still running, compile now and queue confirmation #2
     // so the two confirmations cannot overlap. Otherwise blank and compile normally.
     if(confirmationActive){
       if(V3FunctionConversion::isFunctionComplete() && V3FunctionConversion::compileFunction()){
         bluetoothFunctionValid=true;
         pendingCompiledConfirmation=true;
       }
     }else{
       blankCubeAndStop();
       if(V3FunctionConversion::isFunctionComplete() && V3FunctionConversion::compileFunction()){
         bluetoothFunctionValid=true;
         startConfirmation(CONFIRMATION_COMPILED);
       }else{
         bluetoothFunctionValid=false;
       }
     }
   }
 }

 now=millis();
 serviceConfirmation(now);
 if(confirmationActive)return;

 if(currentCubeMode==0){
   if(now-animationStart>=AUTO_MODE_CAROUSEL_TIME){animationIndex=(animationIndex+1)%BUILTIN_ANIMATIONS;frameCounter=0;animationStart=now;lastFrameTime=now;}
   if(now-lastFrameTime>=FRAME_TIME){lastFrameTime=now;drawAnimationFrame(animationIndex,frameCounter);prepareDisplayData();commitFrame();frameCounter=(frameCounter+1)%50;}
 }else if(currentCubeMode==1 && animationIndex==BLUETOOTH_FUNCTION_ANIMATION && bluetoothFunctionValid){
   if(now-lastFrameTime>=FRAME_TIME){lastFrameTime=now;drawAnimationFrame(animationIndex,frameCounter);prepareDisplayData();commitFrame();frameCounter=(frameCounter+1)%50;}
 }else if(currentCubeMode==1 && animationIndex<BUILTIN_ANIMATIONS){
   if(now-lastFrameTime>=FRAME_TIME){lastFrameTime=now;drawAnimationFrame(animationIndex,frameCounter);prepareDisplayData();commitFrame();frameCounter=(frameCounter+1)%50;}
 }
}
