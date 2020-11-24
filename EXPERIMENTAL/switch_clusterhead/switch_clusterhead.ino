/*
 Copyright (C) 2012 James Coliz, Jr. <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 
 Update 2014 - TMRh20
 */

/**
 * Simplest possible example of using RF24Network 
 *
 * TRANSMITTER NODE
 * Every 2 seconds, send a payload to the receiver node.
 */

#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

RF24 radio(9,10);                    // nRF24L01(+) radio attached using Getting Started board 

RF24Network network(radio);          // Network uses that radio

const uint16_t this_node_id = 2;
const uint16_t this_node = 011;        // Address of our node in Octal format
const uint16_t cluster_head_node = 01; // Address of the cluster head node in Octal format
const uint16_t base_station_node = 00;        // Address of the other node in Octal format

bool is_cluster_head = false;             // Am i a cluster head?

const unsigned long interval = 2000; //ms  // How often to send 'hello world to the other unit

unsigned long last_sent;             // When did we last send?
unsigned long packets_sent;          // How many have we sent already


struct payload_t {                  // Structure of our payload
  uint16_t command;
  unsigned long id;
  unsigned long data;
};

void setup(void)
{
  Serial.begin(115200);
  Serial.println("RF24Network/examples/helloworld_tx/");
 
  SPI.begin();
  radio.begin();
  network.begin(/*channel*/ 90, /*node address*/ this_node);
}

void loop() {
  
  network.update();                          // Check the network regularly

  
  unsigned long now = millis();              // If it's time to send a message, send it!
  if ( now - last_sent >= interval )
  {
    last_sent = now;

    Serial.print("Sending...");
    payload_t payload = { 0, this_node_id, packets_sent++ };
    RF24NetworkHeader header(/*to node*/ base_station_node);
    bool ok = network.write(header,&payload,sizeof(payload));
    if (ok)
      Serial.println("ok.");
    else
      Serial.println("failed.");
  }

  if ( packets_sent >= 20) {
    if ( is_cluster_head ) {
      is_cluster_head = false;
      network.begin(90, 011);
    } else {
      is_cluster_head = true;
      network.begin(90, 01);
    }
    packets_sent = 0;
  }

}
