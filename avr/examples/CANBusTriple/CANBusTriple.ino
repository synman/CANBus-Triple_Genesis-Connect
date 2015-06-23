/*
*  CANBus Triple
*  The Car Hacking Platform
*  https://canb.us
*  https://github.com/CANBus-Triple
*
* 2015 Hyundai Genesis Coupe customizations
* 6/22/2015 - Shell M. Shrader
*/

#include <avr/wdt.h>
#include <SPI.h>
#include <EEPROM.h>
#include <CANBus.h>
#include <Message.h>
#include <QueueArray.h>

#define BUILDNAME "CANBus Triple"
#ifdef HAS_AUTOMATIC_VERSIONING
    #include "_Version.h"
#else
    #define BUILD_VERSION "0.4.5"
#endif
// #define SLEEP_ENABLE

CANBus CANBus1(CAN1SELECT, CAN1RESET, 1, "Bus 1");
CANBus CANBus2(CAN2SELECT, CAN2RESET, 2, "Bus 2");
CANBus CANBus3(CAN3SELECT, CAN3RESET, 3, "Bus 3");
CANBus busses[] = { CANBus1, CANBus2, CANBus3 };

#include "Settings.h"
#include "AutoBaud.h"
#include "GenesisConnect.h"
#include "SerialCommand.h"

byte rx_status;
QueueArray<Message> readQueue;
QueueArray<Message> writeQueue;

int GenesisConnect::powerOn;
unsigned long GenesisConnect::lastDisp;
int GenesisConnect::volume;
int GenesisConnect::mute;
unsigned long GenesisConnect::lastMute;
int GenesisConnect::thermostat;
int GenesisConnect::minute;
int GenesisConnect::outsideTemp;
int GenesisConnect::bluetooth;
int GenesisConnect::vents;
int GenesisConnect::airflow;
int GenesisConnect::compressor;
int GenesisConnect::audioSource;
/*
*  Middleware Setup
*  http://docs.canb.us/firmware/main.html
*/
SerialCommand *serialCommand = new SerialCommand( &writeQueue );
GenesisConnect *genesisConnect = new GenesisConnect(&writeQueue);

Middleware *activeMiddleware[] = {
  serialCommand,
  genesisConnect
};
int activeMiddlewareLength = (int)( sizeof(activeMiddleware) / sizeof(activeMiddleware[0]) );

void setup(){

  Settings::init();
  delay(1);

  /*
  *  Middleware Settings
  */

  Serial.begin( 115200 ); // USB
  Serial1.begin( 57600 ); // UART

  /*
  *  Power LED
  */
  DDRE |= B00000100;
  PORTE |= B00000100;

  /*
  *  BLE112 Init
  */
  pinMode( BT_SLEEP, OUTPUT );
  digitalWrite( BT_SLEEP, HIGH ); // Keep BLE112 Awake


  /*
  *  Boot LED
  */
  pinMode( BOOT_LED, OUTPUT );


  pinMode( CAN1INT_D, INPUT );
  pinMode( CAN2INT_D, INPUT );
  pinMode( CAN3INT_D, INPUT );
  pinMode( CAN1RESET, OUTPUT );
  pinMode( CAN2RESET, OUTPUT );
  pinMode( CAN3RESET, OUTPUT );
  pinMode( CAN1SELECT, OUTPUT );
  pinMode( CAN2SELECT, OUTPUT );
  pinMode( CAN3SELECT, OUTPUT );

  digitalWrite(CAN1RESET, LOW);
  digitalWrite(CAN2RESET, LOW);
  digitalWrite(CAN3RESET, LOW);


  // Setup CAN Busses
  CANBus1.begin();
  CANBus1.setClkPre(1);
  CANBus1.baudConfig(100);
  CANBus1.setRxInt(true);
  CANBus1.bitModify(RXB0CTRL, 0x04, 0x04); // Set buffer rollover enabled
  CANBus1.bitModify(CNF2, 0x20, 0x20); // Enable wake-up filter
  CANBus1.clearFilters();
  CANBus1.setMode(NORMAL);
  // attachInterrupt(CAN1INT, handleInterrupt1, LOW);

  CANBus2.begin();
  CANBus2.setClkPre(1);
  CANBus2.baudConfig(cbt_settings.busCfg[1].baud);
  CANBus2.setRxInt(true);
  CANBus2.bitModify(RXB0CTRL, 0x04, 0x04);
  CANBus2.clearFilters();
  CANBus2.setMode(NORMAL);
  // attachInterrupt(CAN2INT, handleInterrupt2, LOW);

  CANBus3.begin();
  CANBus3.setClkPre(1);
  CANBus3.baudConfig(cbt_settings.busCfg[2].baud);
  CANBus3.setRxInt(true);
  CANBus3.bitModify(RXB0CTRL, 0x04, 0x04);
  CANBus3.clearFilters();
  CANBus3.setMode(NORMAL);
  // attachInterrupt(CAN3INT, handleInterrupt3, LOW);

  for (int b = 0; b<5; b++) {
    digitalWrite( BOOT_LED, HIGH );
    delay(50);
    digitalWrite( BOOT_LED, LOW );
    delay(50);
  }

}

