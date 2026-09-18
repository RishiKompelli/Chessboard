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
// Smaller number = faster movement.
int stepDelayUs = 1200;
int lineStepDelayUs = 1600;

const int STEP_PULSE_US = 5;

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

  void setPosition(long x, long y) {
    currentX = x;
    currentY = y;

    Serial.print(F("Position manually set: X="));
    Serial.print(currentX);
    Serial.print(F(" Y="));
    Serial.println(currentY);
  }

  long getX() {
    return currentX;
  }

  long getY() {
    return currentY;
  }

  void jogStep(int xDir, int yDir) {
    if (xDir > 0) xDir = 1;
    else if (xDir < 0) xDir = -1;

    if (yDir > 0) yDir = 1;
    else if (yDir < 0) yDir = -1;

    if (xDir == 0 && yDir == 0) {
      return;
    }

    stepCoreXYStep(xDir, yDir);

    currentX += xDir;
    currentY += yDir;
  }

  bool goTo(long targetX, long targetY) {
    // Automatic movement uses the exact same stepping as WASD.
    // Horizontal first, then vertical.
    // No separate movement style. No stop-start delay.

    while (currentX != targetX) {
      if (checkEmergencySerial()) {
        Magnet::forceOff();
        Serial.println(F("ERR MOVE_ABORTED_BY_USER"));
        return false;
      }

      if (currentX < targetX) {
        jogStep(1, 0);
      }
      else {
        jogStep(-1, 0);
      }
    }

    while (currentY != targetY) {
      if (checkEmergencySerial()) {
        Magnet::forceOff();
        Serial.println(F("ERR MOVE_ABORTED_BY_USER"));
        return false;
      }

      if (currentY < targetY) {
        jogStep(0, 1);
      }
      else {
        jogStep(0, -1);
      }
    }

    return true;
  }

  bool goLine(long targetX, long targetY) {
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
      return true;
    }

    Serial.print(F("GO_LINE_START currentX="));
    Serial.print(currentX);
    Serial.print(F(" currentY="));
    Serial.print(currentY);
    Serial.print(F(" targetX="));
    Serial.print(targetX);
    Serial.print(F(" targetY="));
    Serial.println(targetY);

    long errorX = 0;
    long errorY = 0;

    int oldStepDelayUs = stepDelayUs;

    // ---------------- ACCELERATION SETTINGS ----------------

    // Start slow so the motor does not grind / skip steps.
    const int START_DELAY_US = 4000;

    // Normal diagonal running speed.
    const int RUN_DELAY_US = 1800;

    // Number of logical steps used to accelerate.
    const long RAMP_STEPS = 250;

    // -------------------------------------------------------

    for (long i = 0; i < totalSteps; i++) {

      if (checkEmergencySerial()) {
        stepDelayUs = oldStepDelayUs;
        Magnet::forceOff();
        Serial.println(F("ERR GO_LINE_ABORTED"));
        return false;
      }

      // Slowly accelerate only at the beginning.
      if (i < RAMP_STEPS) {
        long delayRange = START_DELAY_US - RUN_DELAY_US;

        stepDelayUs =
            START_DELAY_US -
            ((long)delayRange * i / RAMP_STEPS);
      }
      else {
        stepDelayUs = RUN_DELAY_US;
      }

      errorX += stepsX;
      errorY += stepsY;

      int stepX = 0;
      int stepY = 0;

      if (errorX >= totalSteps) {
        stepX = xDir;
        errorX -= totalSteps;
      }

      if (errorY >= totalSteps) {
        stepY = yDir;
        errorY -= totalSteps;
      }

      jogStep(stepX, stepY);
    }

    stepDelayUs = oldStepDelayUs;

    Serial.print(F("GO_LINE_DONE currentX="));
    Serial.print(currentX);
    Serial.print(F(" currentY="));
    Serial.println(currentY);

    return true;
  }

  bool testGoLineRelative(long dx, long dy) {
    long targetX = currentX + dx;
    long targetY = currentY + dy;

    Serial.print(F("TEST_GO_LINE_RELATIVE dx="));
    Serial.print(dx);
    Serial.print(F(" dy="));
    Serial.println(dy);

    return goLine(targetX, targetY);
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
    Serial.print(F("Emergency magnet off during movement. Command: "));
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