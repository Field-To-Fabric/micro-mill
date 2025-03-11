#include <U8glib.h>

// Inspired by https://github.com/ellensp/rrd-glcd-tester/blob/master/rrd-glcd-test.ino
//Standard pins when on a RAMPS 1.4

#define DOGLCD_CS       16
#define DOGLCD_MOSI     17
#define DOGLCD_SCK      23
#define BTN_EN1         31
#define BTN_EN2         33
#define BTN_ENC         35
#define SD_DETECT_PIN   49
#define SDSS            53
#define BEEPER_PIN      37
#define KILL_PIN        41

int TEXT_HEIGHT = 8;                           //The height of text on the controller screen.
u8g_fntpgm_uint8_t *TEXT_FONT = u8g_font_04b_03;
int KILL_PIN_STATUS = 1;                        //Last read status of the stop pin, start at 1 to ensure buzzer is off              
int ENCODER_0_PIN_A_LAST;                       //Used to decode rotory encoder, last value
int ENCODER_0_PIN_A_NOW;                        //Used to decode rotory encoder, current value
int ENCODER_PIN_STATUS;                         //Last read status of the encoder button
unsigned long PREVIOUS_MILLIS = 0;              //Previous Millis value
unsigned long CURRENT_MILLIS;                   //Current Millis value
const long DISPLAY_INTERVAL = 1000/3;           //How often to run the display loop, every 1/3 of a second aproximatly 
const long ENCODER_READ_INTERVAL = 1000/3;      //How often to read the encoder rotation change.
const long ENCODER_BUTTON_READ_INTERVAL = 1000; //How often to read the encoder button press.
const long HILO_START_STOP_INTERVAL = 1000;     //How often to check for machine start/stop.
int ENCODER_CHANGE = 0;                         //Change in the encoder rotation.
boolean ENCODER_BUTTON_TRIGGERED = false;       //Whether the encoder button was pressed.
unsigned long ENCODER_MILLIS_LAST = 0;          //Last time we read the encoder change.
unsigned long ENCODER_BUTTON_MILLIS_LAST = 0;   //Last time we checked for an encoder button press.
unsigned long HILO_START_STOP_LAST_MILLIS = 0;  //Last time we check for start/stop.

// Not using enums because of weird compiler behaviour.
const int INTEGER_SETTING_TYPE = 0;
const int BOOLEAN_SETTING_TYPE = 1;

struct SettingLine {
  char name[10];
  int* value1;
  int settingType;
};

// Iterating through the options goes through all the menu items and all the control items.
const int PAGES_NUMBER = 2;
const int MENU_ITEMS_NUMBER = MOTORS_NUMBER;
const int CONTROL_ITEMS_NUMBER = 3;

SettingLine motorSpeedSettings[MENU_ITEMS_NUMBER] = { 
  { "Motor 1", motorSpeeds[0], INTEGER_SETTING_TYPE },
  { "Motor 2", motorSpeeds[1], INTEGER_SETTING_TYPE },
  { "Motor 3", motorSpeeds[2], INTEGER_SETTING_TYPE },
  { "Motor 4", motorSpeeds[3], INTEGER_SETTING_TYPE },
  { "Motor 5", motorSpeeds[4], INTEGER_SETTING_TYPE }
};

int noValue = 0;
SettingLine settings1[MENU_ITEMS_NUMBER] = {
  { "Serial IO", & ENABLE_SERIAL_IO, BOOLEAN_SETTING_TYPE },
  { "Transmit", & SERIAL_TRANSMIT, BOOLEAN_SETTING_TYPE },
  { "[not used]", & noValue, INTEGER_SETTING_TYPE },
  { "[not used]", & noValue, INTEGER_SETTING_TYPE },
  { "[not used]", & noValue, INTEGER_SETTING_TYPE },
};

SettingLine *pages[PAGES_NUMBER] = { motorSpeedSettings, settings1 };

boolean menuLineSelected = false;
int menuLinePos = 0;
int pagePos = 0;

// Initialize the U8GLIB lib for our screen
// SPI Com: SCK = en = 23, MOSI = rw = 17, CS = di = 16
U8GLIB_ST7920_128X64_1X u8g(DOGLCD_SCK, DOGLCD_MOSI, DOGLCD_CS);

