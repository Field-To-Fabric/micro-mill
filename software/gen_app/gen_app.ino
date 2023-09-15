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

int MOTOR_1_STEPS = 100;
int MOTOR_2_STEPS = 100;
int MOTOR_3_STEPS = 100;
int MOTOR_4_STEPS = 100;
int MOTOR_5_STEPS = 100;

float ACCELERATION = 500.00f;

bool IS_RUNNING = false;

AccelStepper motor1(AccelStepper::DRIVER, PIN_MOTOR_1_STEP, PIN_MOTOR_1_DIR);
AccelStepper motor2(AccelStepper::DRIVER, PIN_MOTOR_2_STEP, PIN_MOTOR_2_DIR);
AccelStepper motor3(AccelStepper::DRIVER, PIN_MOTOR_4_STEP, PIN_MOTOR_4_DIR);
AccelStepper motor4(AccelStepper::DRIVER, PIN_MOTOR_3_STEP, PIN_MOTOR_3_DIR);
AccelStepper motor5(AccelStepper::DRIVER, PIN_MOTOR_5_STEP, PIN_MOTOR_5_DIR);

void setup() {
  Serial.begin(HILO_SERIAL_BAUDRATE);
  
  Serial.println("Starting up...");
  
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  
  // set the mode for the stepper driver enable pins
  pinMode (PIN_MOTOR_1_ENABLE, OUTPUT);
  pinMode (PIN_MOTOR_2_ENABLE, OUTPUT);
  pinMode (PIN_MOTOR_4_ENABLE, OUTPUT);
  pinMode (PIN_MOTOR_3_ENABLE, OUTPUT);
  pinMode (PIN_MOTOR_5_ENABLE, OUTPUT);

  setSteppersEnabled(false);
}

void loop() {
  serialCommunicationLoop();
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
      int speed = data.substring(2).toInt();
      switch (motor) {
        case 1: 
          MOTOR_1_STEPS = speed;
          break;
        case 2: 
          MOTOR_2_STEPS = speed;
          break;
        case 3: 
          MOTOR_3_STEPS = speed;
          break;
        case 4: 
          MOTOR_4_STEPS = speed;
          break;
        case 5:
          MOTOR_5_STEPS = speed;
          break;
        default:
          Serial.print("Unrecognized motor number ");
          Serial.println(motor);
      }
      Serial.print("Set motor ");
      Serial.print(motor);
      Serial.print(" to ");
      Serial.println(speed);
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
  motor1.stop();
  motor1.setCurrentPosition(0);
  motor2.stop();
  motor2.setCurrentPosition(0);
  motor3.stop();
  motor3.setCurrentPosition(0);
  motor4.stop();
  motor4.setCurrentPosition(0);
  motor5.stop();
  motor5.setCurrentPosition(0);

  setSteppersEnabled(false);
}

void startMachine() {
  Serial.println("Starting machine");

  motor1.setMaxSpeed(MOTOR_1_STEPS);
  motor1.setAcceleration(ACCELERATION);
  motor1.move(-1000000);
  motor2.setMaxSpeed(MOTOR_1_STEPS);
  motor2.setAcceleration(ACCELERATION);
  motor2.move(-1000000);
  motor3.setMaxSpeed(MOTOR_1_STEPS);
  motor3.setAcceleration(ACCELERATION);
  motor3.move(-1000000);
  motor4.setMaxSpeed(MOTOR_1_STEPS);
  motor4.setAcceleration(ACCELERATION);
  motor4.move(-1000000);
  motor5.setMaxSpeed(MOTOR_1_STEPS);
  motor5.setAcceleration(ACCELERATION);
  motor5.move(-1000000);

  printMachineSettings();
  
  IS_RUNNING = true;
  setSteppersEnabled(true);
}

void runMachineLoop() {
  if (IS_RUNNING) {
      motor1.run();
      motor2.run();
      motor3.run();
      motor4.run();
      motor5.run();
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
  Serial.print("Motor 1: ");
  Serial.println(MOTOR_1_STEPS);
  Serial.print("Motor 2: ");
  Serial.println(MOTOR_2_STEPS);
  Serial.print("Motor 3: ");
  Serial.println(MOTOR_3_STEPS);
  Serial.print("Motor 4: ");
  Serial.println(MOTOR_4_STEPS);
  Serial.print("Motor 5: ");
  Serial.println(MOTOR_5_STEPS);
}
