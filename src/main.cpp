#include <Arduino.h>
#include <ctype.h>

#include "Motion.h"
#include "Calibration.h"
#include "Magnet.h"
#include "BoardState.h"

// ---------------- INPUT MODE SETTINGS ----------------

char inputMode = 0;
char inputBuffer[8];
int inputIndex = 0;
int inputTargetLength = 0;

// ---------------- JOG SETTINGS ----------------

char jogDirection = 0;

// Same speed style as automatic goTo().
// Bigger = slower.
// Smaller = faster.
const unsigned long JOG_INTERVAL_US = 1600;
unsigned long lastJogMicros = 0;

// ---------------- FUNCTION DECLARATIONS ----------------

void printHelp();

void handleSerialChar(char ch);

void startBufferedCommand(char mode, int targetLength);
void addBufferedChar(char ch);
void clearBufferedCommand();
void executeBufferedCommand();

void startJog(char direction);
void stopJog(bool printMessage);
void updateJogMovement();

void forceMagnetOffCommand();
void toggleMagnetCommand();

bool isValidColor(char color);
bool isValidCastleSide(char side);
bool isValidPromotionPiece(char piece);

// ---------------- SETUP / LOOP ----------------

void setup() {
  Serial.begin(9600);
  delay(1000);

  Serial.println();
  Serial.println(F("Automatic Chessboard Starting..."));

  Motion::begin();
  Magnet::begin();

  BoardState::reset();

  Magnet::forceOff();

  Calibration::loadCalibration();

  printHelp();
}

void loop() {
  while (Serial.available() > 0) {
    char ch = Serial.read();
    handleSerialChar(ch);
  }

  updateJogMovement();
}

// ---------------- SERIAL COMMAND HANDLING ----------------

void handleSerialChar(char ch) {
  if (ch == '\r' || ch == '\n' || ch == ' ' || ch == '\t') {
    return;
  }

  ch = tolower(ch);

  if (ch == '!') {
    clearBufferedCommand();
    stopJog(false);
    Magnet::forceOff();
    Serial.println(F("OK ABORT_INPUT"));
    return;
  }

  if (inputMode != 0) {
    addBufferedChar(ch);
    return;
  }

  if (ch == 'w' || ch == 'a' || ch == 's' || ch == 'd') {
    startJog(ch);
    return;
  }

  if (ch == 'x') {
    stopJog(true);
    return;
  }

  if (ch == 'f' || ch == 'o') {
    forceMagnetOffCommand();
    return;
  }

  if (ch == 'v') {
    toggleMagnetCommand();
    return;
  }

  if (ch == 'q') {
    stopJog(false);
    Calibration::setCurrentPositionAsA1();
    return;
  }

  if (ch == 'z') {
    stopJog(false);
    Calibration::zeroPosition();
    return;
  }

  if (ch == 'c') {
    stopJog(false);
    Calibration::startFourCornerCalibration();
    return;
  }

  if (ch == 'k') {
    stopJog(false);
    Calibration::recordCalibrationPoint();
    return;
  }

  if (ch == 'p') {
    Calibration::printPosition();
    return;
  }

  if (ch == 'g') {
    Calibration::printGrid();
    return;
  }

  if (ch == 't') {
    stopJog(false);
    Calibration::testAllSquares();
    return;
  }

  if (ch == 'u') {
    Calibration::printStatus();
    return;
  }

  if (ch == 'b') {
    BoardState::print();
    return;
  }

  if (ch == 'i') {
    BoardState::reset();
    Calibration::resetCaptureParking();
    BoardState::print();
    Serial.println(F("OK BOARD_RESET"));
    return;
  }

  if (ch == 'h') {
    printHelp();
    return;
  }

  if (ch == 'r') {
    startBufferedCommand('r', 4);
    return;
  }

  if (ch == 'y') {
    startBufferedCommand('y', 5);
    return;
  }

  if (ch == 'l') {
    startBufferedCommand('l', 2);
    return;
  }

  if (ch == 'n') {
    startBufferedCommand('n', 5);
    return;
  }

  if (ch == 'e') {
    startBufferedCommand('e', 7);
    return;
  }

  if (ch == '1') {
    stopJog(false);
    Motion::testGoLineRelative(1000, 1000);
    return;
  }

  if (ch == '2') {
    stopJog(false);
    Motion::testGoLineRelative(-1000, -1000);
    return;
  }

  if (ch == '3') {
    stopJog(false);
    Motion::testGoLineRelative(1000, -1000);
    return;
  }

  if (ch == '4') {
    stopJog(false);
    Motion::testGoLineRelative(-1000, 1000);
    return;
  }

  Serial.print(F("Unknown command: "));
  Serial.println(ch);
}

