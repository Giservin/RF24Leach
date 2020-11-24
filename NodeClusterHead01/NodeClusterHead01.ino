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

const uint16_t this_node_id = 4;
const uint16_t this_node = 021;        // Address of our node in Octal format
const uint16_t cluster_head_node = 01; // Address of the cluster head node in Octal format
const uint16_t base_station_node = 00;        // Address of the other node in Octal format

bool is_cluster_head;             // Am i a cluster head?

const unsigned long interval = 2000; //ms  // How often to send 'hello world to the other unit

unsigned long last_sent;             // When did we last send?
unsigned long packets_sent;          // How many have we sent already


struct payload_t {                  // Structure of our payload
  uint16_t command;
  unsigned long node_id;
  unsigned long data;
};

void setup(void)
{
  Serial.begin(115200);
  Serial.println("RF24Network/examples/helloworld_tx/");

  if ( this_node == 1 || this_node == 2 ) {
    is_cluster_head = true;
    Serial.println("This is Cluster Head");
  }
  else {
    is_cluster_head = false;
  }
 
  SPI.begin();
  radio.begin();
  network.begin(/*channel*/ 90, /*node address*/ this_node);
}

void loop() {
  
  network.update();                          // Check the network regularly

  //========== Receiving ==========//
  while ( network.available() && packets_sent > 10) {
    RF24NetworkHeader received_header;
    payload_t received_payload;
    network.read(received_header, &received_payload, sizeof(received_payload));
    //Cluster head code
    if (is_cluster_head) {
      if ( received_payload.command == 1 ) {
        Serial.println("receiving command to change cluster head");
        //sent ack
        payload_t payload = { 1, this_node_id, packets_sent };
        RF24NetworkHeader header(/*to node*/ received_header.from_node);
        bool ok = network.write(header,&payload,sizeof(payload));
        if (ok) {
          Serial.println("ok. no longer a cluster head");
          packets_sent = 0;
          is_cluster_head = false;
          network.begin(/*channel*/ 90, /*node address*/ received_header.from_node);
        }
        else {
          Serial.println("failed.");
        }
      }
    }

    //Node wanting to be cluster head
    else {
      if ( received_payload.command == 1 ) {
        Serial.println("ok. now this node is a cluster head.");
        packets_sent = 0;
        is_cluster_head = true;
        network.begin(/*channel*/ 90, /*node address*/ cluster_head_node);
      }
    }
  }
  
  //========== Sending  ==========//
  unsigned long now = millis();              // If it's time to send a message, send it!
  if ( now - last_sent >= interval ) {
    last_sent = now;

    Serial.print("Sending...");
    payload_t payload = { 0, this_node_id, packets_sent };
    RF24NetworkHeader header(/*to node*/ base_station_node);
    bool ok = network.write(header,&payload,sizeof(payload));
    if (ok) {
      packets_sent++;
      Serial.println("ok.");
    }
    else {
      Serial.println("failed.");
    }

    //Node ask to be a cluster head if already sent 10 or more packets
    if (packets_sent > 10 && is_cluster_head == false) {
      payload_t payload = {/*command for asking to be a cluster head*/ 1, this_node_id, packets_sent };
      RF24NetworkHeader header(/*to node*/ cluster_head_node);
      ok = network.write(header,&payload,sizeof(payload));
    }
  }
}
