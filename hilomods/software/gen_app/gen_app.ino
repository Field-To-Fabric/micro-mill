#include <ContinuousStepper.h>

#define HILO_SERIAL_BAUDRATE 115200
#define SERIAL_BAUDRATE 300

// Set to true when connected to a serial output (when the arduino is connected to the computer).
#define DEBUG_ENABLED true

#define PIN_LED               13  // Arduino on-board LED

#define PIN_MOTOR_1_STEP     54  // Motor 1 Step Pin
#define PIN_MOTOR_1_DIR      55  // Motor 1 Direction Pin
#define PIN_MOTOR_1_ENABLE   38  // Motor 1 Enable Pin
#define MOTOR_1_ACTIVE_LEVEL LOW // Some NEMA 23 have this set to LOW, so check your motor specs.

#define PIN_MOTOR_2_STEP     60  // Motor 2 Step Pin
#define PIN_MOTOR_2_DIR      61  // Motor 2 Direction Pin
#define PIN_MOTOR_2_ENABLE   56  // Motor 2 Enable Pin
#define MOTOR_2_ACTIVE_LEVEL LOW // Some NEMA 23 have this set to LOW, so check your motor specs.

#define PIN_MOTOR_3_STEP      46  // Motor 3 Step Pin
#define PIN_MOTOR_3_DIR       48  // Motor 3 Direction Pin
#define PIN_MOTOR_3_ENABLE    62  // Motor 3 Enable Pin
#define MOTOR_3_ACTIVE_LEVEL LOW // Some NEMA 23 have this set to LOW, so check your motor specs.

#define PIN_MOTOR_4_STEP   36  // Motors 4 Step Pin
#define PIN_MOTOR_4_DIR    34  // Motors 4 Direction Pin
#define PIN_MOTOR_4_ENABLE 30  // Motors 4 Enable Pin
#define MOTOR_4_ACTIVE_LEVEL LOW // Some NEMA 23 have this set to LOW, so check your motor specs.

#define PIN_MOTOR_5_STEP   26  // Motors 5 Step Pin
#define PIN_MOTOR_5_DIR    28  // Motors 5 Direction Pin
#define PIN_MOTOR_5_ENABLE 24  // Motors 5 Enable Pin
#define MOTOR_5_ACTIVE_LEVEL LOW // Some NEMA 23 have this set to LOW, so check your motor specs.

#define PIN_END_STOP_X_MAX     3  // X Max End Stop Pin
#define PIN_END_STOP_X_MIN     2  // X Min End Stop Pin 
#define PIN_END_STOP_Y_MIN     14  // Y Min End Stop Pin
#define PIN_END_STOP_Y_MAX     15  // Y Max End Stop Pin
#define PIN_END_STOP_Z_MIN     18  // Z Min End Stop Pin
#define PIN_END_STOP_Z_MAX     19  // Z Max End Stop Pin

// Enable/Disable Features
// The endstop plugs into X MIN, except if you are using the elegoo board and it needs to go into x max...
#define ENABLE_END_STOPS true
// The start/stop trigger should be wired to Z MAX
#define ENABLE_START_STOP_TRIGGER false
#define ENABLE_SD_CARD true
// Connect a sensor using an optocoupler, connect to Z MIN. Wire G (opto) to - and V1 (opto) to S.
#define ENABLE_YARN_BREAK_DETECTION false

// Settings that enable length computation
#define STEPS_PER_REVOLUTION 200
#define DELIVERY_MOTOR_INDEX 4
#define DELIVERY_MOTOR_DIAMETER_MM 30
#define DELIVERY_MOTOR_MICRO_STEPS 8

const int MOTORS_NUMBER = 5;
int m0Speed = 0;
int m1Speed = 0;
int m2Speed = 0;
int m3Speed = 0;
int m4Speed = 0;

int motorSpeeds[MOTORS_NUMBER] = {& m0Speed, & m1Speed, & m2Speed, & m3Speed, & m4Speed};

bool IS_RUNNING = false;

// The end stop should not be triggered very frequently.
const volatile long END_STOP_TRIGGER_INTERVAL = 2000; 
const long SWITCH_START_STOP_INTERVAL = 1000; 
// Marked volatile as these are modified from an interrupt method.
volatile unsigned long END_STOP_TRIGGER_MILLIS_LAST = 0;
volatile signed int ELEVATOR_DIRECTION = 1;
volatile boolean END_STOP_TRIGGERED = false;
unsigned long SWITCH_START_STOP_POLL_LAST = 0;