// ---------------- BUFFERED COMMANDS ----------------

void startBufferedCommand(char mode, int targetLength) {
  stopJog(false);

  inputMode = mode;
  inputIndex = 0;
  inputTargetLength = targetLength;

  for (int i = 0; i < 8; i++) {
    inputBuffer[i] = '\0';
  }

  Serial.print(F("Started input mode: "));
  Serial.println(inputMode);

  if (mode == 'r') {
    Serial.println(F("Enter move, example: e2e4"));
  }
  else if (mode == 'y') {
    Serial.println(F("Enter capture, example: e4d5b"));
  }
  else if (mode == 'l') {
    Serial.println(F("Enter castle, example: wk, wq, bk, bq"));
  }
  else if (mode == 'n') {
    Serial.println(F("Enter promotion, example: e7e8q"));
  }
  else if (mode == 'e') {
    Serial.println(F("Enter en passant, example: e5d6d5b"));
  }
}

void addBufferedChar(char ch) {
  if (inputIndex >= 7) {
    Serial.println(F("ERR INPUT_TOO_LONG"));
    clearBufferedCommand();
    return;
  }

  inputBuffer[inputIndex] = ch;
  inputIndex++;
  inputBuffer[inputIndex] = '\0';

  Serial.print(F("Input so far: "));
  Serial.println(inputBuffer);

  if (inputIndex >= inputTargetLength) {
    executeBufferedCommand();
    clearBufferedCommand();
  }
}

void clearBufferedCommand() {
  inputMode = 0;
  inputIndex = 0;
  inputTargetLength = 0;

  for (int i = 0; i < 8; i++) {
    inputBuffer[i] = '\0';
  }
}

void executeBufferedCommand() {
  stopJog(false);

  Magnet::forceOff();
  delay(300);

  if (inputMode == 'r') {
    char fromFile = inputBuffer[0];
    char fromRank = inputBuffer[1];
    char toFile = inputBuffer[2];
    char toRank = inputBuffer[3];

    Serial.println(F("MOVE_COMMAND_STARTED"));

    bool success = Calibration::movePieceSafe(fromFile, fromRank, toFile, toRank);

    Magnet::forceOff();

    if (success) {
      Serial.println(F("OK MOVE_COMMAND"));
    }
    else {
      Serial.println(F("ERR MOVE_COMMAND_FAILED"));
    }

    return;
  }

  if (inputMode == 'y') {
    char fromFile = inputBuffer[0];
    char fromRank = inputBuffer[1];
    char toFile = inputBuffer[2];
    char toRank = inputBuffer[3];
    char capturedColor = inputBuffer[4];

    if (!isValidColor(capturedColor)) {
      Serial.println(F("ERR INVALID_CAPTURE_COLOR"));
      Magnet::forceOff();
      return;
    }

    Serial.println(F("CAPTURE_COMMAND_STARTED"));

    bool success = Calibration::capturePiece(
      fromFile, fromRank,
      toFile, toRank,
      capturedColor
    );

    Magnet::forceOff();

    if (success) {
      Serial.println(F("OK CAPTURE_COMMAND"));
    }
    else {
      Serial.println(F("ERR CAPTURE_COMMAND_FAILED"));
    }

    return;
  }

  if (inputMode == 'l') {
    char color = inputBuffer[0];
    char side = inputBuffer[1];

    if (!isValidColor(color) || !isValidCastleSide(side)) {
      Serial.println(F("ERR INVALID_CASTLE_INPUT"));
      Magnet::forceOff();
      return;
    }

    Serial.println(F("CASTLE_COMMAND_STARTED"));

    bool success = false;

    if (side == 'k') {
      success = Calibration::castleKingside(color);
    }
    else if (side == 'q') {
      success = Calibration::castleQueenside(color);
    }

    Magnet::forceOff();

    if (success) {
      Serial.println(F("OK CASTLE_COMMAND"));
    }
    else {
      Serial.println(F("ERR CASTLE_COMMAND_FAILED"));
    }

    return;
  }

  if (inputMode == 'n') {
    char fromFile = inputBuffer[0];
    char fromRank = inputBuffer[1];
    char toFile = inputBuffer[2];
    char toRank = inputBuffer[3];
    char promotedPiece = inputBuffer[4];

    if (!isValidPromotionPiece(promotedPiece)) {
      Serial.println(F("ERR INVALID_PROMOTION_PIECE"));
      Magnet::forceOff();
      return;
    }

    Serial.println(F("PROMOTION_COMMAND_STARTED"));

    bool success = Calibration::promotePiece(
      fromFile, fromRank,
      toFile,
      toRank,
      promotedPiece
    );

    Magnet::forceOff();

    if (success) {
      Serial.println(F("OK PROMOTION_COMMAND"));
    }
    else {
      Serial.println(F("ERR PROMOTION_COMMAND_FAILED"));
    }

    return;
  }

  if (inputMode == 'e') {
    char fromFile = inputBuffer[0];
    char fromRank = inputBuffer[1];
    char toFile = inputBuffer[2];
    char toRank = inputBuffer[3];
    char capturedFile = inputBuffer[4];
    char capturedRank = inputBuffer[5];
    char capturedColor = inputBuffer[6];

    if (!isValidColor(capturedColor)) {
      Serial.println(F("ERR INVALID_EN_PASSANT_COLOR"));
      Magnet::forceOff();
      return;
    }

    Serial.println(F("EN_PASSANT_COMMAND_STARTED"));

    bool success = Calibration::enPassant(
      fromFile, fromRank,
      toFile, toRank,
      capturedFile, capturedRank,
      capturedColor
    );

    Magnet::forceOff();

    if (success) {
      Serial.println(F("OK EN_PASSANT_COMMAND"));
    }
    else {
      Serial.println(F("ERR EN_PASSANT_COMMAND_FAILED"));
    }

    return;
  }

  Serial.println(F("ERR UNKNOWN_INPUT_MODE"));
  Magnet::forceOff();
}

