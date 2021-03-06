
#include "config.h"

#include "Payload.h"
#include "GardenPayload.h"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

theapi::GardenPayload rx_payload = theapi::GardenPayload();

WiFiUDP Udp;
unsigned int localUdpPort = 12345;  // local port to listen on
char incomingPacket[255];  // buffer for incoming packets
char  replyPacekt[] = "Hi there! Got the message :-)";  // a reply string to send back



// Multicast declarations
IPAddress ipMulti(239, 0, 0, 57);
unsigned int portMulti = 12345;      // local port to listen on

void setup() {
  Serial.begin(115200);
  Serial.println();

  
  WiFi.begin(ssid, password);
  Serial.print("\nConnecting to "); Serial.println(ssid);
  uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED && i++ < 20) delay(500);
  if(i == 21){
    Serial.print("Could not connect to"); Serial.println(ssid);
    while(1) delay(500);
  }

  Udp.beginMulticast(WiFi.localIP(), ipMulti, portMulti);
  Serial.printf("Now listening to IP %s, UDP port %d\n", ipMulti.toString().c_str(), localUdpPort);

}

void loop() {

  int packetSize = Udp.parsePacket();
  if (packetSize) {
    // receive incoming UDP packets
    Serial.printf("Received %d bytes from %s, port %d\n", packetSize, Udp.remoteIP().toString().c_str(), Udp.remotePort());
    int len = Udp.read(incomingPacket, 255);
    if (len > 0)  {
      incomingPacket[len] = 0;
    }
    Serial.printf("UDP packet contents: %s\n", incomingPacket);
    // Check for payload signifier;
    if (incomingPacket[0] == '\t') {
      uint8_t payload_buf[rx_payload.size()];
      memcpy(payload_buf, incomingPacket+1, rx_payload.size());
      rx_payload.unserialize(payload_buf);
      Serial.print("GARDEN: ");
      Serial.print(rx_payload.getMsgType()); Serial.print(", ");
      Serial.print(rx_payload.getMsgId()); Serial.print(", ");
      Serial.print(rx_payload.getVcc()); Serial.print(", ");
      Serial.print(rx_payload.getChargeMv()); Serial.print(", ");
      Serial.print(rx_payload.getChargeMa()); Serial.print(", ");
      Serial.print(rx_payload.getLight()); Serial.print(", ");
      Serial.print(rx_payload.getCpuTemperature()); Serial.print(", ");
      // Convert the temperature to a float.
      float deg = (float) rx_payload.getTemperature() / 10.0;
      Serial.println(deg);
      Serial.println();
    }
  }
  
}

