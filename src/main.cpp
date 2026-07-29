#include <Arduino.h>
#include "Motion.h"
#include "Calibration.h"
#include "Magnet.h"
#include "BoardState.h"

void printHelp();
void processSingleCommand(char cmd);
void processMoveInput(char cmd);
bool isValidMoveChar(char cmd, int index);

bool enteringMove = false;
char moveBuffer[5];
int moveIndex = 0;

void setup() {
  Serial.begin(9600);
  delay(2000);

  Serial.println();
  Serial.println(F("=== CHESSBOARD CODE STARTED VERSION TEST 1 ==="));

  Motion::begin();
  Serial.println(F("Motion started"));

  Magnet::begin();
  Serial.println(F("Magnet started"));

  BoardState::reset();
  Serial.println(F("Board state reset"));

  Calibration::loadCalibration();
  Serial.println(F("Calibration load attempted"));

  Serial.println(F("CoreXY Chessboard Controller Ready"));
  printHelp();
}

void loop() {
  while (Serial.available() > 0) {
    char cmd = Serial.read();

    if (cmd == '\n' || cmd == '\r' || cmd == ' ') {
      continue;
    }

    cmd = tolower(cmd);

    if (enteringMove) {
      processMoveInput(cmd);
    } else {
      processSingleCommand(cmd);
    }
  }

  Motion::update();
}

void processSingleCommand(char cmd) {
  if (cmd == 'w') {
    Motion::setJog(0, 1);
    Serial.println(F("Moving up"));
  }
  else if (cmd == 's') {
    Motion::setJog(0, -1);
    Serial.println(F("Moving down"));
  }
  else if (cmd == 'a') {
    Motion::setJog(-1, 0);
    Serial.println(F("Moving left"));
  }
  else if (cmd == 'd') {
    Motion::setJog(1, 0);
    Serial.println(F("Moving right"));
  }
  else if (cmd == 'x') {
    Motion::stop();
    Magnet::off();
    enteringMove = false;
    moveIndex = 0;
    Serial.println(F("Stopped / aborted"));
  }
  else if (cmd == '+') {
    Motion::speedUp();
  }
  else if (cmd == '-') {
    Motion::slowDown();
  }
  else if (cmd == 'z') {
    Calibration::zeroPosition();
  }
  else if (cmd == 'p') {
    Calibration::printPosition();
  }
  else if (cmd == 'm') {
    Calibration::setBoardMax();
  }
  else if (cmd == 'c') {
  Calibration::startFourCornerCalibration();
  }
  else if (cmd == 'k') {
    Calibration::recordCalibrationPoint();
  }
  else if (cmd == 'g') {
    Calibration::printGrid();
  }
  else if (cmd == 't') {
    Calibration::testAllSquares();
  }
  else if (cmd == 'o') {
    Magnet::on();
  }
  else if (cmd == 'f') {
    Magnet::off();
  }
  else if (cmd == 'v') {
    Magnet::toggle();
  }
  else if (cmd == '1') {
    Motion::rawMotorTest('A', 1);
  }
  else if (cmd == '2') {
    Motion::rawMotorTest('A', -1);
  }
  else if (cmd == '3') {
    Motion::rawMotorTest('B', 1);
  }
  else if (cmd == '4') {
    Motion::rawMotorTest('B', -1);
  }
  else if (cmd == 'r') {
    enteringMove = true;
    moveIndex = 0;

    Motion::stop();

    Serial.println(F("Move mode started."));
    Serial.println(F("Enter move as 4 chars, like d2d3."));
    Serial.println(F("Send x to cancel."));
  }
  else if (cmd == 'h') {
    printHelp();
  }
  else if (cmd == 'b') {
  BoardState::print();
  }
  else if (cmd == 'i') {
    BoardState::reset();
    BoardState::print();
  }
  else {
    Serial.print(F("Unknown command: "));
    Serial.println(cmd);
  }
}

void processMoveInput(char cmd) {
  if (cmd == 'x') {
    enteringMove = false;
    moveIndex = 0;
    Magnet::off();
    Serial.println(F("Move input cancelled."));
    return;
  }

  if (!isValidMoveChar(cmd, moveIndex)) {
    Serial.print(F("Invalid move character: "));
    Serial.println(cmd);
    Serial.println(F("Restarting move input. Enter something like d2d3."));
    moveIndex = 0;
    return;
  }

  moveBuffer[moveIndex] = cmd;
  moveIndex++;

  Serial.print(F("Move input: "));
  for (int i = 0; i < moveIndex; i++) {
    Serial.print(moveBuffer[i]);
  }
  Serial.println();

  if (moveIndex == 4) {
    moveBuffer[4] = '\0';

    char fromFile = moveBuffer[0];
    char fromRank = moveBuffer[1];
    char toFile = moveBuffer[2];
    char toRank = moveBuffer[3];

    enteringMove = false;
    moveIndex = 0;

    Calibration::movePieceSafe(fromFile, fromRank, toFile, toRank);
  }
}

bool isValidMoveChar(char cmd, int index) {
  // index 0 = from file
  // index 1 = from rank
  // index 2 = to file
  // index 3 = to rank

  if (index == 0 || index == 2) {
    return cmd >= 'a' && cmd <= 'h';
  }

  if (index == 1 || index == 3) {
    return cmd >= '1' && cmd <= '8';
  }

  return false;
}

void printHelp() {
  Serial.println();
  Serial.println(F("Commands:"));
  Serial.println(F("w/a/s/d = manual move"));
  Serial.println(F("x = stop / abort"));
  Serial.println(F("+ = faster"));
  Serial.println(F("- = slower"));
  Serial.println(F("z = zero position at a1 / bottom-left"));
  Serial.println(F("p = print position"));
  Serial.println(F("m = set current position as h8 / top-right"));
  Serial.println(F("c = start 4-corner calibration"));
  Serial.println(F("k = save current calibration corner"));
  Serial.println(F("g = print grid"));
  Serial.println(F("t = test all 64 squares"));
  Serial.println(F("o = electromagnet on"));
  Serial.println(F("f = electromagnet off"));
  Serial.println(F("v = toggle electromagnet"));
  Serial.println(F("r = enter chess move mode"));
  Serial.println(F("After r, type move like d2d3"));
  Serial.println(F("1/2 = test Motor A"));
  Serial.println(F("3/4 = test Motor B"));
  Serial.println(F("b = print board state"));
  Serial.println(F("i = reset board state"));
  Serial.println(F("h = help"));
  Serial.println();
}