/*
*  Interrupt Handlers
*  Currently unused - loop() method will poll for logic low before a read
*/
void handleInterrupt1(){
}
void handleInterrupt2(){
}
void handleInterrupt3(){
}


/*
*  Main Loop
*/
void loop() {

  // Run all middleware ticks
  for(int i=0; i<=activeMiddlewareLength-1; i++)
    activeMiddleware[i]->tick();


  if( digitalRead(CAN1INT_D) == 0 ) readBus(CANBus1);
  if( digitalRead(CAN2INT_D) == 0 ) readBus(CANBus2);
  if( digitalRead(CAN3INT_D) == 0 ) readBus(CANBus3);


  // Process message stack
  if( !readQueue.isEmpty() && !writeQueue.isFull() ){
    processMessage( readQueue.pop() );
  }


  boolean success = true;
  while( !writeQueue.isEmpty() && success ){

    Message msg = writeQueue.pop();
    CANBus channel = busses[msg.busId-1];

    success = sendMessage( msg, channel );

    if( !success ){
      // TX Failure, add back to queue
      writeQueue.push(msg);
    }

  }

} // End loop()


/*
*  Load CAN Controller buffer and set send flag
*/
boolean sendMessage( Message msg, CANBus bus ){

  if( msg.dispatch == false ) return true;

  digitalWrite( BOOT_LED, HIGH );

  int ch = bus.getNextTxBuffer();

  switch( ch ){
    case 0:
      bus.load_ff_0( msg.length, msg.frame_id, msg.frame_data );
      bus.send_0();
      break;
    case 1:
      bus.load_ff_1( msg.length, msg.frame_id, msg.frame_data );
      bus.send_1();
      break;
    case 2:
      bus.load_ff_2( msg.length, msg.frame_id, msg.frame_data );
      bus.send_2();
      break;
    default:
      // All TX buffers full
      return false;
      break;
  }

  digitalWrite( BOOT_LED, LOW );

  return true;

}


/*
*  Read Can Controller Buffer
*/
void readBus( CANBus bus ){

  // Abort if readQueue is full
  if( readQueue.isFull() ) return;

  rx_status = bus.readStatus();

  // Check buffer RX0
  if( (rx_status & 0x1) == 0x1 ){
    Message msg;
    msg.busStatus = rx_status;
    msg.busId = bus.busId;
    bus.readDATA_ff_0( &msg.length, msg.frame_data, &msg.frame_id );
    readQueue.push(msg);
  }

  // Abort if readQueue is full
  if( readQueue.isFull() ) return;

  // Check buffer RX1
  if( (rx_status & 0x2) == 0x2 ) {
    Message msg;
    msg.busStatus = rx_status;
    msg.busId = bus.busId;
    bus.readDATA_ff_1( &msg.length, msg.frame_data, &msg.frame_id );
    readQueue.push(msg);
  }

}


/*
*  Process received CAN message through middleware
*/
void processMessage( Message msg ){

  for(int i=0; i<=activeMiddlewareLength-1; i++)
    msg = activeMiddleware[i]->process( msg );

  if( msg.dispatch == true )
    writeQueue.push( msg );

}
