#include <AccelStepper.h>

#define HILO_SERIAL_BAUDRATE 115200

#define PIN_LED               13  // Arduino on-board LED

#define PIN_MOTOR_1_STEP     54  // Motor 1 Step Pin
#define PIN_MOTOR_1_DIR      55  // Motor 1 Direction Pin
#define PIN_MOTOR_1_ENABLE   38  // Motor 1 Enable Pin

#define PIN_MOTOR_2_STEP     60  // Motor 2 Step Pin
#define PIN_MOTOR_2_DIR      61  // Motor 2 Direction Pin
#define PIN_MOTOR_2_ENABLE   56  // Motor 2 Enable Pin

#define PIN_MOTOR_3_STEP      46  // Motor 3 Step Pin
#define PIN_MOTOR_3_DIR       48  // Motor 3 Direction Pin
#define PIN_MOTOR_3_ENABLE    62  // Motor 3 Enable Pin

#define PIN_MOTOR_4_STEP   36  // Motors 4 Step Pin
#define PIN_MOTOR_4_DIR    34  // Motors 4 Direction Pin
#define PIN_MOTOR_4_ENABLE 30  // Motors 4 Enable Pin

#define PIN_MOTOR_5_STEP   26  // Motors 5 Step Pin
#define PIN_MOTOR_5_DIR    28  // Motors 5 Direction Pin
#define PIN_MOTOR_5_ENABLE 24  // Motors 5 Enable Pin

#define PIN_END_STOP_X_MAX     3  // X Max End Stop Pin
#define PIN_END_STOP_X_MIN     2  // X Min End Stop Pin
#define PIN_END_STOP_Y_MIN     14  // Y Min End Stop Pin
#define PIN_END_STOP_Y_MAX     15  // Y Max End Stop Pin
#define PIN_END_STOP_Z_MIN     18  // Z Min End Stop Pin
#define PIN_END_STOP_Z_MAX     19  // Z Max End Stop Pin

signed long MAX_TARGET_POSITION = 10000000;

const int MOTORS_NUMBER = 5;
int motorSpeeds[MOTORS_NUMBER] = {0, 0, 0, 0, 0};

float ACCELERATION = 500.00f;

bool IS_RUNNING = false;


// The end stop should not be triggered very frequently.
const long END_STOP_TRIGGER_INTERVAL = 5000; 
const long SWITCH_START_STOP_INTERVAL = 1000; 
const long MAX_POSITION_INTERVAL = 10000;
// Marked volatile as these are modified from an interrupt method.
volatile unsigned long END_STOP_TRIGGER_MILLIS_LAST = 0;
volatile unsigned long SWITCH_START_STOP_LAST = 0;
unsigned long MAX_POSITION_INTERVAL_LAST = 0;
volatile signed int ELEVATOR_DIRECTION = 1;

// Which motor should be triggered by the end stop.
int END_STOP_MOTOR_INDEX = 4;

// Motor direction
signed long MOTOR_DIR = 1;

AccelStepper motor1(AccelStepper::DRIVER, PIN_MOTOR_1_STEP, PIN_MOTOR_1_DIR);
AccelStepper motor2(AccelStepper::DRIVER, PIN_MOTOR_2_STEP, PIN_MOTOR_2_DIR);
AccelStepper motor3(AccelStepper::DRIVER, PIN_MOTOR_3_STEP, PIN_MOTOR_3_DIR);
AccelStepper motor4(AccelStepper::DRIVER, PIN_MOTOR_4_STEP, PIN_MOTOR_4_DIR);
AccelStepper motor5(AccelStepper::DRIVER, PIN_MOTOR_5_STEP, PIN_MOTOR_5_DIR);

AccelStepper* motors[MOTORS_NUMBER] = {
  & motor1,
  & motor2,
  & motor3,
  & motor4,
  & motor5
};


void setup() {
  Serial.begin(HILO_SERIAL_BAUDRATE);
  
  Serial.println("Starting up...");
  
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  
  // set the mode for the stepper driver enable pins
  pinMode (PIN_MOTOR_1_ENABLE, OUTPUT);
  pinMode (PIN_MOTOR_2_ENABLE, OUTPUT);
  pinMode (PIN_MOTOR_3_ENABLE, OUTPUT);
  pinMode (PIN_MOTOR_4_ENABLE, OUTPUT);
  pinMode (PIN_MOTOR_5_ENABLE, OUTPUT);

  setupScreenController();
  setupEndStops();
  setSteppersEnabled(false);
}

