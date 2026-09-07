#include <Arduino.h>
#include <ctype.h>

#include "Motion.h"
#include "Calibration.h"
#include "Magnet.h"
#include "BoardState.h"

char inputMode = 0;
char inputBuffer[10];
int inputIndex = 0;
int inputTargetLength = 0;

// Continuous jog state
int jogDirX = 0;
int jogDirY = 0;

// Slower jog step for easier calibration and safer movement
long jogStepAmount = 10;
const long MIN_JOG_STEP_AMOUNT = 2;
const long MAX_JOG_STEP_AMOUNT = 300;

unsigned long lastJogTime = 0;
const unsigned long JOG_INTERVAL_MS = 8;

void printHelp();

void handleSerialChar(char ch);
void startBufferedCommand(char mode, int targetLength);
void clearBufferedCommand();
void addBufferedChar(char ch);
void executeBufferedCommand();

void startJog(int dx, int dy);
void stopJog(bool forceMagnetOff = true);
void updateJog();

void forceMagnetOffCommand();
void toggleMagnetCommand();

void setup() {
  Serial.begin(9600);
  delay(500);

  Serial.println();
  Serial.println(F("Automatic Chessboard Starting..."));

  Motion::begin();
  Magnet::begin();

  BoardState::reset();
  Magnet::forceOff();

  Serial.println(F("Loading saved calibration..."));
  Calibration::loadCalibration();

  Serial.println();
  Serial.println(F("Ready."));
  printHelp();
}

void loop() {
  while (Serial.available() > 0) {
    char ch = Serial.read();
    handleSerialChar(ch);
  }

  updateJog();
}

void handleSerialChar(char ch) {
  if (ch == '\r' || ch == '\n' || ch == ' ') {
    return;
  }

  ch = tolower(ch);

  // Emergency cancel. This works even if Arduino is stuck in move-input mode.
  if (ch == '!') {
    clearBufferedCommand();
    stopJog(true);
    Magnet::forceOff();
    Serial.println(F("OK ABORT_INPUT"));
    return;
  }

  if (inputMode != 0) {
    addBufferedChar(ch);
    return;
  }

  if (ch == 'w') {
    startJog(0, 1);
  }
  else if (ch == 's') {
    startJog(0, -1);
  }
  else if (ch == 'a') {
    startJog(-1, 0);
  }
  else if (ch == 'd') {
    startJog(1, 0);
  }
  else if (ch == 'x') {
    stopJog(true);
  }
  else if (ch == '+') {
    jogStepAmount += 5;
    if (jogStepAmount > MAX_JOG_STEP_AMOUNT) {
      jogStepAmount = MAX_JOG_STEP_AMOUNT;
    }

    Serial.print(F("Jog step amount: "));
    Serial.println(jogStepAmount);
  }
  else if (ch == '-') {
    jogStepAmount -= 5;
    if (jogStepAmount < MIN_JOG_STEP_AMOUNT) {
      jogStepAmount = MIN_JOG_STEP_AMOUNT;
    }

    Serial.print(F("Jog step amount: "));
    Serial.println(jogStepAmount);
  }
  else if (ch == 'f' || ch == 'o') {
    forceMagnetOffCommand();
  }
  else if (ch == 'v') {
    toggleMagnetCommand();
  }
  else if (ch == 'q') {
    stopJog(true);
    Calibration::setCurrentPositionAsA1();
  }
  else if (ch == 'z') {
    stopJog(true);
    Calibration::zeroPosition();
  }
  else if (ch == 'c') {
    stopJog(true);
    Calibration::startFourCornerCalibration();
  }
  else if (ch == 'k') {
    stopJog(true);
    Calibration::recordCalibrationPoint();
    Serial.println(F("OK CALIBRATION_POINT"));
  }
  else if (ch == 'p') {
    Calibration::printPosition();
    Serial.println(F("OK POSITION"));
  }
  else if (ch == 'g') {
    stopJog(true);
    Calibration::printGrid();
    Serial.println(F("OK GRID"));
  }
  else if (ch == 't') {
    stopJog(true);
    Calibration::testAllSquares();
    Magnet::forceOff();
    Serial.println(F("OK TEST_ALL_SQUARES"));
  }
  else if (ch == 'u') {
    Calibration::printStatus();
  }
  else if (ch == 'b') {
    BoardState::print();
    Serial.println(F("OK BOARD_PRINT"));
  }
  else if (ch == 'i') {
    stopJog(true);
    BoardState::reset();
    Calibration::resetCaptureParking();
    BoardState::print();
    Magnet::forceOff();
    Serial.println(F("OK BOARD_RESET"));
  }
  else if (ch == 'h') {
    printHelp();
  }

  // Buffered move commands
  else if (ch == 'r') {
    stopJog(true);
    startBufferedCommand('r', 4);
  }
  else if (ch == 'y') {
    stopJog(true);
    startBufferedCommand('y', 5);
  }
  else if (ch == 'l') {
    stopJog(true);
    startBufferedCommand('l', 2);
  }
  else if (ch == 'n') {
    stopJog(true);
    startBufferedCommand('n', 5);
  }
  else if (ch == 'e') {
    stopJog(true);
    startBufferedCommand('e', 7);
  }
  else {
    Serial.print(F("Unknown command: "));
    Serial.println(ch);
    Serial.println(F("Press h for help."));
  }
}

