#include <SPI.h>
#include <SD.h>

// SD Card pins
#define SD_DETECT_PIN   49  
#define SDSS            53

File settingsFile;

void setupSDCard() {
  pinMode(SD_DETECT_PIN, INPUT);      
  digitalWrite(SD_DETECT_PIN, HIGH);
}

void loadSDSettings() {
  int sdDetected = digitalRead(SD_DETECT_PIN);
  if (sdDetected == LOW) {
    Serial.println("SD card detected");
  }
  int connected = SD.begin(SDSS);
  if (connected == 0) {
    Serial.println("Failed to connect to SD Card");
  }
  settingsFile = SD.open("spin_settings.txt", FILE_READ);
  if (settingsFile) {
    Serial.println("Found setting file");
  } else {
    Serial.println("Settings file not found");
  }
}
