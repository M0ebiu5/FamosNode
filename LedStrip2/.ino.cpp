//This is a automatic generated file
//Please do not modify this file
//If you touch this file your change will be overwritten during the next build
//This file has been generated on 2015-02-20 00:05:24

#include "Arduino.h"
#include "enums.h"
#include <dht11.h>
#include <eeprom.h>
void setup() ;
void loop() ;
boolean ReceiveSerial() ;
void CheckModuleTimers() ;
byte NewModule(byte Id, byte Type) ;
void RunModule(byte ModuleId, byte ModuleCmd, bool bTimerCall) ;
void CreateTimer(byte ModuleId, int Interval, TimeUnit tu, byte Mode) ;
void SyncTime() ;
void ReceiveConfigData() ;
void SendConfigData() ;
byte* GetDataPtr(byte Type, byte pos, byte* wptr) ;
void RunModuleCmd() ;
void RunFan(byte Cmd) ;
char getRPMS() ;
void RunDht11(byte Cmd) ;
void RunAnalogIn(byte Cmd) ;
void SendMsg(byte ModuleId, byte Slot, boolean bMaster, byte Length, void* Value, boolean LoopbackOnly) ;
void RecvMsg(bool loopback) ;
void Evaluate(byte Cond) ;
void LoadRegister(byte Reg, byte Pos, byte Len, byte* Val) ;
void ReloadTimer(struct timer_s* pTimer) ;
byte GetModuleSize(byte Type) ;
unsigned int CheckSum(byte ptr[], byte len);
void TimeSync() ;
void SaveConfigEE() ;
void LoadConfigEE() ;


#include "LedStrip2.ino"