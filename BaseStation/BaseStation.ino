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

uint16_t cluster_head_count = 1;

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
        
      case 110:
        Serial.println("Cluster head is changing.");
        for ( int i = 1; i <= cluster_head_count; i++ ) {
          bool ok = false;
          payload_t payload = { 150, 0, 0, 0, false };
          RF24NetworkHeader header(/*to node*/ i);
          while (!ok) {
            ok = network.write(header,&payload,sizeof(payload));
            delay(10);
          }
        }
        break;
        
    }
  }
}
