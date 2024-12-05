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


bool DEBUG_SD_CARD = true;

SdFs sd;
FsFile settingsFile;

char settingsFilename[] = "hilo0001.txt";
int SD_CONNECTED = 0;

void setupSDCard() {
  pinMode(SD_DETECT_PIN, INPUT);      
  digitalWrite(SD_DETECT_PIN, HIGH);
  SD_CONNECTED = sd.begin(SD_CONFIG);
  if (SD_CONNECTED == 0) {
    Serial.println("Could not connect to SD card");
    sd.initErrorHalt(&Serial);
    return;
  }
  if (DEBUG_SD_CARD) {
    printSDCardInfo();
  }
}

void loadSDSettings() {
  if (SD_CONNECTED == 0) {
    Serial.println("SD card not connected");
    return;
  }
  int sdDetected = digitalRead(SD_DETECT_PIN);
  if (sdDetected == LOW) {
    Serial.println("SD card detected");
  }
  int fileExists = sd.exists(settingsFilename);
  if (!fileExists) {
    Serial.println("Settings file not found");
    return;
  }
  settingsFile = sd.open(settingsFilename, FILE_READ);
  if (settingsFile) {
    Serial.println("Found setting file");
    parseSettingsFile();
  } else {
    Serial.println("Error accessing settings file.");
  }
  settingsFile.close();
}

void storeSDSettings() {
  if (SD_CONNECTED == 0) {
    Serial.println("SD card not connected");
    return;
  }
  Serial.println("Storing settings");
  if (!settingsFile.open(settingsFilename, FILE_WRITE)) {
    Serial.println("Could not create settings file");
    return;
  }
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
  Serial.println("Stored settings:");
  while (settingsFile.available()) {
    int n = settingsFile.fgets(line, sizeof(line));
    if (n <= 0) {
      Serial.println("Reading from settings file failed.");
    }
    if (line[n - 1] != '\n' && n == (sizeof(line) - 1)) {
      Serial.println("Settings file line too long");
    }
    Serial.print(line);
    if (!parseLine(line)) {
      Serial.println("Error processing line");
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
      Serial.println("No support for more than 9 motors.");
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
  Serial.println("Setting not recognized");
  return false;
}

void printSDCardInfo() {
//   Serial.println();
//   int sdcardinit = card.init(SPI_HALF_SPEED, SDSS);
//   Serial.print("Card type:         ");
//   switch (card.type()) {
//     case SD_CARD_TYPE_SD1:
//      Serial.println("SD1");
//      break;
//     case SD_CARD_TYPE_SD2:
//      Serial.println("SD2");
//      break;
//     case SD_CARD_TYPE_SDHC:
//      Serial.println("SDHC");
//      break;
//     default:
//      Serial.println("Unknown");
//   }
//   // Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
//   if (!volume.init(card)) {
//    Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
//    return;
//   }
//   Serial.print("Clusters:          ");
//   Serial.println(volume.clusterCount());
//   Serial.print("Blocks x Cluster:  ");
//   Serial.println(volume.blocksPerCluster());
//   Serial.print("Total Blocks:      ");
//   Serial.println(volume.blocksPerCluster() * volume.clusterCount());
//   Serial.println();
//   // print the type and size of the first FAT-type volume
//   uint32_t volumesize;
//   Serial.print("Volume type is:    FAT");
//   Serial.println(volume.fatType(), DEC);
//   volumesize = volume.blocksPerCluster();    // clusters are collections of blocks
//   volumesize *= volume.clusterCount();       // we'll have a lot of clusters
//   volumesize /= 2;                           // SD card blocks are always 512 bytes (2 blocks are 1KB)
//   Serial.print("Volume size (Kb):  ");
//   Serial.println(volumesize);
//   Serial.print("Volume size (Mb):  ");
//   volumesize /= 1024;
//   Serial.println(volumesize);
//   Serial.print("Volume size (Gb):  ");
//   Serial.println((float)volumesize / 1024.0);
//   Serial.println("\nFiles found on the card (name, date and size in bytes): ");
//   root.openRoot(volume);
//   // list all files in the card with date and size
//   root.ls(LS_R | LS_DATE | LS_SIZE);
//   root.close();
}
