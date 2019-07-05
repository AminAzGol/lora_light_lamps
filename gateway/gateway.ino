/*
  LoRa lighting system Gateway
  Author: Mohammad Amin Alizadeh Golestani
*/
#include <SPI.h> // include libraries
#include <LoRa.h>
enum State { server, ack, try_again };
byte gateway_id = 0x01;
byte local_address = 0x01; // address of this device
const int max_packet_id = 10;
const byte max_try_number = 3;
const byte direction_count = 2;
const int timeout = 2000; //timeout for ack of a packet
const String reset_command = "packet_numbers_reset";

byte packet_id = 0;
byte broadcast_address = 0xFF;

State state = server;
byte node_id;
String command;
int current_time;
int current_timeout;
int current_try = 0;


bool serial_log = true;

const int csPin = 10;      // LoRa radio chip select
const int resetPin = 9;    // LoRa radio reset
const int irqPin = 2;      // change for your board; must be a hardware interrupt pin

void setup()
{
  Serial.begin(9600); // initialize serial
  while (!Serial);

  log("LoRa lighting system Gateway");

  // override the default CS, reset, and IRQ pins (optional)
  LoRa.setPins(csPin, resetPin, irqPin); // set CS, reset, IRQ pin

  if (!LoRa.begin(433E6))
  { // initialize ratio at 433 MHz
    log("LoRa init failed. Check your connections.");
    while (true)
      ; // if failed, do nothing
  }

  log("LoRa init succeeded.");
}

void loop()
{
  // parse for a packet, and call onReceive with the result:
  if (state == server)
  {
    //wait for server command

    //for debug------------------

    for(int i=1;  i < 5; i++){

    LoRa_transmit(i, packet_id, 0, 0, "turn_on");
        update_packet_id();
  
    delay(150);
    LoRa_transmit(i, packet_id, 0, 0, "turn_off");
        update_packet_id();

    delay(150);
    }
    return;


    //debug end-----------------------

    if (Serial.available() > 0)
    {
      node_id = Serial.read();
      node_id = node_id - '0';
      command = Serial.readString();
      log("node_id: " + String(node_id, DEC));
      log("command: " + command);
      current_try = 0;
      LoRa_transmit(node_id, packet_id, 0, current_try, command);
  
      current_time = millis();
      current_timeout = current_time + timeout;
      
      change_state(State::ack);
    }
  }
  else if(state == ack){
    
      //wait for ack
      if(onReceive(LoRa.parsePacket())){
        update_packet_id();
        server_response("Node " + String(node_id,DEC) + " status: OK");
        change_state(State::server);
      } 
      else if(current_time <= current_timeout){
        current_time = millis();
      }
      else{
        //Timeout happend
        /* 
        log("change state to Try_again");
        change_state(State::try_again);
        */

       /* if timout happen the packet id still needs to get updated */
        log("Timeout");
        update_packet_id();
        change_state(State::server);
      }
  }
  else if (state == try_again){
    if(current_try < max_try_number){
      current_try++;
      LoRa_transmit(node_id, packet_id, 0, current_try, command);
      current_time = millis();
      current_timeout = current_time + timeout;
      change_state(State::ack);
    }
    else{
      server_response("sending command failed \n Node: " + String(node_id,DEC) + " is not accessable");
      update_packet_id();
      change_state(State::server);
    }
  }
}

void change_state(State new_state){
  log("state changed : " + String(new_state, DEC));
  state = new_state;
}

void LoRa_transmit(byte destination, byte packet_id, byte direction, byte try_count, String outgoing)
{
  log("---------------new packet--------------");  
  LoRa.beginPacket();            // start packet
  LoRa.write(gateway_id);        // add current zone gateway id
  log("gateway id : "+String(gateway_id));
  LoRa.write(destination);       // add destination address
  log("destination : "+String(destination));  
  LoRa.write(local_address);     // add sender address
  log("local_address : "+String(local_address)); 
  LoRa.write(packet_id);         // add message ID
  log("packet id : "+String(packet_id));  
  LoRa.write(direction);         // add direction could be 0 or 1
  log("direction : "+String(direction));   
  LoRa.write(try_count);         // add the count of gateway tryings
  log("try_count : "+String(try_count));   
  LoRa.write(outgoing.length()); // add payload length
  log("payload length : "+String(outgoing.length()));   
  LoRa.print(outgoing);          // add payload
  log("payload : "+String(outgoing)); 
  log("---------------------------------------");  
  LoRa.endPacket();              // finish packet and send it
  
  log("Packet transmited");
}
void update_packet_id(){
  packet_id++;
  if(packet_id >= max_packet_id)
    reset_packet_id();
}
bool onReceive(int packetSize)
{
  if (packetSize == 0)
    false; // if there's no packet, return

  // read packet header bytes:
  byte incoming_gateway_id = LoRa.read();
  int recipient = LoRa.read();           // recipient address
  byte sender = LoRa.read();             // sender address
  byte incoming_packet_id = LoRa.read(); // incoming msg ID
  byte direction = LoRa.read();          // packet direction
  byte try_count = LoRa.read();          // gateway tryings count
  byte incomming_lenght = LoRa.read();   // incoming msg length

  String incoming = "";

  while (LoRa.available())
  {
    incoming += (char)LoRa.read();
  }

  if (incomming_lenght != incoming.length())
  { // check length for error
    // log("error: message length does not match length");
    // log(String(recipient,DEC)+incoming);
    return false; // skip rest of function
  }

  log("Received from: 0x" + String(sender, HEX));
  log("Sent to: 0x" + String(recipient, HEX));
  log("Message ID: " + String(incoming_packet_id));
  log("Message length: " + String(incomming_lenght));
  log("Message: " + incoming);
  log("RSSI: " + String(LoRa.packetRssi()));
  log("Snr: " + String(LoRa.packetSnr()));
  log("");

  if(recipient != node_id){
    log(" this message is not this node ack ");
    return false;
  }

  if(direction == 0){
    // log(" this is a forward (direction = 0) msg ");
    log(" a repeat heard ");
    return false;
  }
  
  if(incoming != "ack"){
    log(" everything is fine but the message is not ack ");
    return false;
  }

  return true;
}

void log(String msg)
{
  if (serial_log)
    Serial.println(msg);
}

void server_response(String msg){
  // log("updating server status");
  // Serial.print("node " + String(node_id,DEC));
  // Serial.println("status: " + msg);
  Serial.println(msg);
}

void reset_packet_id(){
  packet_id = 0;
  LoRa_transmit(0xFF, max_packet_id, 1, 0, reset_command);
}