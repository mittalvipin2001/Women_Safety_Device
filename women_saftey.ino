#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

// Pin Definitions
#define GSM_RX_PIN 13
#define GSM_TX_PIN 15
#define FINGERPRINT_RX_PIN 12
#define FINGERPRINT_TX_PIN 14
#define GPS_RX_PIN 0
#define GPS_TX_PIN 2
#define OLED_RESET_PIN -1
#define BUZZER_PIN 16

// SoftwareSerial instances for communication with modules
SoftwareSerial gsmSerial(GSM_RX_PIN, GSM_TX_PIN); // GSM Module
SoftwareSerial fingerSerial(FINGERPRINT_RX_PIN, FINGERPRINT_TX_PIN); // Fingerprint Module
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerSerial); // Fingerprint library instance
SoftwareSerial gpsSerial(GPS_RX_PIN, GPS_TX_PIN); // GPS Module

// OLED Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET_PIN);

// Flags and variables
bool fingerPressed = false; // Flag to indicate if a fingerprint is pressed
bool dangerAlert = false; // Flag to indicate if a danger alert is active
bool buzzerOn = false; // Flag to indicate if the buzzer is on
unsigned long timerStartTime; // Start time for the timer
unsigned long lastGPSSendTime = 0; // Timestamp for the last GPS data send

// Array to store multiple phone numbers
String phoneNumbers[] = {"+917726832699", "+918619049793", "+917976730624"};

// Function declarations
void displayTimer(unsigned long currentTime);
void displayMatchedFinger();
void displayIncorrectFinger();
void displayDangerAlert();
String getValue(String data, char separator, int index);
void getFingerprintID();
void sendDangerAlert();
String getGPSData();

// Setup function
void setup() {
  // Initialize serial communication for debugging
  Serial.begin(9600);

  // Initialize serial communication with GSM, fingerprint, and GPS modules
  gsmSerial.begin(38400);
  fingerSerial.begin(57600);
  gpsSerial.begin(9600);

  // Initialize Buzzer pin as output
  pinMode(BUZZER_PIN, OUTPUT);

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  
  // Set display properties
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1.8);
  display.setCursor(10, 10);
  
  // Display welcome message
  display.println(F("WELCOME TO"));
  display.setCursor(20, 30);
  display.println(F("PROJECT"));
  display.setCursor(15, 50);
  display.println(F("WOMEN SAFETY"));
  display.display();
  
  delay(10000); // Wait for 10 seconds
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 10);
  display.println(F("SCAN YOUR"));
  display.setCursor(10, 30);
  display.println(F("FINGER"));
  display.display();
}

// Main loop
void loop() {
  if (!fingerPressed && !dangerAlert) {
    getFingerprintID();
  }

  if (fingerPressed) {
    unsigned long currentTime = millis();
    while (millis() - currentTime < 30000) { // Loop for 30 seconds
      getFingerprintID();
      if (fingerPressed) {
        currentTime = millis();
        fingerPressed = false;
      }
      
      displayTimer(currentTime);
    }
    
    if (!fingerPressed) {
      sendDangerAlert();
    }
  }

  if (dangerAlert) {
    if (millis() - lastGPSSendTime >= 10000) {
      sendDangerAlert();
      lastGPSSendTime = millis();
    }
  }
}

// Function to capture fingerprint
void getFingerprintID() {
  uint8_t p = finger.getImage(); // Capture fingerprint image
  if (p == FINGERPRINT_OK) {
    Serial.println("Image taken");
    p = finger.image2Tz(); // Convert image to template
    if (p != FINGERPRINT_OK) {
      Serial.println("Error converting image");
      return;
    }
    p = finger.fingerSearch(); // Search for matching fingerprint
    if (p == FINGERPRINT_OK) {
      Serial.println("Fingerprint matched");
      fingerPressed = true;
      timerStartTime = millis();
      displayMatchedFinger(); // Display matched fingerprint message
      delay(2000);
      display.clearDisplay();
    } else {
      Serial.println("Fingerprint not found");
      displayIncorrectFinger(); // Display incorrect fingerprint message
      delay(2000);
      display.clearDisplay();
    }
  } else {
    Serial.println("Error capturing image");
  }
}

