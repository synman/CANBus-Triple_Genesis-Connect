/*
* 2015 Hyundai Genesis Coupe customizations
* 6/22/2015 - Shell M. Shrader
*/

#ifndef GenesisConnect_H
#define GenesisConnect_H

#define BT_SEND_DELAY 20

#include "Middleware.h"

class GenesisConnect : public Middleware {
  public:
    GenesisConnect( QueueArray<Message> *q );
    void tick();
    Message process( Message msg );
    void commandHandler(byte* bytes, int length);
    Stream* activeSerial;
    static void reset();
  private:
    QueueArray<Message>* mainQueue;
    unsigned long lastTime;
    unsigned long lastTemp;
    static int powerOn;
    static unsigned long lastDisp;
    static int volume;
    static int mute;
    static unsigned long lastMute;
    static int thermostat;
    static int minute;
    static int outsideTemp;
    static int bluetooth;
    static int vents;
    static int airflow;
    static int compressor;
    static int audioSource;
    static int radioStation;
    static int radioBand;
};

GenesisConnect::GenesisConnect( QueueArray<Message> *q ) {
  mainQueue = q;
  
  lastTime = millis();
  lastTemp = lastTime;

  // Default Instance Properties
  activeSerial = &Serial;
  
  // other initializations
  lastTime = 0;
  reset();
}

void GenesisConnect::tick() {
  if( Serial1.available() > 0 ){
    activeSerial = &Serial1;
  }

  if( Serial.available() > 0 ){
    activeSerial = &Serial;
  }
}

void GenesisConnect::GenesisConnect::reset() {

  GenesisConnect::lastDisp = 0;
  GenesisConnect::lastMute = 0;
  GenesisConnect::powerOn = -1;
  GenesisConnect::volume = -1;
  GenesisConnect::mute = -1;
  GenesisConnect::thermostat = -1;
  GenesisConnect::minute = -1;
  GenesisConnect::outsideTemp = -100;
  GenesisConnect::bluetooth = -1;
  GenesisConnect::vents = -1;
  GenesisConnect::airflow = -1;
  GenesisConnect::compressor = -1;
  GenesisConnect::audioSource = -1;
  GenesisConnect::radioStation = -1;
  GenesisConnect::radioBand = -1;
}

