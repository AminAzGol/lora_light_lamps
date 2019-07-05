/*
  LoRa lighting system Node
  Author: Mohammad Amin Alizadeh Golestani
*/
#include <SPI.h>              // include libraries
#include <LoRa.h>
#define LAMP_PIN_NUMBER 7
#define LED_PIN_NUMBER_1 6
#define LED_PIN_NUMBER_2 5

byte gateway_id = 0x01;
byte local_address = 0x02;     // address of this device
const int max_packet_id = 128;
const byte max_try_number = 3;
const byte direction_count = 2;

 /*array to check if a packet is duplicate [packet_id][try][direction]*/
bool visited_packets[max_packet_id][max_try_number][direction_count] = { 0 };
byte broadcast_address = 0xFF;

bool serial_log = true;
byte waite_time_base = 2;
byte wait_time_range = 10;     // time to wait befor repeat (millis)
const int csPin = 10;          // LoRa radio chip select
const int resetPin = 9;       // LoRa radio reset
const int irqPin = 2;         // change for your board; must be a hardware interrupt pin
int delay_holder = 0;
bool blink_data_to_write = 0;
void setup() { 
  Serial.begin(9600);                   // initialize serial
  while (!Serial);
  
  log("LoRa lighting system Node");

  pinMode(LAMP_PIN_NUMBER,OUTPUT);
  pinMode(LED_PIN_NUMBER_1,OUTPUT);
  pinMode(LED_PIN_NUMBER_2,OUTPUT);

  // override the default CS, reset, and IRQ pins (optional)
  LoRa.setPins(csPin, resetPin, irqPin);// set CS, reset, IRQ pin

  if (!LoRa.begin(433E6)) {             // initialize ratio at 433 MHz
    log("LoRa init failed. Check your connections.");
    while (true);                       // if failed, do nothing
  }

  Serial.println("LoRa init succeeded.");
}

void loop() {
  // parse for a packet, and call onReceive with the result:
  onReceive(LoRa.parsePacket());
  // led_blink_no_delay();
  
}

void led_blink_no_delay(){
  if(delay_holder == 0)
    delay_holder = millis();
  int now = millis();  
  if (now - delay_holder > 200){
    delay_holder = millis();
    blink_data_to_write = !blink_data_to_write;
    digitalWrite(LED_PIN_NUMBER_1, blink_data_to_write);

  }

}

void LoRa_transmit(byte destination, byte packet_id, byte direction, byte try_count, String outgoing) {
  LoRa.beginPacket();                   // start packet
  LoRa.write(gateway_id);               // add current zone gateway id
  LoRa.write(destination);              // add destination address
  LoRa.write(local_address);            // add sender address
  LoRa.write(packet_id);                // add message ID
  LoRa.write(direction);                // add direction could be 0 or 1
  LoRa.write(try_count);                // add the count of gateway tryings
  LoRa.write(outgoing.length());        // add payload length
  LoRa.print(outgoing);                 // add payload
  LoRa.endPacket();                     // finish packet and send it
  visited_packets[packet_id][try_count][direction] = 1;    // store this packet as visited for check duplicate
  log("Packet transmited");

}

void onReceive(int packetSize) {
  if (packetSize == 0) return;          // if there's no packet, return

  // read packet header bytes:
  digitalWrite(LED_PIN_NUMBER_1, HIGH);
  byte incoming_gateway_id = LoRa.read();
  int recipient = LoRa.read();          // recipient address
  byte sender = LoRa.read();            // sender address
  byte incoming_packet_id = LoRa.read();     // incoming msg ID
  byte direction = LoRa.read();         // packet direction
  byte try_count = LoRa.read();         // gateway tryings count
  byte incomming_lenght = LoRa.read();    // incoming msg length

  String incoming = "";
  log("");
  log("Received from: 0x" + String(sender, HEX));
  log("Sent to: 0x" + String(recipient, HEX));
  log("Message ID: " + String(incoming_packet_id));
  log("Message length: " + String(incomming_lenght));
  log("RSSI: " + String(LoRa.packetRssi()));
  log("Snr: " + String(LoRa.packetSnr()));
  
  while (LoRa.available()) {
    incoming += (char)LoRa.read();
  }

  if (incomming_lenght != incoming.length()) {   // check length for error
    // log("error: message length does not match length");
    return;                             // skip rest of function
  }

  log("Message: " + incoming);
  

  /*check if this msg is from this node gateway*/
  if(incoming_gateway_id != gateway_id){
    log("This message gateway id doesn't matach mine.");
  }

  /*check if packet id is more than max*/
  else if( incoming_packet_id >= max_packet_id){
    log("This message packet id is more than max");
    LoRa_transmit(recipient, incoming_packet_id, 1, 0, "ERROR: packet id is more than max");
    
  } 

  /*check if this I recieved this msg before or not*/
  else if(duplicate(incoming_packet_id, try_count, direction)){
    log("This message is duplicate I received it before");
    
  }
  
  // if the recipient isn't this device,
  else if (recipient != local_address) {
    log("This message is not for me.");
    repeat(recipient, incoming_packet_id, direction, try_count, incoming);
    
  }
  
  // if the message is a broadcast command
  else if(recipient == broadcast_address){
    log("this message is a global command");
    run_command(incoming);
    repeat(recipient, incoming_packet_id, direction, try_count, incoming);
    
  }

  // if this message is just for this device
  else if(recipient == local_address){
    run_command(incoming);
    acknowledge(recipient, incoming_packet_id);
    
  }
  digitalWrite(LED_PIN_NUMBER_1, LOW);
}

void log(String msg){
  if (serial_log)
  Serial.println(msg);
}

bool duplicate(byte packet_id,byte try_count, byte direction){
  return visited_packets[packet_id][try_count][direction];
}
void repeat(byte destination, byte msg_id, byte driection, byte try_count,String outgoing) {
  byte time_to_wait = rand() * wait_time_range + waite_time_base;
  log("repeating the message after: " + String(time_to_wait, DEC) );
  delay(time_to_wait);
  LoRa_transmit(destination, msg_id, driection, try_count, outgoing);
}

void run_command(String incoming){
  log("running the incomming Command" + incoming);
  if( incoming == "turn_on"){
    digitalWrite(LAMP_PIN_NUMBER, HIGH);
    
    /* also turn one LED for debugging */
    digitalWrite(LED_PIN_NUMBER_2, HIGH);
  }

  else if( incoming  == "turn_off" ){
    digitalWrite(LAMP_PIN_NUMBER, LOW);

    /* Turn off debug LED */
    digitalWrite(LED_PIN_NUMBER_2, LOW);
  }

  else if( incoming == "packet_numbers_reset")
    clear_visited();
}

void acknowledge(byte destination, byte msg_id){
  LoRa_transmit(destination, msg_id, 1, 0, "ack");
}

void clear_visited(){
  for (int i = 0; i < max_packet_id;i++)
    for(byte j=0;j< max_try_number; j++)
      for(byte k=0; k<direction_count; k++) 
        visited_packets[i][j][k] = 0;
}