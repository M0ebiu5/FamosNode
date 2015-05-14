/*
 * eeprom structure
 * 0....1010 (datapartitionsize)	module config data (1 byte len -> cfg array (eg modules))
 * 1021				load config from eeprom flag (1..active) - disable before update with changed module structure!
 * 1022				log level (0-3)
 * 1023				node id (1-255)
 */
#define CFG_READEEPROM 1021
#define CFG_LOGLEVEL 1022
#define CFG_NODEID 1023

#define DATAPARTITIONSIZE 1010		//module config space vs node config space

#define __PROG_TYPES_COMPAT__
#define verbose
#define version 002

#define ENABLE_DHT
#define ENABLE_ANA
#define ENABLE_FAN
#define ENABLE_DIM
//#define ENABLE_BUT
//#define ENABLE_ONE

#define SERIAL_TIMEOUT 2000

#define MODULESMAX 250
#define MODULIDMAX 16
#define REGISTERSMAX 16
#define TIMERSMAX 6
#define LINKMAX 10
#define CONDMAX 10

#include "enums.h"
#include <dht11.h>
#include <eeprom.h>
#include <SoftReset.h>
#include <avr/wdt.h>

#ifdef ENABLE_DIM
#include <TimerOne.h>
#endif
//#include <DallasTemperature.h>

struct timer_s
{
  byte Type : 5;          //allways 0
  byte ModuleId : 4;
  TimeUnit Unit : 3;         //milli, tensec, tenhour
  byte Mode : 2;         //0..disabled; 1..only once; 2..repeat inf                                         
  byte Wait4Over : 1;    //timer wait 4 overflow
  word Interval;
  byte Cmd;
  word NextTimer;
};
timer_s* pActTimer;
#define TIMTYPE 0
#define TIMSIZE sizeof(timer_s)

/*
bit order from left to right!
S	HO	IF	T	F	E						Id			T
1	1	0	1	0	1	0	0   | 	0	1	0	0	0	0	1	1
multi byte values are little endian -> int 23 =  [23|00]
*/
struct modhead_s		//3 bytes
{
  byte Type : 5;        //Module type                                          
  byte Id : 5;             
  byte Enabled : 1;
  byte HasFollower : 1; //next module will be called with same timer
  byte HasTimer : 1;
  byte IsFollower : 1;
  byte HystOverride : 1;	//ignore hysteresis for evaluation or suppress messaging for actor modules
  byte Startup : 1;			//run module at startup
  byte TimerPtr : 4;
  byte State : 4;
};
modhead_s* pActHead;

struct cond_s
{
   Operator Op : 4;  //<; <=; >; >=; <>; !; =; and; or; +; -;
   byte Left : 5;		//register
   byte Right : 5;		//register
   byte Mode : 4;     //0..just evaluate; 1..evaluate & trigger cmd on true; 2..evaluate on true eval CondPtr; 3..evaluate & trigger cmd on true, on false eval. ConditionPtr; 4..evaluate - on true eval ConditionPtr, on false eval TrgModule (Condition); 5..eval on true trigger and eval; 6..evaluate on true trigger trgmod, on false trigger mod conditionptr (conditionpayload)
   byte Invert : 1;   //1..evaluation false = true
   byte ConditionPtr : 5;
   byte ConditionPayload;
   byte TrgModuleId;
   byte TrgCmd;
};

struct link_s
{
   byte SrcNode;
   byte SrcModule : 5;
   byte SrcSlot : 3;
   byte Mode : 3;             //0..disabled; 1..store to register Reg; 2..store reg and exec cond;  3..exec cond only; 4..copy value from conditionptr (=register) to reg
   byte Pos : 3;			  //byte position in message
   byte Reg : 5;
   byte Len : 3;
   byte ConditionPtr: 5;
   byte Dummy : 5;
};
link_s* pActLink;

struct payload_s {
   byte P1; 
   byte P2;
   byte P3;
   byte P4;
   byte P5;
   byte P6;
   byte P7;
};

struct msg_s
{
   union {
	 byte data[12];
	 struct {
		 byte Length;
		 byte SrcNode;		//> 127...master node -> 7 bits target node; 0xFF broadcast
		 byte SrcModule;
		 byte SrcSlot;
		 byte aPay[8];
	 } p;
   };
};
#define MSGSIZE sizeof(msg_s)

#ifdef ENABLE_BUT
struct butmod_s
{
modhead_s Head;
byte Mode : 3;        //0..disabled; 1..short on; 2..short / long on; 3..short/long/dblclick on;                                                                           
byte ActiveHigh : 1;  //1..active high else active low                                  
byte LongPress : 4;   //debounce time * 10                                         
byte DblClick : 4;    //debounce time * 10                                                
byte Debounce : 3;    // x * 1/100 sec = millis * 10                           
byte LongCount : 4;   //counter for long press                     
byte DblCount : 4;                        
byte PinVal : 1;      //store last value                        
byte Port : 2;            
byte Pin : 4;
word ReArmTime; //time (tensec) to wait for reactivation
};
#define BUTTYPE 1
#define BUTSIZE sizeof(butmod_s)
#endif

#ifdef ENABLE_FAN
struct fanmod_s
{
modhead_s Head;
byte PowerDynamic;    //Power %
byte PowerLow;        //Power %
byte PowerHigh;       //Power %
word OnTime;
word DelayTime;
TimeUnit DelayTimeUnit : 3;
TimeUnit OnTimeUnit : 3;
byte Mode : 3;         //0..low, 1..high, 2..dynamic rpm / hygro
byte PowerPin : 5;
byte InvertPowerPin : 1;	//if 1 -> turn on = LOW
byte PwmPin : 5;
byte Dummy : 4;
};
                                                                        
#define FANTYPE 2
#define FANSIZE sizeof(fanmod_s)
#define FANPDYN sizeof(modhead_s)
#define FANPLOW sizeof(modhead_s) +  1
#define FANPHIG sizeof(modhead_s) +  2
#define FANOTIML sizeof(modhead_s) +  3
#define FANOTIMH sizeof(modhead_s) +  4
#define FANDTIML sizeof(modhead_s) +  5
#define FANDTIMH sizeof(modhead_s) +  6
#endif

#ifdef ENABLE_DIM
const unsigned int dimTable[] PROGMEM  = {0,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,5,5,5,5,5,5,6,6,6,6,6,7,7,7,7,8,8,8,8,9,9,9,10,10,10,11,11,12,12,12,13,13,14,14,15,15,16,16,17,17,18,19,19,20,21,22,22,23,24,25,26,26,27,28,29,30,31,33,34,35,36,37,39,40,41,43,44,46,48,49,51,53,55,56,58,61,63,65,67,69,72,74,77,80,83,85,88,92,95,98,102,105,109,113,117,121,125,129,134,138,143,148,154,159,165,170,176,182,189,195,202,209,217,224,232,240,249,258,267,276,286,296,306,317,328,339,351,364,376,390,403,417,432,447,463,479,496,513,531,550,569,589,610,631,653,676,700,725,750,776,804,832,861,891,922,955,988,1023};

struct dimmod_s
{
modhead_s Head;
byte Mode : 3;         //0..normal (from PwrLow -> PwrHigh with up and downtime) (discard dim values), 1..normal - store dim values
byte PowerPin : 5;
byte PwmPin : 5;
TimeUnit OnTimeUnit : 3;
byte PowerLow;        //Power %
byte PowerHigh;       //Power %
word UpTime;		   //tensec
word DownTime;		   //tensec
word OnTime;   			//tensec - time on high power
byte PwmPeriod;		//pwm Cycle * 100
byte DimTime;      	//in 10 millis - time btw steps on dimming operation
byte OffDimTime;      	//in 100 micros - time btw steps on dimming operation for manual off
byte PVal;				//current power (pwm val max 1023)
};

#define DIMTYPE 5
#define DIMSIZE sizeof(dimmod_s)
#define DIMPLOW sizeof(modhead_s) +  2
#define DIMPHIG sizeof(modhead_s) +  3
#define DIMUPTL sizeof(modhead_s) +  4
#define DIMUPTH sizeof(modhead_s) +  5
#define DIMDOWNTL sizeof(modhead_s) +  6
#define DIMDOWNTH sizeof(modhead_s) +  7
#define DIMONL sizeof(modhead_s) +  8
#define DIMONH sizeof(modhead_s) +  9
#define DIMPWM sizeof(modhead_s) +  10
#define DIMTIME sizeof(modhead_s) +  11
#define DIMOFFTIME sizeof(modhead_s) +  12
#endif
//TODO: 433Mhz, accelerometer, 1-wire

#ifdef ENABLE_DHT
dht11 DHT11;

struct dhtmod_s
{
modhead_s Head;
TimeUnit IntervalUnit : 3;
byte DhtPin : 5;
word MeasureInterval;         //seconds
byte TempHyst;                //Hysteresis 0.1 degrees                  
byte HumiHyst;                //Hysteresis 0.1 %
int TempVal;
word HumiVal;
};
                    
#define DHTTYPE 3
#define DHTSIZE sizeof(dhtmod_s)
#define DHTINTE sizeof(modhead_s) +  1
#define DHTTHYS sizeof(modhead_s) +  3
#define DHTHHYS sizeof(modhead_s) +  4
#define DHTTVAL sizeof(modhead_s) +  5
#define DHTHVAL sizeof(modhead_s) +  7
#endif
#ifdef ENABLE_ANA

struct anamod_s
{
  modhead_s Head;
  byte AnaPin : 3;
  TimeUnit IntervalUnit : 3;
  byte Dummy : 2;
  word MeasureInterval;
  byte Hyst;
  byte Calibration;			//cut off value * 4 for % calculation - eg: calibration val = 110 -> 440 -> 1023 - 440 -> 583 is 100%
  word Value;
  byte PVal;				//percent value
};
#define ANATYPE 4
#define ANASIZE sizeof(anamod_s)
#define ANAHYST sizeof(modhead_s) +  3
#define ANACAL sizeof(modhead_s) +  4
#define ANAVAL sizeof(modhead_s) +  5
#define ANAPVAL sizeof(modhead_s) +  7
#endif

