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

#define PIN_END_STOP_X_MIN     3  // X Min End Stop Pin
#define PIN_END_STOP_X_MAX     2  // X Max End Stop Pin

const int MOTORS_NUMBER = 5;
int motorSpeeds[MOTORS_NUMBER] = {0, 0, 0, 0, 0};

float ACCELERATION = 500.00f;

bool IS_RUNNING = false;

// The end stop should not be triggered very frequently.
const long END_STOP_TRIGGER_INTERVAL = 5000; 
// Marked volatile as these are modified from an interrupt method.
volatile unsigned long END_STOP_TRIGGER_MILLIS_LAST = 0;

// Which motor should be triggered by the end stop.
int END_STOP_MOTOR_INDEX = 4;

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
    AccelStepper motor = motors[i];
    motor.stop();
    motor.setCurrentPosition(0);
  }  
  setSteppersEnabled(false);
}

void startMachine() {
  Serial.println("Starting machine");
  for(int i = 0; i < MOTORS_NUMBER; i++ ) {
    motors[i]->setMaxSpeed(motorSpeeds[i]);
    motors[i]->setAcceleration(ACCELERATION);
    motors[i]->move(-1000000);
  }
  printMachineSettings();
  
  IS_RUNNING = true;
  setSteppersEnabled(true);
}

void runMachineLoop() {
  if (IS_RUNNING) {
    for(int i = 0; i < MOTORS_NUMBER; i++ ) {
      AccelStepper* motor = motors[i];
      motor->run();
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
  speed = speed + 50 * direction;
  motorSpeeds[motorNumber] = speed;
  return speed;
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
    Serial.println("End stop triggered");
    long currentTarget = motors[END_STOP_MOTOR_INDEX]->targetPosition();
    motors[END_STOP_MOTOR_INDEX]->setCurrentPosition(0);
    motors[END_STOP_MOTOR_INDEX]->move(-currentTarget);
    END_STOP_TRIGGER_MILLIS_LAST = currentMillis;
  }
}
