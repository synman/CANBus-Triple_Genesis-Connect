
/*
// Serial Commands

System Info and EEPROM
----------------------
0x01 0x01        Print System Debug to Serial
0x01 0x02        Dump eeprom value
0x01 0x03        read and save eeprom
0x01 0x04        restore eeprom to stock values
0x01 0x10 0x01   Print Bus 1 Debug to Serial
0x01 0x10 0x02   Print Bus 1 Debug to Serial
0x01 0x10 0x03   Print Bus 1 Debug to Serial
0x01 0x16        Reboot to bootloader


Send CAN Packet
---------------
Cmd    Bus id  PID    data 0-7                  length
0x02   01      290    00 00 00 00 00 00 00 00   8


Set logging output (Filters are optional)
--------------------------------------------------
Cmd  Bus  On/Off Message ID 1   Message ID 2
0x03 0x01 0x01   0x290          0x291   // Set logging on Bus 1 to ON
0x03 0x01 0x00                          // Set logging on Bus 1 to OFF


Set Bluetooth Message ID filter
----------------------------------------
Cmd  Bus  Message ID 1 Message ID 2
0x04 0x01 0x290        0x291          // Enable Message ID 290 output over BT
0x04 0x01 0x0000       0x0000         // Disable


Bluetooth Functions
-------------------
Cmd  Function
0x08  0x01 Reset BLE112
0x08  0x02 Enter pass through mode to talk to BLE112
0x08  0x03 Exit pass through mode 
TODO: Implenent this ^^^
*/


// #define JSON_OUT

#define COMMAND_OK 0xFF
#define COMMAND_ERROR 0x80
#define NEWLINE "\r"

#include "Middleware.h"


class SerialCommand : Middleware
{
  public:
    // static unsigned int logOutputFilter;
    static CANBus busses[];
    static Stream* activeSerial;
    static void init( QueueArray<Message> *q, CANBus b[] );
    static void tick();
    static Message process( Message msg );
    static void printMessageToSerial(Message msg);
    static void resetToBootloader();
  private:
    static int freeRam();
    static QueueArray<Message>* mainQueue;
    static void printChannelDebug();
    static void printChannelDebug(CANBus);
    static void processCommand(int command);
    static int  getCommandBody( byte* cmd, int length );
    static void clearBuffer();
    static void getAndSend();
    static void printSystemDebug();
    static void settingsCall();
    static void dumpEeprom();
    static void getAndSaveEeprom();
    static void logCommand();
    static void bluetooth();
    static void setBluetoothFilter();
    static char btMessageIdFilters[][2];
    static boolean passthroughMode;
    static byte busLogEnabled;
    static Message newMessage;
    static byte buffer[];
    
};



// Defaults
QueueArray<Message> *SerialCommand::mainQueue;
byte SerialCommand::busLogEnabled = 0;               // Start with all busses logging disabled
boolean SerialCommand::passthroughMode = false;
Stream* SerialCommand::activeSerial = &Serial;

char SerialCommand::btMessageIdFilters[][2] = {
                    {0x28F,0x290},
                    {0x0,0x0},
                    {0x28F,0x290},
                    };


void SerialCommand::init( QueueArray<Message> *q, CANBus b[] )
{
  Serial.begin( 115200 );
  Serial1.begin( 57600 );
  
  // TODO use passed in busses!
  // busses = { b[0], b[1], b[2] };
  
  mainQueue = q;
}

void SerialCommand::tick()
{
  
  // Pass-through mode for bluetooth DFU mode
  if( passthroughMode ){
    while(Serial.available()) Serial1.write(Serial.read());
    while(Serial1.available()) Serial.write(Serial1.read());
    return;
  }
  
  if( Serial1.available() > 0 ){
    activeSerial = &Serial1;
    processCommand( Serial1.read() );
  }
  
  if( Serial.available() > 0 ){
    activeSerial = &Serial;
    processCommand( Serial.read() );
  }
  
}

Message SerialCommand::process( Message msg )
{
  printMessageToSerial(msg);
  return msg;
}