void startBufferedCommand(char mode, int targetLength) {
  inputMode = mode;
  inputIndex = 0;
  inputTargetLength = targetLength;

  for (int i = 0; i < 10; i++) {
    inputBuffer[i] = '\0';
  }

  if (mode == 'r') {
    Serial.println(F("Move mode. Send move like e2e4."));
  }
  else if (mode == 'y') {
    Serial.println(F("Capture mode. Send like e4d5b. Last char is captured color w/b."));
  }
  else if (mode == 'l') {
    Serial.println(F("Castle mode. Send wk, wq, bk, or bq."));
  }
  else if (mode == 'n') {
    Serial.println(F("Promotion mode. Send like e7e8q."));
  }
  else if (mode == 'e') {
    Serial.println(F("En passant mode. Send like e5d6d5b."));
  }
}

void clearBufferedCommand() {
  inputMode = 0;
  inputIndex = 0;
  inputTargetLength = 0;

  for (int i = 0; i < 10; i++) {
    inputBuffer[i] = '\0';
  }
}

void addBufferedChar(char ch) {
  if (inputIndex >= 9) {
    Serial.println(F("ERR INPUT_BUFFER_OVERFLOW"));
    clearBufferedCommand();
    Magnet::forceOff();
    return;
  }

  inputBuffer[inputIndex] = ch;
  inputIndex++;

  Serial.print(F("Input so far: "));
  for (int i = 0; i < inputIndex; i++) {
    Serial.print(inputBuffer[i]);
  }
  Serial.println();

  if (inputIndex >= inputTargetLength) {
    executeBufferedCommand();
    clearBufferedCommand();
  }
}

void executeBufferedCommand() {
  Magnet::forceOff();
  delay(300);

  bool success = false;

  if (inputMode == 'r') {
    char fromFile = inputBuffer[0];
    char fromRank = inputBuffer[1];
    char toFile = inputBuffer[2];
    char toRank = inputBuffer[3];

    Serial.println(F("MOVE_COMMAND_STARTED"));

    success = Calibration::movePieceSafe(fromFile, fromRank, toFile, toRank);

    Magnet::forceOff();
    delay(300);

    if (success) {
      Serial.println(F("OK MOVE_COMMAND"));
    }
    else {
      Serial.println(F("ERR MOVE_COMMAND_FAILED"));
    }
  }

  else if (inputMode == 'y') {
    char fromFile = inputBuffer[0];
    char fromRank = inputBuffer[1];
    char toFile = inputBuffer[2];
    char toRank = inputBuffer[3];
    char capturedColor = inputBuffer[4];

    Serial.println(F("CAPTURE_COMMAND_STARTED"));

    success = Calibration::capturePiece(fromFile, fromRank, toFile, toRank, capturedColor);

    Magnet::forceOff();
    delay(300);

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

    Serial.println(F("CASTLE_COMMAND_STARTED"));

    if (side == 'k') {
      success = Calibration::castleKingside(color);
    }
    else if (side == 'q') {
      success = Calibration::castleQueenside(color);
    }
    else {
      Serial.println(F("ERR INVALID_CASTLE_SIDE"));
      success = false;
    }

    Magnet::forceOff();
    delay(300);

    if (success) {
      Serial.println(F("OK CASTLE_COMMAND"));
    }
    else {
      Serial.println(F("ERR CASTLE_COMMAND_FAILED"));
    }
  }

  else if (inputMode == 'n') {
    char fromFile = inputBuffer[0];
    char fromRank = inputBuffer[1];
    char toFile = inputBuffer[2];
    char toRank = inputBuffer[3];
    char promotedPiece = inputBuffer[4];

    Serial.println(F("PROMOTION_COMMAND_STARTED"));

    success = Calibration::promotePiece(fromFile, fromRank, toFile, toRank, promotedPiece);

    Magnet::forceOff();
    delay(300);

    if (success) {
      Serial.println(F("OK PROMOTION_COMMAND"));
    }
    else {
      Serial.println(F("ERR PROMOTION_COMMAND_FAILED"));
    }
  }

  else if (inputMode == 'e') {
    char fromFile = inputBuffer[0];
    char fromRank = inputBuffer[1];
    char toFile = inputBuffer[2];
    char toRank = inputBuffer[3];
    char capturedFile = inputBuffer[4];
    char capturedRank = inputBuffer[5];
    char capturedColor = inputBuffer[6];

    Serial.println(F("EN_PASSANT_COMMAND_STARTED"));

    success = Calibration::enPassant(fromFile, fromRank,
                                     toFile, toRank,
                                     capturedFile, capturedRank,
                                     capturedColor);

    Magnet::forceOff();
    delay(300);

    if (success) {
      Serial.println(F("OK EN_PASSANT_COMMAND"));
    }
    else {
      Serial.println(F("ERR EN_PASSANT_COMMAND_FAILED"));
    }
  }

  else {
    Magnet::forceOff();
    Serial.println(F("ERR UNKNOWN_INPUT_MODE"));
  }
}