#ifdef ENABLE_ONE
struct onemod_s
{
  modhead_s Head;
  byte OnePin : 4;
  TimeUnit IntervalUnit : 3;
  byte Dummy : 1;
  word MeasureInterval;
  byte NodeCount;
  byte Hyst;
  int Val[6];
};
#define ONETYPE 5
#define ONESIZE sizeof(onemod_s)
#define ONECOUNT sizeof(modhead_s) +  3
#define ONEHYST sizeof(modhead_s) +  4
#define ONEVAL1 sizeof(modhead_s) +  5
#define ONEVAL2 sizeof(modhead_s) +  7
#define ONEVAL3 sizeof(modhead_s) +  9
#define ONEVAL4 sizeof(modhead_s) +  11
#define ONEVAL5 sizeof(modhead_s) +  13
#define ONEVAL6 sizeof(modhead_s) +  15
#endif
/*
typedef void (*function) (byte cmd) ;
function arrOfFunctions[] = { RunFan, RunDht11 };
*/

/*
 * Example function pointer array
void (*p[10]) (byte cmd);
p[0] = RunFan;
result = (*p[0]) (i);
*/

byte NodeId = EEPROM.read(CFG_NODEID);   //Change: : 7 0 1 6 3 255 2
byte LogLevel = 0;  //EEPROM.read(CFG_LOGLEVEL); //0..no logging - Change: : 7 0 1 6 3 254 2

byte Module[MODULESMAX];
byte CountModules;
byte ModuleWritePtr = 0;
byte ModId[MODULIDMAX];

timer_s Timer[TIMERSMAX];
byte CountTimer = 0;                
byte TimerWritePtr = 0;

byte Register[REGISTERSMAX];
byte RegCount = 0;
byte DirectRegCount = 0;

link_s Link[LINKMAX];
byte CountLink;

cond_s Condition[CONDMAX];
byte CountCond;

#define OUTMAX MSGSIZE * 3
#define INMAX MSGSIZE * 3

byte MessageOut[OUTMAX];
byte MessageIn[INMAX];
byte msgoutwrite = 0;
byte msgoutread = 0;
byte msginread = 0;
byte msginwrite = 0;
byte msginstate = 0;		//0..waiting for start sign :; 1..waiting for length; 2..data;
unsigned int msginchk = 0;
msg_s* pActOutMsg = (msg_s*)&MessageOut[0];
msg_s* pActInMsg = (msg_s*)&MessageIn[0];
char buf = 0;

unsigned long ms = 0;
unsigned int mc = 0;
unsigned int mcold = 0;
unsigned int mil = 0;
unsigned int milold = 0;
unsigned int calmilold = 0;
unsigned int tsec = 0;
unsigned int tsecold = 0;
unsigned int hour = 0;
unsigned int hourold = 0;
boolean secOverflow = false;
boolean micOverflow = false;
boolean milOverflow = false;
boolean houOverflow = false;
unsigned int lastinput = 0;

byte tmp = 0;
unsigned int itmp = 0;
unsigned long ltmp = 0;

void setup() {
  byte modpos;
  Serial.begin(57600);
//  Serial.begin(115200);
#if defined(verbose)
  Serial.println("Starting...");
/*
  Serial.print("size: ");
  Serial.print(sizeof(test_s));
  Serial.print("---");
  Serial.println(sizeof(modhead_s));
*/
#endif


LoadConfigEE();
return;


Register[RegCount++] = MODULESMAX - 1 - DirectRegCount++;			//REG 0: seconds
Register[RegCount++] = MODULESMAX - 1 - DirectRegCount++;			//REG 1: minutes
Register[RegCount++] = MODULESMAX - 1 - DirectRegCount++;			//REG 2: hours
Register[RegCount++] = MODULESMAX - 1 - DirectRegCount++;			//REG 3: weekday: 1..monday...7..sunday

byte ptr;
/*
ptr = NewModule(5,DIMTYPE);
dimmod_s* pActDim = (dimmod_s*)&Module[ptr];
//(head:         type   ,id,enabled ,follower,hastimer,isfollower, loopover,startup,timerptr,state),mode ,powerpin ,pwmpin,dummy, onunit,powerlow,powerhigh,uptime,downtime,ontime,pwmperiod * 100,dimtime,offdimtime
dimmod_s tmp = {{DIMTYPE,5 ,1       ,0       ,1       ,0         ,1        ,1     ,0       ,0}     ,0    ,8        ,9     ,0    ,tensec ,0       ,50       ,50   ,300      ,5000  ,5				,1      ,1};
*pActDim = tmp;
RunModule(5,0xff,false);
*/

ptr = NewModule(2,DHTTYPE);
dhtmod_s* pActDht = (dhtmod_s*)&Module[ptr];
//(head:         type   ,id,enabled ,follower,hastimer,isfollower, hystover,startup,timerptr,state), unit ,pin ,interv ,hysttmp,hysthum,tempval,humival
dhtmod_s tmp = {{DHTTYPE,2 ,1       ,0       ,1       ,0         ,1        ,1     ,0       ,0}     ,tensec,6   ,150      ,1       ,1      ,0      ,0    };
/*
 * Sample memory dump for dht module with sendconfig
 *
struct dhtmod_s
{
modhead_s Head;
TimeUnit IntervalUnit;
unsigned int MeasureInterval;         //seconds
byte DhtPin;
byte TempHyst;                //Hysteresis 0.1 degrees
byte HumiHyst;                //Hysteresis 0.1 %
int TempVal;
unsigned int HumiVal;
};
035 012 001 000 004 067 212 000 001 150 000 006 001 035 011 001 000 004 001 023 000 058 000
:   len nod mod slt h1  h2  h3 unit iv1 iv2 pin hyt  :  len nod mod slt hyh t1  t2  h1  h2
<----msg header --><-mod head-><---mod data--------> <----msg header--><-mod data--------->

*/
*pActDht = tmp;
RunModule(2,0xff,false);
Register[RegCount++] = ptr + DHTHVAL;								//REG 4: Humi value
Register[RegCount++] = MODULESMAX - 1 - DirectRegCount++;			//REG 5: Free Register 1 store Humi comparison value high
Module[Register[RegCount-1]] = 51;									//set Humi Comp Value
Register[RegCount++] = MODULESMAX - 1 - DirectRegCount++;			//REG 6: Free Register 1 store Humi comparison value low
Module[Register[RegCount-1]] = 47;									//set Humi Comp Value

/*
byte AnaPin : 3;
  TimeUnit IntervalUnit : 3;
  byte Dummy : 2;
  word MeasureInterval;
  byte Hyst;
  byte Calibration;			//cut off value * 4 for % calculation - eg: calibration val = 110 -> 440 -> 1023 - 440 -> 583 is 100%
  word Value;
  byte PVal;				//percent value
*/

ptr = NewModule(1,ANATYPE);
anamod_s* pActAna = (anamod_s*)&Module[ptr];
//(head:         type    ,id ,enabled,follower,hastimer,isfollower, loopover,start,timerptr,state),pin,int.unit,dmy,interv,hyst,calibration,val,pval
anamod_s tmpa = {{ANATYPE, 1 ,1      ,0       ,1       ,0         ,0        ,1    ,0       ,0 }   ,0  ,tensec  ,0  ,150   ,1   ,100        ,0  ,0        };
*pActAna = tmpa;
//CreateTimer(0xff,0,tensec,0);
RunModule(1,0xff,false);
Register[RegCount++] = ptr + ANAPVAL;		//REG 7: Ambi % value
Register[RegCount++] = MODULESMAX - 1 - DirectRegCount++;			//REG 8: Free Register 2 store Ambi comparison value
Module[Register[RegCount-1]] = 10;										//set Ambi Comp Value
//Register[RegCount++] = MODULESMAX - 1 - DirectRegCount++;

/*
modhead_s Head;
unsigned char PowerDynamic;    //Power %
unsigned char PowerLow;        //Power %
unsigned char PowerHigh;       //Power %
unsigned int OnTime;
unsigned int DelayTime;
TimeUnit DelayTimeUnit;
TimeUnit OnTimeUnit;
byte Mode : 3;         //0..low, 1..high, 2..dynamic rpm / hygro
byte PowerPin : 5;
byte InvertPowerPin : 1;	//if 1 -> turn on = LOW
byte PwmPin : 5;
byte Dummy : 4;
};
 */

ptr = NewModule(3,FANTYPE);
fanmod_s* pActFan = (fanmod_s*)&Module[ptr];
//(head:          type   ,id,enabled ,follower,hastimer,isfollower,loopover, start,timerptr,state) ,pwrdyn,pwrlow,pwrhigh,ontime,delaytime,delayunit,onunit,mode,powerpin,invertpower,pwmpin,dummy
fanmod_s tmpf = {{FANTYPE,3 ,1       ,0       ,1       ,0         ,0       ,0     ,0       ,0}     ,0     ,20    ,90     ,400   ,200      ,tensec   ,tensec,0   ,7       ,0          ,3 };
*pActFan = tmpf;
RunModule(3,0xff,false);
Register[RegCount++] = ptr + FANPDYN;								//REG 5: Humi value


/*
Operator Op : 4;  //<; <=; >; >=; <>; !; =; and; or; +; -;
   byte Left : 5;		//register
   byte Right : 5;		//register
   byte Mode : 4;     //0..just evaluate; 1..evaluate & trigger cmd on true; 2..evaluate on true eval CondPtr; 3..evaluate & trigger cmd on true, on false eval. ConditionPtr; 4..evaluate - on true eval ConditionPtr, on false eval TrgModule (Condition); 5..on true trigger and eval; 6..on true trigger trgmod, on false trigger conditionptr (conditionpayload)
   byte Store : 1;	  //1..store result
   byte Invert : 1;   //1..evaluation false = true
   byte ConditionPtr : 5;
   byte TrgModulePtr;
   byte TrgCmd;
*/
// Check humi value high
Condition[CountCond].Op = op_greater;
Condition[CountCond].Left = 4;
Condition[CountCond].Right = 5;
Condition[CountCond].Mode = 4;
Condition[CountCond].Invert = 0;
Condition[CountCond].ConditionPtr = 2;
Condition[CountCond].TrgModuleId = 1;
Condition[CountCond].TrgCmd = 4;
CountCond++;

// Check humi value low
Condition[1].Op = op_greater;
Condition[1].Left = 4;
Condition[1].Right = 6;
Condition[1].Mode = 1;
Condition[1].Invert = 0;
Condition[1].ConditionPtr = 0;
Condition[1].TrgModuleId = 3;
Condition[1].TrgCmd = 2;
CountCond++;

// Check ambi value
//ambi val < reg4 (=dark) -> run fan high else low
Condition[2].Op = op_greater;
Condition[2].Left = 8;
Condition[2].Right = 7;
Condition[2].Mode = 6;
Condition[2].Invert = 0;
Condition[2].ConditionPtr = 3;
Condition[2].ConditionPayload = 2;			//low
Condition[2].TrgModuleId = 3;
Condition[2].TrgCmd = 3;					//high with delay
CountCond++;

// Check ambi value
//ambi val > reg4 (bright) -> reduce high speed
Condition[3].Op = op_greater;
Condition[3].Left = 7;
Condition[3].Right = 8;
Condition[3].Mode = 1;
Condition[3].Invert = 0;
Condition[3].ConditionPtr = 0;
Condition[3].TrgModuleId = 3;
Condition[3].TrgCmd = 5;								//reduce from high
CountCond++;

//Hygro over
//			   operator, ,left,right,result,mode,store,invert,condptr,trgmod,trgcmd
//cond_s tmpc2 = {op_greater,0   ,1	,0	   ,3	,0	  ,0	 ,0		 ,0		,0};	//fanmod cmd=max power
//Condition[1] = tmpc2;

/*
struct link_s
{
   byte SrcNode;
   byte SrcModule : 5;
   byte SrcSlot : 3;
   byte Mode : 3;                          //0..disabled; 1..store to register Reg; 2..store reg and exec cond;  3..exec cond only; 4..copy value from conditionptr (=register) to reg
   byte Pos : 3;			  //byte position in message
   byte Reg : 5;
   byte Len : 3;
   byte ConditionPtr: 5;
};
*/


Link[0].SrcNode = NodeId;
Link[0].SrcModule = 2;
Link[0].SrcSlot = 1;
Link[0].Mode = 3;			//eval only
Link[0].Pos = 0;
Link[0].Reg = 5;			//store humi to fan dynamic power val
Link[0].Len = 1;
Link[0].ConditionPtr = 0;
CountLink++;



Link[1].SrcNode = NodeId;
Link[1].SrcModule = 1;
Link[1].SrcSlot = 0;
Link[1].Mode = 3;
Link[1].Pos = 0;
Link[1].Reg = 0;
Link[1].ConditionPtr = 3;
CountLink++;


SaveConfigEE();

/*
CreateTimer(5,200,tensec,1);
pActTimer->Cmd = 3;
ReloadTimer(pActTimer);
*/

modpos = 0;
//RunModule(5,1,false);
RunModule(2,0,false);
RunModule(1,0,false);
//RunModule(3,2,false);
//ReadConfigEE();
//SaveConfigEE();

/*
pActInMsg->p.SrcNode = 0;		//master message
pActInMsg->p.SrcModule = 2;		//read condig
pActInMsg->p.SrcSlot = 5;		//eeprom
pActInMsg->p.aPay[0] = 0;		//adr
pActInMsg->p.aPay[1] = 7;		//number of bytes
RecvMsg(false);
*/

}

