
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

int TEXT_HEIGHT = 12;                           //The height of text on the controller screen.
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

// Hilo Menu 
struct MenuLine {
  char name[10];
  int values;
  int* value1;
  char* value2;
};

MenuLine menuItems[6] = { 
  { "Delivery",  1, MOTOR_1_STEPS},
  { "Apron",  1, MOTOR_2_STEPS},
  { "Break",  1, MOTOR_3_STEPS},
  { "Elevator",  1, MOTOR_4_STEPS},
  { "Spindle",  2, MOTOR_5_STEPS,  MOTOR_5_DIRECTION },
  { "Start", 0 }
};

boolean menuLineSelected = false;
int menuLinePos = 0;
int menuLineItemPos = 0;

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
      menuLinePos = abs(menuLinePos + ENCODER_CHANGE) % 4;
     }
     ENCODER_CHANGE = 0;
  }
}

void updateMenuValue() {
  if (ENCODER_CHANGE == 0) {
    // Nothing to do.
    return;
  }
  switch(menuLinePos) {
    case 0:
      menuItems[0].value1 = incrementMOTOR1STEPS(ENCODER_CHANGE);
      break;
    case 1:
      menuItems[1].value1 = incrementMOTOR2STEPS(ENCODER_CHANGE);
      break;
    case 2:
      menuItems[2].value1 = incrementMOTOR3STEPS(ENCODER_CHANGE);
      break;
    case 3:
      menuItems[3].value1 = incrementMOTOR4STEPS(ENCODER_CHANGE);
      break;
    case 4: 
      if (menuLineItemPos == 0) {
        menuItems[4].value1 = incrementMOTOR5STEPS(ENCODER_CHANGE);
      } else {
        menuItems[4].value2 = toggleSpindleDirection();
      }
      break;
     default:
       Serial.println("Unrecognized menu line position");
       break;
  }
}

int getMenuLineItemValues() {
  return menuItems[menuLinePos].values;  
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
      int values = getMenuLineItemValues();
      if (menuLineItemPos < values - 1) {
       // There are still values to go so don't unselect the line and move the line item selection
       int values = getMenuLineItemValues();
       menuLineItemPos = (menuLineItemPos + 1) % values;
      } else {
        // Unselect the line and reset the line item pos.
        menuLineItemPos = 0;
        menuLineSelected = false;
      }
    } else if (menuLinePos == 3) {
      return toggleHilo();
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
  u8g.setFont(u8g_font_helvR08);        // Set the font for the display
  u8g.setDefaultForegroundColor();
  unsigned int h = u8g.getFontAscent()-u8g.getFontDescent();
  unsigned int w = u8g.getWidth();
  u8g.drawBox(0, 0, w, h + 2);
  u8g.setDefaultBackgroundColor();
  u8g.drawStr(2, 10, "Hallo Hilo!");
  u8g.setDefaultForegroundColor();
  
  int i;
  int s = 2;
  for( i = 0; i < 4; i++ ) {
    drawMenuLine(menuItems[i], i, s, h, w);
  }  
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
  u8g.setFont(u8g_font_helvR08);
  u8g.setDefaultForegroundColor();
  u8g.drawStr(2, 10, "Running...");
  u8g.drawStr(2, 19, "Press to stop.");
}

void stopHilo() {
  startStopMachine();
}

void drawMenuLine( struct MenuLine menuItem, int i, int s, int h, int w) {
  u8g.setDefaultForegroundColor();
  if (menuLinePos != i) {
    u8g.drawStr( 2, (i+s)*TEXT_HEIGHT, menuItem.name);
    if (menuItem.values > 0) {
       char value1[4];
       sprintf (value1, "%d", menuItem.value1);
       u8g.drawStr( 78, (i+s)*TEXT_HEIGHT, value1);
    }
    if (menuItem.values > 1) {
       char value2[1];
       sprintf (value2, "%c", menuItem.value2);
       u8g.drawStr( 110, (i+s)*TEXT_HEIGHT, value2);
    }
  } else {
      if (!menuLineSelected) {
        u8g.drawBox(0, ((i+s)*TEXT_HEIGHT-h), w, h+2);
        u8g.setDefaultBackgroundColor();
      }
      u8g.drawStr( 2, (i+s)*TEXT_HEIGHT, menuItem.name);
      if (menuItem.values > 0) {
        char value1[4];
        sprintf (value1, "%d", menuItem.value1);
        int boxWidth = 6 * getNumberLength(menuItem.value1);
        if (menuLineSelected && menuLineItemPos == 0) {      
          u8g.setDefaultForegroundColor();  
          u8g.drawBox(78, ((i+s)*TEXT_HEIGHT-h), boxWidth, h+2);
          u8g.setDefaultBackgroundColor();
        }
        u8g.drawStr( 78, (i+s)*TEXT_HEIGHT, value1);
      }
      if (menuItem.values > 1) {
        char value2[1];
        sprintf (value2, "%c", menuItem.value2);
        if (menuLineSelected && menuLineItemPos == 1) {
          u8g.setDefaultForegroundColor(); 
          u8g.drawBox(110, ((i+s)*TEXT_HEIGHT-h), 10, h+2);
          u8g.setDefaultBackgroundColor();
        } else if (menuLineSelected) {
           u8g.setDefaultForegroundColor();
        }
        u8g.drawStr(110, (i+s)*TEXT_HEIGHT, value2);
      }
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