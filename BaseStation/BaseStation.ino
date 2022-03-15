#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include <DS3231.h>

// Init the DS3231 using the hardware interface
DS3231  rtc(SDA, SCL);

RF24 radio(9,10);                // nRF24L01(+) radio attached using Getting Started board 

RF24Network network(radio);      // Network uses that radio
const uint16_t this_node = 00;    // Address of our node in Octal format ( 04,031, etc) //Kenapa node perlu address, apa gk cukup pakai id?
const uint16_t base_station_node = 00;   // Address of the other node in Octal format

//  SETTINGS variable
const uint16_t node_count = 8;                    //change to add node  //jumlah cluster head nya klo disini udah di fix in dulu
const uint16_t cluster_head_count = 2;            //change to add node

uint16_t received_address[node_count];
unsigned long leach_rounds = 0;
unsigned long leach_counter = 0;
unsigned long setup_counter = 0;
uint16_t discovery_counter = 0;
uint16_t discovery_id[node_count + cluster_head_count];
uint16_t discovery_address[node_count + cluster_head_count];
float energy_level[node_count];
bool is_leach[node_count];

struct payload_t {                 // Structure of our payload
  uint16_t command;
  char node_id;
  unsigned long data;
  float avg_current; //dari sensor arus.
  bool leach;
};

void reset_all_nodes(uint16_t *except, bool leach_increment); //untuk apa? Dapat dilihat except adalah parameter pertama tipe pointer (untuk array) untuk yg bukan CH

void setup(void)
{
  for ( int i = 0; i < 10; i++ ) {
    discovery_address[i] = 0; //pertama address nya dikasih 0 dulu?
  }
  Serial.begin(115200);
  
  // Initialize the rtc object
  rtc.begin(); //untuk timer nya brarti
  
  Serial.println("RF24Leach/BaseStation/");
 
  SPI.begin(); //buat koneksi nRF

  radio.setPALevel(RF24_PA_LOW);  // RF24_PA_MAX is default. //biar low energy communication kan di nRF nya?
  radio.begin();
  network.begin(/*channel*/ 90, /*node address*/ this_node);
}