void loop() {
  serialCommunicationLoop();
  screenControllerLoop();
  runMachineLoop();
  startStopBySwitchTrigger();
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
    if (data.startsWith("m")) {
      int motor = data.substring(1,2).toInt();
      int motorSpeed = data.substring(2).toInt();
      motorSpeeds[motor] = motorSpeed;
      Serial.print("Set motor ");
      Serial.print(motor);
      Serial.print(" to ");
      Serial.println(motorSpeed);
    }
    if (data.startsWith("R")) {
      Serial.println("Reversion motor direction");
      MOTOR_DIR = -MOTOR_DIR;
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
  for(int i = 0; i < MOTORS_NUMBER; i++ ) {
    AccelStepper* motor = motors[i];
    motor->stop();
    motor->setCurrentPosition(0);
  }  
  setSteppersEnabled(false);
}

void startMachine() {
  Serial.println("Starting machine");
  for(int i = 0; i < MOTORS_NUMBER; i++ ) {
    motors[i]->setCurrentPosition(0);
    motors[i]->setMaxSpeed(motorSpeeds[i]);
    motors[i]->setAcceleration(ACCELERATION);
    int moveTarget = MAX_TARGET_POSITION * MOTOR_DIR;
    if (i == END_STOP_MOTOR_INDEX) {
      moveTarget = moveTarget * ELEVATOR_DIRECTION;
    }
    motors[i]->move(moveTarget);
  }
  printMachineSettings();
  
  IS_RUNNING = true;
  MAX_POSITION_INTERVAL_LAST = millis();
  setSteppersEnabled(true);
}

void runMachineLoop() {
  if (IS_RUNNING) {
    for(int i = 0; i < MOTORS_NUMBER; i++ ) {
      AccelStepper* motor = motors[i];
      motor->run();
    }
    // Check if any target positions need updating
    const unsigned long currentMillis = millis();
    if (currentMillis - MAX_POSITION_INTERVAL_LAST > MAX_POSITION_INTERVAL) {
      for(int i = 0; i < MOTORS_NUMBER; i++ ) {
        AccelStepper* motor = motors[i];
        long distanceToGo = motor->distanceToGo();
        if (distanceToGo < 10000) {
          motor->setCurrentPosition(0);
          // Side effect is that speed gets set to 0 so we have to set that again
          motor->setMaxSpeed(motorSpeeds[i]);
          motor->setAcceleration(10000.00f);
          int moveTarget = MAX_TARGET_POSITION * MOTOR_DIR;
          motor->move(moveTarget);
          motor->run();
        }
      }
      MAX_POSITION_INTERVAL_LAST = currentMillis;
    };
  }
} 

// Enables or disables all steppers. Used for saving power
// and allowing adjustments by hand when the machine isn't running.
void setSteppersEnabled(bool enabled) {
  int value = LOW;
  if (!enabled) value = HIGH;

  digitalWrite(PIN_MOTOR_1_ENABLE, value);
  digitalWrite(PIN_MOTOR_2_ENABLE, value);
  digitalWrite(PIN_MOTOR_3_ENABLE, value);
  digitalWrite(PIN_MOTOR_4_ENABLE, value);
  digitalWrite(PIN_MOTOR_5_ENABLE, value);
}

void printMachineSettings() {
  for(int i = 0; i < MOTORS_NUMBER; i++ ) {
    Serial.print("Motor ");
    Serial.print(i);
    Serial.print(" ");
    Serial.println(motorSpeeds[i]);
  }
}

int incrementMotorSpeed(int motorNumber, int direction) {
  int speed = motorSpeeds[motorNumber];
  speed = speed + 25 * direction;
  motorSpeeds[motorNumber] = speed;
  return speed;
}

void setupEndStops() {
  pinMode(PIN_END_STOP_X_MIN, INPUT_PULLUP);
  // Not needed at the moment.
  //pinMode(PIN_END_STOP_X_MAX, INPUT_PULLUP);
  pinMode(PIN_END_STOP_Y_MIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_END_STOP_X_MIN), endStopTrigger, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_END_STOP_X_MAX), endStopTrigger, FALLING);
}

void endStopTrigger() {
  const unsigned long currentMillis = millis();
  if (currentMillis > (END_STOP_TRIGGER_MILLIS_LAST + END_STOP_TRIGGER_INTERVAL)) {
    Serial.println("End stop triggered");
    long currentTarget = motors[END_STOP_MOTOR_INDEX]->targetPosition();
    ELEVATOR_DIRECTION = -ELEVATOR_DIRECTION;
    motors[END_STOP_MOTOR_INDEX]->setCurrentPosition(0);
    motors[END_STOP_MOTOR_INDEX]->move(-currentTarget);
    END_STOP_TRIGGER_MILLIS_LAST = currentMillis;
  }
}

void startStopBySwitchTrigger() {
  int triggered = digitalRead(PIN_END_STOP_Y_MIN);
  const unsigned long currentMillis = millis();
  if (triggered == LOW) {
    if (currentMillis > (SWITCH_START_STOP_LAST + SWITCH_START_STOP_INTERVAL)) {
      Serial.println("Start/Stop endstop triggered");
      SWITCH_START_STOP_LAST = currentMillis;
      startStopMachine();
    }
  }
}