void setupScreenController() {
  pinMode(SD_DETECT_PIN, INPUT);        // Set SD_DETECT_PIN as an input
  digitalWrite(SD_DETECT_PIN, HIGH);    // turn on pullup resistors
  pinMode(KILL_PIN, INPUT);             // Set KILL_PIN as an input
  digitalWrite(KILL_PIN, HIGH);         // turn on pullup resistors
  pinMode(BTN_EN1, INPUT_PULLUP);       // Set BTN_EN1 as an unput, half of the encoder
  pinMode(BTN_EN2, INPUT_PULLUP);       // Set BTN_EN2 as an input, second half of the encoder
  pinMode(BTN_ENC, INPUT);              // Set BTN_ENC as an input, encoder button
  digitalWrite(BTN_ENC, HIGH);          // turn on pullup resistors
  u8g.begin();
}

void screenControllerLoop() {
  CURRENT_MILLIS = millis();
  
  encoderButtonTrigger();
  if (IS_RUNNING) {
    // While running we want to do as little as possible to ensure a smooth run.
    return;
  }
  encoderTrigger();
  updateMenu();

  //check if it is time to update the display 
  if (CURRENT_MILLIS - PREVIOUS_MILLIS >= DISPLAY_INTERVAL) {
    PREVIOUS_MILLIS = CURRENT_MILLIS;

    //Draw new screen
    u8g.firstPage();
    do {  
      drawHilo();
    } while( u8g.nextPage() );
  }
}

void updateMenu() {
  // If the time has passed we check for a change, update the position and reset the trigger.
  if ((CURRENT_MILLIS - ENCODER_BUTTON_MILLIS_LAST) >= ENCODER_BUTTON_READ_INTERVAL) {
    ENCODER_BUTTON_MILLIS_LAST = CURRENT_MILLIS;
    ENCODER_BUTTON_TRIGGERED = false;
  }
  if ((CURRENT_MILLIS - ENCODER_MILLIS_LAST) >= ENCODER_READ_INTERVAL) {
     ENCODER_MILLIS_LAST = CURRENT_MILLIS;
     if (menuLineSelected) {
       updateMenuValue();
     } else {
      menuLinePos = abs(menuLinePos + ENCODER_CHANGE) % (MENU_ITEMS_NUMBER + CONTROL_ITEMS_NUMBER);
     }
     ENCODER_CHANGE = 0;
  }
}

void updateMenuValue() {
  if (ENCODER_CHANGE == 0) {
    // Nothing to do.
    return;
  } 
  switch (pagePos) {
    case 0: 
      incrementMotorSpeed(menuLinePos, ENCODER_CHANGE); 
      break;
    case 1: 
      handlePage1SettingsChange(menuLinePos, ENCODER_CHANGE);
      break;
    default:
      // do nothing
      break;
  }
}

void encoderButtonTrigger() {
  if (ENCODER_BUTTON_TRIGGERED) {
    // We've detected a trigger so skip.
    return;
  }
  //read the encoder button status
  ENCODER_PIN_STATUS = digitalRead(BTN_ENC);
  if (!ENCODER_PIN_STATUS) {
    if (menuLineSelected) {
      menuLineSelected = false;
    } else if (menuLinePos - MENU_ITEMS_NUMBER == 0) {
      return toggleHilo();
    } else if (menuLinePos - MENU_ITEMS_NUMBER == 1) {
      return resetRunCounter();
    } else if (menuLinePos - MENU_ITEMS_NUMBER == 2) {
      return nextPage();
      return;
    } else {
        menuLineSelected = true;
    }
    ENCODER_BUTTON_TRIGGERED = true;
  }
}

void encoderTrigger() {
  if (ENCODER_CHANGE != 0) {
    // We've detected an encoder change so we are done until it resets.
    return;
  }
  
  ENCODER_0_PIN_A_NOW = digitalRead(BTN_EN2);  // Current Digital read of scrollRight
  if ((ENCODER_0_PIN_A_LAST == HIGH) && (ENCODER_0_PIN_A_NOW == LOW)) {
    if (digitalRead(BTN_EN1) == LOW) {
      ENCODER_CHANGE = 1;
    } else {
      ENCODER_CHANGE = -1;
    }
  }
  ENCODER_0_PIN_A_LAST = ENCODER_0_PIN_A_NOW;
  // No change so do nothing
}

void drawHilo() {
  u8g.setFont(TEXT_FONT);        // Set the font for the display
  u8g.setDefaultForegroundColor();
  unsigned int h = u8g.getFontAscent()-u8g.getFontDescent();
  unsigned int w = u8g.getWidth();
  u8g.drawBox(0, 0, w, h+2);
  u8g.setDefaultBackgroundColor();
  u8g.drawStr(2, 8, "Hallo Hilo!         Run:");
  // Draw the current run value:
  char CURRENT_RUN_DISTANCE_STRING[11];
  dtostrf(CURRENT_RUN_DISTANCE, -6, 1, CURRENT_RUN_DISTANCE_STRING);
  u8g.drawStr(95, 8, CURRENT_RUN_DISTANCE_STRING);
  u8g.setDefaultForegroundColor();
  int i;
  int s = 2;
  for(i = 0; i < MENU_ITEMS_NUMBER; i++ ) {
    drawMenuLine(*(pages[pagePos] + i), i, s, h, w);
  }
  drawControlMenu(i, s, h, w);
}