// Yarn break detection state
const long YARN_BREAK_POLL_INTERVAL = 1250;
long YARN_BREAK_POLL_LAST = 0;
long YARN_BREAK_DETECTED_LAST = 0;

// Which motor should be triggered by the end stop.
int END_STOP_MOTOR_INDEX = 4;

// Motor direction
signed long MOTOR_DIR = 1;

// State for computing length of run
unsigned long RUN_START_MILLIS = 0;
int CURRENT_RUN_STEPS = 0;
float CURRENT_RUN_DISTANCE = 0;

// When enabled, the start/stop signal will be sent through Serial3, so that it
// can be picked up by a second arduino board. 
// The extra serial ports are: Serial1 on pins 19 (RX) and 18 (TX), Serial2 on pins 17 (RX) and 16 (TX), Serial3 on pins 15 (RX) and 14 (TX)
// To use Serial3 connect the Y MIN of board 1 to Y MAX of board 2 (connect TX of board 1 to RX of board 2, we don't need to connect the other way because 
// board 2 doesn't talk back. Wire GND to GND, S to S and V to V. 
// Be careful - directly wiring the serial connection will fry the voltage regulator on the receiving arduino, so you need to isolate the two using an optocoupler (the 817 will
// do at this low baudrate). Wiring (from RAMPS endstop Y MIN to Y MAX): Ymin (+) to OPTO (Vin), Ymin (S) to OPTO (G), OPTO (Vout) to Ymax (+) OPTO (G) to Ymax (S)
int ENABLE_SERIAL_IO = 0;
// Whether this ard
int SERIAL_TRANSMIT = 0;

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
  if (DEBUG_ENABLED) {
    Serial.begin(HILO_SERIAL_BAUDRATE); 
  }
  debugln("Starting up...");
  
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  initMotors();
  setupScreenController();
  if (ENABLE_END_STOPS) {
    setupEndStops();
  }
  if (ENABLE_START_STOP_TRIGGER) {
    setupStartStopTrigger();
  }
  if (ENABLE_SD_CARD) {
    setupSDCard();
    loadSDSettings();
  }
  if (ENABLE_YARN_BREAK_DETECTION) {
    setupYarnBreakDetection();
  }
  if (ENABLE_SERIAL_IO) {
    enableSerialIO(); 
  }
}

void loop() {
  serialCommunicationLoop();
  if (ENABLE_SERIAL_IO && !SERIAL_TRANSMIT){
    serial3CommunicationLoop(); 
  }
  screenControllerLoop();
  runMachineLoop();
  if (ENABLE_START_STOP_TRIGGER) {
    startStopBySwitchTrigger();
  }
  if (ENABLE_YARN_BREAK_DETECTION) {
    yarnBreakTrigger();
  }
}

void initMotors() {
  motor1.begin(PIN_MOTOR_1_STEP, PIN_MOTOR_1_DIR);
  motor1.setEnablePin(PIN_MOTOR_1_ENABLE, MOTOR_1_ACTIVE_LEVEL);
  motor1.powerOff();
  motor2.begin(PIN_MOTOR_2_STEP, PIN_MOTOR_2_DIR);
  motor2.setEnablePin(PIN_MOTOR_2_ENABLE, MOTOR_2_ACTIVE_LEVEL);
  motor2.powerOff();
  motor3.begin(PIN_MOTOR_3_STEP, PIN_MOTOR_3_DIR);
  motor3.setEnablePin(PIN_MOTOR_3_ENABLE, MOTOR_3_ACTIVE_LEVEL);
  motor3.powerOff();
  motor4.begin(PIN_MOTOR_4_STEP, PIN_MOTOR_4_DIR);
  motor4.setEnablePin(PIN_MOTOR_4_ENABLE, MOTOR_4_ACTIVE_LEVEL);
  motor4.powerOff();
  motor5.begin(PIN_MOTOR_5_STEP, PIN_MOTOR_5_DIR);
  motor5.setEnablePin(PIN_MOTOR_5_ENABLE, MOTOR_5_ACTIVE_LEVEL);
  motor5.powerOff();
}

// This loops allows you to send commands to the machine through the Arduino Serial Monitor.
void serialCommunicationLoop() {
  if (Serial.available() > 0) {
    // read a character from serial, if one is available
    String data = Serial.readStringUntil('\n');
    processSerialData(data);
  }
}