void loop() {
 ms = millis();
 mil = ms;
 tsec = ms / 100; 
 hour = tsec / 360;

  if (mil < milold) {
	milOverflow = true;
	if (tsec < tsecold) {
	   secOverflow = true;
	   if (hour < hourold)
		  houOverflow = true;
	   else
		  houOverflow = false;
	}
	else
	   secOverflow = false;
   }
 else
   milOverflow = false;


  if ((mil - calmilold) / 1000 > 0)	{  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  //TicTac handler
	  Module[Register[0]] = Module[Register[0]] + ((mil - calmilold) / 1000);		//add seconds
	  if (Module[Register[0]] > 59) {											//second overflow
		  Module[Register[0]] = Module[Register[0]] - 60;
		  Module[Register[1]]++;
		  if (Module[Register[1]] > 59) {										//minute overflow
			  Module[Register[1]] = Module[Register[1]] - 60;
			  Module[Register[2]]++;
			  if (Module[Register[2]] > 23) {									//hour overflow
				  Module[Register[2]] = Module[Register[2]] - 24;
				  Module[Register[3]]++;
				  if (Module[Register[3]] > 7)									//weekday overflow
					  Module[Register[3]] = 1;
			  }
		  }
	  }
	  calmilold = mil;
  }
                       
  CheckModuleTimers();

  if(Serial.available())
  {
      ReceiveSerial();
  }

  if (msginread != msginwrite) {
//	  Serial.println("msg");
	  pActInMsg = (msg_s*)&MessageIn[msginread];
	  RecvMsg(false);
		if (msginread + MessageIn[msginread] < INMAX - sizeof(msg_s))
			msginread+= MessageIn[msginread];
		else
			msginread = 0;
  }
/*
Serial.print("read: ");
Serial.println(msginread);
Serial.print("write: ");
Serial.println(msginwrite);
*/
  milold = mil;
  tsecold = tsec;
  hourold = hour;
}


byte getVal(char c)
{
   if (c >= '0' && c <= '9')
     return (byte)(c - '0');
   else
     return (byte)(c-'A'+10);
}

boolean CheckInput(char c)
{
	if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))
		return true;
	else
		return false;
}

//receive msg
//:lddddd
//msg starts with : followed by length and data
boolean ReceiveSerial() {

	if (mil - lastinput > SERIAL_TIMEOUT)
		msginstate = 0;

	pActInMsg = (msg_s*)&MessageIn[msginwrite];
	lastinput = mil;

	char rec = (char)Serial.read();
//	Serial.print("mstate: " );
//	Serial.println(msginstate);
	if (msginstate == 0) {
		if (rec == ':')			//hex: 3A - Dec 58
			msginstate=1;
	}
	else if(msginstate == 1) {
		if (CheckInput(rec)){
			buf = rec;
			msginstate++;
		}
		else
			msginstate = 0;
	}
	else if(msginstate == 2) {
		if (CheckInput(rec)){
			pActInMsg->p.Length = getVal(rec) + (getVal(buf) << 4);
//			Serial.print("mlen: " );
//			Serial.println(pActInMsg->p.Length);
			if (pActInMsg->p.Length > 3 && pActInMsg->p.Length < 13)
				msginstate++;
			else
				msginstate = 0;
		}
		else
			msginstate = 0;
	}
	else if(msginstate >= 3) {
//		rec-=48;
		if (msginstate < 0xfe) {
			tmp = (msginstate / 2) -1;
			if (msginstate % 2 == 1)
			{
				buf = rec;
			}
			else {
				pActInMsg->data[tmp] = getVal(rec) + (getVal(buf) << 4);
/*				Serial.print("tmp: ");
				Serial.print(tmp);
				Serial.print(" ddd: ");
				Serial.println(pActInMsg->data[tmp]);
*/
			}
			msginstate++;

			if (tmp >= pActInMsg->p.Length - 1) {
/*				Serial.print("tmp: ");
				Serial.println(tmp);
				Serial.print("le: ");
				Serial.println(pActInMsg->p.Length);
*/
				if (msginwrite + pActInMsg->p.Length < INMAX - sizeof(msg_s))
					msginwrite+= pActInMsg->p.Length;
				else
					msginwrite = 0;
				if (msginwrite == msginread)
					SendMsg(0,0,1,0,&rec,2);		//send overflow error: Module 0, Slot 0
//				Serial.print("state: ");
//				Serial.println(msginstate);
//				Serial.print("msg: " );
//				Serial.println(pActInMsg->p.SrcModule);
				msginstate = 0;
				return true;							//msg complete!
//				msginstate = 0xfe;
			}
		}
		else {											//checksum disabled
			msginchk = msginchk << 8;
			msginchk = rec;
			msginstate++;
			if (msginstate == 0) {
				CheckSum(&pActInMsg->data[2],pActInMsg->p.Length);
			}
		}
	}
	/*
	Serial.print("len: ");
	Serial.println(pActInMsg->p.Length);
	Serial.print("state: ");
	Serial.println(msginstate);
	*/
	return false;
}


void CheckModuleTimers() {
  byte i = 0;
  unsigned int comp;
/*
	 Serial.print("count timer: ");
	 Serial.println(CountTimer);
*/
  for (i = 0; i < CountTimer; i++) {
     pActTimer = &Timer[i];
/*
   	 Serial.print("int: ");
   	 Serial.print(pActTimer->Interval);
   	 Serial.print("timer: ");
   	 Serial.print(pActTimer->NextTimer);
   	 Serial.print("mode: ");
   	 Serial.print(pActTimer->Mode);
   	 Serial.print("tmr i: ");
   	 Serial.println(i);
*/
     if (pActTimer->Mode) {
        if (pActTimer->Wait4Over) {
          if (pActTimer->Unit == milli) {
			   if (milOverflow)
				 pActTimer->Wait4Over = false;
			   else
				 continue;
			}
          if (pActTimer->Unit == tensec) {
             if (milOverflow)
               pActTimer->Wait4Over = false;
             else
               continue;
           }
        }
        
        if (pActTimer->Unit == milli)
             comp = ms;
        else if (pActTimer->Unit == tensec)
             comp = tsec;
        else 
             comp = hour;
/*
   	 Serial.print("int: ");
   	 Serial.println(pActTimer->Interval);
   	 Serial.print("timer: ");
   	 Serial.println(pActTimer->NextTimer);
   	 Serial.print("tmr i: ");
   	 Serial.println(i);
*/
        if (pActTimer->NextTimer > comp) 
            continue;
       
       if (pActTimer->Mode == 1)
           pActTimer->Mode = 0;
       else {
/*
    	 Serial.print("unit: ");
    	 Serial.println(pActTimer->Unit);
*/


    	 comp += pActTimer->Interval;
//         pActTimer->NextTimer = comp + pActTimer->Interval;
    	 if (comp < pActTimer->Interval) {
//Serial.print("over: ");
//Serial.println(comp);
    		 pActTimer->Wait4Over = true;
    	 }
//         pActTimer->Wait4Over = (SREG & 0x01);      //check carry flag 4 overflow
    	 pActTimer->NextTimer = comp;
       }
       
       RunModule(pActTimer->ModuleId, pActTimer->Cmd, true);
       break;                                  
     }
  }
}