void toggleHilo() {
  if (CURRENT_MILLIS - HILO_START_STOP_LAST_MILLIS < HILO_START_STOP_INTERVAL) {
    // Too soon to toggle Hilo;
    return;
  }
  HILO_START_STOP_LAST_MILLIS = CURRENT_MILLIS;
  if (IS_RUNNING) {
    stopHilo();
  } else {
    //Special case of starting the machine.
    startHilo();
  }
}

void startHilo() {
  //Draw Hilo stop screen
  u8g.firstPage();
  do {  
    drawHiloStop();
  } while( u8g.nextPage() );
  
  startStopMachine();
}

void drawHiloStop() {
  u8g.setFont(TEXT_FONT);
  u8g.setDefaultForegroundColor();
  u8g.drawStr(2, 10, "Running...");
  u8g.drawStr(2, 19, "Press to stop.");
}

void stopHilo() {
  startStopMachine();
}

void drawMenuLine(struct SettingLine menuItem, int i, int s, int h, int w) {
  u8g.setDefaultForegroundColor();
  if (menuLinePos != i) {
    u8g.drawStr( 2, (i+s)*TEXT_HEIGHT, menuItem.name);
    char value1[5];
    if (menuItem.value1) {
      formatSettingsValue(*menuItem.value1, value1, menuItem.settingType);
    }
    u8g.drawStr( 78, (i+s)*TEXT_HEIGHT, value1);
  } else {
    if (!menuLineSelected) {
      u8g.drawBox(0, ((i+s)*TEXT_HEIGHT-h), w, h+2);
      u8g.setDefaultBackgroundColor();
    }
    u8g.drawStr( 2, (i+s)*TEXT_HEIGHT, menuItem.name);
    char value1[5];
    int boxWidth = 0;
    if (menuItem.value1) {
      formatSettingsValue(*menuItem.value1, value1, menuItem.settingType);
      boxWidth = 6 * getNumberLength(*menuItem.value1);
    }
    if (menuItem.settingType == BOOLEAN_SETTING_TYPE) {
      boxWidth = 36;
    }
    if (menuLineSelected) {      
      u8g.setDefaultForegroundColor();  
      u8g.drawBox(78, ((i+s)*TEXT_HEIGHT-h), boxWidth, h+2);
      u8g.setDefaultBackgroundColor();
     }
     u8g.drawStr( 78, (i+s)*TEXT_HEIGHT, value1);
  }
}

void drawControlMenu(int i, int s, int h, int w) {
  drawControlMenuItem("START", 0, i, s, h, w);
  drawControlMenuItem("RESET", 1, i, s, h, w);
  drawControlMenuItem("NEXT", 2, i, s, h, w);
}

void drawControlMenuItem(char controlName[10], int controlPos, int i, int s, int h, int w) {
  u8g.setDefaultForegroundColor();
  int start = controlPos * 40;
  if (menuLinePos - MENU_ITEMS_NUMBER == controlPos) {      
    u8g.setDefaultForegroundColor();
    u8g.drawBox(start, ((i+s)*TEXT_HEIGHT-h), 30, h+2);
    u8g.setDefaultBackgroundColor();
   }
  u8g.drawStr( start, (i+s)*TEXT_HEIGHT, controlName);
}

void nextPage() {
  pagePos = (pagePos + 1) % 2;
}

void handlePage1SettingsChange(int menuLinePos, int ENCODER_CHANGE) {
  switch (menuLinePos) {
    case 0:
      toggleSerialIO();
      break;
    case 1:
      toggleIsMaster();
    default:
      // do nothing
      break;
  }
}

void formatSettingsValue(int value, char formattedValue[5], int st) {
  switch (st) {
    case BOOLEAN_SETTING_TYPE: 
      (value == 0) ? sprintf(formattedValue, "false") : sprintf(formattedValue, "true");
      break;
    case INTEGER_SETTING_TYPE: 
      sprintf(formattedValue, "%d", value);
      break;
    default:
      sprintf(formattedValue, "?");
      break;
  } 
}

int getNumberLength(int n) {
  if (abs(n) < 10) return 1;
  if (abs(n) < 100) return 2;
  if (abs(n) < 1000) return 3;
  if (abs(n) < 10000) return 4;
  if (abs(n) < 100000) return 5;
  return 1;
}