void processSerialData(String data) {
  debug("Received: ");
  debug(data);
  debugln("#");
  if (data == " ") {
    startStopMachine();
  }
  if (data.startsWith("s")) {
    startStopMachine();
  }
  if (data.startsWith("m")) {
    int motor = data.substring(1,2).toInt();
    int motorSpeed = data.substring(2).toInt();
    setMotorSpeed(motor, motorSpeed);
  }
  if (data.startsWith("R")) {
    debugln("Reversion motor direction");
    MOTOR_DIR = -MOTOR_DIR;
  }
  if (data.startsWith("IO")) {
    debugln("Toggling Serial IO");
    toggleSerialIO();
  }
  if (data.startsWith("IOT")) {
    debugln("Testing IO Serial");
    emitSerialMessage("ping");
  }
}

void serial3CommunicationLoop() {
  if (Serial3.available() > 0) {
    // read a character from serial, if one is available
    String data = Serial3.readStringUntil('\n');
    debugln("Received on serial 3");
    processSerialData(data);
  }
}

boolean startStopMachine() {
  debugln("Start/stopping machine");
  emitStartStop();
  if (IS_RUNNING) {
    stopMachine();
    storeSDSettings();
  } else {
    startMachine();
    storeSDSettings();
  }
  return IS_RUNNING;
}

void stopMachine() {
  debugln("Stopping machine");
  unsigned long currentMillis = millis();
  updateCurrentRunSteps(currentMillis);
  IS_RUNNING = false;
  for(int i = 0; i < MOTORS_NUMBER; i++ ) {
    ContinuousStepper<StepperDriver>* motor = motors[i];
    motor->stop();
    motor->powerOff();
  }  
}

void startMachine() {
  debugln("Starting machine");
  unsigned long currentMillis = millis();
  RUN_START_MILLIS = currentMillis;
  for(int i = 0; i < MOTORS_NUMBER; i++ ) {
    int *currentSpeed = motorSpeeds[i];
    if (*currentSpeed == 0) {
      // This motor is not doing anything so lets skip it.
      continue;
    }
    int motorSpeed = *currentSpeed * MOTOR_DIR;
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
    if (END_STOP_TRIGGERED) {
      int *currentSpeed = motorSpeeds[END_STOP_MOTOR_INDEX];
      motors[END_STOP_MOTOR_INDEX]->spin((*currentSpeed) * ELEVATOR_DIRECTION);
      END_STOP_TRIGGERED = false;
    }
    for(int i = 0; i < MOTORS_NUMBER; i++ ) {
      ContinuousStepper<StepperDriver>* motor = motors[i];
      if (motor->isPowered()) {
        motor->loop(); 
      }
    }
  }
} 

void printMachineSettings() {
  for(int i = 0; i < MOTORS_NUMBER; i++ ) {
    int *motorSpeed = motorSpeeds[i];
    debug("Motor ");
    debug(i);
    debug(" ");
    debugln(*motorSpeed);
  }
}

void setMotorSpeed(int motorNumber, int newMotorSpeed) {
  int *motorSpeed = motorSpeeds[motorNumber];
  *motorSpeed = newMotorSpeed;
  debug("Set motor ");
  debug(motorNumber);
  debug(" to ");
  debugln(newMotorSpeed);
}

int incrementMotorSpeed(int motorNumber, int direction) {
  int *speed = motorSpeeds[motorNumber];
  *speed = *speed + 25 * direction;
  return speed;
}

void setupEndStops() {
  debugln("Setting up X MIN end stop");
  pinMode(PIN_END_STOP_X_MIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_END_STOP_X_MIN), endStopTrigger, FALLING);
}

void setupStartStopTrigger() {
  pinMode(PIN_END_STOP_Z_MAX, INPUT_PULLUP); 
}

void setupYarnBreakDetection() {
  pinMode(PIN_END_STOP_Z_MIN, INPUT_PULLUP);
}

void endStopTrigger() {
  const unsigned long currentMillis = millis();
  if (currentMillis > (END_STOP_TRIGGER_MILLIS_LAST + END_STOP_TRIGGER_INTERVAL)) {
    debugln("End stop triggered");
    ELEVATOR_DIRECTION = -ELEVATOR_DIRECTION;   
    END_STOP_TRIGGER_MILLIS_LAST = currentMillis;
    END_STOP_TRIGGERED = true;
  }
}

void startStopBySwitchTrigger() {
  const unsigned long currentMillis = millis();
  if (currentMillis > (SWITCH_START_STOP_POLL_LAST + SWITCH_START_STOP_INTERVAL)) {
    // Lets poll
    SWITCH_START_STOP_POLL_LAST = currentMillis;
    int triggered = digitalRead(PIN_END_STOP_Z_MAX);
    if (triggered == LOW) {
      debugln("Start/Stop endstop triggered");
      startStopMachine();
    }
  }
}