byte NewModule(byte Id, byte Type) {
/*
	Serial.print("id: ");
	Serial.print(Id);
	Serial.print(" size: ");
	Serial.print(GetModuleSize(Type));
	Serial.print(" ptr: ");
	Serial.println(ModuleWritePtr);
*/
	byte oldptr = ModuleWritePtr;
	ModId[Id] = ModuleWritePtr;
	ModuleWritePtr += GetModuleSize(Type);
    CountModules++;
    return oldptr;
}

void RunModule(byte ModuleId, byte ModuleCmd, bool bTimerCall) {
	pActHead = (modhead_s*)&Module[ModId[ModuleId]];
/*
	Serial.print("r id: ");
	Serial.print(ModuleId);
	Serial.print("r mid: ");
	Serial.print(pActHead->Id);
	Serial.print("r type: ");
	Serial.print(pActHead->Type);
	Serial.print(" Modptr: ");
	Serial.println(ModId[ModuleId]);
*/
	switch(pActHead->Type) {
#ifdef ENABLE_FAN
	case FANTYPE:
		RunFan(ModuleCmd);
	  	break;
#endif
#ifdef ENABLE_DHT
	case DHTTYPE:
		RunDht11(ModuleCmd);
	    break;
#endif
#ifdef ENABLE_ANA
	case ANATYPE:
		RunAnalogIn(ModuleCmd);
	    break;
#endif
#ifdef ENABLE_BUT
	case BUTTYPE:

		break;
#endif
#ifdef ENABLE_DIM
	case DIMTYPE:
		RunDim(ModuleCmd);
		break;
#endif
	default:
		break;
	}

  if (bTimerCall && pActHead->HasFollower) {
     RunModule(++ModuleId, ModuleCmd, bTimerCall);
  }
}

void CreateTimer(byte ModuleId, int Interval, TimeUnit tu, byte Mode) {
#if defined(verbose)
	Serial.print(" timer mod id: ");
	Serial.print(ModuleId);
	Serial.print(" timer unit: ");
	Serial.print(tu);
	Serial.print(" timer int: ");
	Serial.println(Interval);
#endif
     pActTimer = &Timer[TimerWritePtr];      
//     *pActTimer = {TIMTYPE,ModulePtr,Interval,tu,Mode};
     pActTimer->Type = TIMTYPE;
     pActTimer->ModuleId = ModuleId;
     pActTimer->Interval = Interval;
     pActTimer->Unit = tu;
     pActTimer->Mode = Mode;
     CountTimer++;
     TimerWritePtr++;
}

//sync local time to network time
//and adapt all timers
void SyncTime() {

}


//--------------------------------------------------
//--------------- New Config Data -----------------------------
//create new data object from msg
//Slot: 0..modules; 1..timer; 2..link; 3..condition; 4..register; 5..direct register;
//Module: pay0: -> module id; pay1: -> module type
//Timer: pay0: -> module id; 1: intervall; 2: unit; 3: mode

void NewConfigData() {
	byte type = pActInMsg->p.SrcSlot;

	switch(type)
	  {
		 case 0:
			 NewModule(pActInMsg->p.aPay[0],pActInMsg->p.aPay[1]);
			 break;
		 case 1:
			 CreateTimer(pActInMsg->p.aPay[0],pActInMsg->p.aPay[1],(TimeUnit)pActInMsg->p.aPay[2],pActInMsg->p.aPay[3]);
			 break;
		 case 2:
			 if (pActInMsg->p.Length < 8)
				 break;
			 Link[CountLink].SrcNode = pActInMsg->p.aPay[0];
			 Link[CountLink].SrcModule = pActInMsg->p.aPay[1];
			 Link[CountLink].SrcSlot = pActInMsg->p.aPay[2];
			 Link[CountLink].Mode = pActInMsg->p.aPay[3];
			 Link[CountLink].Pos = pActInMsg->p.aPay[4];
			 Link[CountLink].Reg = pActInMsg->p.aPay[5];
			 Link[CountLink].Len = pActInMsg->p.aPay[6];
			 Link[CountLink].ConditionPtr = pActInMsg->p.aPay[7];
			 CountLink++;
			 break;
		 case 3:
			 if (pActInMsg->p.Length < 8)
			 	break;
			 Condition[CountCond].Op = pActInMsg->p.aPay[0];
			 Condition[CountCond].Left = pActInMsg->p.aPay[1];
			 Condition[CountCond].Right = pActInMsg->p.aPay[2];
			 Condition[CountCond].Mode = pActInMsg->p.aPay[3];
			 Condition[CountCond].Invert = pActInMsg->p.aPay[4];
			 Condition[CountCond].ConditionPtr = pActInMsg->p.aPay[5];
			 Condition[CountCond].TrgModuleId = pActInMsg->p.aPay[6];
			 Condition[CountCond].TrgCmd = pActInMsg->p.aPay[7];
			 CountCond++;
			 break;
		 case 4:
			 Register[RegCount++] = pActInMsg->p.aPay[0];
		 	 break;
		 case 5:
			 Register[RegCount++] = MODULESMAX - 1 - DirectRegCount++;
			 Module[Register[RegCount-1]] = pActInMsg->p.aPay[0];
		 	 break;
	  }

}

//--------------------------------------------------
//--------------- Change Config Data -----------------------------
//---------------- cmd: 1 ----------------------------
//load data from msg into node storage
//Slot: 0..modules; 1..timer; 2..link; 3..condition; 4..register; 5..direct register; 6..eeprom
//pay0: -> 0xff..load at writeptr / other: item index / high byte eeprom
//pay1: -> low byte address eeprom  / others: offset count from start
//change eeprom startup flag 1022: 58 7 0 1 6 3 254 1

void ChangeConfigData() {
	byte type = pActInMsg->p.SrcSlot;
	byte wptr;
	byte rptr = 0;
	byte* dataptr = 0;

	if (type < 6) {
		dataptr = GetDataPtr(type,pActInMsg->p.aPay[0],&wptr);
		dataptr+= pActInMsg->p.aPay[1];
		for (byte j=2; j < pActInMsg->p.Length - 4; j++) {
			*dataptr = pActInMsg->p.aPay[j];
			dataptr++;
	//		Module[wptr++] = pActInMsg->aPay[j];
		}
		SendMsg(0,1,true,1,&dataptr,2);
	}
	else {
		itmp = pActInMsg->p.aPay[0];
		itmp = itmp << 8;
		itmp = itmp + pActInMsg->p.aPay[1];
		for (rptr = 6; rptr < pActInMsg->p.Length; rptr++) {
			EEPROM.write(itmp++,(unsigned char)pActInMsg->data[rptr]);
#ifdef verbose
			Serial.print(itmp-1);
			Serial.print("---");
			Serial.println(pActInMsg->data[rptr]);
#endif
		}
		itmp--;
		SendMsg(0,1,true,2,&itmp,2);
	}
}
//--------------------------------------------------
//--------------- Send Config Data -----------------------------
//--------------- cmd: 2   -----------------------------------
//read data from module array
//Slot: 0..modules; 1..timer; 2..link; 3..condition; 4..register; 5..direct register; 6..eeprom
//pay0: -> eeprom: load address; / others: item id or index / high byte eeprom ; 0xff..read all
//pay1: -> low byte eeprom address  / others: offset bytes from start
//pay2: length
//									  : len src m  s p0 p1 p2
//ex: read first 12 bytes of modules: 58 7  0   2  0 0  0  12
//ex: read first 32 bytes of eeprom : 58 7  0   2  6 0  0  32
void SendConfigData() {
	byte type = pActInMsg->p.SrcSlot;
	byte rptr = 0;
	byte* dataptr = 0;
	byte wptr = 0;
	byte len;

#ifdef verbose
	Serial.print("Sendconfigdata");
	Serial.print(type);
	Serial.print("---");
	Serial.print(pActInMsg->p.aPay[0]);
	Serial.print("---");
	Serial.print(pActInMsg->p.aPay[1]);
	Serial.print("---");
	Serial.println(pActInMsg->p.aPay[2]);
#endif

	len = pActInMsg->p.aPay[2];
	pActOutMsg->p.SrcModule = 0;
	if (type < 6) {
		dataptr = GetDataPtr(type,pActInMsg->p.aPay[0],&rptr);
		dataptr += pActInMsg->p.aPay[1];
		for (rptr = 0; rptr < len; rptr++) {
			pActOutMsg->p.aPay[wptr++] = *dataptr;
			dataptr++;
			if (wptr > 7) {
				SendMsg(0,2,true,wptr,&pActOutMsg->p.aPay[0],2);
				wptr = 0;
			}
		}
		if (wptr != 0)
			SendMsg(0,2,true,wptr,&pActOutMsg->p.aPay[0],2);
	}
	else {
		itmp = pActInMsg->p.aPay[0];
		itmp = itmp << 8;
		itmp = itmp + pActInMsg->p.aPay[1];
//		Serial.print("itmp");
//		Serial.println(itmp);
		for (rptr = 0; rptr < len; rptr++) {
			pActOutMsg->p.aPay[wptr++] = EEPROM.read(itmp++);
			/*
			Serial.print(rptr);
			Serial.print("---");
			Serial.print(itmp);
			Serial.print("---");
			Serial.println(pActOutMsg->p.aPay[rptr]);
			*/
			if (wptr > 7) {
				SendMsg(0,2,true,wptr,&pActOutMsg->p.aPay[0],2);
				wptr = 0;
			}
		}
		if (wptr != 0)
			SendMsg(0,2,true,wptr,&pActOutMsg->p.aPay[0],2);
	}
}

