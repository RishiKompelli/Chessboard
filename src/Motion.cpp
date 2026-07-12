#include "Motion.h"

// ---------- PINS ----------
// Motor A
const int A_STEP_PIN = 3;
const int A_DIR_PIN  = 2;

// Motor B
const int B_STEP_PIN = 5;
const int B_DIR_PIN  = 4;

const int ENABLE_PIN = 8;

// ---------- DIRECTION FIXES ----------
const bool MOTOR_A_INVERT = false;
const bool MOTOR_B_INVERT = false;

const bool X_AXIS_INVERT = false;
const bool Y_AXIS_INVERT = true;

// ---------- MOVEMENT STATE ----------
int stepDelay = 700; // lower = faster

bool moving = false;

int jogX = 0;
int jogY = 0;

// Virtual board position, not motor position.
long currentX = 0;
long currentY = 0;

// ---------- INTERNAL FUNCTIONS ----------
void coreXYStep(int xDir, int yDir);
int applyInvert(int dir, bool invert);
bool checkAbort();

namespace Motion {

  void begin() {
    pinMode(A_STEP_PIN, OUTPUT);
    pinMode(A_DIR_PIN, OUTPUT);

    pinMode(B_STEP_PIN, OUTPUT);
    pinMode(B_DIR_PIN, OUTPUT);

    pinMode(ENABLE_PIN, OUTPUT);

    // A4988: LOW = enabled
    digitalWrite(ENABLE_PIN, LOW);

    stop();
  }

  void update() {
    if (moving) {
      coreXYStep(jogX, jogY);
    }
  }

  void setJog(int xDirection, int yDirection) {
    jogX = xDirection;
    jogY = yDirection;
    moving = true;
  }

  void stop() {
    moving = false;
    jogX = 0;
    jogY = 0;
  }

  void speedUp() {
    stepDelay -= 100;

    if (stepDelay < 150) {
      stepDelay = 150;
    }

    Serial.print("Speed up. Step delay = ");
    Serial.println(stepDelay);
  }

  void slowDown() {
    stepDelay += 100;

    Serial.print("Slow down. Step delay = ");
    Serial.println(stepDelay);
  }

  void zeroPosition() {
    currentX = 0;
    currentY = 0;
  }

  long getX() {
    return currentX;
  }

  long getY() {
    return currentY;
  }

  int getStepDelay() {
    return stepDelay;
  }

  void rawMotorTest(char motor, int direction, long steps) {
    stop();

    int stepPin;
    int dirPin;

    if (motor == 'A') {
      stepPin = A_STEP_PIN;
      dirPin = A_DIR_PIN;
      Serial.println("Testing Motor A");
    }
    else if (motor == 'B') {
      stepPin = B_STEP_PIN;
      dirPin = B_DIR_PIN;
      Serial.println("Testing Motor B");
    }
    else {
      Serial.println("Invalid motor.");
      return;
    }

    Serial.print("Direction = ");
    Serial.println(direction);

    digitalWrite(dirPin, direction > 0 ? HIGH : LOW);
    delay(10);

    for (long i = 0; i < steps; i++) {
      digitalWrite(stepPin, HIGH);
      delayMicroseconds(stepDelay);
      digitalWrite(stepPin, LOW);
      delayMicroseconds(stepDelay);
    }

    Serial.println("Raw motor test done.");
  }

  bool moveTo(long targetX, long targetY) {
    stop();

    Serial.print("Moving to X=");
    Serial.print(targetX);
    Serial.print(", Y=");
    Serial.println(targetY);

    // Move X first.
    while (currentX != targetX) {
      if (checkAbort()) {
        Serial.println("Move aborted.");
        stop();
        return false;
      }

      int xDir = targetX > currentX ? 1 : -1;
      coreXYStep(xDir, 0);
    }

    // Then move Y.
    while (currentY != targetY) {
      if (checkAbort()) {
        Serial.println("Move aborted.");
        stop();
        return false;
      }

      int yDir = targetY > currentY ? 1 : -1;
      coreXYStep(0, yDir);
    }

    Serial.println("Move complete.");
    return true;
  }
}

// ---------- COREXY MOTION ----------

void coreXYStep(int xDir, int yDir) {
  if (xDir == 0 && yDir == 0) {
    return;
  }

  int logicalX = xDir;
  int logicalY = yDir;

  int physicalX = X_AXIS_INVERT ? -logicalX : logicalX;
  int physicalY = Y_AXIS_INVERT ? -logicalY : logicalY;

  /*
    CoreXY transform:

    Motor A = X + Y
    Motor B = X - Y
  */

  int motorADir = physicalX + physicalY;
  int motorBDir = physicalX - physicalY;

  if (motorADir > 0) motorADir = 1;
  if (motorADir < 0) motorADir = -1;

  if (motorBDir > 0) motorBDir = 1;
  if (motorBDir < 0) motorBDir = -1;

  motorADir = applyInvert(motorADir, MOTOR_A_INVERT);
  motorBDir = applyInvert(motorBDir, MOTOR_B_INVERT);

  if (motorADir != 0) {
    digitalWrite(A_DIR_PIN, motorADir > 0 ? HIGH : LOW);
  }

  if (motorBDir != 0) {
    digitalWrite(B_DIR_PIN, motorBDir > 0 ? HIGH : LOW);
  }

  if (motorADir != 0) {
    digitalWrite(A_STEP_PIN, HIGH);
  }

  if (motorBDir != 0) {
    digitalWrite(B_STEP_PIN, HIGH);
  }

  delayMicroseconds(stepDelay);

  if (motorADir != 0) {
    digitalWrite(A_STEP_PIN, LOW);
  }

  if (motorBDir != 0) {
    digitalWrite(B_STEP_PIN, LOW);
  }

  delayMicroseconds(stepDelay);

  // Track logical board coordinates.
  currentX += logicalX;
  currentY += logicalY;
}

int applyInvert(int dir, bool invert) {
  if (invert) {
    return -dir;
  }

  return dir;
}

bool checkAbort() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();

    if (cmd == 'x') {
      return true;
    }
  }

  return false;
}