void yarnBreakTrigger() {
  if (!IS_RUNNING) {
    // No need to run detection if the machine is not running.
    return;
  }
  const unsigned long currentMillis = millis();
  if (currentMillis > (YARN_BREAK_POLL_LAST + YARN_BREAK_POLL_INTERVAL)) {
    // time to poll
    YARN_BREAK_POLL_LAST = currentMillis;
    int triggered = digitalRead(PIN_END_STOP_Z_MIN);
    // When the yarn is there, the signal gets set to HIGH. A break occurs if we measure LOW three times in a row
    if (triggered == LOW) {
      if ((YARN_BREAK_DETECTED_LAST + (3 * YARN_BREAK_POLL_INTERVAL)) < currentMillis) {
        debugln("Yarn Break Detected");
        stopMachine();
      }
    } else {
      YARN_BREAK_DETECTED_LAST = currentMillis;
    }
  }
}

void updateCurrentRunSteps(unsigned long stopMillis) {
  long runTimeMillis = (stopMillis - RUN_START_MILLIS);
  int *speed = motorSpeeds[DELIVERY_MOTOR_INDEX];
  int effectiveStepsPerRevolution = STEPS_PER_REVOLUTION * DELIVERY_MOTOR_MICRO_STEPS;
  int revolutionDistanceMM = M_PI * DELIVERY_MOTOR_DIAMETER_MM;
  long numberOfSteps_scale1000 = runTimeMillis * *speed;
  long numberOfFullRevolutions_scale1000 = numberOfSteps_scale1000 / effectiveStepsPerRevolution;
  long totalRevolutionDistanceMM_scale1000 = numberOfFullRevolutions_scale1000 * revolutionDistanceMM;
  long totalRevolutionDistanceMM = totalRevolutionDistanceMM_scale1000 / 1000;
  long stepsRemainder = (numberOfSteps_scale1000 / 1000) % effectiveStepsPerRevolution;
  float totalRemainderDistanceMM = stepsRemainder * (float(revolutionDistanceMM) / float(effectiveStepsPerRevolution));
  long totalDistanceMM = totalRevolutionDistanceMM + totalRemainderDistanceMM;
  long totalDistanceCM = totalDistanceMM / 10;
  CURRENT_RUN_DISTANCE += totalDistanceCM / 100.0f;
}

void setCurrentRunDistance(float distance) {
  debugln("Setting current run distance");
  CURRENT_RUN_DISTANCE = distance;
}

void resetRunCounter() {
  CURRENT_RUN_DISTANCE = 0;
}

void emitStartStop() {
  if (SERIAL_TRANSMIT) {
    emitSerialMessage("s");
  }
}

void toggleSerialIO() {
  ENABLE_SERIAL_IO = (ENABLE_SERIAL_IO + 1) % 2;
  if (ENABLE_SERIAL_IO == 0) {
    disableSerialIO();
  } else {
    enableSerialIO();
  }
}

void setEnableSerialIO(int active) {
  ENABLE_SERIAL_IO = active;
}

void setSerialTransmit(int active) {
  SERIAL_TRANSMIT = active;
}

void disableSerialIO() {
  debugln("Disabling serial IO");
  Serial3.end();
}

void enableSerialIO() {
  debugln("Enabling serial IO");
  Serial3.begin(SERIAL_BAUDRATE); 
  emitSerialMessage("Ping");
}

void toggleSerialTransmit() {
  SERIAL_TRANSMIT = (SERIAL_TRANSMIT + 1) % 2;
}

void emitSerialMessage(String msg) {
  if (SERIAL_TRANSMIT) {
    debug("Emitting: ");
    debug(msg);
    debugln("#");
    // Only emit if required.
    Serial3.println(msg); 
  }
}

void debug(char* msg) {
  if (DEBUG_ENABLED) {
    Serial.print(msg);
  }
}

void debugln(char* msg) {
  if (DEBUG_ENABLED) {
    Serial.println(msg);
  }
}

void debug(String msg) {
  if (DEBUG_ENABLED) {
    Serial.print(msg);
  }
}

void debugln(String msg) {
  if (DEBUG_ENABLED) {
    Serial.println(msg);
  }
}

void debug(int msg) {
  if (DEBUG_ENABLED) {
    Serial.print(msg);
  }
}

void debugln(int msg) {
  if (DEBUG_ENABLED) {
    Serial.println(msg);
  }
}
