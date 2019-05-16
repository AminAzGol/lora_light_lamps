/*
  LoRa lighting system Node
  Author: Mohammad Amin Alizadeh Golestani
*/
#include <SPI.h>              // include libraries
#include <LoRa.h>
#define LAMP_PIN_NUMBER 7


byte gateway_id = 0x01;
byte local_address = 0x01;     // address of this device
const int max_packet_id = 128;
const byte max_try_number = 3;
const byte direction_count = 2;

 /*array to check if a packet is duplicate [packet_id][try][direction]*/
bool visited_packets[max_packet_id][max_try_number][direction_count] = { 0 };
byte broadcast_address = 0xFF;

bool serial_log = true;
byte waite_time_base = 2;
byte wait_time_range = 1;     // time to wait befor repeat (millis)
const int csPin = 10;          // LoRa radio chip select
const int resetPin = 9;       // LoRa radio reset
const int irqPin = 2;         // change for your board; must be a hardware interrupt pin


void setup() { 
  Serial.begin(9600);                   // initialize serial
  while (!Serial);
  
  log("LoRa lighting system Node");

  pinMode(LAMP_PIN_NUMBER,OUTPUT);
  // override the default CS, reset, and IRQ pins (optional)
  LoRa.setPins(csPin, resetPin, irqPin);// set CS, reset, IRQ pin

  if (!LoRa.begin(433E6)) {             // initialize ratio at 915 MHz
    log("LoRa init failed. Check your connections.");
    while (true);                       // if failed, do nothing
  }

  Serial.println("LoRa init succeeded.");
}

void loop() {
  // parse for a packet, and call onReceive with the result:
  onReceive(LoRa.parsePacket());
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
  byte incoming_gateway_id = LoRa.read();
  int recipient = LoRa.read();          // recipient address
  byte sender = LoRa.read();            // sender address
  byte incoming_packet_id = LoRa.read();     // incoming msg ID
  byte direction = LoRa.read();         // packet direction
  byte try_count = LoRa.read();         // gateway tryings count
  byte incomming_lenght = LoRa.read();    // incoming msg length

  String incoming = "";

  while (LoRa.available()) {
    incoming += (char)LoRa.read();
  }

  if (incomming_lenght != incoming.length()) {   // check length for error
    // log("error: message length does not match length");
    return;                             // skip rest of function
  }

  log("");
  log("Received from: 0x" + String(sender, HEX));
  log("Sent to: 0x" + String(recipient, HEX));
  log("Message ID: " + String(incoming_packet_id));
  log("Message length: " + String(incomming_lenght));
  log("Message: " + incoming);
  log("RSSI: " + String(LoRa.packetRssi()));
  log("Snr: " + String(LoRa.packetSnr()));
  
  /*check if packet id is more than max*/
  if( incoming_packet_id >= max_packet_id){
    log("This message packet id is more than max");
    LoRa_transmit(recipient, incoming_packet_id, 1, 0, "ERROR: packet id is more than max");
  } 
  /*check if this msg is from this node gateway*/
  if(incoming_gateway_id != gateway_id){
    log("This message gateway id doesn't matach mine.");
    return;
  }

  /*check if this I recieved this msg before or not*/
  if(duplicate(incoming_packet_id, try_count, direction)){
    log("This message is duplicate I received it before");
    return;
  }
  
  // if the recipient isn't this device,
  if (recipient != local_address) {
    log("This message is not for me.");
    repeat(recipient, incoming_packet_id, direction, try_count, incoming);
    return;
  }
  
  // if the message is a broadcast command
  if(recipient == broadcast_address){
    log("this message is a global command");
    run_command(incoming);
    repeat(recipient, incoming_packet_id, direction, try_count, incoming);
    return;
  }

  // if this message is just for this device
  if(recipient == local_address){
    run_command(incoming);
    acknowledge(recipient, incoming_packet_id);
    return;
  }

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
  if( incoming == "turn_on")
    digitalWrite(LAMP_PIN_NUMBER, HIGH);

  else if( incoming  == "turn_off" )
    digitalWrite(LAMP_PIN_NUMBER, LOW);

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