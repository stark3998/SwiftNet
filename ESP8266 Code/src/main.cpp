#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

const char ssid[] = "VIT2.4G";  // your network SSID (name)
const char pass[] = "16BCE0943"; // your network password

#define VERSIONNUMBER 28
#define PIN 0

#define LOGGERIPINC 20
#define SWARMSIZE 6
#define SWARMTOOOLD 30000

enum PacketType {
  LIGHT_UPDATE_PACKET = 0,
  RESET_SWARM_PACKET,
  CHANGE_TEST_PACKET,
  RESET_ME_PACKET,
  DEFINE_SERVER_LOGGER_PACKET,
  LOG_TO_SERVER_PACKET,
  MASTER_CHANGE_PACKET,
  BLINK_BRIGHT_LED
};

class LightSwarm {
public:
  LightSwarm();
  void setup();
  void loop();

private:
  void colorWipe(uint32_t c, uint8_t wait, int bright);
  void handlePacket();
  void handleLightUpdatePacket();
  void handleResetSwarmPacket();
  void handleChangeTestPacket();
  void handleResetMePacket();
  void handleDefineServerLoggerPacket();
  void handleBlinkBrightLedPacket();
  void sendLightUpdatePacket(IPAddress &address);
  void broadcastARandomUpdatePacket();
  void checkAndSetIfMaster();
  int setAndReturnMySwarmIndex(int incomingID);
  void sendLogToServer();

  unsigned int localPort = 2910; // local port to listen for UDP packets
  boolean masterState = true; // True if master, False if not
  int swarmClear[SWARMSIZE];
  int swarmVersion[SWARMSIZE];
  int swarmState[SWARMSIZE];
  long swarmTimeStamp[SWARMSIZE]; // for aging
  IPAddress serverAddress = IPAddress(0, 0, 0, 0); // default no IP Address
  int swarmAddresses[SWARMSIZE]; // Swarm addresses
  int clearColor;
  int redColor;
  int blueColor;
  int greenColor;
  const int PACKET_SIZE = 14; // Light Update Packet
  static const int BUFFERSIZE = 1024;
  byte packetBuffer[BUFFERSIZE]; // buffer to hold incoming and outgoing packets
  WiFiUDP udp;
  Adafruit_NeoPixel strip = Adafruit_NeoPixel(8, PIN, NEO_GRB + NEO_KHZ800);
  IPAddress localIP;
};

LightSwarm::LightSwarm() {
  for (int i = 0; i < SWARMSIZE; i++) {
    swarmAddresses[i] = 0;
    swarmClear[i] = 0;
    swarmTimeStamp[i] = -1;
  }
}

void LightSwarm::setup() {
  Serial.begin(115200);
  Serial.println("\n\n--------------------------");
  Serial.println("LightSwarm");
  Serial.print("Version ");
  Serial.println(VERSIONNUMBER);
  Serial.println("--------------------------");
  Serial.println(F(" 09/03/2015"));
  Serial.print(F("Compiled at:"));
  Serial.print(F(__TIME__));
  Serial.print(F(" "));
  Serial.println(F(__DATE__));
  Serial.println();

  pinMode(A0, INPUT);
  pinMode(14, OUTPUT);
  pinMode(16, OUTPUT);
  randomSeed(analogRead(A0));

  Serial.print("Analog read from A0: ");
  Serial.println(analogRead(A0));

  Serial.print("Connecting to SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());

  swarmClear[0] = 0;
  swarmTimeStamp[0] = 1;
  clearColor = swarmClear[0];
  swarmVersion[0] = VERSIONNUMBER;
  swarmState[0] = masterState;

  localIP = WiFi.localIP();
  swarmAddresses[0] = localIP[3];
}

void LightSwarm::loop() {
  clearColor = analogRead(A0);
  Serial.print("\nPhotoresistor value: ");
  Serial.println(clearColor);
  int brightness = map(clearColor, 0, 1023, 0, 255);
  analogWrite(14, brightness);
  delay(5);

  int bright = map(clearColor, 0, 1023, 1, 8);
  Serial.print("LED brightness level: ");
  Serial.println(bright);
  colorWipe(strip.Color(10, 10, 10), 50, bright);

  swarmClear[0] = clearColor;

  delay(100);

  int cb = udp.parsePacket();
  if (cb) {
    udp.read(packetBuffer, PACKET_SIZE);
    handlePacket();
  } else {
    Serial.print(".");
  }

  checkAndSetIfMaster();
  broadcastARandomUpdatePacket();
  sendLogToServer();
}

void LightSwarm::colorWipe(uint32_t c, uint8_t wait, int bright) {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, i < bright ? c : 0);
    strip.show();
    delay(wait);
  }
}

