#include <SPI.h>
#include "SdFat.h"

// Please format your sd card first using http://www.sdcard.org/downloads

// SD Card pins
#define SD_DETECT_PIN   49  
#define SDSS            53
#define SDCARD_SS_PIN   53

// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 3

#ifndef SDCARD_SS_PIN
const uint8_t SD_CS_PIN = SS;
#else   // SDCARD_SS_PIN
// Assume built-in SD is used.
const uint8_t SD_CS_PIN = SDCARD_SS_PIN;
#endif  // SDCARD_SS_PIN

// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur.
#define SPI_CLOCK SPI_HALF_SPEED

// Try to select the best SD card configuration.
#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK)
#else  // HAS_SDIO_CLASS
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK)
#endif  // HAS_SDIO_CLASS

SdFs sd;
FsFile settingsFile;

char settingsFilename[] = "hilo0001.txt";
int SD_CONNECTED = 0;

void setupSDCard() {
  pinMode(SD_DETECT_PIN, INPUT);      
  digitalWrite(SD_DETECT_PIN, HIGH);
  SD_CONNECTED = sd.begin(SD_CONFIG);
  if (SD_CONNECTED == 0) {
    debugln("Could not connect to SD card");
    sd.initErrorHalt(&Serial);
    return;
  }
}

void loadSDSettings() {
  if (SD_CONNECTED == 0) {
    debugln("SD card not connected");
    return;
  }
  int sdDetected = digitalRead(SD_DETECT_PIN);
  if (sdDetected == LOW) {
    debugln("SD card detected");
  }
  int fileExists = sd.exists(settingsFilename);
  if (!fileExists) {
    debugln("Settings file not found");
    return;
  }
  settingsFile = sd.open(settingsFilename, FILE_READ);
  if (settingsFile) {
    debugln("Found setting file");
    parseSettingsFile();
  } else {
    debugln("Error accessing settings file.");
  }
  settingsFile.close();
}

void storeSDSettings() {
  if (SD_CONNECTED == 0) {
    debugln("SD card not connected");
    return;
  }
  debugln("Storing settings");
  if (!settingsFile.open(settingsFilename, FILE_WRITE)) {
    debugln("Could not create settings file");
    return;
  }
  // First clear everything in the file:
  settingsFile.truncate(0);
  for (int i = 0; i < MOTORS_NUMBER; i++) {
    printMotorSettings(i,motorSpeeds[i]);
  }
  settingsFile.close();
}

void printMotorSettings(int motorIndex, int* speed) {
  settingsFile.print("m");
  settingsFile.print(motorIndex, 10);
  settingsFile.print(":");
  settingsFile.println(*speed, 10);
}

void parseSettingsFile() {
  //Max of 500 chars settings file. Should be fine for a little while.
  char line[40];
  debugln("Stored settings:");
  while (settingsFile.available()) {
    int n = settingsFile.fgets(line, sizeof(line));
    if (n <= 0) {
      debugln("Reading from settings file failed.");
    }
    if (line[n - 1] != '\n' && n == (sizeof(line) - 1)) {
      debugln("Settings file line too long");
    }
    debug(line);
    if (!parseLine(line)) {
      debugln("Error processing line");
    }
  }
  settingsFile.close();
}

bool parseLine(char* str) {
  // Set strtok start of line.
  str = strtok(str, ":");
  if (!str) return false;
  if (str[0] == 'm') {
    if (strlen(str) > 2) {
      debugln("No support for more than 9 motors.");
      return false;
    }
    // C way to get int from char - subtracting ASCII numbers.
    int motor = str[1] - '0';
    // Subsequent calls to strtok expects a null pointer. This should
    // return the rest.
    str = strtok(NULL, ":");
    if (str == NULL) return false;
    int motorSpeed = atoi(str);
    setMotorSpeed(motor, motorSpeed);
    return true;
  }
  debugln("Setting not recognized");
  return false;
}
