#include <AccelStepper.h>

#define HILO_SERIAL_BAUDRATE 115200

#define PIN_LED               13  // Arduino on-board LED

#define PIN_DRAFTING_STEP     54  // Drafting Motor Step Pin
#define PIN_DRAFTING_DIR      55  // Drafting Motor Direction Pin
#define PIN_DRAFTING_ENABLE   38  // Drafting Motor Enable Pin

#define PIN_DELIVERY_STEP     60  // Delivery Motor Step Pin
#define PIN_DELIVERY_DIR      61  // Delivery Motor Direction Pin
#define PIN_DELIVERY_ENABLE   56  // Delivery Motor Enable Pin

#define PIN_SPINDLE_STEP      46  // Spindle Motor Step Pin
#define PIN_SPINDLE_DIR       48  // Spindle Motor Direction Pin
#define PIN_SPINDLE_ENABLE    62  // Spindle Motor Enable Pin

#define PIN_ELEVATOR_A_STEP   36  // Elevator Motors Step Pin
#define PIN_ELEVATOR_A_DIR    34  // Elevator Motors Direction Pin
#define PIN_ELEVATOR_A_ENABLE 30  // Elevator Motors Enable Pin

#define PIN_FLYER_STEP   26  // Flyer Motors Step Pin
#define PIN_FLYER_DIR    28  // Flyer Motors Direction Pin
#define PIN_FLYER_ENABLE 24  // Flyer Motors Enable Pin

#define PIN_END_STOP_X_MIN     3  // X Min End Stop Pin
#define PIN_END_STOP_X_MAX     2  // X Max End Stop Pin

int DRAFTING_SPEED_PERCENTAGE = 40;
int DELIVERY_SPEED            = 150;
int SPINDLE_SPEED             = 700;
int ELEVATOR_SPEED            = 100;
int FLYER_SPEED               = 100;
char SPINDLE_DIRECTION = 'S';

// The end stop should not be triggered very frequently.
const long END_STOP_TRIGGER_INTERVAL = 5000; 
// Marked volatile as these are modified from an interrupt method.
volatile int ELEVATOR_DIRECTION = -1;
volatile bool END_STOP_TRIGGERED = false;
volatile unsigned long END_STOP_TRIGGER_MILLIS_LAST = 0;

float ACCELERATION = 500.00f;

bool IS_RUNNING = false;

// Setting the enable the driver flyer mod.
// TODO Add this option to the screen controller.
bool ENABLE_FLYER = false;
// When spinning it's useful to delay the drafting start to allow the twist to build up a bit. 
// When using the flyer this is unnecessary and will break the roving.
int DRAFTING_START_DELAY = 10000;
long START_COUNTER = ENABLE_FLYER ? 0 : DRAFTING_START_DELAY;

AccelStepper motorDrafting(AccelStepper::DRIVER, PIN_DRAFTING_STEP, PIN_DRAFTING_DIR);
AccelStepper motorDelivery(AccelStepper::DRIVER, PIN_DELIVERY_STEP, PIN_DELIVERY_DIR);
AccelStepper motorElevator(AccelStepper::DRIVER, PIN_ELEVATOR_A_STEP, PIN_ELEVATOR_A_DIR);
AccelStepper motorSpindle(AccelStepper::DRIVER, PIN_SPINDLE_STEP, PIN_SPINDLE_DIR);
AccelStepper motorFlyer(AccelStepper::DRIVER, PIN_FLYER_STEP, PIN_FLYER_DIR);

void setup() {
  Serial.begin(HILO_SERIAL_BAUDRATE);

  // Setup ScreenController
  setupScreenController();
  setupEndStops();
  
  Serial.println("Starting HILO Machine");
  
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  
  // set the mode for the stepper driver enable pins
  pinMode (PIN_DRAFTING_ENABLE,   OUTPUT);
  pinMode (PIN_DELIVERY_ENABLE,   OUTPUT);
  pinMode (PIN_ELEVATOR_A_ENABLE, OUTPUT);
  pinMode (PIN_SPINDLE_ENABLE,    OUTPUT);
  pinMode (PIN_FLYER_ENABLE,    OUTPUT);

  setSteppersEnabled(false);
}