void LightSwarm::handlePacket() {
  Serial.println("Packet received:");
  switch (packetBuffer[1]) {
    case LIGHT_UPDATE_PACKET:
      Serial.println("Handling LIGHT_UPDATE_PACKET");
      handleLightUpdatePacket();
      break;
    case RESET_SWARM_PACKET:
      Serial.println("Handling RESET_SWARM_PACKET");
      handleResetSwarmPacket();
      break;
    case CHANGE_TEST_PACKET:
      Serial.println("Handling CHANGE_TEST_PACKET");
      handleChangeTestPacket();
      break;
    case RESET_ME_PACKET:
      Serial.println("Handling RESET_ME_PACKET");
      handleResetMePacket();
      break;
    case DEFINE_SERVER_LOGGER_PACKET:
      Serial.println("Handling DEFINE_SERVER_LOGGER_PACKET");
      handleDefineServerLoggerPacket();
      break;
    case BLINK_BRIGHT_LED:
      Serial.println("Handling BLINK_BRIGHT_LED");
      handleBlinkBrightLedPacket();
      break;
    default:
      Serial.println("Unknown packet type");
      break;
  }
}

void LightSwarm::handleLightUpdatePacket() {
  int senderID = packetBuffer[2];
  int index = setAndReturnMySwarmIndex(senderID);
  swarmClear[index] = packetBuffer[5] * 256 + packetBuffer[6];
  swarmVersion[index] = packetBuffer[4];
  swarmState[index] = packetBuffer[3];
  swarmTimeStamp[index] = millis();
  Serial.print("Light update from ID ");
  Serial.print(senderID);
  Serial.print(": clearColor=");
  Serial.print(swarmClear[index]);
  Serial.print(", version=");
  Serial.print(swarmVersion[index]);
  Serial.print(", state=");
  Serial.println(swarmState[index]);
}

void LightSwarm::handleResetSwarmPacket() {
  masterState = true;
  Serial.println("Reset Swarm: I just BECAME Master (and everybody else!)");
}

void LightSwarm::handleChangeTestPacket() {
  Serial.println("CHANGE_TEST_PACKET Packet Received");
  for (int i = 0; i < PACKET_SIZE; i++) {
    Serial.print("LPS[");
    Serial.print(i);
    Serial.print("] = ");
    Serial.println(packetBuffer[i], HEX);
  }
}

void LightSwarm::handleResetMePacket() {
  if (packetBuffer[2] == swarmAddresses[0]) {
    masterState = true;
    Serial.println("Reset Me: I just BECAME Master");
  } else {
    Serial.print("Reset Me: Wanted #");
    Serial.print(packetBuffer[2]);
    Serial.println(" Not me - reset ignored");
  }
}

void LightSwarm::handleDefineServerLoggerPacket() {
  serverAddress = IPAddress(packetBuffer[4], packetBuffer[5], packetBuffer[6], packetBuffer[7]);
  Serial.print("Server address received: ");
  Serial.println(serverAddress);
}

void LightSwarm::handleBlinkBrightLedPacket() {
  if (packetBuffer[2] == swarmAddresses[0]) {
    Serial.print("Blinking bright LED for ");
    Serial.print(packetBuffer[4] * 100);
    Serial.println(" milliseconds");
    delay(packetBuffer[4] * 100);
  } else {
    Serial.print("Blink Bright LED: Wanted #");
    Serial.print(packetBuffer[2]);
    Serial.println(" Not me - reset ignored");
  }
}