byte* GetDataPtr(byte Type, byte pos, byte* wptr) {
	if (Type == 0) {
		if (pos == 0xff)
			*wptr = ModuleWritePtr;
		else {
			*wptr = pos;
		}
		return &Module[ModId[pos]];		//get start adr of module by id
	}
	else if (Type == 1) {
		return (byte*)&Timer[pos];
	}
	else if (Type == 2) {
		return (byte*)&Link[pos];
	}
	else if (Type == 3) {
		return (byte*)&Condition[pos];
	}
	else if (Type == 4) {
		return &Register[pos];
	}
	else
		return &Module[Register[pos]];
}
//--------------------------------------------------
//--------------- Run Cfg Command -----------------------------
//cmd id == Slot; command payload == Pay[0]....
//slot: 1..reset
void RunCfgCmd() {
	byte mp = 0;

	if (pActInMsg->p.SrcSlot == 1){		//reset
		soft_restart();
	}
}
//===================================================


//--------------------------------------------------
//--------------- Run Module -----------------------------
//run module id == Slot; command == Pay[0]
//
void RunModuleCmd() {
	byte mp = 0;
	for(byte j = 0; j < CountModules; j++) {
		pActHead = (modhead_s*)&Module[mp];
		if (pActHead->Id == pActInMsg->p.SrcSlot) {
			RunModule(pActHead->Id,pActInMsg->p.aPay[0],0);
			return;
		}
		mp+=GetModuleSize(pActHead->Type);
	}
}
//===================================================
#ifdef ENABLE_FAN
//--------------------------------------------------
//--------------- FAN -----------------------------
//--------------------------------------------------
// Cmd: 1...turn off; 0...timer call; 2..turn on slow; 3..turn on high delay; 4..turn on high; 5..reduce from high ; 6..turn on dynamic delay; 0xFF...config
void RunFan(byte Cmd) {
  byte PowerVal;
  byte LastState;
  byte LastMode;

  fanmod_s* pFanMod = (fanmod_s*)pActHead;
  
  LastState = pFanMod->Head.State;
  LastMode = pFanMod->Mode;

  #if defined(verbose)  
      Serial.print("Runfan: ");
      Serial.print(Cmd);
      Serial.print(" State:");
      Serial.println(pFanMod->Head.State);
//      Serial.print("timerptr");
//      Serial.println(pFanMod->Head.TimerPtr);
//      getRPMS();
  #endif

  if (Cmd == 0xff) {
	 if (pFanMod->PowerPin) {
		 pinMode(pFanMod->PowerPin, OUTPUT);
		 if (pFanMod->InvertPowerPin)
			 digitalWrite(pFanMod->PowerPin, HIGH);
		 else
			 digitalWrite(pFanMod->PowerPin, LOW);
	 }
	 if (pFanMod->PwmPin) {
		 pinMode(pFanMod->PwmPin, OUTPUT);
	 }
  //pinMode(2, INPUT);		//for rpm data
     // Set up Fast PWM on Pin 3                          
     TCCR2A = 0x23;                             
     // Set prescaler                   
     TCCR2B = 0x0A;   // WGM21, Prescaler = /8
     // Set TOP and initialize duty cycle to zero(0)
     OCR2A = 79;    // TOP DO NOT CHANGE, SETS PWM PULSE RATE
     OCR2B = 0;    // duty cycle for Pin 3 (0-79) generates 1 500nS pulse even when 0 :(                                                                         

     if (pFanMod->Head.HasTimer) {
    	 pFanMod->Head.TimerPtr = TimerWritePtr;
    	 if (!pFanMod->Head.IsFollower)
    		 CreateTimer(pActHead->Id,pFanMod->DelayTime,pFanMod->DelayTimeUnit,0);
     }
     return;
  }

  if (Cmd == 1) 						//turn off
        pFanMod->Head.State = 2;
  else if (Cmd == 2) {					//slow
	  pFanMod->Head.State = 2;
	  pFanMod->Mode = 0;
  }
  else if (Cmd == 3 || Cmd == 6) {		//high or dynamic with delay
	  if (Cmd == 3)
		  pFanMod->Mode = 1;
	  else
		  pFanMod->Mode = 2;
  	  if (pFanMod->Head.State == 1)		//allready waiting?
  	  	return;
  	  if (pFanMod->Head.State == 3)		//allready running? update dynamic value and extend runtime
  		pFanMod->Head.State = 2;
  	  else
  		  pFanMod->Head.State = 0;
  }
  else if (Cmd == 4) {					//high
	  pFanMod->Mode = 1;
		pFanMod->Head.State = 2;
  }
  else if (Cmd == 5) {					//all, but high
	  if (pFanMod->Mode > 0)
		  pFanMod->Mode = 0;
	  else
		  return;
	  if (pFanMod->Head.State == 3)
		  pFanMod->Head.State = 2;
	  else
		  return;
  }
  
  if (pFanMod->Head.HasTimer) {
  	  pActTimer = &Timer[pFanMod->Head.TimerPtr];
  }

  switch(pFanMod->Head.State)
  {
	 case 0:
//		OCR2B = 0;  //fan off
		pFanMod->Head.State = 1;
		if (pFanMod->Head.HasTimer) {
			pActTimer->Mode = 1;
			pActTimer->Interval = pFanMod->DelayTime;
			pActTimer->Unit = pFanMod->DelayTimeUnit;
			ReloadTimer(pActTimer);
		}
		break;
	 case 1:			//delay
		if (Cmd != 0) 	//not from timer - keep waiting
			return;
	 case 2:            //Turn on
		 if (pFanMod->InvertPowerPin)
			 digitalWrite(pFanMod->PowerPin, LOW);
		 else
			 digitalWrite(pFanMod->PowerPin, HIGH);
	   if (pFanMod->Mode == 0)
		 PowerVal = pFanMod->PowerLow;
	   if (pFanMod->Mode == 1)
		 PowerVal = pFanMod->PowerHigh;
	   if (pFanMod->Mode == 2) {
		 PowerVal = pFanMod->PowerDynamic;
	   }
	   OCR2B = (PowerVal * 8) / 10;
	   if (pFanMod->Head.HasTimer) {
		   pActTimer->Mode = 1;
		   pActTimer->Interval = pFanMod->OnTime;
		   pActTimer->Unit = pFanMod->OnTimeUnit;
		   ReloadTimer(pActTimer);
	   }
	   pFanMod->Head.State = 3;
	   if (LastState != 3 || pFanMod->Mode != LastMode)
		   SendMsg(pActHead->Id,2,false,1,&PowerVal,pFanMod->Head.HystOverride);
	   break;
	 case 3:            //Turn off
		 OCR2B = 0;
		 pFanMod->Head.State = 0;
		 if (pFanMod->InvertPowerPin)
			 digitalWrite(pFanMod->PowerPin, HIGH);
		 else
			 digitalWrite(pFanMod->PowerPin, LOW);
		 SendMsg(pActHead->Id,1,false,1,&PowerVal,pFanMod->Head.HystOverride);
		 if (pFanMod->Head.HasTimer) {
			 pActTimer->Mode = 0;
		 }
	   break;
  }
}

char getRPMS() {
  unsigned long time = pulseIn(2, LOW,10000);		//timeout: 10ms
//  unsigned long rpm = (1000000 * 60) / (time * 4);
//  String stringRPM = String(rpm);
  
//  if (stringRPM.length() < 5) {
    Serial.print("Rpm: ");
    Serial.println(time);
    return time;
//  }
}
//===================================================                                                     
#endif
#ifdef ENABLE_DHT
//--------------------------------------------------
//--------------- DHT11 -----------------------------
//--------------------------------------------------
// Cmd: 0...turn on interval measurement; 1...turn off; 2..measure now; 0xFF...config                                                                                     
void RunDht11(byte Cmd) {
  dhtmod_s* pDhtMod = (dhtmod_s*)pActHead;
  
 #if defined(verbose)  
//      Serial.print("Enter rundht11: ");
//      Serial.println(Cmd);
 #endif
  
 if (Cmd == 0xff) {
	if (pDhtMod->Head.HasTimer) {
		pActTimer = &Timer[TimerWritePtr];
		pDhtMod->Head.TimerPtr = TimerWritePtr;
		if (!pDhtMod->Head.IsFollower) {
			CreateTimer(pActHead->Id,pDhtMod->MeasureInterval, pDhtMod->IntervalUnit,0);
		 }
	 }
     return;
  }

  if (pDhtMod->Head.HasTimer) {
	  pActTimer = &Timer[pDhtMod->Head.TimerPtr];
	  if (Cmd == 1) {         //off
		 pActTimer->Mode = 0;
		 return;
	  }
	  else if (Cmd == 0) {    //on
		 pActTimer->Mode = 2;
	  }
	  ReloadTimer(pActTimer);
  }
//  Serial.print("int init: ");
//  Serial.println(pActTimer->Interval);
  
  int chk = DHT11.read(pDhtMod->DhtPin);
#if defined(verbose)
  //  Serial.print("chk: ");
  //  Serial.println(chk);
  switch (chk)
  {
    case DHTLIB_ERROR_CHECKSUM: 
		Serial.println("DHT Checksum error");
		break;
    case DHTLIB_ERROR_TIMEOUT: 
		Serial.println("DHT Time out error");
		break;
    case DHTLIB_OK:
//		Serial.println("OK");
    	Serial.print("H(%): ");
		Serial.print((float)DHT11.humidity, 2);
		Serial.print(" T(C): ");
		Serial.println((float)DHT11.temperature, 2);
		break;
    default: 
		Serial.println("DHT Unknown error");
		break;
  }
#endif

  if (chk != DHTLIB_OK) {
	  if (pDhtMod->TempVal <= -100) {		//error count - prevent flooding
		  if (pDhtMod->TempVal == -115)	//max 16 error messages
			  return;
		  pDhtMod->TempVal--;
	  }
	  else
		  pDhtMod->TempVal = -100;
  	  chk = (chk * 32) + pDhtMod->TempVal;		//include error count
	  SendMsg(pActHead->Id,0xff,true,2,&chk,2);            //send error to master
  }
  else {
	 if (abs(DHT11.temperature - pDhtMod->TempVal) >= pDhtMod->TempHyst  || Cmd == 2 || pDhtMod->Head.HystOverride) {
		  pDhtMod->TempVal = DHT11.temperature;
		  SendMsg(pActHead->Id,0,false,2,&DHT11.temperature,pDhtMod->Head.HystOverride);
     }
     if (abs(DHT11.humidity - pDhtMod->HumiVal) >= pDhtMod->HumiHyst  || Cmd == 2 || pDhtMod->Head.HystOverride) {
    	 pDhtMod->HumiVal = DHT11.humidity;
    	 SendMsg(pActHead->Id,1,false,2,&DHT11.humidity,pDhtMod->Head.HystOverride);
     }
  }
}
//===================================================
#endif

