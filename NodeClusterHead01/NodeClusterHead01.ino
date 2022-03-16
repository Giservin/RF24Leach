#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include <Wire.h> //include Library wire untuk apa ya ntar?
#include <Adafruit_INA219.h> //sensor arus

//  INA219 variable deklarasi untuk sensor arus
Adafruit_INA219 ina219; //objek
float avg_current = 0; //arus
unsigned long sample_count = 0;
unsigned long last_sampling = 0;
const uint16_t sampling_rate = 250; // 250ms // akan di sampling (brarti sinyal arusnya analog ya?)

//  RADIO SETUP variable, nRF set nya sama kek di base station
RF24 radio(9,10);                    // nRF24L01(+) radio attached using Getting Started board 
RF24Network network(radio);          // Network uses that radio

//  ADDRESSING variable
/*const*/ char this_node_id;// = 1;
/*const*/ uint16_t this_node = 01;// = 01;        // Address of our node in Octal format, Kenapa address nya 01? apakah setiap node beda? apakah gara2 addressnya dikasih base station? dia disini cuma inisialisasi nya adalah 01
const uint16_t cluster_head_node = 01; // Address of the cluster head node in Octal format, Seperti nya untuk menjadikan CH, harus ada pertukaran address disini, makanya diatur di base station(?) akan di cari tahu lebih lanjut
const uint16_t base_station_node = 00;        // Address of the other node in Octal format

//  LEACH variable
bool is_cluster_head;             // Am i a cluster head?
//  LEACH FORMULA variable
const float leach_percentage = 0.2; // 20 %, artinya dri 10 node, akan ada 2 node CH
const uint16_t leach_percentage_simplified = 1 / leach_percentage;
unsigned long leach_rounds = 1;
//bool leach_already_ch = false;
bool discovered = false; //apakah harus discovery ya? mungkin untk paramater nanti(?)

// SENDING variable
const unsigned long interval = 2000; //ms  // How often to send 'hello world to the other unit, brarti message exchange lah, setiap 2 detik ya?
unsigned long last_sent;             // When did we last send?
unsigned long packets_sent;          // How many have we sent already

//  SLEEP variable
typedef enum { wdt_16ms = 0, wdt_32ms, wdt_64ms, wdt_128ms, wdt_250ms, wdt_500ms, wdt_1s, wdt_2s, wdt_4s, wdt_8s } wdt_prescalar_e; //fungsi enum? (AKAN DICARI TAHU), kmudian menggunakan Watch Dog Timer untuk sleep ya(?)
unsigned long awakeTime = 500;                          // How long in ms the radio will stay awake after leaving sleep mode, apakah sleep nya gk sesuai turn nya ya?
unsigned long sleepTimer = 0;                           // Used to keep track of how long the system has been awake
uint16_t sleep_count = 0;

//  DATA STRUCT
struct payload_t {                  // Structure of our payload, payload nya disamain dengan base station
  uint16_t command;
  char node_id;
  unsigned long data;
  float avg_current;
  bool leach;
};

void setup(void)
{
//  this_node_id = 65;  //A
//  this_node_id = 66;  //B
//  this_node_id = 67;  //C
//  this_node_id = 68;  //D
//  this_node_id = 69;  //E
//  
  this_node_id = 70;  //F, brarti dengan ASCII untuk ngasih ID nya,akan dibedakan tiap node nya brarti.
//  this_node_id = 71;  //G
//  this_node_id = 72;  //H
//  this_node_id = 73;  //I
//  this_node_id = 74;  //J

  if (! ina219.begin()) {
    Serial.println("Failed to find INA219 chip");
    while (1) { delay(10); }
  }

  //LED STATUS
  pinMode(8, OUTPUT);
  
  Serial.begin(115200);
  Serial.println("RF24Leach/NodeClusterHead01");
 
  SPI.begin();

  radio.setPALevel(RF24_PA_LOW);  // RF24_PA_MAX is default.
  radio.begin();
  network.begin(/*channel*/ 90, /*node address*/ this_node); //sama kek di base station

  uint32_t currentFrequency; //frekuensi buat apa?
  
  ina219.setCalibration_16V_400mA(); //kalibrasi gmn ini maksudnya?

  Serial.println("Measuring voltage and current with INA219 ...");
  
  /******************************** This is the configuration for sleep mode ***********************/
  //The watchdog timer will wake the MCU and radio every second to send a sleep payload, then go back to sleep
  network.setup_watchdog(wdt_250ms);

  // random seed
  randomSeed(analogRead(A0));
}

