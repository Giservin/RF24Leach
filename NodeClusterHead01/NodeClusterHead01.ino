#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

Adafruit_INA219 ina219;
float avg_current = 0;
unsigned long sample_count = 0;
unsigned long last_sampling = 0;
const uint16_t sampling_rate = 100;

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

const uint16_t node_count = 4;
const uint16_t cluster_head_per_cluster = 1;
uint16_t received_address[node_count];
unsigned long leach_rounds = 0;
unsigned long leach_counter = 0;
unsigned long setup_counter = 0;
bool is_leach[node_count];

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
  float avg_current;
  bool leach;
};

int pilih;
void setup(void)
{
  pilih = 0;
//  pilih = 1;
//  pilih = 2;
//  pilih = 3;
//  pilih = 4;

//  pilih = 5;
//  pilih = 6;
//  pilih = 7;
//  pilih = 8;
//  pilih = 9;
   switch(pilih){
    case 0:
      this_node_id = 11;
      this_node = 01;        // Address of our node in Octal format
      if (! ina219.begin()) {
        Serial.println("Failed to find INA219 chip");
        while (1) { delay(10); }
      }
      break;
    case 1:
      this_node_id = 22;
      this_node = 011;        // Address of our node in Octal format
      break;
    case 2:
      this_node_id = 33;
      this_node = 021;        // Address of our node in Octal format
      break;
    case 3:
      this_node_id = 44;
      this_node = 031;        // Address of our node in Octal format
      break;
    case 4:
      this_node_id = 55;
      this_node = 041;        // Address of our node in Octal format
      break;
      
    case 5:
      this_node_id = 111;
      this_node = 02;        // Address of our node in Octal format
      break;
    case 6:
      this_node_id = 222;
      this_node = 012;        // Address of our node in Octal format
      break;
    case 7:
      this_node_id = 333;
      this_node = 022;        // Address of our node in Octal format
      break;
    case 8:
      this_node_id = 444;
      this_node = 032;        // Address of our node in Octal format
      break;
    case 9:
      this_node_id = 555;
      this_node = 042;        // Address of our node in Octal format
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

  uint32_t currentFrequency;
  
  ina219.setCalibration_16V_400mA();

  Serial.println("Measuring voltage and current with INA219 ...");
  
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
    bool registered = false;
    bool ok = false;
    uint16_t except;
    switch ( received_payload.command ) {
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
//          if ( received_payload.leach == true && leach_counter < cluster_head_per_cluster ) {
            is_leach[setup_counter] = received_payload.leach;
//            leach_counter++;
//          }
//          else {
//            is_leach[setup_counter] = false;
//          }
          received_address[setup_counter] = received_header.from_node;
          setup_counter++;
        }
        //anggota telah penuh, kirim command ke base station
        if(is_cluster_head == true && setup_counter == node_count) {
          Serial.println("Sending command to base station, cluster head will change");
          /*bool*/ ok = false;
          payload_t payload = { 110, this_node_id, leach_counter, 0, false };
          RF24NetworkHeader header(/*to node*/ base_station_node);
          while (!ok) {
            ok = network.write(header,&payload,sizeof(payload));
            delay(10);
          }
          leach_counter = 0;
          setup_counter = 0;
        }
        break;
      
      case 150:
        //reset other nodes other than cluster head candidate
//        reset_false_nodes();
        //change cluster head
        Serial.println("Changing cluster head.");
        for ( int i = 0; i < node_count; i++ ) {
          /*bool*/ ok = false;
          if ( is_leach[i] == true ) {
            Serial.print("0");
            Serial.print(received_address[i], OCT);
            Serial.print(" 210 ");
            Serial.println(is_leach[i]);
            payload_t payload = { 210, this_node, /*use this CH as an address*/0, 0, false };
            RF24NetworkHeader header(/*to node*/ received_address[i]);
            while (!ok) {
              ok = network.write(header,&payload,sizeof(payload));
              delay(10);
            }
            packets_sent = 0;
            is_cluster_head = false;
            this_node = received_address[i];
            except = this_node;
            network.begin(/*channel*/ 90, /*node address*/ this_node);
            break;
          }
        }
        reset_all_nodes(except);
        empty_array();
        break;

      case 170:
        //reset other nodes other than cluster head candidate
        reset_false_nodes();
        Serial.println("Changing cluster head.");
        //other cluster's head first
        for ( int i = 0; i < node_count; i++ ) {
          /*bool*/ ok = false;
          if ( is_leach[i] == true ) {
            Serial.print("0");
            Serial.print(received_address[i], OCT);
            Serial.print(" 210 ");
            Serial.println(is_leach[i]);
            payload_t payload = { 210, this_node, /*use other CH's address*/1, 0, false };
            RF24NetworkHeader header(/*to node*/ received_address[i]);
            while (!ok) {
              ok = network.write(header,&payload,sizeof(payload));
              delay(10);
            }
            is_leach[i] = false;
            break;
          }
        }
        //then this cluster's head
        for ( int i = 0; i < node_count; i++ ) {
          /*bool*/ ok = false;
          if ( is_leach[i] == true ) {
            Serial.print("0");
            Serial.print(received_address[i], OCT);
            Serial.print(" 210 ");
            Serial.println(is_leach[i]);
            payload_t payload = { 210, this_node, /*use this CH as an address*/0, 0, false };
            RF24NetworkHeader header(/*to node*/ received_address[i]);
            while (!ok) {
              ok = network.write(header,&payload,sizeof(payload));
              delay(10);
            }
            packets_sent = 0;
            is_cluster_head = false;
            this_node = received_address[i];
            network.begin(/*channel*/ 90, /*node address*/ this_node);
            break;
          }
        }
        reset_all_nodes(except);
        empty_array();
        break;

      case 180:
        //reset other nodes other than cluster head candidate
        reset_all_nodes(except);
        break;
        
      case 200:
        Serial.println("ok. reset.");
        packets_sent = 0;
        leach_rounds++;
        break;
      case 210:
        Serial.println("ok. now this node is a cluster head.");
        if ( received_payload.data == 0 ) {
          this_node = received_header.from_node;
        }
        else if ( received_payload.data == 1 ) {
          if ( received_header.from_node == 01 ) {
            this_node = 02;
          }
          else if ( received_header.from_node == 02 ) {
            this_node = 01;
          }
        }
        packets_sent = 0;
        leach_rounds++;
        is_cluster_head = true;
        network.begin(/*channel*/ 90, /*node address*/ this_node);
        break;
    }
  }
  
  //========== Sending  ==========//
  unsigned long now = millis();              // If it's time to send a message, send it!
  if ( now - last_sent >= interval ) {
    last_sent = now;
    Serial.print("Sending...");
    payload_t payload = { 0, this_node_id, packets_sent, avg_current, true };
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
      Serial.println("ask for cluster head role");
      payload_t payload = {/*command for asking to be a cluster head*/ 100, 0, 0, 0, true };
      RF24NetworkHeader header(/*to node*/ cluster_head_node);
      ok = network.write(header,&payload,sizeof(payload));
    }
  }

  /***************************** CALLING THE NEW SLEEP FUNCTION ************************/    
 
  if ( millis() - sleepTimer > awakeTime && !is_cluster_head && packets_sent < 10 ) {  // Want to make sure the Arduino stays awake for a little while when data comes in. Do NOT sleep if master node.
    sleepTimer = millis();
    Serial.println("Sleep");                           // Reset the timer value
    delay(100);                                      // Give the Serial print some time to finish up
    radio.stopListening();                           // Switch to PTX mode. Payloads will be seen as ACK payloads, and the radio will wake up
    network.sleepNode(8,0);                          // Sleep the node for 8 cycles of 1second intervals
    Serial.println("Awake"); 
  }

  if ( this_node_id == 11 && millis() - last_sampling > sampling_rate ) {
    last_sampling = millis();
    sample_count++;
    avg_current = avg_current * (sample_count - 1) / sample_count + (ina219.getCurrent_mA() / sample_count);
////    Serial.println(sample_count);
//    Serial.print("Current:       "); Serial.print(ina219.getCurrent_mA()); Serial.println(" mA");
//    Serial.print("Avg Current   :"); Serial.print(avg_current); Serial.println(" mA");
  }

  //========== LED CH STATUS ==========//
  if (is_cluster_head) {
    digitalWrite(8, HIGH);
  }
  else {
    digitalWrite(8, LOW);
  }
  
}

void empty_array() {
  for ( int i = 0; i < node_count; i++ ) {
    received_address[i] = NULL;
    is_leach[i] = NULL;
  }
}

void reset_false_nodes() {
  Serial.println("Reseting nodes.");
  for ( int i = 0; i < node_count; i++ ) {
    bool ok = false;
    if ( is_leach[i] == false ) {
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

void reset_all_nodes(uint16_t except) {
  Serial.println("Reseting nodes.");
  for ( int i = 0; i < node_count; i++ ) {
    bool ok = false;
    if ( received_address[i] != except ) {
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