void LightSwarm::sendLightUpdatePacket(IPAddress &address) {
  memset(packetBuffer, 0, PACKET_SIZE);
  packetBuffer[0] = 0xF0;
  packetBuffer[1] = LIGHT_UPDATE_PACKET;
  packetBuffer[2] = localIP[3];
  packetBuffer[3] = masterState;
  packetBuffer[4] = VERSIONNUMBER;
  packetBuffer[5] = (clearColor & 0xFF00) >> 8;
  packetBuffer[6] = (clearColor & 0x00FF);
  packetBuffer[7] = (redColor & 0xFF00) >> 8;
  packetBuffer[8] = (redColor & 0x00FF);
  packetBuffer[9] = (greenColor & 0xFF00) >> 8;
  packetBuffer[10] = (greenColor & 0x00FF);
  packetBuffer[11] = (blueColor & 0xFF00) >> 8;
  packetBuffer[12] = (blueColor & 0x00FF);
  packetBuffer[13] = 0x0F;

  udp.beginPacketMulticast(address, localPort, WiFi.localIP());
  udp.write(packetBuffer, PACKET_SIZE);
  udp.endPacket();
  Serial.print("Sent light update packet to ");
  Serial.println(address);
}

void LightSwarm::broadcastARandomUpdatePacket() {
  IPAddress sendSwarmAddress(192, 168, 211, 255);
  sendLightUpdatePacket(sendSwarmAddress);
  Serial.println("Broadcasted a random update packet");
}

void LightSwarm::checkAndSetIfMaster() {
  boolean setMaster = true;
  for (int i = 0; i < SWARMSIZE; i++) {
    int howLongAgo = millis() - swarmTimeStamp[i];
    if (swarmTimeStamp[i] == 0 || swarmTimeStamp[i] == -1 || howLongAgo > SWARMTOOOLD) {
      swarmTimeStamp[i] = 0;
      swarmClear[i] = 0;
    }
    if (swarmClear[0] < swarmClear[i]) {
      setMaster = false;
      break;
    }
  }
  masterState = setMaster;
  swarmState[0] = masterState;
  Serial.print("Master state: ");
  Serial.println(masterState ? "True" : "False");
}

int LightSwarm::setAndReturnMySwarmIndex(int incomingID) {
  for (int i = 0; i < SWARMSIZE; i++) {
    if (swarmAddresses[i] == incomingID) {
      return i;
    } else if (swarmAddresses[i] == 0) {
      swarmAddresses[i] = incomingID;
      return i;
    }
  }

  int oldSwarmID = 0;
  long oldTime = millis();
  for (int i = 0; i < SWARMSIZE; i++) {
    if (oldTime > swarmTimeStamp[i]) {
      oldTime = swarmTimeStamp[i];
      oldSwarmID = i;
    }
  }

  swarmAddresses[oldSwarmID] = incomingID;
  return oldSwarmID;
}

void LightSwarm::sendLogToServer() {
  if (!masterState || (serverAddress[0] == 0 && serverAddress[1] == 0)) {
    return;
  }

  char myBuildString[1000] = {0};
  for (int i = 0; i < SWARMSIZE; i++) {
    char stateString[3] = "PR";
    if (swarmTimeStamp[i] == 0) {
      strcpy(stateString, "TO");
    } else if (swarmTimeStamp[i] == -1) {
      strcpy(stateString, "NP");
    }

    char swarmString[50];
    sprintf(swarmString, " %i,%i,%i,%i,%s,%i ", i, swarmState[i], swarmVersion[i], swarmClear[i], stateString, swarmAddresses[i]);
    strcat(myBuildString, swarmString);
    if (i < SWARMSIZE - 1) {
      strcat(myBuildString, "|");
    }
  }

  memset(packetBuffer, 0, BUFFERSIZE);
  packetBuffer[0] = 0xF0;
  packetBuffer[1] = LOG_TO_SERVER_PACKET;
  packetBuffer[2] = localIP[3];
  packetBuffer[3] = strlen(myBuildString);
  packetBuffer[4] = VERSIONNUMBER;
  strcpy((char *)(packetBuffer + 5), myBuildString);

  int packetLength = strlen(myBuildString) + 5 + 1;
  udp.beginPacket(serverAddress, localPort);
  udp.write(packetBuffer, packetLength);
  udp.endPacket();
  Serial.println("Sent log to server");
}

LightSwarm lightSwarm;

void setup() {
  lightSwarm.setup();
}

void loop() {
  lightSwarm.loop();
}