#ifdef ENABLE_BUT
//--------------------------------------------------
//--------------- Buttons -------------------------
//--------------------------------------------------
// cmd: 0..normal operation; 1..off; 0xff..configuration
void RunButton(byte Cmd) {
  byte newstate;
  byte change;
  byte val;
  butmod_s* pButMod = (butmod_s*)pActHead;
  
  if (Cmd == 0xff) {                 
     if (pButMod->Port == 0) {
       DDRB = DDRB | (1 << pButMod->Pin);
       PORTB = PORTB | (1 << pButMod->Pin);
       }
     else if (pButMod->Port == 1) {
       DDRC = DDRC | (1 << pButMod->Pin);
       PORTC = PORTC | (1 << pButMod->Pin);
       }
     else {
       DDRD = DDRD | (1 << pButMod->Pin);
       PORTD = PORTD | (1 << pButMod->Pin);
     }

     if (pButMod->Head.HasTimer) {
    	pActTimer = &Timer[TimerWritePtr];
    	pButMod->Head.TimerPtr = TimerWritePtr;
		pActTimer = &Timer[TimerWritePtr];
		pButMod->Head.TimerPtr = TimerWritePtr;
		if (!pButMod->Head.IsFollower) {
			CreateTimer(pActHead->Id,pButMod->Debounce * 10,milli,0);
		}
     }
     

     if (pButMod->Port == 0)
       pButMod->PinVal = PINB & (1 << pButMod->Pin);
     else if (pButMod->Port == 1)
       pButMod->PinVal = PINC & (1 << pButMod->Pin);
     else 
       pButMod->PinVal = PIND & (1 << pButMod->Pin);
     return;
  }
  if (Cmd == 0) {                          
     pActTimer->Mode = 2;
  }
  if (Cmd == 1) {         
     pActTimer->Mode = 0;
     return;
  }
  
  timer_s* pTimer = &Timer[pButMod->Head.TimerPtr];
  ReloadTimer(pTimer);
    
   if (pButMod->Port == 0)
     newstate = PINB & (1 << pButMod->Pin);
   else if (pButMod->Port == 1)
     newstate = PINC & (1 << pButMod->Pin);
   else 
     newstate = PIND & (1 << pButMod->Pin);
     
   change = newstate ^ pButMod->PinVal;
   if (change == 0)                       
     return;
   for (byte j = 0; j < 8; j++) {
      if (change & (1 << j)) {
        if (newstate & (1 << j)) {
          val = pButMod->ActiveHigh;
                                 
            }
        else {
          val = pButMod->ActiveHigh;
                                 
            }
        SendMsg(pButMod->Head.Id, j,false, 1,&val,pButMod->Head.HystOverride);
      }
   }
   pButMod->PinVal = newstate >> pButMod->Pin;
}
//===================================================
#endif

#ifdef ENABLE_DIM
//--------------------------------------------------
//--------------- DIM -----------------------------
//--------------------------------------------------
// Cmd:0...timer call; 1..turn on normal; 2...off normal; 3..off manual (fast dim); 4..off immediatly;
//     5..dim up start normal; 6..dim up start restart; 7..dim up start up/down; 8..dim down start; 9..dim down start restart; 10..dim down start up/down; 11..dim operation stop; 0xFF...config
void RunDim(byte Cmd) {
  dimmod_s* pDimMod = (dimmod_s*)pActHead;
  byte dimmode = 0;

  #if defined(verbose)
      Serial.print("Enter rundim:");
      Serial.print(Cmd);
      Serial.print("state:");
      Serial.print(pDimMod->Head.State);
      Serial.print(" pval:");
      Serial.println(pDimMod->PVal);
  #endif

  if (Cmd == 0xff) {
	 if (pDimMod->PowerPin) {
		 pinMode(pDimMod->PowerPin, OUTPUT);
     	 digitalWrite(pDimMod->PowerPin, LOW);
  	  }
  	  if (pDimMod->PwmPin) {
  		  pinMode(pDimMod->PwmPin, OUTPUT);
  		  Timer1.initialize(pDimMod->PwmPeriod * 100);
  		Serial.print("pwmpin");
  		Serial.println(pDimMod->PwmPin);
  		Serial.print("pwmper");
  		Serial.println(pDimMod->PwmPeriod);
  	  }

     if (pDimMod->Head.HasTimer) {
    	 pDimMod->Head.TimerPtr = TimerWritePtr;
    	 if (!pDimMod->Head.IsFollower)
    		 CreateTimer(pActHead->Id,pDimMod->UpTime,tensec,0);
     }
     return;
  }

  if (pDimMod->Head.HasTimer) {
    	  pActTimer = &Timer[pDimMod->Head.TimerPtr];
  }
  else				//TODO: send error message?
	  return;		//dim module requires a timer!

  if (Cmd == 1) {
  	  dimmode = 0;
  	  if (pDimMod->Head.State == 3)		//already on full power?
  		  pDimMod->Head.State = 2;		//just reload timer
  	  else if (pDimMod->Head.State == 0 || pDimMod->Head.State == 4)
  		  pDimMod->Head.State = 0;
    }

  if (Cmd == 2) {					//turn off normal
    	  pDimMod->Head.State = 3;		//start dimming down
    	  dimmode = 0;
  }

  if (Cmd == 3) {					//turn off fast
      	  pDimMod->Head.State = 3;		//start dimming down fast
      	  dimmode = 2;
    }

  if (Cmd == 5) {
	  pDimMod->Head.State = 0;		//start dimming up
	  dimmode = 1;
  }

  if (Cmd == 8) {
  	  pDimMod->Head.State = 3;		//start dimming down
  	  dimmode = 1;
    }

  if (Cmd == 11) {					//stop dimmin mode
	  dimmode = 0;
 	  pDimMod->Head.State = 2;		//just reload timer
	  if (pDimMod->Mode == 1)
		  pDimMod->PowerHigh = pDimMod->PVal;	//store dim value for further use
  }
  /*
  Serial.print(" 1State: ");
   Serial.print(pDimMod->Head.State);
   Serial.print(" 1Pval: ");
   Serial.print(pDimMod->PVal);
*/

  switch(pDimMod->Head.State)
  {
	 case 0:
		 DimConfigRamp(false,dimmode,pDimMod);
		 pDimMod->Head.State = 1;		//dim up
		 if (pDimMod->PowerPin)
		      	 digitalWrite(pDimMod->PowerPin, HIGH);
		 Timer1.pwm(pDimMod->PwmPin,pDimMod->PVal, pDimMod->PwmPeriod * 100);
	 case 1:			//dim up
		 pDimMod->PVal++;
		 itmp = pgm_read_word_near(dimTable + pDimMod->PVal);
		 Timer1.setPwmDuty(pDimMod->PwmPin,itmp);
		 if (pDimMod->PVal < pDimMod->PowerHigh) {
			 ReloadTimer(pActTimer);
			 break;
		 }
	 case 2:            //Max power reached
		 pActTimer->Mode = 2;
		 pActTimer->Interval = pDimMod->OnTime;
		 pActTimer->Unit = pDimMod->OnTimeUnit;
		 pDimMod->Head.State = 3;
		 ReloadTimer(pActTimer);
/*
		 Serial.print(" TmrIn: ");
		   Serial.print(pActTimer->Interval);
		   Serial.print(" Next: ");
		   Serial.print(pActTimer->NextTimer);
		   Serial.print(" Over: ");
		   		 Serial.print(pActTimer->Wait4Over);
		   Serial.print(" TmrUni: ");
		   Serial.println(pActTimer->Unit);
*/
		 break;
	 case 3:            //Dim down
		 DimConfigRamp(true,dimmode,pDimMod);
		 pDimMod->Head.State = 4;
	 case 4:            //Dimming
		 if (pDimMod->PVal)
			 pDimMod->PVal--;
		 itmp = pgm_read_word_near(dimTable + pDimMod->PVal);
		 Timer1.setPwmDuty(pDimMod->PwmPin,itmp);
		 if (pDimMod->PVal > pDimMod->PowerLow) {
			 ReloadTimer(pActTimer);
//			 Serial.println("Reload");
			 break;
		 }
		 Timer1.stop();
		 if (pDimMod->PowerPin)
			 digitalWrite(pDimMod->PowerPin, LOW);

		 pActTimer->Mode = 0;
		 pDimMod->Head.State = 0;
		 break;
  }
  /*
  Serial.print(" 2State: ");
  Serial.print(pDimMod->Head.State);
  Serial.print(" 2Pval: ");
  Serial.println(pDimMod->PVal);
  */
}

void DimConfigRamp(bool Down,byte dimmode, struct dimmod_s* pDimMod) {

	pActTimer->Unit = milli;
	if (!dimmode) {
		itmp = pDimMod->PowerHigh - pDimMod->PowerLow;	//distance
		Serial.print(" Distance: ");
			  Serial.print(itmp);

		  if (Down)
			  ltmp = pDimMod->DownTime * 100;		//to milliseconds
		  else
			  ltmp = pDimMod->UpTime * 100;			//to milliseconds
		  ltmp = (ltmp / itmp);					//step time
	} else {
		if (dimmode == 2) {
			ltmp = pDimMod->OffDimTime;	  	       //to milliseconds
		}
		else
			ltmp = pDimMod->DimTime * 10;			//to 10 * millis
	}

  Serial.print(" Interval: ");
  Serial.print(ltmp);

	pActTimer->Mode = 2;
	pActTimer->Interval = ltmp;

}
//===================================================
#endif

