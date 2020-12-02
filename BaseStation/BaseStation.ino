/*
 Copyright (C) 2012 James Coliz, Jr. <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 
 Update 2014 - TMRh20
 */

/**
 * Simplest possible example of using RF24Network,
 *
 * RECEIVER NODE
 * Listens for messages from the transmitter and prints them out.
 */

#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include <DS3231.h>

// Init the DS3231 using the hardware interface
DS3231  rtc(SDA, SCL);

RF24 radio(9,10);                // nRF24L01(+) radio attached using Getting Started board 

RF24Network network(radio);      // Network uses that radio
const uint16_t this_node = 00;    // Address of our node in Octal format ( 04,031, etc)
const uint16_t base_station_node = 01;   // Address of the other node in Octal format

struct payload_t {                 // Structure of our payload
  uint16_t command;
  unsigned long node_id;
  unsigned long data;
  float avg_current;
};


void setup(void)
{
  Serial.begin(115200);
  
  // Initialize the rtc object
  rtc.begin();
  
  Serial.println("RF24Leach/BaseStation/");
 
  SPI.begin();
  radio.begin();
  network.begin(/*channel*/ 90, /*node address*/ this_node);
}

void loop(void){
  
  network.update();                  // Check the network regularly

  
  while ( network.available() ) {     // Is there anything ready for us?
    
    RF24NetworkHeader header;        // If so, grab it and print it out
    payload_t payload;
    network.read(header,&payload,sizeof(payload));
    // Send date
    Serial.print(rtc.getDateStr());
    Serial.print(" - ");
    // Send time
    Serial.print(rtc.getTimeStr());
    Serial.print(" -- ");
    Serial.print("Node 0");
    Serial.print(header.from_node, OCT);
    Serial.print(", ID: ");
    Serial.print(payload.node_id);
    Serial.print(" => #");
    Serial.print(payload.data);
    Serial.print(" - Average current: ");
    Serial.print(payload.avg_current);
    Serial.println(" mA");
  }
}
