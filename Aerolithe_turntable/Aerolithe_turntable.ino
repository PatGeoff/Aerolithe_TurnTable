// Projet Final pour la table tournante de Aerolithe
// Ce programme est pour faire fonctionner un ESP32 WaveShare Servo avec écran
// https://www.waveshare.com/product/modules/motors-servos/drivers/servo-driver-with-esp32.html
// Board ESP32 Dev Module

#include <WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SCServo.h>

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 32  // OLED display height, in pixels

const char* ssid = "Aerolithe";
const char* password = "Galactium2013";
const int ID = 1;
const int udpPort = 44466;  // Change as needed.   44477 for scissor lift, 44466 for the turntable
//const char* windows_hostname = "WinImacProPat";  // À changer au nom du pc qui sera utilisé
const IPAddress windows_IP(192, 168, 2, 4);  // À cause de Parallels Desktop, c'est l'adresse de Windows en mode Bridged Wifi et elle peut changer alors vérifier
unsigned int senderPort = 55544;             // Port of the remote coputer sending messages, initializes to 0;

SMS_STS st;
// the uart used to control servos.
// GPIO 18 - S_RXD, GPIO 19 - S_TXD, as default.
#define S_RXD 18
#define S_TXD 19

// nombre de servo connectés au ESP32

WiFiUDP udp;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Static IP configuration
IPAddress local_IP(192, 168, 2, 12);  // "192,168,2,12" for turntable
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 255, 0);

unsigned long previousMillis = 0;  // Stores the last time the display was updated
const long interval = 3000;        // Interval to display packet data (3 seconds)
bool displayPacket = false;        // Flag to indicate if packet data should be displayed
const long sendPosInterval = 500;
double previousPos = 0;
double currentPos = 0;

enum ServoState {
  IDLE,
  MOVING
};

ServoState servoState = IDLE;
double targetPosition = 0;
int servoID = 1;

void displayData(char packetData[]);
void displayStatus();
void analizePacket(int packetSize);
void analizeMessage(char packetData[]);
bool isServoAtPosition(int servoID, double targetPosition, double tolerance = 5.0) {
  double currentPosition = st.ReadPos(servoID);
  return abs(currentPosition - targetPosition) <= tolerance;
}


void setup() {
  Serial.begin(115200);
  // Initialize the display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  display.display();
  Serial1.begin(1000000, SERIAL_8N1, S_RXD, S_TXD);
  st.pSerial = &Serial1;
  delay(2000);  // Pause for 2 seconds

  // Configure static IP
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }
  // Connect to WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting...");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Tentative connexion au WIFI");
    display.print("Aerolithe");
    display.display();
  }
  Serial.println("Connected to WiFi");

  // Begin UDP
  udp.begin(udpPort);
  Serial.println("UDP server started");
}


void loop() {
  unsigned long currentMillis = millis();

  // Check for UDP packets
  int packetSize = udp.parsePacket();
  if (packetSize) {
    // Receive incoming UDP packet
    analizePacket(packetSize);
    previousMillis = currentMillis;  // Reset the timer
    displayPacket = true;            // Set flag to display packet data
  }

  // Check if it's time to switch back to WiFi status display
  if (displayPacket && currentMillis - previousMillis >= interval) {
    displayPacket = false;  // Reset the flag
  }


  // Display the appropriate information
  if (displayPacket) {
    // Packet data is already displayed in analizePacket
  } else {
    displayStatus();
  }

  // Non-blocking servo position check
  if (servoState == MOVING) {
    double currentPosition = st.ReadPos(servoID);
    //Serial.print("Current Position: ");
    //Serial.println(currentPosition);
    if (isServoAtPosition(servoID, targetPosition)) {
      //Serial.println("Servo has reached the desired position!");
      //sendResponse("Message de Table Tournante: Position atteinte");
      servoState = IDLE;  // Change state to IDLE
    }
    
    // if (currentPosition != previousPos) {
    //   String message = "waveshare -> position," + String((int)currentPosition);
    //   const char* cMessage = message.c_str();
    //   sendResponse(cMessage);      
    // }
    previousPos = currentPosition;
    delay(100);  // Add a small delay to allow the servo to move
  }
}



void displayData(char packetData[]) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Received:");
  display.println(packetData);
  display.display();
}

void displayStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("Wifi: Aerolithe (");
    display.print(WiFi.localIP());
    display.print(") UDP: ");
    display.print((String)udpPort);
    display.print("Servo ID: ");
    display.display();
  }
}

void analizePacket(int packetSize) {
  char packetData[255];
  udp.read(packetData, packetSize);
  packetData[packetSize] = '\0';  // Null terminate the string
  Serial.print("Received packet: ");
  Serial.println(packetData);
  displayData(packetData);
  analizeMessage(packetData);
}


void analizeMessage(char packetData[]) {
  // Check if the message is "status"
  if (strcmp(packetData, "status") == 0) {
    sendResponse("waveshare --> status ok");
    return;
  }

  if (strncmp(packetData, "Aerolithe_Asks_GetPosition", 25) == 0) {
    double speed = st.ReadPos(servoID);
    String message = "waveshare -> position," + String((int)speed);
    const char* cMessage = message.c_str();
    sendResponse(cMessage);
    return;
  } else {
    //Serial.println("UDP -> Error: Invalid stepmotor command format.");
  }

  // Assuming the message format is "turntable,position,speed"
  char* token = strtok(packetData, ",");
  if (token != NULL && strcmp(token, "turntable") == 0) {
    token = strtok(NULL, ",");
    if (token != NULL) {
      targetPosition = atof(token);
      token = strtok(NULL, ",");
      if (token != NULL) {
        int speed = atoi(token);
        Serial.print("Position: ");
        Serial.print(targetPosition);
        Serial.print(" Speed: ");
        Serial.println(speed);
        st.WritePosEx(servoID, targetPosition, speed, 50);  // Use the parsed speed value
        servoState = MOVING;                                // Change state to MOVING
      }
    }
  }
}




void sendResponse(const char* message) {
  Serial.print("Sending response: ");
  Serial.print(message);
  Serial.print(" on port ");
  Serial.println(senderPort);

  udp.beginPacket(windows_IP, senderPort);
  udp.write((const uint8_t*)message, strlen(message));  // Correct conversion and length

  int result = udp.endPacket();
  if (result == 1) {
    Serial.println("Packet sent successfully");
  } else {
    Serial.print("Error sending packet, result: ");
    Serial.println(result);
  }
}