// ---------------- JOG MOVEMENT ----------------

void startJog(char direction) {
  jogDirection = direction;

  Serial.print(F("Jog direction: "));
  Serial.println(jogDirection);
}

void stopJog(bool printMessage) {
  jogDirection = 0;

  if (printMessage) {
    Serial.println(F("STOP"));
  }
}

void updateJogMovement() {
  if (jogDirection == 0) {
    return;
  }

  unsigned long now = micros();

  if (now - lastJogMicros < JOG_INTERVAL_US) {
    return;
  }

  lastJogMicros = now;

  int xDir = 0;
  int yDir = 0;

  if (jogDirection == 'w') {
    yDir = 1;
  }
  else if (jogDirection == 's') {
    yDir = -1;
  }
  else if (jogDirection == 'a') {
    xDir = 1;
  }
  else if (jogDirection == 'd') {
    xDir = -1;
  }

  Motion::jogStep(xDir, yDir);
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

// ---------------- VALIDATION HELPERS ----------------

bool isValidColor(char color) {
  return color == 'w' || color == 'b';
}

bool isValidCastleSide(char side) {
  return side == 'k' || side == 'q';
}

bool isValidPromotionPiece(char piece) {
  return piece == 'q' || piece == 'r' || piece == 'b' || piece == 'n';
}

// ---------------- HELP MENU ----------------

void printHelp() {
  Serial.println();
  Serial.println(F("===== AUTOMATIC CHESSBOARD HELP ====="));
  Serial.println(F("Manual movement:"));
  Serial.println(F("  w = jog up"));
  Serial.println(F("  s = jog down"));
  Serial.println(F("  a = jog left"));
  Serial.println(F("  d = jog right"));
  Serial.println(F("  x = stop jog"));
  Serial.println();
  Serial.println(F("Magnet:"));
  Serial.println(F("  f = force magnet off"));
  Serial.println(F("  o = force magnet off"));
  Serial.println(F("  v = toggle magnet"));
  Serial.println();
  Serial.println(F("Calibration:"));
  Serial.println(F("  q = set current position as a1 without clearing EEPROM"));
  Serial.println(F("  z = zero position and CLEAR saved calibration"));
  Serial.println(F("  c = start 4-corner calibration"));
  Serial.println(F("  k = save current calibration point"));
  Serial.println(F("  p = print current position"));
  Serial.println(F("  g = print grid"));
  Serial.println(F("  t = test all squares"));
  Serial.println(F("  u = status"));
  Serial.println();
  Serial.println(F("Board state:"));
  Serial.println(F("  b = print board"));
  Serial.println(F("  i = reset board to starting position"));
  Serial.println();
  Serial.println(F("Chess move commands:"));
  Serial.println(F("  r + e2e4     = regular move"));
  Serial.println(F("  y + e4d5b    = capture command"));
  Serial.println(F("  l + wk       = white kingside castle"));
  Serial.println(F("  l + wq       = white queenside castle"));
  Serial.println(F("  l + bk       = black kingside castle"));
  Serial.println(F("  l + bq       = black queenside castle"));
  Serial.println(F("  n + e7e8q    = promotion"));
  Serial.println(F("  e + e5d6d5b  = en passant"));
  Serial.println();
  Serial.println(F("Other:"));
  Serial.println(F("  ! = abort current input mode and force magnet off"));
  Serial.println(F("  h = help"));
  Serial.println(F("====================================="));
  Serial.println();
}