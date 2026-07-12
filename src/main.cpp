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

  Motion::begin();
  Magnet::begin();
  BoardState::reset();

  Serial.println("CoreXY Chessboard Controller Ready");
  BoardState::print();

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
    Serial.println("Moving up");
  }
  else if (cmd == 's') {
    Motion::setJog(0, -1);
    Serial.println("Moving down");
  }
  else if (cmd == 'a') {
    Motion::setJog(-1, 0);
    Serial.println("Moving left");
  }
  else if (cmd == 'd') {
    Motion::setJog(1, 0);
    Serial.println("Moving right");
  }
  else if (cmd == 'x') {
    Motion::stop();
    Magnet::off();
    enteringMove = false;
    moveIndex = 0;
    Serial.println("Stopped / aborted");
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

    Serial.println("Move mode started.");
    Serial.println("Enter move as 4 chars, like d2d3.");
    Serial.println("Send x to cancel.");
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
    Serial.print("Unknown command: ");
    Serial.println(cmd);
  }
}

void processMoveInput(char cmd) {
  if (cmd == 'x') {
    enteringMove = false;
    moveIndex = 0;
    Magnet::off();
    Serial.println("Move input cancelled.");
    return;
  }

  if (!isValidMoveChar(cmd, moveIndex)) {
    Serial.print("Invalid move character: ");
    Serial.println(cmd);
    Serial.println("Restarting move input. Enter something like d2d3.");
    moveIndex = 0;
    return;
  }

  moveBuffer[moveIndex] = cmd;
  moveIndex++;

  Serial.print("Move input: ");
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

    Calibration::movePiece(fromFile, fromRank, toFile, toRank);
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
  Serial.println("Commands:");
  Serial.println("w/a/s/d = manual move");
  Serial.println("x = stop / abort");
  Serial.println("+ = faster");
  Serial.println("- = slower");
  Serial.println("z = zero position at a1 / bottom-left");
  Serial.println("p = print position");
  Serial.println("m = set current position as h8 / top-right");
  Serial.println("g = print grid");
  Serial.println("t = test all 64 squares");
  Serial.println("o = electromagnet on");
  Serial.println("f = electromagnet off");
  Serial.println("v = toggle electromagnet");
  Serial.println("r = enter chess move mode");
  Serial.println("After r, type move like d2d3");
  Serial.println("1/2 = test Motor A");
  Serial.println("3/4 = test Motor B");
  Serial.println("b = print board state");
  Serial.println("i = reset board state");
  Serial.println("h = help");
  Serial.println();
}