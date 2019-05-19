#include <EtherCard.h>
#define TCP_FLAGS_FIN_V 1    //as declared in net.h
#define TCP_FLAGS_ACK_V 0x10 //as declared in net.h
static byte myip[] = {192, 168, 1, 200};
static byte gwip[] = {192, 168, 1, 1};
static byte mymac[] = {0x74, 0x69, 0x69, 0x2D, 0x30, 0x39};
byte Ethernet::buffer[700]; // tcp ip send and receive buffer

const char pageA[] PROGMEM =
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "\r\n"
    "<html>"
    "<head><title>"
    "multipackets Test"
    "</title></head>"
    "<body>"
    "<h3>LoRa Lighting System</h3>";

const char pageEnd[] PROGMEM =
"<h3>footer</h3>";



// String temp = "{{lamp}}HELISADLKHVIRO";
char ppp[200];
void setup()
{
  Serial.begin(9600);
  // Change 'SS' to your Slave Select pin, if you arn't using the default pin
  ether.begin(sizeof Ethernet::buffer, mymac, A0);
  ether.staticSetup(myip, gwip);
  Serial.println("[multipackets]");   

  Serial.println(ppp);
}

void loop()
{
  word pos = ether.packetLoop(ether.packetReceive());
  // check if valid tcp data is received
  if (pos)
  {
    char *data = (char *)Ethernet::buffer + pos;
    // Serial.println(data);
    char comm[] = "GET /?command=";
    if (strncmp(comm, data, 14) == 0)
    {
      int num = 0;
      char command[20];
      int p = 14;
      sscanf(&data[14], "%d%s", &num, command);
      Serial.println("Node: "+String(num, DEC) + " - Command: "+command);
    }

    ether.httpServerReplyAck();                       // send ack to the request
    memcpy_P(ether.tcpOffset(), pageA, sizeof pageA); // send first packet and not send the terminate flag
    ether.httpServerReply_with_flags(sizeof pageA - 1, TCP_FLAGS_ACK_V);

    for (int i = 1; i < 10; i++)
    {

      String button_template =
        "<h3>packet 2</h3>"
        "<p><em>"
        "lamp {{lamp}}"
        "</em></p>"
        "<a href='/?command={{node_on}}turn_on'>Turn_on</a><p>---</p>"
        "<a href='/?command={{node_off}}turn_off'>Turn_off</a><br>"
        "<hr>";

      button_template.replace("{{lamp}}", String(i,DEC));
      button_template.replace("{{node_off}}", String(i,DEC));
      button_template.replace("{{node_on}}", String(i,DEC));
      button_template.toCharArray(ppp, 200);
      strcpy(ether.tcpOffset(), ppp); // send first packet and not send the terminate flag
      ether.httpServerReply_with_flags(strlen(ppp) - 1, TCP_FLAGS_ACK_V);
    }
   memcpy_P(ether.tcpOffset(), pageEnd, sizeof pageEnd); // send first packet and not send the terminate flag
    ether.httpServerReply_with_flags(sizeof pageEnd - 1, TCP_FLAGS_ACK_V | TCP_FLAGS_FIN_V);
  }
}
