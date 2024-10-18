#include <ContinuousStepper.h>

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

const int MOTORS_NUMBER = 5;
int motorSpeeds[MOTORS_NUMBER] = {0, 0, 0, 0, 0};

float ACCELERATION = 500.00f;

bool IS_RUNNING = false;
bool ENABLE_END_STOPS = true;


// The end stop should not be triggered very frequently.
const long END_STOP_TRIGGER_INTERVAL = 5000; 
const long SWITCH_START_STOP_INTERVAL = 1000; 
// Marked volatile as these are modified from an interrupt method.
volatile unsigned long END_STOP_TRIGGER_MILLIS_LAST = 0;
volatile unsigned long SWITCH_START_STOP_LAST = 0;
volatile signed int ELEVATOR_DIRECTION = 1;

// Which motor should be triggered by the end stop.
int END_STOP_MOTOR_INDEX = 4;

// Motor direction
signed long MOTOR_DIR = 1;

// Variables for measuring run of delivery
int DELIVERY_MOTOR_INDEX = 4;
float DELIVERY_MOTOR_DIAMETER_MM = 30.0f;
float DELIVERY_MOTOR_MICRO_STEPS = 8.0f;
int RUN_START_MILLIS = 0;
float CURRENT_RUN_STEPS = 0.0f;
float CURRENT_RUN_DISTANCE = 0.0f; // Should always be set with CURRENT_RUN_STEPS
char CURRENT_RUN_DISTANCE_STRING[10] = "0";

ContinuousStepper<StepperDriver> motor1;
ContinuousStepper<StepperDriver> motor2;
ContinuousStepper<StepperDriver> motor3;
ContinuousStepper<StepperDriver> motor4;
ContinuousStepper<StepperDriver> motor5;

ContinuousStepper<StepperDriver>* motors[MOTORS_NUMBER] = {
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
  initMotors();
  setupScreenController();
  if (ENABLE_END_STOPS) {
    setupEndStops();
  }
  setSteppersEnabled(false);
}

void loop() {
  serialCommunicationLoop();
  screenControllerLoop();
  runMachineLoop();
  if (ENABLE_END_STOPS) {
    startStopBySwitchTrigger();
  }
}

void initMotors() {
  // set the mode for the stepper driver enable pins
  pinMode (PIN_MOTOR_1_ENABLE, OUTPUT);
  pinMode (PIN_MOTOR_2_ENABLE, OUTPUT);
  pinMode (PIN_MOTOR_3_ENABLE, OUTPUT);
  pinMode (PIN_MOTOR_4_ENABLE, OUTPUT);
  pinMode (PIN_MOTOR_5_ENABLE, OUTPUT);
  
  motor1.begin(PIN_MOTOR_1_STEP, PIN_MOTOR_1_DIR);
  motor1.setEnablePin(PIN_MOTOR_1_ENABLE, LOW);
  motor2.begin(PIN_MOTOR_2_STEP, PIN_MOTOR_2_DIR);
  motor2.setEnablePin(PIN_MOTOR_2_ENABLE, LOW);
  motor3.begin(PIN_MOTOR_3_STEP, PIN_MOTOR_3_DIR);
  motor3.setEnablePin(PIN_MOTOR_3_ENABLE, LOW);
  motor4.begin(PIN_MOTOR_4_STEP, PIN_MOTOR_4_DIR);
  motor4.setEnablePin(PIN_MOTOR_4_ENABLE, LOW);
  motor5.begin(PIN_MOTOR_5_STEP, PIN_MOTOR_5_DIR);
  motor5.setEnablePin(PIN_MOTOR_5_ENABLE, LOW);
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
  unsigned long currentMillis = millis();
  updateCurrentRunSteps(currentMillis);
  IS_RUNNING = false;
  for(int i = 0; i < MOTORS_NUMBER; i++ ) {
    ContinuousStepper<StepperDriver>* motor = motors[i];
    motor->stop();
    motor->powerOff();
  }  
  setSteppersEnabled(false);
}

void startMachine() {
  Serial.println("Starting machine");
  unsigned long currentMillis = millis();
  RUN_START_MILLIS = currentMillis;
  setSteppersEnabled(true);
  for(int i = 0; i < MOTORS_NUMBER; i++ ) {
    int motorSpeed = motorSpeeds[i] * MOTOR_DIR;
    if (i == END_STOP_MOTOR_INDEX) {
      motorSpeed = motorSpeed * ELEVATOR_DIRECTION;
    }
    motors[i]->powerOn();
    motors[i]->spin(motorSpeed);
  }
  printMachineSettings(); 
  IS_RUNNING = true; 
}

void runMachineLoop() {
  if (IS_RUNNING) {
    for(int i = 0; i < MOTORS_NUMBER; i++ ) {
      ContinuousStepper<StepperDriver>* motor = motors[i];
      motor->loop();
    }
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
  //attachInterrupt(digitalPinToInterrupt(PIN_END_STOP_X_MAX), endStopTrigger, FALLING);
}

void endStopTrigger() {
  const unsigned long currentMillis = millis();
  if (currentMillis > (END_STOP_TRIGGER_MILLIS_LAST + END_STOP_TRIGGER_INTERVAL)) {
    Serial.println("End stop triggered");
    ELEVATOR_DIRECTION = -ELEVATOR_DIRECTION;
    motors[END_STOP_MOTOR_INDEX]->spin(motorSpeeds[END_STOP_MOTOR_INDEX]*ELEVATOR_DIRECTION);
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

void updateCurrentRunSteps(unsigned long stopMillis) {
  int runTime = (stopMillis - RUN_START_MILLIS);
  float speed = float(motorSpeeds[DELIVERY_MOTOR_INDEX]);
  float totalSteps = ((float(runTime) / 1000.0f) * speed) / DELIVERY_MOTOR_MICRO_STEPS;
  CURRENT_RUN_STEPS = float(abs(totalSteps));
  CURRENT_RUN_DISTANCE = ((3.14f * (DELIVERY_MOTOR_DIAMETER_MM / 1000.0f)) / 360.0f) * 1.8 * CURRENT_RUN_STEPS;
  dtostrf(CURRENT_RUN_DISTANCE, 6, 2, CURRENT_RUN_DISTANCE_STRING);
  RUN_START_MILLIS = 0;
}