Message GenesisConnect::process( Message msg ) {
  
  // bail if not BTLE
  if (activeSerial == &Serial) return msg;
  
  // 0x28 == disp button and bass, midrange, treble, fader, balance
  if (msg.frame_id == 0x28) {
    delay(BT_SEND_DELAY);
    activeSerial->write(0x7C);
    activeSerial->write(0x7C);
    activeSerial->write(0x28);
    activeSerial->write(msg.frame_data[1]);
    activeSerial->write(msg.frame_data[2]);
    activeSerial->write(msg.frame_data[3]);
    activeSerial->write(msg.frame_data[4]);
    activeSerial->write(msg.frame_data[5]);
    GenesisConnect::lastDisp = millis();
    return msg;
  }

  // 0x101 == power button
  if (msg.frame_id == 0x101 && GenesisConnect::powerOn != msg.frame_data[5]) {
    delay(BT_SEND_DELAY);
    activeSerial->write(0x7C);
    activeSerial->write(0x7C);
    activeSerial->write(0x10);
    activeSerial->write(msg.frame_data[5]);
    GenesisConnect::powerOn = msg.frame_data[5];
    return msg;
  }
  
  if (msg.frame_id == 0x10A) {
    // 0x10A - volume, source 2=RADIO 10=XM A0=USB 51=BT, radio station, radio band
    if ((msg.frame_data[1] == 0x10 && (GenesisConnect::volume != msg.frame_data[2]) || 
                                       GenesisConnect::audioSource != msg.frame_data[0] ||
                                       GenesisConnect::radioStation != msg.frame_data[4] ||
                                       GenesisConnect::radioBand != msg.frame_data[5])) {
      delay(BT_SEND_DELAY);
      activeSerial->write(0x7C);
      activeSerial->write(0x7C);
      activeSerial->write(0x11);
      activeSerial->write(msg.frame_data[2]);
      activeSerial->write(msg.frame_data[0]);
      activeSerial->write(msg.frame_data[4]);
      activeSerial->write(msg.frame_data[5]);
      GenesisConnect::volume = msg.frame_data[2];
      GenesisConnect::audioSource = msg.frame_data[0];
      GenesisConnect::radioStation = msg.frame_data[4];
      GenesisConnect::radioBand = msg.frame_data[5];
      return msg; 
    }  
    
    // 0x10A - mute
    if (msg.frame_data[1] == 0x44 && GenesisConnect::mute != msg.frame_data[1]) {
      delay(BT_SEND_DELAY);
      activeSerial->write(0x7C);
      activeSerial->write(0x7C);
      activeSerial->write(0x12);
      activeSerial->write(0x01);
      GenesisConnect::lastMute = millis();
      GenesisConnect::mute = msg.frame_data[1];
      return msg;
    }  
    
    // 0x10A - unmute
    if (msg.frame_data[1] == 0x00 && GenesisConnect::lastMute + 500 < millis() && GenesisConnect::mute != msg.frame_data[1]) {
      delay(BT_SEND_DELAY);
      activeSerial->write(0x7C);
      activeSerial->write(0x7C);
      activeSerial->write(0x12);
      activeSerial->write((byte) 0x00);
      GenesisConnect::mute = msg.frame_data[1];
      GenesisConnect::lastMute = 0;
      return msg;
    }    
  }

  // 0x131 - thermostat
  if (msg.frame_id == 0x131 && GenesisConnect::thermostat != msg.frame_data[2]) {
    delay(BT_SEND_DELAY);
    activeSerial->write(0x7C);
    activeSerial->write(0x7C);
    activeSerial->write(0x13);
    activeSerial->write(msg.frame_data[2]);
    GenesisConnect::thermostat = msg.frame_data[2];
    return msg;
  }    
  
  //0x502 - time of day
  if (msg.frame_id == 0x502 && lastTime + 2000 < millis()) {
    delay(BT_SEND_DELAY);
    activeSerial->write(0x7C);
    activeSerial->write(0x7C);
    activeSerial->write(0x14);
    activeSerial->write(msg.frame_data[0]);
    activeSerial->write(msg.frame_data[1]);
    GenesisConnect::minute = msg.frame_data[1];
    lastTime = millis();
    return msg;
  }

  //0x531 - outside temperature
  if (msg.frame_id == 0x531 && lastTemp + 5000 < millis()) {
    delay(BT_SEND_DELAY);
    activeSerial->write(0x7C);
    activeSerial->write(0x7C);
    activeSerial->write(0x15);
    activeSerial->write(msg.frame_data[2]);
    GenesisConnect::outsideTemp = msg.frame_data[2];
    lastTemp = millis();
    return msg;
  }
  
  //0x102 - bluetooth connection
  if (msg.frame_id == 0x102 && msg.frame_data[1] != GenesisConnect::bluetooth) {
    delay(BT_SEND_DELAY);
    activeSerial->write(0x7C);
    activeSerial->write(0x7C);
    activeSerial->write(0x16);
    activeSerial->write(msg.frame_data[1]);
    GenesisConnect::bluetooth = msg.frame_data[1];
    return msg;
  }
  
  //0x132 - ac vents, auto/fresh/recirc, ac compressor
  if (msg.frame_id == 0x132 && (msg.frame_data[0] != GenesisConnect::vents || 
                                msg.frame_data[1] != GenesisConnect::airflow ||
                                msg.frame_data[2] != GenesisConnect::compressor) &&
                                msg.frame_data[1] != 0x14 && 
                                msg.frame_data[1] != 0x10 && 
                                msg.frame_data[1] != 0x11 &&
                                msg.frame_data[1] != 0x15) {
    delay(BT_SEND_DELAY);
    activeSerial->write(0x7C);
    activeSerial->write(0x7C);
    activeSerial->write(0x17);
    activeSerial->write(msg.frame_data[0]);
    activeSerial->write(msg.frame_data[1]);
    activeSerial->write(msg.frame_data[2]);
    GenesisConnect::vents = msg.frame_data[0];
    GenesisConnect::airflow = msg.frame_data[1];
    GenesisConnect::compressor = msg.frame_data[2];
    return msg;
  }
  
  return msg;
}

void GenesisConnect::commandHandler(byte* bytes, int length){}

#endif
