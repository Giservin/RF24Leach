#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include <DS3231.h>

// Init the DS3231 using the hardware interface
DS3231  rtc(SDA, SCL);

RF24 radio(9,10);                // nRF24L01(+) radio attached using Getting Started board 

RF24Network network(radio);      // Network uses that radio
const uint16_t this_node = 00;    // Address of our node in Octal format ( 04,031, etc)
const uint16_t base_station_node = 00;   // Address of the other node in Octal format

//  SETTINGS variable
const uint16_t node_count = 8;                    //change to add node
const uint16_t cluster_head_count = 2;            //change to add node

uint16_t received_address[node_count];
unsigned long leach_rounds = 0;
unsigned long leach_counter = 0;
unsigned long setup_counter = 0;
bool is_leach[node_count];

struct payload_t {                 // Structure of our payload
  uint16_t command;
  unsigned long node_id;
  unsigned long data;
  float avg_current;
  bool leach;
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
    
    RF24NetworkHeader received_header;        // If so, grab it and print it out
    payload_t received_payload;
    network.read(received_header,&received_payload,sizeof(received_payload));
    // Send date
    Serial.print(rtc.getDateStr());
    Serial.print(" - ");
    // Send time
    Serial.print(rtc.getTimeStr());
    Serial.print(" -- ");
    bool registered;
    bool ok;
    uint16_t except[cluster_head_count];
    switch ( received_payload.command ) {
      case 0:
        Serial.print("Node 0");
        Serial.print(received_header.from_node, OCT);
        Serial.print(", ID: ");
        Serial.print(received_payload.node_id);
        Serial.print(" => #");
        Serial.print(received_payload.data);
        Serial.print(" - Average current: ");
        Serial.print(received_payload.avg_current);
        Serial.println(" mA");
        break;
        
      case 100:
        /*bool*/ registered = false; 
        for ( int i = 0; i < node_count; i++ ) {
          if ( received_address[i] == received_header.from_node ) {
            Serial.print("Already receiving setup command from Node 0");
            Serial.println(received_header.from_node, OCT);
            registered = true;
            break;
          }
        }
        //kumpulkan anggota hingga penuh
        if ( !registered ) {
          Serial.print("Received setup command from Node 0");
          Serial.print(received_header.from_node, OCT);
          Serial.print(" with leach payload ");
          Serial.println(received_payload.leach);
          if ( received_payload.leach == true ) {
            is_leach[setup_counter] = received_payload.leach;
            leach_counter++;
          }
          received_address[setup_counter] = received_header.from_node;
          setup_counter++;
        }
        //anggota telah penuh, kirim command ke base station
        if(setup_counter == node_count && leach_counter >= cluster_head_count) {
          network.update();                          // Check the network regularly
          Serial.println("Cluster head will change.");
          for ( int cluster_head_address = 1; cluster_head_address <= cluster_head_count; cluster_head_address++ ) {
            for ( int i = 0; i < node_count; i++ ) {
              /*bool*/ ok = false;
              if ( is_leach[i] == true ) {
                is_leach[i] = false;
                except[cluster_head_address - 1] = received_address[i];
                break;
              }
            }
          }
          reset_all_nodes(except, true);
          for ( int address = 1; address <= cluster_head_count; address++ ) {
            network.update();                          // Check the network regularly
            /*bool*/ ok = false;
            Serial.print("150: 0");
            Serial.print(address, OCT);
            Serial.print(" => 170: 0");
            Serial.println(except[address - 1], OCT);
            payload_t payload = { 150, this_node, except[address - 1], 0, false };
            RF24NetworkHeader header(/*to cluster head*/ address);
            while (!ok) {
              ok = network.write(header,&payload,sizeof(payload));
              delay(10);
            }
          }
          setup_counter = 0;
          leach_counter = 0;
        }
        else if (setup_counter == node_count) {
          reset_all_nodes(except, false);
          setup_counter = 0;
          leach_counter = 0;
        }
        break;
    }
  }
}

void reset_all_nodes(uint16_t *except, bool leach_increment) {
  if ( leach_increment == true ) {
    Serial.println("Resetting nodes.");
    for ( int i = 0; i < node_count; i++ ) {
      bool ok = false;
      if ( received_address[i] != except[0] && received_address[i] != except[1] ) {
        Serial.print("0");
        Serial.print(received_address[i], OCT);
        Serial.print(" 200 ");
        Serial.println(is_leach[i]);
        payload_t payload = { 200, this_node, received_address[i], 0, false };
        RF24NetworkHeader header(/*to node*/ received_address[i]);
        while (!ok) {
          ok = network.write(header,&payload,sizeof(payload));
          delay(10);
        }
      }
    }
  }
  else {
    Serial.println("Resetting nodes. but not incrementing LEACH round.");
    for ( int i = 0; i < node_count; i++ ) {
      bool ok = false;
      Serial.print("0");
      Serial.print(received_address[i], OCT);
      Serial.print(" 210 ");
      Serial.println(is_leach[i]);
      payload_t payload = { 210, this_node, received_address[i], 0, false };
      RF24NetworkHeader header(/*to node*/ received_address[i]);
      while (!ok) {
        ok = network.write(header,&payload,sizeof(payload));
        delay(10);
      }
    }
  }
  for ( int i = 0; i < node_count; i++ ) {
    received_address[i] = NULL;
    is_leach[i] = NULL;
  }
}
