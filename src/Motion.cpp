#include "Motion.h"
#include "Magnet.h"
#include <Arduino.h>
#include <ctype.h>

// ---------------- PIN SETUP ----------------
//
// Motor A STEP = D3
// Motor A DIR  = D2
// Motor B STEP = D5
// Motor B DIR  = D4
// ENABLE       = D8

const int A_DIR_PIN = 2;
const int A_STEP_PIN = 3;

const int B_DIR_PIN = 4;
const int B_STEP_PIN = 5;

const int ENABLE_PIN = 8;

// Bigger number = slower movement.
// This is the simple speed version that worked before.
int stepDelayUs = 2200;

const int STEP_PULSE_US = 5;
const int MOVE_SETTLE_DELAY_MS = 120;

// Change these only if a motor direction itself is wrong.
const bool INVERT_MOTOR_A = false;
const bool INVERT_MOTOR_B = false;

// Software position tracking
long currentX = 0;
long currentY = 0;

static void stepMotorA(int dir);
static void stepMotorB(int dir);
static void stepCoreXYStep(int xDir, int yDir);
static bool checkEmergencySerial();

namespace Motion {

  void begin() {
    pinMode(A_DIR_PIN, OUTPUT);
    pinMode(A_STEP_PIN, OUTPUT);

    pinMode(B_DIR_PIN, OUTPUT);
    pinMode(B_STEP_PIN, OUTPUT);

    pinMode(ENABLE_PIN, OUTPUT);

    digitalWrite(A_STEP_PIN, LOW);
    digitalWrite(B_STEP_PIN, LOW);

    // A4988 enable is active-low
    digitalWrite(ENABLE_PIN, LOW);

    Serial.println(F("Motion system ready."));
  }

  void zeroPosition() {
    currentX = 0;
    currentY = 0;

    Serial.println(F("Motion position zeroed."));
  }

  long getX() {
    return currentX;
  }

  long getY() {
    return currentY;
  }

  bool moveTo(long targetX, long targetY) {
    long dx = targetX - currentX;
    long dy = targetY - currentY;

    long stepsX = abs(dx);
    long stepsY = abs(dy);

    int xDir = 0;
    int yDir = 0;

    if (dx > 0) xDir = 1;
    else if (dx < 0) xDir = -1;

    if (dy > 0) yDir = 1;
    else if (dy < 0) yDir = -1;

    long totalSteps = max(stepsX, stepsY);

    if (totalSteps == 0) {
      delay(MOVE_SETTLE_DELAY_MS);
      return true;
    }

    long errorX = 0;
    long errorY = 0;

    for (long i = 0; i < totalSteps; i++) {
      if (checkEmergencySerial()) {
        Magnet::forceOff();
        Serial.println(F("ERR MOVE_ABORTED_BY_USER"));
        delay(500);
        return false;
      }

      errorX += stepsX;
      errorY += stepsY;

      int doX = 0;
      int doY = 0;

      if (errorX >= totalSteps) {
        doX = xDir;
        errorX -= totalSteps;
      }

      if (errorY >= totalSteps) {
        doY = yDir;
        errorY -= totalSteps;
      }

      stepCoreXYStep(doX, doY);

      currentX += doX;
      currentY += doY;
    }

    delay(MOVE_SETTLE_DELAY_MS);
    return true;
  }
}

// ---------------- EMERGENCY SERIAL CHECK ----------------

static bool checkEmergencySerial() {
  if (Serial.available() <= 0) {
    return false;
  }

  char ch = Serial.read();

  if (ch == '\r' || ch == '\n' || ch == ' ') {
    return false;
  }

  ch = tolower(ch);

  if (ch == '!' || ch == 'f' || ch == 'o' || ch == 'x') {
    Magnet::forceOff();
    Serial.print(F("Emergency magnet off during move. Command: "));
    Serial.println(ch);
    return true;
  }

  return false;
}

// ---------------- COREXY STEPPING ----------------
//
// CoreXY mapping:
// +X => A +, B +
// -X => A -, B -
// +Y => A +, B -
// -Y => A -, B +

static void stepCoreXYStep(int xDir, int yDir) {
  int aDir = xDir + yDir;
  int bDir = xDir - yDir;

  if (aDir > 0) {
    stepMotorA(1);
  }
  else if (aDir < 0) {
    stepMotorA(-1);
  }

  if (bDir > 0) {
    stepMotorB(1);
  }
  else if (bDir < 0) {
    stepMotorB(-1);
  }
}

static void stepMotorA(int dir) {
  bool level = dir > 0;

  if (INVERT_MOTOR_A) {
    level = !level;
  }

  digitalWrite(A_DIR_PIN, level ? HIGH : LOW);
  delayMicroseconds(3);

  digitalWrite(A_STEP_PIN, HIGH);
  delayMicroseconds(STEP_PULSE_US);
  digitalWrite(A_STEP_PIN, LOW);

  delayMicroseconds(stepDelayUs);
}

static void stepMotorB(int dir) {
  bool level = dir > 0;

  if (INVERT_MOTOR_B) {
    level = !level;
  }

  digitalWrite(B_DIR_PIN, level ? HIGH : LOW);
  delayMicroseconds(3);

  digitalWrite(B_STEP_PIN, HIGH);
  delayMicroseconds(STEP_PULSE_US);
  digitalWrite(B_STEP_PIN, LOW);

  delayMicroseconds(stepDelayUs);
}