void loop() {
  
  network.update();                          // Check the network regularly

  //========== Receiving ==========//
  while ( network.available() ) {
    RF24NetworkHeader received_header;
    payload_t received_payload;
    network.read(received_header, &received_payload, sizeof(received_payload));
    bool registered = false; //kyknya bool registered buat tanda membangun jaringan (?)
    bool ok = false;
    Serial.println(received_payload.command);
    
    //change cluster head
    if ( received_payload.command == 10 ) { //nge cek cluster head
      if ( received_payload.node_id == this_node_id ) { //brarti kejangkau semua node nya ya? yg di kirim packet nya oleh base station.
        discovered = true;
        this_node = received_payload.data; //ini address yang dikasih dari base station.
        if ( this_node == 1 || this_node == 2 ) {
          is_cluster_head = true; //jika address nya 1 atau 2, maka dia jadi cluster head.
//          leach_already_ch = true;
          Serial.println("This is Cluster Head");
        }
        else {
          is_cluster_head = false;
        }
        Serial.print("This new address is: ");
        Serial.println(this_node, OCT);
        network.begin(/*channel*/ 90, /*node address*/ this_node); //akan diulang lg network.begin dengan address yang baru. Nah disini pertanyaan (apakah diawal boleh langsung diinisialisasi address nya di setiap node?)
      }
    }
    else if ( received_payload.command == 150 && packets_sent > 3 ) { //cluster head switch
      Serial.println("Changing cluster head.");
      Serial.print("0");
      Serial.print(received_payload.data, OCT);
      Serial.println(" 170 ");
      while (!ok) {
        payload_t payload = { 170, this_node_id, /*use this CH as an address*/this_node, 0, false }; //170 buat ngasih tau ke node lain buat jadi cluster head(?)
        RF24NetworkHeader header(/*to node*/ received_payload.data); //ngirim address nya ke node lain dengan header, brarti header membawa ke alamat yg dituju (penting untuk membuat jadi cluster head(?)
        ok = network.write(header,&payload,sizeof(payload));
        delay(20);
      }
      this_node = received_payload.data;
      packets_sent = 0;
      leach_rounds++; //round akan bertambah ketika cluster head telah di switch.
      is_cluster_head = false;
      network.begin(/*channel*/ 90, /*node address*/ this_node); //update ke address baru yg sbeelomnya 1 atau 2 itu address CH
    }
    else if ( received_payload.command == 170 && packets_sent > 3  ) { //170 untuk command ganti jd cluster head
      Serial.println("ok. now this node is a cluster head.");
//      leach_already_ch = true;
      this_node = received_payload.data; //tentu jadi address CH
      packets_sent = 0;
      leach_rounds++;
      is_cluster_head = true;
      network.begin(/*channel*/ 90, /*node address*/ this_node);
    }
    else if ( received_payload.command == 200 && packets_sent > 3  ) {
      Serial.println("ok. reset.");
      packets_sent = 0;
      leach_rounds++;
    }
    else if ( received_payload.command == 210 && packets_sent > 3 ) { //buat apa?
      Serial.println("continue. no leach increment.");
      packets_sent = 8;
    }
  }
  
  //========== Sending  ==========//
  if ( !discovered ) { //agar ke discover dengan base stationnya(?)
    payload_t payload = { 10, this_node_id, 0, 0, false };
    RF24NetworkHeader header(/*to node*/ base_station_node); //header brarti emg berisi address tujuan node.
    bool ok = network.write(header,&payload,sizeof(payload));
    if (ok) {
      Serial.println("sending discovery... ok");
    } else {
      Serial.println("sending discovery... failed");
    }
  }
  
  unsigned long now = millis();              // If it's time to send a message, send it!
  if ( now - last_sent >= interval || ( sleep_count > 7 && !is_cluster_head ) ) { //interval nya 2 sekon , dan bukan CH
    unsigned long transmission_time = millis();
    last_sent = now;
    Serial.print("Sending...");
    payload_t payload = { 0, this_node_id, packets_sent, avg_current, true }; //mulai pengiriman data arus
    RF24NetworkHeader header(/*to node*/ base_station_node); //klo bukan cluster head kok ke base station ya ngirimnya?
    bool ok = network.write(header,&payload,sizeof(payload));
    sleep_count = 0;
    if (ok) {
      packets_sent++;
      Serial.print("ok, transmission time: ");
      transmission_time = millis() - transmission_time; //brarti ini delay transmisi
      Serial.print(transmission_time);
      Serial.println("ms");
    }
    else {
      Serial.println("failed.");
    }

    //reset if all nodes already a CH
//    if ( leach_rounds % leach_percentage_simplified == 0 && packets_sent > 10 && leach_rounds > 1) {
//      Serial.println("reset leach_already_ch to false");
//      leach_already_ch = false;
//    }
    //Node ask to be a cluster head if already sent 10 or more packets
    if (packets_sent > 10 && is_cluster_head == false && packets_sent % 5 == 1) {
      Serial.println("ask for cluster head role");
      payload_t payload = {/*command for asking to be a cluster head*/ 100, this_node_id, 0, avg_current, true };
      RF24NetworkHeader header(/*to node*/ base_station_node);
      ok = network.write(header,&payload,sizeof(payload));
    }
  }

  /***************************** CALLING THE NEW SLEEP FUNCTION ************************/    
 
  if ( !is_cluster_head && packets_sent < 10 ) {  // Want to make sure the Arduino stays awake for a little while when data comes in. Do NOT sleep if master node.
    sleepTimer = millis();
    sleep_count++;
    Serial.println("Sleep");                           // Reset the timer value
    delay(100);                                      // Give the Serial print some time to finish up
    radio.stopListening();                           // Switch to PTX mode. Payloads will be seen as ACK payloads, and the radio will wake up
    network.sleepNode(1,255);                          // Sleep the node for 8 cycles of 1second intervals
    Serial.println("Awake"); 
  }

  if ( millis() - last_sampling > sampling_rate || !is_cluster_head && packets_sent < 10 ) { //sampling data sensor arus, oh sample nya ini buat jdiin average, jadi buat waktunya)
    last_sampling = millis();
    sample_count++;
    avg_current = avg_current * (sample_count - 1) / sample_count + (ina219.getCurrent_mA() / sample_count);
//    Serial.println(sample_count);
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

//Catatan Coding dr Base Station
//1. Base station hanya ngirim command 150 ke node yg sebelom nya jd cluster head
//2. yg dapat ngejadiin cluster head itu ya tetep node nya sndiri setelah dia menerima 150, dia akan ngirim 170 ke node lain untk jd cluster head.

//THE BIGGEST QUESTION
//Kenapa anggota Cluster ngirim data nya gk ke CH tapi ke base station? guna CH buat apaan dong disini? Bukannya anggota cluster ngirim data nya ke Cluster Head ya?