void loop() {
  serialCommunicationLoop();
  screenControllerLoop();
  runMachineLoop();
}

// This loops allows you to send commands to the machine through the Arduino Serial Monitor.
void serialCommunicationLoop() {
  if (Serial.available() > 0) {
    // read a character from serial, if one is available
    String data = Serial.readStringUntil('\n');
    Serial.print("Received ");
    Serial.println(data);
    if (data == " ") {
      startStopMachine();
    }
    if (data.startsWith("s")) {
      int spindleSpeed = data.substring(1).toInt();
      SPINDLE_SPEED = spindleSpeed;
      Serial.print("Spindle speed set to: ");
      Serial.println(SPINDLE_SPEED);
    }
    if (data.startsWith("f")) {
      int flyerSpeed = data.substring(1).toInt();
      FLYER_SPEED = flyerSpeed;
      Serial.print("FLyer speed set to: ");
      Serial.println(FLYER_SPEED);
    }
    if (data.startsWith("d")) {
      int deliverySpeed = data.substring(1).toInt();
      DELIVERY_SPEED = deliverySpeed;
      Serial.print("Delivery speed set to: ");
      Serial.println(DELIVERY_SPEED);
    }
    if (data.startsWith("p")) {
      int draftingPercentage = data.substring(1).toInt();
      DRAFTING_SPEED_PERCENTAGE = draftingPercentage;
      Serial.print("Drafting percentage set to: ");
      Serial.println(DRAFTING_SPEED_PERCENTAGE);
    }
    if (data.startsWith("e")) {
      ELEVATOR_DIRECTION = -ELEVATOR_DIRECTION;
      Serial.print("Elevator direction set to: ");
      Serial.println(ELEVATOR_DIRECTION);
    }
    if (data.startsWith("t")) {
      toggleSpindleDirection();
      Serial.print("Twist direction set to: ");
      Serial.println(SPINDLE_DIRECTION);
    }
  }
}

boolean startStopMachine() {
  if (IS_RUNNING) {
    stopMachine();
  } else {
    startMachine();
  }
  return IS_RUNNING;
}

void stopMachine() {
  Serial.println("Stopping machine");
  IS_RUNNING = false;
  START_COUNTER = 0;
  motorDrafting.stop();
  motorDrafting.setCurrentPosition(0);
  motorDelivery.stop();
  motorDelivery.setCurrentPosition(0);
  motorSpindle.stop();
  motorSpindle.setCurrentPosition(0);
  motorElevator.stop();
  motorElevator.setCurrentPosition(0);

  if (ENABLE_FLYER) {
    motorFlyer.stop();
    motorFlyer.setCurrentPosition(0);
  }
  
  setSteppersEnabled(false);
}

void startMachine() {
  Serial.println("Starting machine");

  // DELIVERY
  motorDelivery.setMaxSpeed(DELIVERY_SPEED);
  motorDelivery.setAcceleration(ACCELERATION);
  motorDelivery.move(-1000000);

  // DRAFTING
  int draftingSpeed = (int)map(DRAFTING_SPEED_PERCENTAGE, 0, 100, 0, DELIVERY_SPEED);
  motorDrafting.setMaxSpeed(draftingSpeed);
  motorDrafting.setAcceleration(ACCELERATION);\
  motorDrafting.move(-1000000);

  // SPINDLE  
  motorSpindle.setMaxSpeed(SPINDLE_SPEED);
  motorSpindle.setAcceleration(ACCELERATION);
  int spindleDirection = (SPINDLE_DIRECTION == 'Z') ? 1 : -1;
  motorSpindle.move(1000000 * spindleDirection);

  // ELEVATOR
  motorElevator.setMaxSpeed(ELEVATOR_SPEED);
  motorElevator.setAcceleration(ACCELERATION);
  setElevatorMove();

  //FLYER
  if (ENABLE_FLYER) {
    motorFlyer.setMaxSpeed(FLYER_SPEED);
    motorFlyer.setAcceleration(ACCELERATION);
    // The flyer should go in the same direction as the spindle.
    motorFlyer.move(1000000 * spindleDirection);
  }

  printMachineSettings(draftingSpeed);
  
  IS_RUNNING = true;
  setSteppersEnabled(true);
}