#ifdef ENABLE_ANA
//--------------------------------------------------
//--------------- ANALOG -----------------------------
//--------------------------------------------------
// msg structure 3 bytes: 1 byte % val; 2 byte raw analog val
// cmd: 0..interval measure; 1..off; 2..measure now; 0xff..config
void RunAnalogIn(byte Cmd) {
  anamod_s* pAnaMod = (anamod_s*)pActHead;
  union u {
	  byte d[3];
	  struct s {
		  byte percent;
		  unsigned int t;
	  } parts;
  } data;

 #if defined(verbose)  
      Serial.print("Runanalogin: ");
      Serial.println(Cmd);
 #endif
  
  if (Cmd == 0xff) {                 
    pinMode(pAnaMod->AnaPin, INPUT); 

    if (pAnaMod->Head.HasTimer) {
    	pActTimer = &Timer[TimerWritePtr];
    	pAnaMod->Head.TimerPtr = TimerWritePtr;
    	if (!pAnaMod->Head.IsFollower) {
    		CreateTimer(pActHead->Id,pAnaMod->MeasureInterval,pAnaMod->IntervalUnit,0);
    	}
    }
    return;
  }
  if (pAnaMod->Head.HasTimer) {
	  pActTimer = &Timer[pAnaMod->Head.TimerPtr];

	  if (Cmd == 1) {
		 pActTimer->Mode = 0;
		 return;
	  }
	  else if (Cmd == 0) {
		 pActTimer->Mode = 2;
	  }
	  ReloadTimer(pActTimer);
  }
  
  data.parts.t = (int)analogRead(pAnaMod->AnaPin);
  /*
#if defined(verbose)
  Serial.print("Analog pin: ");
  Serial.print(pAnaMod->AnaPin);
  Serial.print(" Analog data: ");
  Serial.println(data.parts.t);
#endif
*/

  if (abs(data.parts.t - pAnaMod->Value) >= pAnaMod->Hyst || Cmd == 2 || pAnaMod->Head.HystOverride) {
       if (data.parts.t < 10)
    	   data.parts.percent = data.parts.t;
       else {
//    	 t = t * 10;
//    	 t = t / 102;
    	 float f = (100 / (1023 - (float)pAnaMod->Calibration * 4));	//calculate % value - was: (val * 100) / 1023
//    	 Serial.print("f: ");
//    	 Serial.println(f,2);
    	 data.parts.percent = f * data.parts.t;
         if (data.parts.percent < 10)
        	 data.parts.percent = 10;
         else if (data.parts.percent > 100)
        	 data.parts.percent = 100;
       }
#if defined(verbose)  
   Serial.print("Data%: ");
   Serial.print(data.parts.percent);
   Serial.print("Analog data: ");
   Serial.println(data.parts.t);
 #endif
       pAnaMod->Value = data.parts.t;
       pAnaMod->PVal = data.parts.percent;
       SendMsg(pActHead->Id,0,false,3,&data,pAnaMod->Head.HystOverride);
     }
}
//===================================================                                                     
#endif


//Loopbackmode: 0..normal (send + loopb); 1..only loopback; 2..only send
void SendMsg(byte ModuleId, byte Slot, boolean bMaster, byte Length, void* Value, byte LoopbackMode)
{
#if defined(verbose)
//	Serial.println("Enter Sendmsg:");
#endif

  /*
  Serial.print(" length: ");
    Serial.print(Length);
  Serial.print(" outwrite: ");
  Serial.println(msgoutwrite);
  */
  pActOutMsg->p.SrcNode = NodeId;
  pActOutMsg->p.SrcModule = ModuleId;
  pActOutMsg->p.SrcSlot = Slot;
  pActOutMsg->p.Length = Length + 4;
  if (Value != &pActOutMsg->p.aPay[0]) {				//msg payload already filled?
//	  Serial.println("copy");
	  for (byte j=0; j < Length; j++) {
		pActOutMsg->p.aPay[j] = ((byte *)Value)[j];
//		Serial.println(((byte *)Value)[j]);
	  }
  }

#if defined(verbose)
  /*
Serial.print(" id: ");
Serial.print(pActOutMsg->p.SrcNode);
Serial.print(" mod: ");
Serial.print(pActOutMsg->p.SrcModule);
Serial.print(" slot: ");
Serial.print(pActOutMsg->p.SrcSlot);
Serial.print(" Master: ");
Serial.print(" Len: ");
Serial.print(pActOutMsg->p.Length);
Serial.print(" pay: ");
Serial.print(pActOutMsg->p.aPay[0]);
Serial.print("-");
Serial.print(pActOutMsg->p.aPay[1]);
Serial.print("-");
Serial.print(pActOutMsg->p.aPay[2]);
Serial.print("-");
Serial.print(pActOutMsg->p.aPay[3]);
Serial.print("-");
Serial.print(pActOutMsg->p.aPay[4]);
Serial.print("-");
Serial.print(pActOutMsg->p.aPay[5]);
Serial.print("-");
Serial.print(pActOutMsg->p.aPay[6]);
Serial.print("-");
Serial.println(pActOutMsg->p.aPay[7]);
*/
#endif

  msgoutwrite+=(Length+4);
  if (msgoutwrite + MSGSIZE >= OUTMAX) {
	  msgoutwrite = 0;
	  pActOutMsg = (msg_s*)&MessageOut[msgoutwrite];
  }
  if (LoopbackMode != 2)
	  RecvMsg(true);		//loopback
  if (LoopbackMode == 1)
	  return;

  Serial.print('#');
  for (tmp = 0; tmp < pActOutMsg->p.Length; tmp++) {
	  Serial.print(pActOutMsg->data[tmp],HEX);
	  Serial.print("|");
  }
//	  Serial.write(pActOutMsg->data,pActOutMsg->p.Length);

}

void RecvMsg(bool loopback)
{
    if (loopback)
      pActInMsg = pActOutMsg;
#ifdef verbose
/*
	Serial.println("Enter Recv");
	   Serial.print("srcnode");
	   Serial.print(pActInMsg->p.SrcNode);
	  Serial.print(" srcmod: ");
	  Serial.print(pActInMsg->p.SrcModule);
	  Serial.print(" srcSlot: ");
	  Serial.println(pActInMsg->p.SrcSlot);
*/
#endif
    if (pActInMsg->p.SrcNode > 127) {		//master message
    	if (!(pActInMsg->p.SrcNode - 128 == NodeId || pActInMsg->p.SrcNode == 0xFF))	//check target address or broadcast
    		return;
    	if (pActInMsg->p.SrcNode == 0xFF)
    		delay(NodeId * 2);						//delay handling of broadcast messages to prevent bus overload

    	if (pActInMsg->p.SrcModule == 1) {			//Receive config
    		ChangeConfigData();
    	}
    	else if (pActInMsg->p.SrcModule == 2) {		//Send config
    		SendConfigData();
    	}
    	else if (pActInMsg->p.SrcModule == 3) {		//Run cmd
			RunModuleCmd();
		}
    	else if (pActInMsg->p.SrcModule == 4) {		//Time sync
			TimeSync();
		}
    	else if (pActInMsg->p.SrcModule == 5) {		//Save config
			SaveConfigEE();
		}
    	else if (pActInMsg->p.SrcModule == 6) {		//Load config
			LoadConfigEE();
		}
    	else if (pActInMsg->p.SrcModule == 7) {		//Send node cfg data - version TODO: add modules, etc
			Serial.println("TEST!");
			tmp = version;
			SendMsg(0,7,false,1,&tmp,2);
		}
    	else if (pActInMsg->p.SrcModule == 8) {		//Run cfg cmd
    		RunCfgCmd();
		}
    } else {                                         
       for (byte j = 0; j < CountLink; j++) {
/*
    	   Serial.print("node");
    	   Serial.print(Link[j].SrcNode);
    	  Serial.print(" mod: ");
    	  Serial.print(Link[j].SrcModule);
    	  Serial.print(" Slot: ");
    	  Serial.println(Link[j].SrcSlot);
*/
          if (Link[j].SrcNode == pActInMsg->p.SrcNode && Link[j].SrcModule == pActInMsg->p.SrcModule && Link[j].SrcSlot == pActInMsg->p.SrcSlot) {
             pActLink = &Link[j];
             if (Link[j].Mode < 2) {
            	 LoadRegister(Link[j].Reg,Link[j].Pos,Link[j].Len,(byte*)&pActInMsg->p.aPay[0]);
             }
             if (Link[j].Mode > 0) {
            	 Evaluate(Link[j].ConditionPtr);
             }
          }
       }    
    }
}