// ---------------- JOGGING ----------------

void startJog(int dx, int dy) {
  jogDirX = dx;
  jogDirY = dy;
  lastJogTime = 0;

  Serial.print(F("Jogging "));
  if (dx == 1) Serial.println(F("right"));
  else if (dx == -1) Serial.println(F("left"));
  else if (dy == 1) Serial.println(F("up"));
  else if (dy == -1) Serial.println(F("down"));
}

void stopJog(bool forceMagnetOff) {
  jogDirX = 0;
  jogDirY = 0;

  if (forceMagnetOff) {
    Magnet::forceOff();
  }

  Serial.println(F("Jog stopped."));
  Calibration::printPosition();
}

void updateJog() {
  if (jogDirX == 0 && jogDirY == 0) {
    return;
  }

  unsigned long now = millis();

  if (now - lastJogTime < JOG_INTERVAL_MS) {
    return;
  }

  lastJogTime = now;

  long currentX = Motion::getX();
  long currentY = Motion::getY();

  long targetX = currentX + jogDirX * jogStepAmount;
  long targetY = currentY + jogDirY * jogStepAmount;

  Motion::moveTo(targetX, targetY);
}

// ---------------- MAGNET COMMANDS ----------------

void forceMagnetOffCommand() {
  Magnet::forceOff();
  Serial.println(F("OK MAGNET_OFF"));
}

void toggleMagnetCommand() {
  if (Magnet::isOn()) {
    Magnet::forceOff();
    Serial.println(F("OK MAGNET_OFF"));
  }
  else {
    Magnet::on();
    Serial.println(F("OK MAGNET_ON"));
  }
}

// ---------------- HELP ----------------

void printHelp() {
  Serial.println();
  Serial.println(F("===== AUTOMATIC CHESSBOARD HELP ====="));
  Serial.println(F("Emergency:"));
  Serial.println(F("  ! = abort current input mode and force magnet off"));
  Serial.println();
  Serial.println(F("Manual movement:"));
  Serial.println(F("  w = jog up"));
  Serial.println(F("  s = jog down"));
  Serial.println(F("  a = jog left"));
  Serial.println(F("  d = jog right"));
  Serial.println(F("  x = stop jog and force magnet off"));
  Serial.println(F("  + = larger jog step"));
  Serial.println(F("  - = smaller jog step"));
  Serial.println();
  Serial.println(F("Magnet:"));
  Serial.println(F("  v = toggle magnet"));
  Serial.println(F("  f = force magnet off"));
  Serial.println(F("  o = force magnet off"));
  Serial.println();
  Serial.println(F("Calibration:"));
  Serial.println(F("  q = set current position as a1 without clearing EEPROM"));
  Serial.println(F("  z = zero position and clear saved calibration"));
  Serial.println(F("  c = start 4-corner calibration"));
  Serial.println(F("  k = save current calibration point"));
  Serial.println(F("  p = print position"));
  Serial.println(F("  g = print grid"));
  Serial.println(F("  t = test all squares"));
  Serial.println(F("  u = status"));
  Serial.println();
  Serial.println(F("Board state:"));
  Serial.println(F("  i = reset board state"));
  Serial.println(F("  b = print board state"));
  Serial.println();
  Serial.println(F("Moves:"));
  Serial.println(F("  r e2e4    = normal/safe move"));
  Serial.println(F("  y e4d5b   = capture, captured piece is black"));
  Serial.println(F("  l wk      = white kingside castle"));
  Serial.println(F("  l wq      = white queenside castle"));
  Serial.println(F("  l bk      = black kingside castle"));
  Serial.println(F("  l bq      = black queenside castle"));
  Serial.println(F("  n e7e8q   = promotion"));
  Serial.println(F("  e e5d6d5b = en passant"));
  Serial.println();
  Serial.println(F("Other:"));
  Serial.println(F("  h = help"));
  Serial.println(F("====================================="));
  Serial.println();
}