void runMachineLoop() {
  if (IS_RUNNING) {
      motorSpindle.run();
      motorElevator.run();
      if (ENABLE_FLYER) { 
        motorFlyer.run();
      }
      if (START_COUNTER > DRAFTING_START_DELAY) { 
        // Build in a delay in starting the delivery, to allow for some twist build up.
        motorDelivery.run();
        motorDrafting.run();
      } else {
        START_COUNTER = START_COUNTER + 1;
      }
      if (END_STOP_TRIGGERED) {
        END_STOP_TRIGGERED = false;
        setElevatorMove();
      }
  }
}

// Enables or disables all steppers. Used for saving power
// and allowing adjustments by hand when the machine isn't running.
void setSteppersEnabled(bool enabled) {
  int value = LOW;
  if (!enabled) value = HIGH;

  digitalWrite(PIN_DRAFTING_ENABLE,   value);
  digitalWrite(PIN_DELIVERY_ENABLE,   value);
  digitalWrite(PIN_ELEVATOR_A_ENABLE, value);
  digitalWrite(PIN_SPINDLE_ENABLE,    value);
  if (ENABLE_FLYER) {
    digitalWrite(PIN_FLYER_ENABLE,    value);
  }
}

// Direction is +/- 1
int incrementDraftingSpeedPercentage(int direction) {
  if (DRAFTING_SPEED_PERCENTAGE >= 100 && direction >= 1) {
    return 100;
  }
  if (DRAFTING_SPEED_PERCENTAGE <= 0 && direction <= 0) {
    return 0;
  }
  DRAFTING_SPEED_PERCENTAGE = DRAFTING_SPEED_PERCENTAGE + 10 * direction;
  return DRAFTING_SPEED_PERCENTAGE;
}

int incrementDeliverySpeed(int direction) {
  DELIVERY_SPEED = DELIVERY_SPEED + 50 * direction;
  return DELIVERY_SPEED;
}

int incrementSpindleSpeed(int direction) {
  SPINDLE_SPEED = SPINDLE_SPEED + 100 * direction;
  return SPINDLE_SPEED;
}

char toggleSpindleDirection() {
  if (SPINDLE_DIRECTION == 'S') {
    SPINDLE_DIRECTION = 'Z';
  } else {
    SPINDLE_DIRECTION = 'S';
  }
  return SPINDLE_DIRECTION;
}

void setupEndStops() {
  pinMode(PIN_END_STOP_X_MIN, INPUT_PULLUP);
  pinMode(PIN_END_STOP_X_MAX, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_END_STOP_X_MIN), endStopTrigger, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_END_STOP_X_MAX), endStopTrigger, FALLING);
}

void endStopTrigger() {
  const unsigned long currentMillis = millis();
  if (currentMillis > (END_STOP_TRIGGER_MILLIS_LAST + END_STOP_TRIGGER_INTERVAL)) {
    ELEVATOR_DIRECTION = -ELEVATOR_DIRECTION; 
    END_STOP_TRIGGERED = true;
    END_STOP_TRIGGER_MILLIS_LAST = currentMillis;
  }
}

void setElevatorMove() {
  Serial.println("Reversing elevator direction");
  motorElevator.setCurrentPosition(0);
  motorElevator.move(1000000 * ELEVATOR_DIRECTION);
}

void printMachineSettings(int draftingSpeed) {
  Serial.print("Drafting percentage: ");
  Serial.println(DRAFTING_SPEED_PERCENTAGE);
  Serial.print("Drafting speed: ");
  Serial.println(draftingSpeed); 
  Serial.print("Delivery speed: ");
  Serial.println(DELIVERY_SPEED);
  Serial.print("Spindle speed: ");
  Serial.println(SPINDLE_SPEED);
  Serial.print("Elevator direction:");
  Serial.println(ELEVATOR_DIRECTION);
  Serial.print("Flyer enabled:");
  Serial.println(ENABLE_FLYER);
}
