#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

/***********************************************************************
************* Set the Node Address *************************************
/***********************************************************************/
 
// These are the Octal addresses that will be assigned
const uint16_t node_address_set[5] = { 01, 011, 021, 031, 041 };

RF24 radio(9,10);                    // nRF24L01(+) radio attached using Getting Started board 

RF24Network network(radio);          // Network uses that radio

/*const*/ uint16_t this_node_id;// = 1;
/*const*/ uint16_t this_node;// = 01;        // Address of our node in Octal format
const uint16_t cluster_head_node = 01; // Address of the cluster head node in Octal format
const uint16_t base_station_node = 00;        // Address of the other node in Octal format

bool is_cluster_head;             // Am i a cluster head?

const unsigned long interval = 2000; //ms  // How often to send 'hello world to the other unit

unsigned long last_sent;             // When did we last send?
unsigned long packets_sent;          // How many have we sent already

//This is for sleep mode. It is not really required, as users could just use the number 0 through 10
typedef enum { wdt_16ms = 0, wdt_32ms, wdt_64ms, wdt_128ms, wdt_250ms, wdt_500ms, wdt_1s, wdt_2s, wdt_4s, wdt_8s } wdt_prescalar_e;
 
unsigned long awakeTime = 500;                          // How long in ms the radio will stay awake after leaving sleep mode
unsigned long sleepTimer = 0;                           // Used to keep track of how long the system has been awake


struct payload_t {                  // Structure of our payload
  uint16_t command;
  unsigned long node_id;
  unsigned long data;
};

void setup(void)
{
  int pilih = 0;
//  int pilih = 1;
//  int pilih = 2;
//  int pilih = 3;
//  int pilih = 4;
   switch(pilih){
    case 0:
      this_node_id = 5;
      this_node = 01;        // Address of our node in Octal format
      break;
    case 1:
      this_node_id = 46;
      this_node = 011;        // Address of our node in Octal format
      break;
    case 2:
      this_node_id = 93;
      this_node = 021;        // Address of our node in Octal format
      break;
    case 3:
      this_node_id = 125;
      this_node = 031;        // Address of our node in Octal format
      break;
    case 4:
      this_node_id = 312;
      this_node = 041;        // Address of our node in Octal format
      break;
  }

  //LED STATUS
  pinMode(8, OUTPUT);
  
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

  /******************************** This is the configuration for sleep mode ***********************/
  //The watchdog timer will wake the MCU and radio every second to send a sleep payload, then go back to sleep
  network.setup_watchdog(wdt_1s);
}

void loop() {
  
  network.update();                          // Check the network regularly

  //========== Receiving ==========//
  while ( network.available() ) {
    RF24NetworkHeader received_header;
    payload_t received_payload;
    network.read(received_header, &received_payload, sizeof(received_payload));

    switch ( received_payload.command ) {
      case 100:
        if ( packets_sent > 5 ) {
          Serial.println("receiving command to switch cluster head");
          
          //sent reset command to other node
          bool ok = false;
          for( int i = 0; i < 5; i++ ) {
            ok = false;
            while(!ok) {
              if ( node_address_set[i] == this_node ) {
                ok = true;
              }
              else if ( node_address_set[i] == received_header.from_node ) {
                ok = true;
              }
              else {
                Serial.print("sending reset command to node with address: ");
                Serial.println(node_address_set[i], OCT);
                payload_t payload = { 150, this_node_id, packets_sent };
                RF24NetworkHeader header(/*to node*/ node_address_set[i]);
                ok = network.write(header,&payload,sizeof(payload));
              }
            }
          }

          // sent exchange command to cluster head candidate
          if (ok) {
            Serial.println("Sending ack to CH's candidate...");
            //sent ack to cluster head candidate
            payload_t cluster_head_payload = { 200, this_node_id, packets_sent };
            RF24NetworkHeader cluster_head_header(/*to node*/ received_header.from_node);
            ok = network.write(cluster_head_header,&cluster_head_payload,sizeof(cluster_head_payload));
            packets_sent = 0;
            if (ok) {
              Serial.println("ok. this node is no longer a cluster head");
              is_cluster_head = false;
              this_node = received_header.from_node;
              network.begin(/*channel*/ 90, /*node address*/ received_header.from_node);
            }
            else {
              Serial.println("failed to send ack to CH's candidate... this node is still a cluster head");
            }
          }
          else {
            Serial.println("failed.");
          }
          
        } else {
          payload_t payload = { 150, this_node_id, packets_sent };
          RF24NetworkHeader header(/*to node*/ received_header.from_node);
          /*bool ok = */network.write(header,&payload,sizeof(payload));
        }
        break;
        
      case 150:
        Serial.println("ok. reset.");
        packets_sent = 0;
        break;
      case 200:
        Serial.println("ok. now this node is a cluster head.");
        packets_sent = 0;
        is_cluster_head = true;
        this_node = cluster_head_node;
        network.begin(/*channel*/ 90, /*node address*/ cluster_head_node);
        break;
    }
  }
  
  //========== Sending  ==========//
  unsigned long now = millis();              // If it's time to send a message, send it!
  if ( now - last_sent >= interval ) {
    last_sent = now;

    Serial.print("Sending...");
    packets_sent++;
    payload_t payload = { 0, this_node_id, packets_sent };
    RF24NetworkHeader header(/*to node*/ base_station_node);
    bool ok = network.write(header,&payload,sizeof(payload));
    if (ok) {
      Serial.println("ok.");
    }
    else {
      Serial.println("failed.");
    }

    //Node ask to be a cluster head if already sent 10 or more packets
    if (packets_sent > 10 && is_cluster_head == false) {
      Serial.println("ask for cluster head role");
      payload_t payload = {/*command for asking to be a cluster head*/ 100, this_node_id, packets_sent };
      RF24NetworkHeader header(/*to node*/ cluster_head_node);
      ok = network.write(header,&payload,sizeof(payload));
    }
  }

  /***************************** CALLING THE NEW SLEEP FUNCTION ************************/    
 
  if ( millis() - sleepTimer > awakeTime && !is_cluster_head && packets_sent < 10 ) {  // Want to make sure the Arduino stays awake for a little while when data comes in. Do NOT sleep if master node.
     Serial.println("Sleep");
     sleepTimer = millis();                           // Reset the timer value
     delay(100);                                      // Give the Serial print some time to finish up
     radio.stopListening();                           // Switch to PTX mode. Payloads will be seen as ACK payloads, and the radio will wake up
     network.sleepNode(8,0);                          // Sleep the node for 8 cycles of 1second intervals
     Serial.println("Awake"); 
  }

  //========== LED CH STATUS ==========//
  if (is_cluster_head) {
    digitalWrite(8, HIGH);
  }
  else {
    digitalWrite(8, LOW);
  }
  
}
