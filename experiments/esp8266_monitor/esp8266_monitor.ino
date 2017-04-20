/**
 * Takes a serial payload from the LoRa client (LoRa_Radiohead_RX.ino) 
 * and publishes it on various protocols.
 * 
 * To subscribe to the UDP data: listen for broadcasts on 239.0.0.57 port 12345
 * To subscribe to the MQTT data: mosquitto_sub -t solar
 * To subscribe to the websocket go to WiFi.localIP() port 80
 */

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include "config.h"
#include "SSD1306.h"
#include "TheapiPayloadGarden.h"
#include "html.h"

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;

// Multicast declarations
IPAddress ipMulti(239, 0, 0, 57);
unsigned int portMulti = 12345;      // local port to listen on

ESP8266WebServer server = ESP8266WebServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

// Initialize the OLED display using Wire library
SSD1306  display(0x3c, 4, 5);

TheapiPayloadGarden rx_payload = TheapiPayloadGarden();
uint8_t input_string[TheapiPayloadGarden_SIZE];
uint8_t payload_state = 0;
uint8_t serial_byte_count = 0;

unsigned long previousMillis = 0;
const long interval = 100;

const long ping_interval = 3000;
unsigned long ping_last = 0;

const char* mqtt_server = MQTT_SERVER;
uint16_t mqtt_port = MQTT_PORT;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

        // send message to client
        webSocket.sendTXT(num, "Connected");
      }
      break;
  }

}

void setup() {
  Serial.begin(115200);
  Serial.println();

  // Initialise the display.
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_16);

  rx_payload.setMsgId(254);
  rx_payload.setVcc(1234);
  rx_payload.setChargeMv(5678);
  
  display.clear();
  display.drawString(0, 20, String(rx_payload.getMsgId()));
  display.drawString(0, 45, String(rx_payload.getVcc()));
  display.drawString(80, 45, String(rx_payload.getChargeMv())); 
  display.display();

  
  WiFi.begin(ssid, password);
  Serial.print("\nConnecting to "); Serial.println(ssid);
  uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED && i++ < 20) delay(500);
  if(i == 21){
    Serial.print("Could not connect to"); Serial.println(ssid);
    while(1) delay(500);
  }

    // start webSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // handle index
  server.on("/", []() {
    // send index.html
    server.send(200, "text/html", index_html);
  });

  server.begin();

  mqtt_client.setServer(mqtt_server, mqtt_port);

  Serial.print("Web server on: ");
  Serial.println(WiFi.localIP());
  Serial.println("Ready! Listen for UDP broadcasts on 239.0.0.57 port 12345");
}

void loop() {
  webSocket.loop();
  server.handleClient();

  // Read the data from the LoRa receiver.
  while (Serial.available()) {
    // get the new byte:
    uint8_t in = (uint8_t) Serial.read();
    //Serial.println(in, HEX);
    if (payload_state == 0) {
      // Check for the start of the payload
      if (in == '\t') {
        payload_state = 1;
      }
    }
    
    else if (payload_state == 1) {
      //Serial.print(in, HEX);
      // add it to the inputString:
      input_string[serial_byte_count] = in;
      ++serial_byte_count;
      
      // if the the last byte is received, set a flag
      // so the main loop can do something about it:
      if (serial_byte_count == TheapiPayloadGarden_SIZE) {
        serial_byte_count = 0;
        payload_state = 2;
        rx_payload.unserialize(input_string);
//        Serial.print(rx_payload.getDeviceId()); Serial.print(", ");
//      Serial.print(rx_payload.getMsgId()); Serial.print(", ");
//      Serial.print(rx_payload.getA()); Serial.print(", ");
//      Serial.print(rx_payload.getB()); Serial.print(", ");
//      Serial.print(rx_payload.getC()); Serial.print(", ");
//      Serial.print(rx_payload.getD()); Serial.print(", ");
//      Serial.print(rx_payload.getE()); Serial.print(", ");
//      Serial.println(rx_payload.getF());
//      Serial.println();
      }
    } else {
      // Passthru other serial messages.
      //Serial.print(char(in));
      
    }
  }

  unsigned long currentMillis = millis();
  
  // Send payload to TCP clients when ready.
  if (payload_state == 2) {
    payload_state = 0;

    // No need to ping if we're sending real data.
    ping_last = currentMillis;
    broadcast_udp();
    broadcast_websocket();
    broadcast_mqtt();
  } 
  // Send the data continually, as its UDP some may get missed.
  else if (currentMillis - ping_last >= ping_interval) {
    ping_last = currentMillis;
    broadcast_udp();
    broadcast_websocket();
    broadcast_mqtt();
  }

  // Update the display  
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    display.clear();
  
    display.drawString(0, 0, String(currentMillis));
    
    display.drawString(0, 20, String(rx_payload.getMsgId()));
    display.drawString(35, 20, String(rx_payload.getVcc()));
    display.drawString(80, 20, String(rx_payload.getChargeMv())); 
  
    display.drawString(0, 45, WiFi.localIP().toString());
    
    display.display();
  }
  
}

void broadcast_udp() {
    size_t len = TheapiPayloadGarden_SIZE;
    uint8_t sbuf[len];
    rx_payload.serialize(sbuf);
    Udp.beginPacketMulticast(ipMulti, portMulti, WiFi.localIP());
    Udp.write('\t'); // Payload start byte
    Udp.write(sbuf, len);
    Udp.write('\n');
    Udp.endPacket();  
    Udp.stop();  
}

void broadcast_websocket() {
  String str = String(rx_payload.getMsgType()) + ",";
  str += String(rx_payload.getMsgId()) + ",";
  str += String(rx_payload.getVcc()) + ",";
  str += String(rx_payload.getChargeMv()) + ",";
  str += String(rx_payload.getChargeMa()) + ",";
  str += String(rx_payload.getLight()) + ",";
  str += String(rx_payload.getSoil()) + ",";
  str += String(rx_payload.getTemperature());
  webSocket.broadcastTXT(str);
}

void broadcast_mqtt() {
  char msg[50];
  String str = String(rx_payload.getMsgType()) + ",";
  str += String(rx_payload.getMsgId()) + ",";
  str += String(rx_payload.getVcc()) + ",";
  str += String(rx_payload.getChargeMv()) + ",";
  str += String(rx_payload.getChargeMa()) + ",";
  str += String(rx_payload.getLight()) + ",";
  str += String(rx_payload.getSoil()) + ",";
  str += String(rx_payload.getTemperature());
  str.toCharArray(msg, 50);
  if (mqtt_connect()) {
    mqtt_client.publish("solar", msg);
  }
}

bool mqtt_connect() {
  // Loop until we're reconnected
  if (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt_client.connect("SolarClient")) {
      Serial.println("connected");
      return true;
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      return true;
    }
  }
  return true;
}