void Evaluate(byte Cond) {
	byte result = 0;
	bool NumericOp;

	switch(Condition[Cond].Op) {
		case op_equal:
				result = (Module[Register[Condition[Cond].Left]] == Module[Register[Condition[Cond].Right]]);
			break;
		case op_inequal:
			result = (Module[Register[Condition[Cond].Left]] != Module[Register[Condition[Cond].Right]]);
			break;
		case op_greater:
			result = (Module[Register[Condition[Cond].Left]] > Module[Register[Condition[Cond].Right]]);
			break;
		case op_greatequal:
			result = (Module[Register[Condition[Cond].Left]] >= Module[Register[Condition[Cond].Right]]);
			break;
		case op_less:
			result = (Module[Register[Condition[Cond].Left]] < Module[Register[Condition[Cond].Right]]);
			break;
		case op_lessequal:
			result = (Module[Register[Condition[Cond].Left]] <= Module[Register[Condition[Cond].Right]]);
			break;
		case op_not:
			result = !(Module[Register[Condition[Cond].Left]]);
			break;
		case op_and:
			result = (Module[Register[Condition[Cond].Left]] && Module[Register[Condition[Cond].Right]]);
			break;
		case op_or:
			result = (Module[Register[Condition[Cond].Left]] || Module[Register[Condition[Cond].Right]]);
			break;
		case op_add:
			result = (Module[Register[Condition[Cond].Left]] + Module[Register[Condition[Cond].Right]]);
			NumericOp = true;
			break;
		case op_sub:
			result = (Module[Register[Condition[Cond].Left]] - Module[Register[Condition[Cond].Right]]);
			NumericOp = true;
			break;
		case op_mult:
			result = (Module[Register[Condition[Cond].Left]] * Module[Register[Condition[Cond].Right]]);
			NumericOp = true;
			break;
		case op_div:
			result = (Module[Register[Condition[Cond].Left]] / Module[Register[Condition[Cond].Right]]);
			NumericOp = true;
			break;
		case op_mod:
			result = (Module[Register[Condition[Cond].Left]] % Module[Register[Condition[Cond].Right]]);
			NumericOp = true;
			break;
		case op_bitand:
			result = (Module[Register[Condition[Cond].Left]] & Module[Register[Condition[Cond].Right]]);
			NumericOp = true;
			break;
		case op_bitor:
			result = (Module[Register[Condition[Cond].Left]] | Module[Register[Condition[Cond].Right]]);
			NumericOp = true;
			break;
	}
#if defined(verbose)
Serial.print("con:");
Serial.print(Cond);
Serial.print(" result:");
Serial.print(result);
Serial.print(" left: ");
Serial.print(Module[Register[Condition[Cond].Left]]);
Serial.print(" right: ");
Serial.println(Module[Register[Condition[Cond].Right]]);
#endif
	if (Condition[Cond].Invert && !NumericOp)
			result = !result;

	if (Condition[Cond].Mode == 7)
		Register[Condition[Cond].TrgModuleId] = result;

	//0..just evaluate; 1..trigger cmd on true; 2..on true eval CondPtr; 3..trigger cmd on true, on false eval. ConditionPtr; 4..on true eval ConditionPtr, on false eval TrgModule (Condition); 5..on true trigger and eval; 6..on true trigger trgmod, on false trigger mod conditionpayload
	if (result) {
		if (Condition[Cond].Mode == 1 || Condition[Cond].Mode == 3 || Condition[Cond].Mode == 5 || Condition[Cond].Mode == 6) {
			RunModule(Condition[Cond].TrgModuleId, Condition[Cond].TrgCmd, false);
		}
		if (Condition[Cond].Mode == 2 || Condition[Cond].Mode == 4 || Condition[Cond].Mode == 5) {
			Evaluate(Condition[Cond].ConditionPtr);
		}
	}
	else {
		if (Condition[Cond].Mode == 3) {
			Evaluate(Condition[Cond].ConditionPtr);
		}
		else if (Condition[Cond].Mode == 4) {
			Evaluate(Condition[Cond].TrgModuleId);
		}
		else if (Condition[Cond].Mode == 6) {
			RunModule(Condition[Cond].ConditionPtr, Condition[Cond].ConditionPayload, false);
		}
	}
}

void LoadRegister(byte Reg, byte Pos, byte Len, byte* Val) {
	for (byte j = 0; j < Len; j++)
		Register[Reg] = Val[Pos];
#if defined(verbose)
	Serial.print("regload: ");
	Serial.println(Val[0]);
#endif
}

void ReloadTimer(struct timer_s* pTimer) {
	unsigned int newval;

	if (pTimer->Unit == milli) {
		itmp = ms;
		newval = itmp + pTimer->Interval;
	}
	else if (pTimer->Unit == tensec) {
		itmp = tsec;
		newval = itmp + pTimer->Interval;
	}
	else {
		itmp = hour;
		newval = itmp + pTimer->Interval;
	}
	if (newval < itmp)
		pTimer->Wait4Over = true;

	pTimer->NextTimer = newval;
	/*
	Serial.print("timerval");
	Serial.print(newval);
	Serial.print("waitover");
	Serial.println(pTimer->Wait4Over);
	*/
}
 
byte GetModuleSize(byte Type) {
	switch(Type) {
#ifdef ENABLE_FAN
	case FANTYPE:
		return FANSIZE;
	  	break;
#endif
#ifdef ENABLE_DHT
	case DHTTYPE:
	    return DHTSIZE;
	    break;
#endif
#ifdef ENABLE_ANA
	case ANATYPE:
	    return ANASIZE;
	    break;
#endif
#ifdef ENABLE_BUT
	case BUTTYPE:
	    return BUTSIZE;
	    break;
#endif
#ifdef ENABLE_DIM
	case DIMTYPE:
	    return DIMSIZE;
	    break;
#endif
	default:
		break;
	}
	 return 0;					//bad! so bad!
}

unsigned int CheckSum(byte ptr[], byte len){
  int XOR = 0;
  for(int i=0; i<(len); i++) {
    XOR = XOR ^ ptr[i];
  }
  return XOR;
}

void TimeSync() {
	Module[Register[0]] = pActInMsg->p.aPay[0];		//sec
	Module[Register[1]] = pActInMsg->p.aPay[1];					//min
	Module[Register[2]] = pActInMsg->p.aPay[2];					//hrs
}

//--------------------------------------------------
//--------------- Save Config Eeprom -----------------------------
//---------------- cmd: 5 ----------------------------
//write config data to eeprom
//structure:   lmmmmmmmlddddddddlrrrrrrrrrlkkkkkkkkkk
//l: length m: module bytes; d dirct registers
//
// cmd: 58 4    0   5   0
//		:  len src mod slot

void SaveConfigEE() {
	unsigned int pos = 0;
	byte* ptr;
	byte j = 0;

	Serial.println("SaveConfigEE");
	itmp = ModuleWritePtr + RegCount +  CountLink * sizeof(link_s) + CountCond * sizeof(cond_s);
	if (itmp > DATAPARTITIONSIZE) {
		itmp = 1025;
		SendMsg(0,5,1,2,&itmp,2);					//ERROR: not enough space on eeprom for config - send 1025
		return;
	}

	EEPROM.write(pos++,ModuleWritePtr);			//module
	for (j=0; j < ModuleWritePtr; j++) {
//		Serial.println(Module[j]);
		EEPROM.write(pos++,Module[j]);
	}
	EEPROM.write(pos++,DirectRegCount);			//direct register
	for (j=0; j < DirectRegCount; j++) {
		EEPROM.write(pos++,Module[MODULESMAX - 1 - j]);
	}
	EEPROM.write(pos++,RegCount);				//module register
	for (j=0; j < RegCount; j++) {
		EEPROM.write(pos++,Register[j]);
	}

	EEPROM.write(pos++,CountLink);				//link
	ptr = (byte*)&Link[0];
	for (j=0; j < CountLink * sizeof(link_s); j++) {
		EEPROM.write(pos++,ptr[j]);
//		Serial.println(ptr[j]);
	}

	EEPROM.write(pos++,CountCond);				//condition
	ptr = (byte*)&Condition[0];
	for (j=0; j < CountCond * sizeof(cond_s); j++) {
		EEPROM.write(pos++,ptr[j]);
//		Serial.println(ptr[j]);
	}
	SendMsg(0,5,true,2,&pos,2);				//send bytes saved
}

//--------------------------------------------------
//--------------- Load Config from Eeprom -----------------------------
//---------------- cmd: 6 ----------------------------
//read config data from eeprom
//configure and start modules
// cmd: 58 4    0   6   0
//		:  len src mod slot
void LoadConfigEE() {
	byte j = 0;
	unsigned int pos = 0;
	byte end = 0;
	byte* ptr;

	Serial.println("LoadConfigEE1");

	if (EEPROM.read(CFG_READEEPROM) != 1)			//read cfg from eeprom flag set?
		return;

	Serial.println("LoadConfigEE2");

	end = EEPROM.read(pos++);			//module
	Serial.println(end);
	for (ModuleWritePtr=0; ModuleWritePtr < end; ModuleWritePtr++) {
		Module[ModuleWritePtr] = EEPROM.read(pos++);
		Serial.print(ModuleWritePtr);
		Serial.print("--");
		Serial.println(Module[ModuleWritePtr]);
	}
	Serial.print("mods: ");
	Serial.println(pos);
	end = EEPROM.read(pos++);			//direct register
	for (DirectRegCount=0; DirectRegCount < end; DirectRegCount++) {
		Module[MODULESMAX - 1 - DirectRegCount] = EEPROM.read(pos++);
	}
	Serial.print("direct: ");
	Serial.println(pos);
	end = EEPROM.read(pos++);			//module register
	for (RegCount=0; RegCount < end; RegCount++) {
		Register[RegCount] = EEPROM.read(pos++);
	}
	Serial.print("regs: ");
	Serial.println(pos);

	CountLink = EEPROM.read(pos++);				//link
	ptr = (byte*)&Link[0];
	for (j=0; j < CountLink * sizeof(link_s); j++) {
		ptr[j] = EEPROM.read(pos++);
		Serial.println(ptr[j]);
	}
	Serial.print("link: ");
	Serial.println(pos);

	CountCond = EEPROM.read(pos++);				//condition
	ptr = (byte*)&Condition[0];
	for (j=0; j < CountCond * sizeof(cond_s); j++) {
		ptr[j] = EEPROM.read(pos++);
		Serial.println(ptr[j]);
	}
	Serial.print("cond: ");
	Serial.println(pos);

	j = 0;
	while (j + 1 < ModuleWritePtr) {			//configure modules
		Serial.print("mod cfg: ");
		Serial.println(j);
		pActHead = (modhead_s*)&Module[j];
		Serial.print("id: ");
		Serial.println(pActHead->Id);
		ModId[pActHead->Id] = j;
		j += GetModuleSize(pActHead->Type);
		RunModule(pActHead->Id,0xff,false);
		CountModules++;
	}
	j = 0;
	while (j + 1 < ModuleWritePtr) {			//run modules
		Serial.print("mod run: ");
		Serial.println(j);
		pActHead = (modhead_s*)&Module[j];
		j += GetModuleSize(pActHead->Type);
		if (pActHead->Startup)
			RunModule(pActHead->Id,0,false);
	}
	if (LogLevel > 1)
		SendMsg(0,6,true,2,&pos,2);				//send bytes read
}