void SerialCommand::printMessageToSerial( Message msg )
{
  
  // Bus Filter
  byte flag;
  flag = 0x1 << (msg.busId-1);
  if( !(SerialCommand::busLogEnabled & flag) ){
    return;
  }
  
  
  
  #ifdef JSON_OUT
    // Output to serial as json string
    activeSerial->print(F("{\"packet\": {\"status\":\""));
    activeSerial->print( msg.busStatus,HEX);
    activeSerial->print(F("\",\"channel\":\""));
    activeSerial->print( busses[msg.busId-1].name );
    activeSerial->print(F("\",\"length\":\""));
    activeSerial->print(msg.length,HEX);
    activeSerial->print(F("\",\"id\":\""));
    activeSerial->print(msg.frame_id,HEX);
    activeSerial->print(F("\",\"timestamp\":\""));
    activeSerial->print(millis(),DEC);
    activeSerial->print(F("\",\"payload\":[\""));
    for (int i=0; i<8; i++) {
      activeSerial->print(msg.frame_data[i],HEX);
      if( i<7 ) activeSerial->print(F("\",\""));
    }
    activeSerial->print(F("\"]}}"));
    activeSerial->println();
    
  #else
    
    // Bluetooth filter
    // TODO TEST
    if( activeSerial == &Serial1 &&
        btMessageIdFilters[msg.busId][0] != msg.frame_id &&
        btMessageIdFilters[msg.busId][1] != msg.frame_id
        ) return;
    
    activeSerial->write( 0x03 ); // Prefix with logging command
    activeSerial->write( msg.busId );
    activeSerial->write( msg.frame_id >> 8 );
    activeSerial->write( msg.frame_id );
    
    for (int i=0; i<8; i++) 
      activeSerial->write(msg.frame_data[i]);
    
    activeSerial->write( msg.length );
    activeSerial->write( msg.busStatus );
    activeSerial->write( NEWLINE );
    
  #endif
  
}




void SerialCommand::processCommand(int command)
{
  
  delay(20);
  
  switch( command ){
    case 0x01:
      settingsCall();
    break;
    case 0x02:
      getAndSend();
    break;
    case 0x03:
      logCommand();
    break;
    case 0x04:
      setBluetoothFilter();
    break;
    case 0x08:
      bluetooth();
    break;
  }
  
  SerialCommand::clearBuffer();
}


void SerialCommand::settingsCall()
{
  
  byte cmd[1];
  int bytesRead = getCommandBody( cmd, 1 );
  
  // Debug Command
  switch( cmd[0] ){
    case 0x01:
      printSystemDebug();
    break;
    case 0x02:
      dumpEeprom();
    break;
    case 0x03:
      // TODO Read from input and store settings
      getAndSaveEeprom();
    break;
    case 0x04:
      Settings::firstbootSetup();
    break;
    case 0x10:
        printChannelDebug();
    break;
    case 0x16:
      resetToBootloader();
    break;
  }
  
  
}



void SerialCommand::setBluetoothFilter(){

  byte cmd[5];
  int bytesRead = getCommandBody( cmd, 5 );
  
  if( cmd[0] >= 0 && cmd[0] <= 3 ){
    SerialCommand::btMessageIdFilters[cmd[0]][0] = (cmd[1] << 8)+cmd[2];
    SerialCommand::btMessageIdFilters[cmd[0]][1] = (cmd[3] << 8)+cmd[4];
  }
  
}



void SerialCommand::logCommand()
{
  byte cmd[6] = {0};
  int bytesRead = getCommandBody( cmd, 6 );
  
  if( cmd[0] < 1 || cmd[0] > 3 ){
    activeSerial->write(COMMAND_ERROR);
    return;
  }
  CANBus bus = busses[ cmd[0]-1 ];
  
  if( cmd[1] )
    SerialCommand::busLogEnabled |= cmd[1] << (cmd[0]-1);
    else
    SerialCommand::busLogEnabled &= cmd[1] << (cmd[0]-1);
  
  // Set filter if we got pids in the command
  if( bytesRead > 2 ){
    
    bus.setMode(CONFIGURATION);
    bus.clearFilters();
    if( cmd[2] + cmd[3] + cmd[4] + cmd[5] ) 
      bus.setFilter( (cmd[2]<<8) + cmd[3], (cmd[4]<<8) + cmd[5] );
    bus.setMode(NORMAL);
    
  }
  
  activeSerial->write(COMMAND_OK);
  activeSerial->write(NEWLINE);
  
}


void SerialCommand::getAndSaveEeprom()
{
  
  #define CHUNK_SIZE 32
  
  byte* settings = (byte *) &cbt_settings;
  byte cmd[CHUNK_SIZE+1];
  int bytesRead = getCommandBody( cmd, CHUNK_SIZE+2 );
  
  if( bytesRead == CHUNK_SIZE+2 && cmd[CHUNK_SIZE+1] == 0xA1 ){
      
    memcpy( settings+(cmd[0]*CHUNK_SIZE), &cmd[1], CHUNK_SIZE );
    
    activeSerial->print( F("{\"event\":\"eepromData\", \"result\":\"success\", \"chunk\":\"") );
    activeSerial->print(cmd[0]);
    activeSerial->println(F("\"}"));
    
    if( cmd[0]+1 == 512/CHUNK_SIZE ){ // At last chunk
      Settings::save(&cbt_settings);
      activeSerial->println(F("{\"event\":\"eepromSave\", \"result\":\"success\"}"));
    }
    
  }else{
    activeSerial->print( F("{\"event\":\"eepromData\", \"result\":\"failure\", \"chunk\":\"") );
    activeSerial->print(cmd[0]);
    activeSerial->println(F("\"}"));
  }
  
  
}


