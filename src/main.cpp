#include <Arduino.h>
#include "Motion.h"
#include "Calibration.h"
#include "Magnet.h"
#include "BoardState.h"

void printHelp();
void processSingleCommand(char cmd);
void processBufferedInput(char cmd);
bool isValidBufferedChar(char cmd, int index);
void runBufferedCommand();
void startInputMode(char mode, int targetLength);

char inputMode = 0;
char inputBuffer[8];
int inputIndex = 0;
int inputTargetLength = 0;

// Input modes:
// r = regular move, 4 chars: e2e4
// y = capture, 5 chars: e4d5b
// l = castle, 2 chars: wk
// n = promotion, 5 chars: e7e8q
// e = en passant, 7 chars: e5d6d5b

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

    if (inputMode != 0) {
      processBufferedInput(cmd);
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
    inputMode = 0;
    inputIndex = 0;
    inputTargetLength = 0;
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
  else if (cmd == 'q') {
    Calibration::setCurrentPositionAsA1();
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
    startInputMode('r', 4);
  }
  else if (cmd == 'y') {
  startInputMode('y', 5);
  }
  else if (cmd == 'l') {
    startInputMode('l', 2);
  }
  else if (cmd == 'n') {
    startInputMode('n', 5);
  }
  else if (cmd == 'e') {
    startInputMode('e', 7);
  }
  else if (cmd == 'u') {
    Calibration::printStatus();
    BoardState::print();
  }
  else if (cmd == 'h') {
    printHelp();
  }
  else if (cmd == 'b') {
  BoardState::print();
  }
  else if (cmd == 'i') {
    BoardState::reset();
    Calibration::resetCaptureParking();
    BoardState::print();
  }
  else {
    Serial.print(F("Unknown command: "));
    Serial.println(cmd);
  }
}

void startInputMode(char mode, int targetLength) {
  inputMode = mode;
  inputIndex = 0;
  inputTargetLength = targetLength;

  Motion::stop();

  Serial.println();

  if (mode == 'r') {
    Serial.println(F("Regular move mode started."));
    Serial.println(F("Enter move as 4 chars, like d2d3."));
  }
  else if (mode == 'y') {
    Serial.println(F("Capture mode started."));
    Serial.println(F("Enter capture as 5 chars, like e4d5b."));
    Serial.println(F("Last char is captured color: w or b."));
  }
  else if (mode == 'l') {
    Serial.println(F("Castle mode started."));
    Serial.println(F("Enter castle as 2 chars: wk, wq, bk, or bq."));
  }
  else if (mode == 'n') {
    Serial.println(F("Promotion mode started."));
    Serial.println(F("Enter promotion as 5 chars, like e7e8q."));
    Serial.println(F("Promotion piece can be q, r, b, or n."));
  }
  else if (mode == 'e') {
    Serial.println(F("En passant mode started."));
    Serial.println(F("Enter en passant as 7 chars, like e5d6d5b."));
    Serial.println(F("Format: from, to, captured square, captured color."));
  }

  Serial.println(F("Send x to cancel."));
}

void processBufferedInput(char cmd) {
  if (cmd == 'x') {
    inputMode = 0;
    inputIndex = 0;
    inputTargetLength = 0;
    Magnet::off();
    Serial.println(F("Input cancelled."));
    return;
  }

  if (!isValidBufferedChar(cmd, inputIndex)) {
    Serial.print(F("Invalid character: "));
    Serial.println(cmd);
    Serial.println(F("Restarting this input mode."));
    inputIndex = 0;
    return;
  }

  inputBuffer[inputIndex] = cmd;
  inputIndex++;

  Serial.print(F("Input so far: "));
  for (int i = 0; i < inputIndex; i++) {
    Serial.print(inputBuffer[i]);
  }
  Serial.println();

  if (inputIndex == inputTargetLength) {
    inputBuffer[inputIndex] = '\0';
    runBufferedCommand();

    inputMode = 0;
    inputIndex = 0;
    inputTargetLength = 0;
  }
}