void loop(void){
  
  network.update();                  // Check the network regularly, network itu mengarah ke RF24Network (kyk nya termasuk syntax wajib).

  
  while ( network.available() ) {     // Is there anything ready for us?
    
    RF24NetworkHeader received_header;        // If so, grab it and print it out
    payload_t received_payload;               //object dr struct payload_t
    network.read(received_header,&received_payload,sizeof(received_payload)); //header nya auto dr nRF nya, received_payloed refer to address, disini network read artinya , nRF ngeread wireless apakah ada yg ngirim data , disini akan diertima di payload dengan size payload dari struct
    // Send date
    Serial.print(rtc.getDateStr()); //dari library timer
    Serial.print(" - ");
    // Send time
    Serial.print(rtc.getTimeStr());
    Serial.print(" -- ");
    bool registered;
    bool ok;
    uint16_t except[cluster_head_count]; //untuk apa ini? untuk node yg gk jadi CH

    // command 0 = untuk nerima data (di payload nya) kemudian di serial monitor kan.
    if ( received_payload.command == 0 ) {
      Serial.print("Node 0");
      Serial.print(received_header.from_node, OCT); //OCT??
      Serial.print(", ID: ");
      Serial.print(received_payload.node_id);
      Serial.print(" => #");
      Serial.print(received_payload.data);
      Serial.print(" - Average current: ");
      Serial.print(received_payload.avg_current);
      Serial.println(" mA");
    }

    // command 10 =  untuk discovery
    else if ( received_payload.command == 10 ) {
      uint16_t discovery;
      bool check = false;
      for ( int i = 0; i < 10; i++ ) { //ini nge cek apakah node nya sudah di discover oleh base station.
        if ( discovery_id[i] == received_payload.node_id ) {
          check = true;
          discovery = i;
          break;
        }
      }
      if ( !check ) { //kalau belum terdiscovery
        Serial.print("Received discovery command by node ID: ");
        Serial.print(received_payload.node_id);
        if ( discovery_counter == 0 ) {
          discovery = 1;
        } else if ( discovery_counter == 1 ) {
          discovery = 9;
        } else if ( discovery_counter == 2 ) {
          discovery = 17;
        } else if ( discovery_counter == 3 ) {
          discovery = 25;
        } else if ( discovery_counter == 4 ) {
          discovery = 33;
        } else if ( discovery_counter == 5 ) {
          discovery = 2;
        } else if ( discovery_counter == 6 ) {
          discovery = 10;
        } else if ( discovery_counter == 7 ) {
          discovery = 18;
        } else if ( discovery_counter == 8 ) {
          discovery = 26;
        } else if ( discovery_counter == 9 ) {
          discovery = 34; // 1, 9, 17, 25, 33, 2, 10, 18, 26, 34?????? maksudnya gimana? mungkin address nya.
        }
        discovery_id[discovery_counter] = received_payload.node_id;
        discovery_address[discovery_counter] = discovery;
        discovery_counter++;
      }
      else {
        Serial.print("Already received discovery command by node ID: ");
        Serial.print(received_payload.node_id);
        discovery = discovery_address[discovery]; //klo udh discovery, node nya
      }
      Serial.print(", assigned with address: 0");
      Serial.println(discovery, OCT); //OCT?
      payload_t payload = {10, received_payload.node_id, discovery, 0, false }; //ngisi struct nya
      RF24NetworkHeader header(received_header.from_node);
      ok = network.write(header,&payload,sizeof(payload)); //ngirim packet ke node2 nya scr wireless.
    }

    // command 100
    else if ( received_payload.command == 100 ) {
      /*bool*/ registered = false; 
      for ( int i = 0; i < node_count; i++ ) {
        if ( received_address[i] == received_header.from_node ) {
          Serial.print("Already receiving setup command from Node 0");
          Serial.print(received_header.from_node, OCT);
          Serial.print(" with id ");
          Serial.println(received_payload.node_id);
          registered = true;
          break;
        }
      }
      //kumpulkan anggota hingga penuh
      if ( !registered ) {
        Serial.print("Received setup command from Node 0");
        Serial.print(received_header.from_node, OCT);
        Serial.print(" with id ");
        Serial.print(received_payload.node_id);
        Serial.print(" with leach payload ");
        Serial.print(received_payload.leach);
        Serial.print(" and energy level ");
        Serial.println(received_payload.avg_current);
//        if ( received_payload.leach == true ) {
//          is_leach[setup_counter] = received_payload.leach;
//          leach_counter++;
//        }
        energy_level[setup_counter] = received_payload.avg_current;
        received_address[setup_counter] = received_header.from_node;
        setup_counter++;
      }
      //anggota telah penuh, kirim command ke base station
      if ( setup_counter == node_count ) {
        //energy SORT
        bool had_swapped;
        do {
          had_swapped = false;
          for ( int i = 0; i < node_count - 1; i++ ) {
            if ( energy_level[i] > energy_level[i + 1] ) {
              float temp = energy_level[i];
              energy_level[i] = energy_level[i + 1];
              energy_level[i + 1] = temp;
              uint16_t temp_address = received_address[i];
              received_address[i] = received_address[i + 1];
              received_address[i + 1] = temp_address;            
              had_swapped = true;
            }
          }
        }
        //print sorted energy level
        while (had_swapped);
        for (int i = 0; i < node_count; i++) {
          Serial.print(energy_level[i]);
          Serial.print("  ");
        }
        Serial.println("");
        for (int i = 0; i < node_count; i++) {
          Serial.print("0");
          Serial.print(received_address[i], OCT);
          Serial.print("  ");
        }
        Serial.println("");
        //sending reset and CH switch to node
        for( int i = node_count - 1; i >= 0; i-- ) {
          if ( i < 2 ) {
            network.update();                          // Check the network regularly
            bool ok = false;
            Serial.print("150: 0");
            Serial.print(i + 1, OCT);
            Serial.print(" => 170: 0");
            Serial.println(received_address[i], OCT);
            payload_t payload = { 150, this_node, received_address[i], 0, false };
            RF24NetworkHeader header(/*to cluster head*/ i + 1);
//          while (!ok) {
            ok = network.write(header,&payload,sizeof(payload));
//            delay(50);
//          }
            delay(250);
          }
          else {
            Serial.print("0");
            Serial.print(received_address[i], OCT);
            Serial.print(" 200 ");
            Serial.println(is_leach[i]);
            payload_t payload = { 200, this_node, received_address[i], 0, false };
            RF24NetworkHeader header(/*to node*/ received_address[i]);
//          while (!ok) {
            ok = network.write(header,&payload,sizeof(payload));
//            delay(50);
//          }
          }
        }
        for ( int i = 0; i < node_count; i++ ) {
          received_address[i] = 0;
          energy_level[i] = 0;
        }
        setup_counter = 0;
      }
    }
  }
}