void SerialCommand::getAndSend()
{
  
  byte cmd[12];
  int bytesRead = getCommandBody( cmd, 12 );
  
  Message msg;
  msg.busId = cmd[0];
  msg.frame_id = (cmd[1]<<8) + cmd[2];
  msg.frame_data[0] = cmd[3];
  msg.frame_data[1] = cmd[4];
  msg.frame_data[2] = cmd[5];
  msg.frame_data[3] = cmd[6];
  msg.frame_data[4] = cmd[7];
  msg.frame_data[5] = cmd[8];
  msg.frame_data[6] = cmd[9];
  msg.frame_data[7] = cmd[10];
  msg.length = cmd[11];
  msg.dispatch = true;
  
  mainQueue->push( msg );
  
}



void SerialCommand::bluetooth(){

  byte cmd[1];
  int bytesRead = getCommandBody( cmd, 1 );
  
  switch( cmd[0] ){
    case 1:
      digitalWrite(BT_RESET, HIGH);
      delay(100);
      digitalWrite(BT_RESET, LOW);
    break;
    case 2:
      passthroughMode = true;
      digitalWrite(BOOT_LED, HIGH);
      delay(100);
      digitalWrite(BOOT_LED, LOW);
      delay(100);
      digitalWrite(BOOT_LED, HIGH);
    break;
    case 3:
      passthroughMode = false;
    break;
  }
  
}





int SerialCommand::getCommandBody( byte* cmd, int length )
{
  unsigned int i = 0;
  
  // Loop until requested amount of bytes are sent. Needed for BT latency
  //while( activeSerial->available() && i < length ){
  while( i < length && activeSerial->available() ){
    cmd[i] = activeSerial->read();
    //if(cmd[i] != 0xFF) i++;
    i++;
  }
  
  return i;
}

void SerialCommand::clearBuffer()
{
  while(activeSerial->available())
    activeSerial->read();
}


void SerialCommand::printSystemDebug()
{
  activeSerial->print(F("{\"event\":\"version\", \"name\":\"CANBus Triple Mazda\", \"version\":\"0.2.5\", \"memory\":\""));
  activeSerial->print(freeRam());
  activeSerial->println(F("\"}"));  
}

void SerialCommand::printChannelDebug(){
  
  byte cmd[1];
  getCommandBody( cmd, 1 );
  
  if( cmd[0] > -1 && cmd[0] <= 3 )
    printChannelDebug( busses[cmd[0]-1] );
  
  
}

void SerialCommand::printChannelDebug(CANBus channel){
  
  activeSerial->print( F("{\"e\":\"busdgb\", \"name\":\"") );
  activeSerial->print( channel.name );
  activeSerial->print( F("\", \"canctrl\":\""));
  activeSerial->print( channel.readRegister(CANCTRL), HEX );
  activeSerial->print( F("\", \"status\":\""));
  activeSerial->print( channel.readStatus(), HEX );
  activeSerial->print( F("\", \"error\":\""));
  activeSerial->print( channel.readRegister(EFLG), HEX );
  activeSerial->print( F("\", \"nextTxBuffer\":\""));
  activeSerial->print( channel.getNextTxBuffer(), DEC );
  activeSerial->println(F("\"}"));
  
}


void SerialCommand::resetToBootloader()
{
  cli();
  UDCON = 1;
  USBCON = (1<<FRZCLK);  // disable USB
  UCSR1B = 0;
  delay(5);
  EIMSK = 0; PCICR = 0; SPCR = 0; ACSR = 0; EECR = 0; ADCSRA = 0;
  TIMSK0 = 0; TIMSK1 = 0; TIMSK3 = 0;
  asm volatile("jmp 0x7000");
}


void SerialCommand::dumpEeprom()
{
  // dump eeprom
  for(int i=0; i<512; i++){
    activeSerial->print( EEPROM.read(i), HEX );
    if(i<511) activeSerial->print( ":" );
  }
  
}

int SerialCommand::freeRam (){
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}