// Function to send danger alert to multiple numbers
void sendDangerAlert() {
  String gpsData = getGPSData();
  String latitude = getValue(gpsData, ',', 0);
  String longitude = getValue(gpsData, ',', 1);
  String locationLink = "https://maps.google.com/?q=" + latitude + "," + longitude;
  
  for (int i = 0; i < sizeof(phoneNumbers) / sizeof(phoneNumbers[0]); i++) {
    // Send danger alert message to each number in the array
    gsmSerial.println("AT+CMGF=1");
    delay(1000);
    gsmSerial.print("AT+CMGS=\"");
    gsmSerial.print(phoneNumbers[i]);
    gsmSerial.println("\"\r");
    delay(1000);
    gsmSerial.print("I am in danger! Location: ");
    gsmSerial.println(locationLink);
    gsmSerial.write((char)26);
    delay(1000);
    Serial.print("Danger alert sent to: ");
    Serial.println(phoneNumbers[i]);
  }
  
  displayDangerAlert();
  dangerAlert = true;
  digitalWrite(BUZZER_PIN, HIGH);
  buzzerOn = true;
}

// Function to retrieve GPS data
String getGPSData() {
  unsigned long startTime = millis();
  String gpsData;
  
  // Wait for GPS data for 10 seconds
  while (millis() - startTime < 10000) {
    while (gpsSerial.available()) {
      gpsData = gpsSerial.readStringUntil('\n');
      Serial.println("Raw GPS data: " + gpsData); // Debug: Print raw GPS data
      
      if (gpsData.startsWith("$GPGGA") || gpsData.startsWith("$GPRMC")) {
        String latitude, longitude;
        if (gpsData.startsWith("$GPGGA")) {
          latitude = getValue(gpsData, ',', 2);
          longitude = getValue(gpsData, ',', 4);
        } else if (gpsData.startsWith("$GPRMC")) {
          latitude = getValue(gpsData, ',', 3);
          longitude = getValue(gpsData, ',', 5);
        }
        
        // Convert latitude and longitude to decimal degrees if needed
        latitude = convertToDecimalDegrees(latitude, gpsData.charAt(gpsData.startsWith("$GPGGA") ? 3 : 4));
        longitude = convertToDecimalDegrees(longitude, gpsData.charAt(gpsData.startsWith("$GPGGA") ? 5 : 6));
        
        // Debug: Print extracted latitude and longitude
        Serial.println("Latitude: " + latitude);
        Serial.println("Longitude: " + longitude);
        
        return latitude + "," + longitude;
      }
    }
  }
  
  Serial.println("GPS data not available");
  return "26.26694444,73.03694444";
}

// Function to convert GPS coordinates to decimal degrees
String convertToDecimalDegrees(String coord, char direction) {
  float decimal = coord.toFloat() / 100.0;
  int degrees = (int)decimal;
  float minutes = (decimal - degrees) * 100.0 / 60.0;
  decimal = degrees + minutes;
  if (direction == 'S' || direction == 'W') {
    decimal = -decimal;
  }
  return String(decimal, 6);
}

// Function to display timer on OLED
void displayTimer(unsigned long currentTime) {
  display.clearDisplay();
  display.setCursor(10, 10);
  display.print("TIMER: ");
  unsigned long timeLeft = 30 - ((millis() - currentTime) / 1000);
  display.print(timeLeft);
  display.println(" SECONDS");
  display.display();
}

// Function to display matched fingerprint message on OLED
void displayMatchedFinger() {
  display.clearDisplay();
  display.setCursor(15, 10);
  display.println(F("Finger"));
  display.setCursor(15, 30);
  display.println(F("Matched"));
  display.display();
}

// Function to display incorrect fingerprint message on OLED
void displayIncorrectFinger() {
  display.clearDisplay();
  display.setCursor(15, 10);
  display.println(F("Incorrect"));
  display.setCursor(15, 30);
  display.println(F("Finger"));
  display.display();
}

// Function to display danger alert message on OLED
void displayDangerAlert() {
  display.clearDisplay();
  display.setCursor(15, 10);
  display.println(F("Danger"));
  display.setCursor(15, 30);
  display.println(F("Alert"));
  display.setCursor(15, 50);
  display.println(F("SEND"));
  display.display();
}

// Function to extract values from comma-separated string
String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
