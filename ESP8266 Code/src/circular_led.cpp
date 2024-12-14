#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

const char ssid[] = "VIT2.4G";  // your network SSID (name)
const char pass[] = "16BCE0943"; // your network password

#define VERSIONNUMBER 28
#define PIN 2
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
    byte packetBuffer[LightSwarm::BUFFERSIZE]; //buffer to hold incoming and outgoing packets
    WiFiUDP udp;
    Adafruit_NeoPixel strip = Adafruit_NeoPixel(8, PIN, NEO_GRB + NEO_KHZ800);
    IPAddress localIP;
    int mySwarmID;
};

LightSwarm::LightSwarm() : mySwarmID(0) {}

void LightSwarm::setup() {
    Serial.begin(115200);
    Serial.println("\n\n--------------------------");
    Serial.println("LightSwarm");
    Serial.print("Version: ");
    Serial.println(VERSIONNUMBER);
    Serial.println("--------------------------");
    Serial.println(F("Date: 12/13/2024"));
    Serial.print(F("Compiled at: "));
    Serial.print(F(__TIME__));
    Serial.print(F(" "));
    Serial.println(F(__DATE__));
    Serial.println();

    pinMode(A0, INPUT);
    pinMode(16, OUTPUT);
    randomSeed(analogRead(A0));
    strip.begin();
    strip.show(); // Initialize all pixels to 'off'

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

    udp.begin(localPort);
    Serial.print("Local port: ");
    Serial.println(udp.localPort());

    for (int i = 0; i < SWARMSIZE; i++) {
        swarmAddresses[i] = 0;
        swarmClear[i] = 0;
        swarmTimeStamp[i] = -1;
    }
    swarmClear[mySwarmID] = 0;
    swarmTimeStamp[mySwarmID] = 1; // I am always in time to myself
    clearColor = swarmClear[mySwarmID];
    swarmVersion[mySwarmID] = VERSIONNUMBER;
    swarmState[mySwarmID] = masterState;

    localIP = WiFi.localIP();
    swarmAddresses[0] = localIP[3];
    mySwarmID = 0;
}

void LightSwarm::loop() {
    clearColor = analogRead(A0);
    int bright = map(clearColor, 0, 1023, 1, 8);
    colorWipe(strip.Color(10, 10, 10), 50, bright);

    swarmClear[mySwarmID] = clearColor;
    delay(100);

    int cb = udp.parsePacket();
    if (cb) {
        udp.read(packetBuffer, PACKET_SIZE);
        handlePacket();
    }

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
    switch (packetBuffer[1]) {
        case LIGHT_UPDATE_PACKET:
            handleLightUpdatePacket();
            break;
        case RESET_SWARM_PACKET:
            masterState = true;
            Serial.println("Received RESET_SWARM_PACKET");
            break;
        case CHANGE_TEST_PACKET:
            Serial.println("Received CHANGE_TEST_PACKET");
            break;
        case RESET_ME_PACKET:
            if (packetBuffer[2] == swarmAddresses[mySwarmID]) {
                masterState = true;
                Serial.println("Received RESET_ME_PACKET for this device");
            }
            break;
        case DEFINE_SERVER_LOGGER_PACKET:
            serverAddress = IPAddress(packetBuffer[4], packetBuffer[5], packetBuffer[6], packetBuffer[7]);
            Serial.print("Received DEFINE_SERVER_LOGGER_PACKET, Server Address: ");
            Serial.println(serverAddress);
            break;
        case BLINK_BRIGHT_LED:
            if (packetBuffer[2] == swarmAddresses[mySwarmID]) {
                delay(packetBuffer[4] * 100);
                Serial.println("Received BLINK_BRIGHT_LED for this device");
            }
            break;
        default:
            Serial.println("Unknown packet type received");
            break;
    }
}

void LightSwarm::handleLightUpdatePacket() {
    int swarmIndex = setAndReturnMySwarmIndex(packetBuffer[2]);
    swarmClear[swarmIndex] = packetBuffer[5] * 256 + packetBuffer[6];
    swarmVersion[swarmIndex] = packetBuffer[4];
    swarmState[swarmIndex] = packetBuffer[3];
    swarmTimeStamp[swarmIndex] = millis();
    checkAndSetIfMaster();
    Serial.println("Handled LIGHT_UPDATE_PACKET");
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
    Serial.print("Sent LIGHT_UPDATE_PACKET to ");
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
        if (swarmClear[mySwarmID] < swarmClear[i]) {
            setMaster = false;
            break;
        }
    }
    if (setMaster != masterState) {
        masterState = setMaster;
        digitalWrite(16, masterState ? LOW : HIGH);
        Serial.print("Master state changed to: ");
        Serial.println(masterState ? "Master" : "Not Master");
    }
    swarmState[mySwarmID] = masterState;
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

    udp.beginPacket(serverAddress, localPort);
    udp.write(packetBuffer, strlen(myBuildString) + 5);
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