bool isValidBufferedChar(char cmd, int index) {
  if (inputMode == 'r') {
    // e2e4
    if (index == 0 || index == 2) return cmd >= 'a' && cmd <= 'h';
    if (index == 1 || index == 3) return cmd >= '1' && cmd <= '8';
  }

  else if (inputMode == 'y') {
    // e4d5b
    if (index == 0 || index == 2) return cmd >= 'a' && cmd <= 'h';
    if (index == 1 || index == 3) return cmd >= '1' && cmd <= '8';
    if (index == 4) return cmd == 'w' || cmd == 'b';
  }

  else if (inputMode == 'l') {
    // wk, wq, bk, bq
    if (index == 0) return cmd == 'w' || cmd == 'b';
    if (index == 1) return cmd == 'k' || cmd == 'q';
  }

  else if (inputMode == 'n') {
    // e7e8q
    if (index == 0 || index == 2) return cmd >= 'a' && cmd <= 'h';
    if (index == 1 || index == 3) return cmd >= '1' && cmd <= '8';
    if (index == 4) return cmd == 'q' || cmd == 'r' || cmd == 'b' || cmd == 'n';
  }

  else if (inputMode == 'e') {
    // e5d6d5b
    if (index == 0 || index == 2 || index == 4) return cmd >= 'a' && cmd <= 'h';
    if (index == 1 || index == 3 || index == 5) return cmd >= '1' && cmd <= '8';
    if (index == 6) return cmd == 'w' || cmd == 'b';
  }

  return false;
}

void runBufferedCommand() {
  bool success = false;

  if (inputMode == 'r') {
    success = Calibration::movePieceSafe(
      inputBuffer[0],
      inputBuffer[1],
      inputBuffer[2],
      inputBuffer[3]
    );

    if (success) {
      Serial.println(F("OK MOVE_COMMAND"));
    }
    else {
      Serial.println(F("ERR MOVE_COMMAND_FAILED"));
    }
  }

  else if (inputMode == 'y') {
    success = Calibration::capturePiece(
      inputBuffer[0],
      inputBuffer[1],
      inputBuffer[2],
      inputBuffer[3],
      inputBuffer[4]
    );

    if (success) {
      Serial.println(F("OK CAPTURE_COMMAND"));
    }
    else {
      Serial.println(F("ERR CAPTURE_COMMAND_FAILED"));
    }
  }

  else if (inputMode == 'l') {
    char color = inputBuffer[0];
    char side = inputBuffer[1];

    if (side == 'k') {
      success = Calibration::castleKingside(color);
    }
    else if (side == 'q') {
      success = Calibration::castleQueenside(color);
    }

    if (success) {
      Serial.println(F("OK CASTLE_COMMAND"));
    }
    else {
      Serial.println(F("ERR CASTLE_COMMAND_FAILED"));
    }
  }

  else if (inputMode == 'n') {
    success = Calibration::promotePiece(
      inputBuffer[0],
      inputBuffer[1],
      inputBuffer[2],
      inputBuffer[3],
      inputBuffer[4]
    );

    if (success) {
      Serial.println(F("OK PROMOTION_COMMAND"));
    }
    else {
      Serial.println(F("ERR PROMOTION_COMMAND_FAILED"));
    }
  }

  else if (inputMode == 'e') {
    success = Calibration::enPassant(
      inputBuffer[0],
      inputBuffer[1],
      inputBuffer[2],
      inputBuffer[3],
      inputBuffer[4],
      inputBuffer[5],
      inputBuffer[6]
    );

    if (success) {
      Serial.println(F("OK EN_PASSANT_COMMAND"));
    }
    else {
      Serial.println(F("ERR EN_PASSANT_COMMAND_FAILED"));
    }
  }
}

void printHelp() {
  Serial.println();
  Serial.println(F("Commands:"));
  Serial.println(F("w/a/s/d = manual move"));
  Serial.println(F("x = stop / abort / cancel input"));
  Serial.println(F("+ = faster"));
  Serial.println(F("- = slower"));
  Serial.println(F("z = zero position AND clear saved EEPROM calibration"));
  Serial.println(F("q = set current position as a1 without clearing EEPROM"));
  Serial.println(F("p = print position"));
  Serial.println(F("u = print full status"));
  Serial.println(F("m = old quick calibration using current position as h8"));
  Serial.println(F("c = start 4-corner calibration"));
  Serial.println(F("k = save current calibration corner"));
  Serial.println(F("g = print grid"));
  Serial.println(F("t = test all 64 squares"));
  Serial.println(F("o = electromagnet on"));
  Serial.println(F("f = electromagnet off"));
  Serial.println(F("v = toggle electromagnet"));
  Serial.println(F("r = regular move mode, then type d2d3"));
  Serial.println(F("y = capture mode, then type e4d5b"));
  Serial.println(F("l = castle mode, then type wk/wq/bk/bq"));
  Serial.println(F("n = promotion mode, then type e7e8q"));
  Serial.println(F("e = en passant mode, then type e5d6d5b"));
  Serial.println(F("1/2 = test Motor A"));
  Serial.println(F("3/4 = test Motor B"));
  Serial.println(F("b = print board state"));
  Serial.println(F("i = reset board state and capture parking"));
  Serial.println(F("h = help"));
  Serial.println();
}
