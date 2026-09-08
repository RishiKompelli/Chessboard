#include <Arduino.h>
#include <EEPROM.h>
#include <math.h>
#include <stddef.h>

#include "Calibration.h"
#include "Motion.h"
#include "Magnet.h"
#include "BoardState.h"

struct PointF {
  float x;
  float y;
};

PointF cornerA1;
PointF cornerH1;
PointF cornerA8;
PointF cornerH8;

long boardMaxX = 0;
long boardMaxY = 0;

float squareSpacingX = 0;
float squareSpacingY = 0;

bool boardCalibrated = false;
bool calibrationMode = false;
int calibrationStep = 0;

// ---------------- TIMING SETTINGS ----------------

const int SQUARE_PAUSE_MS = 900;

const int BEFORE_MAGNET_ON_DELAY_MS = 500;
const int MAGNET_PICKUP_DELAY_MS = 2200;

const int BEFORE_MAGNET_OFF_DELAY_MS = 800;
const int MAGNET_RELEASE_HOLD_MS = 1200;

const int MAGNET_DROP_DELAY_MS = 2200;
const int MAGNET_BETWEEN_MOVE_BUFFER_MS = 1500;

const float RELEASE_FORWARD_OFFSET_SQUARES = 0.08;

// ---------------- EEPROM SETTINGS ----------------

const uint32_t CALIBRATION_MAGIC = 0x43414C34UL; // "CAL4"
const uint16_t CALIBRATION_VERSION = 1;
const int CALIBRATION_EEPROM_ADDRESS = 0;

int whiteCapturedCount = 0;
int blackCapturedCount = 0;

struct StoredCalibration {
  uint32_t magic;
  uint16_t version;
  int32_t coordinates[8];
  uint16_t checksum;
};

// ---------------- INTERNAL HELPER DECLARATIONS ----------------

static void promptCalibrationStep();
static void finishFourCornerCalibration(bool saveToEeprom = true);
static void saveCalibration();
static void clearSavedCalibration();
static uint16_t calibrationChecksum(const StoredCalibration &stored);

static PointF lerp(PointF a, PointF b, float t);
static bool gridToPosition(float fileCoord, float rankCoord, long &x, long &y);
static bool squareToPosition(char file, char rank, long &x, long &y);

static bool magnetPickupSequence();
static void magnetReleaseSequence();
static void makeSureMagnetIsOff(const __FlashStringHelper *reason);

static bool moveToReleaseOffset(float approachFileCoord, float approachRankCoord,
                                float targetFileCoord, float targetRankCoord);

static bool isDiagonalPathClear(int fromFileIndex, int fromRankIndex,
                                int toFileIndex, int toRankIndex);

static bool isValidSquare(char file, char rank);
static int fileToIndex(char file);
static int rankToIndex(char rank);

// ---------------- CALIBRATION NAMESPACE ----------------

namespace Calibration {

  void zeroPosition() {
    Motion::zeroPosition();

    boardCalibrated = false;
    calibrationMode = false;
    calibrationStep = 0;
    clearSavedCalibration();

    Magnet::forceOff();
    delay(MAGNET_DROP_DELAY_MS);

    Serial.println(F("Position zeroed."));
    Serial.println(F("Existing board calibration cleared."));
    printPosition();
  }

  void setCurrentPositionAsA1() {
    Motion::zeroPosition();

    Magnet::forceOff();
    delay(MAGNET_DROP_DELAY_MS);

    Serial.println(F("OK POSITION_SET_A1"));
    Serial.println(F("Current position set to a1."));
    Serial.println(F("Saved EEPROM calibration was NOT cleared."));
    printPosition();
  }

  void printPosition() {
    Serial.print(F("Current position: X = "));
    Serial.print(Motion::getX());
    Serial.print(F(", Y = "));
    Serial.println(Motion::getY());
  }

  void startFourCornerCalibration() {
    Magnet::forceOff();
    delay(MAGNET_DROP_DELAY_MS);

    boardCalibrated = false;
    calibrationMode = true;
    calibrationStep = 0;

    Serial.println();
    Serial.println(F("Starting 4-corner calibration."));
    Serial.println(F("Order: a1 -> a8 -> h8 -> h1"));
    Serial.println(F("Use WASD to move to each square center."));
    Serial.println(F("Press x to stop before saving each point."));
    Serial.println(F("Then press k to save that corner."));
    Serial.println();

    promptCalibrationStep();
  }

  void recordCalibrationPoint() {
    if (!calibrationMode) {
      Serial.println(F("Not in calibration mode."));
      Serial.println(F("Press c to start 4-corner calibration."));
      return;
    }

    Magnet::forceOff();
    delay(MAGNET_DROP_DELAY_MS);

    PointF currentPoint;
    currentPoint.x = Motion::getX();
    currentPoint.y = Motion::getY();

    if (calibrationStep == 0) {
      cornerA1 = currentPoint;
      Serial.print(F("Saved a1: "));
    }
    else if (calibrationStep == 1) {
      cornerA8 = currentPoint;
      Serial.print(F("Saved a8: "));
    }
    else if (calibrationStep == 2) {
      cornerH8 = currentPoint;
      Serial.print(F("Saved h8: "));
    }
    else if (calibrationStep == 3) {
      cornerH1 = currentPoint;
      Serial.print(F("Saved h1: "));
    }

    Serial.print(F("X = "));
    Serial.print(currentPoint.x);
    Serial.print(F(", Y = "));
    Serial.println(currentPoint.y);

    calibrationStep++;

    if (calibrationStep >= 4) {
      finishFourCornerCalibration();
    }
    else {
      promptCalibrationStep();
    }
  }

  bool loadCalibration() {
    StoredCalibration stored;
    EEPROM.get(CALIBRATION_EEPROM_ADDRESS, stored);

    if (stored.magic != CALIBRATION_MAGIC ||
        stored.version != CALIBRATION_VERSION ||
        stored.checksum != calibrationChecksum(stored)) {
      boardCalibrated = false;
      Serial.println(F("No saved calibration found."));
      return false;
    }

    PointF *corners[] = {&cornerA1, &cornerH1, &cornerA8, &cornerH8};

    for (int corner = 0; corner < 4; corner++) {
      corners[corner]->x = stored.coordinates[corner * 2];
      corners[corner]->y = stored.coordinates[corner * 2 + 1];
    }

    finishFourCornerCalibration(false);
    Serial.println(F("Calibration loaded from EEPROM."));
    return true;
  }

  void setBoardMax() {
    Magnet::forceOff();
    delay(MAGNET_DROP_DELAY_MS);

    cornerA1.x = 0;
    cornerA1.y = 0;

    cornerH1.x = Motion::getX();
    cornerH1.y = 0;

    cornerA8.x = 0;
    cornerA8.y = Motion::getY();

    cornerH8.x = Motion::getX();
    cornerH8.y = Motion::getY();

    finishFourCornerCalibration();
  }

  void printGrid() {
    if (!boardCalibrated) {
      Serial.println(F("Board not calibrated yet."));
      Serial.println(F("Use c for 4-corner calibration."));
      return;
    }

    Serial.println();
    Serial.println(F("4-corner calibration data:"));

    Serial.print(F("a1 = ("));
    Serial.print(cornerA1.x);
    Serial.print(F(", "));
    Serial.print(cornerA1.y);
    Serial.println(F(")"));

    Serial.print(F("h1 = ("));
    Serial.print(cornerH1.x);
    Serial.print(F(", "));
    Serial.print(cornerH1.y);
    Serial.println(F(")"));

    Serial.print(F("a8 = ("));
    Serial.print(cornerA8.x);
    Serial.print(F(", "));
    Serial.print(cornerA8.y);
    Serial.println(F(")"));

    Serial.print(F("h8 = ("));
    Serial.print(cornerH8.x);
    Serial.print(F(", "));
    Serial.print(cornerH8.y);
    Serial.println(F(")"));

    Serial.println();
    Serial.print(F("Average square spacing X = "));
    Serial.println(squareSpacingX);

    Serial.print(F("Average square spacing Y = "));
    Serial.println(squareSpacingY);

    Serial.println();
    Serial.println(F("Approx square centers:"));

    for (int rank = 0; rank < 8; rank++) {
      for (int file = 0; file < 8; file++) {
        char fileChar = 'a' + file;
        char rankChar = '1' + rank;

        long squareX;
        long squareY;

        gridToPosition(file, rank, squareX, squareY);

        Serial.print(fileChar);
        Serial.print(rankChar);
        Serial.print(F("("));
        Serial.print(squareX);
        Serial.print(F(","));
        Serial.print(squareY);
        Serial.print(F(") "));
      }

      Serial.println();
    }

    Serial.println();
  }

  void printStatus() {
    Serial.println();
    Serial.println(F("===== CHESSBOARD STATUS ====="));

    Serial.print(F("Board calibrated: "));
    Serial.println(boardCalibrated ? F("yes") : F("no"));

    Serial.print(F("Calibration mode: "));
    Serial.println(calibrationMode ? F("yes") : F("no"));

    Serial.print(F("Calibration step: "));
    Serial.println(calibrationStep);

    Serial.print(F("Current X: "));
    Serial.println(Motion::getX());

    Serial.print(F("Current Y: "));
    Serial.println(Motion::getY());

    Serial.print(F("Square spacing X: "));
    Serial.println(squareSpacingX);

    Serial.print(F("Square spacing Y: "));
    Serial.println(squareSpacingY);

    Serial.print(F("White captured count: "));
    Serial.println(whiteCapturedCount);

    Serial.print(F("Black captured count: "));
    Serial.println(blackCapturedCount);

    Serial.print(F("Magnet pickup delay ms: "));
    Serial.println(MAGNET_PICKUP_DELAY_MS);

    Serial.print(F("Magnet drop delay ms: "));
    Serial.println(MAGNET_DROP_DELAY_MS);

    Serial.print(F("Between move buffer ms: "));
    Serial.println(MAGNET_BETWEEN_MOVE_BUFFER_MS);

    Serial.print(F("Magnet state: "));
    Serial.println(Magnet::isOn() ? F("on") : F("off"));

    Serial.println(F("============================="));
    Serial.println(F("OK STATUS"));
    Serial.println();
  }

  void resetCaptureParking() {
    whiteCapturedCount = 0;
    blackCapturedCount = 0;

    Magnet::forceOff();
    delay(MAGNET_DROP_DELAY_MS);

    Serial.println(F("Capture counters reset."));
  }

  bool isCalibrated() {
    return boardCalibrated;
  }

  long getBoardMaxX() {
    return boardMaxX;
  }

  long getBoardMaxY() {
    return boardMaxY;
  }

  float getSquareSpacingX() {
    return squareSpacingX;
  }

  float getSquareSpacingY() {
    return squareSpacingY;
  }

  bool moveToSquare(char file, char rank) {
    if (!boardCalibrated) {
      Serial.println(F("Board not calibrated yet."));
      Serial.println(F("Use c for 4-corner calibration first."));
      return false;
    }

    long targetX;
    long targetY;

    if (!squareToPosition(file, rank, targetX, targetY)) {
      Serial.println(F("Invalid square."));
      return false;
    }

    Serial.print(F("Going to "));
    Serial.print(file);
    Serial.print(rank);
    Serial.print(F(" -> X="));
    Serial.print(targetX);
    Serial.print(F(", Y="));
    Serial.println(targetY);

    bool success = Motion::moveTo(targetX, targetY);

    printPosition();

    return success;
  }

  void testAllSquares() {
    if (!boardCalibrated) {
      Serial.println(F("Board not calibrated yet."));
      Serial.println(F("Use c for 4-corner calibration first."));
      return;
    }

    makeSureMagnetIsOff(F("testAllSquares"));

    Serial.println();
    Serial.println(F("Starting 64-square test."));
    Serial.println(F("Send x during movement to abort."));
    Serial.println();

    for (int rankIndex = 0; rankIndex < 8; rankIndex++) {
      if (rankIndex % 2 == 0) {
        for (int fileIndex = 0; fileIndex < 8; fileIndex++) {
          char file = 'a' + fileIndex;
          char rank = '1' + rankIndex;

          bool success = moveToSquare(file, rank);
          if (!success) {
            Magnet::forceOff();
            delay(MAGNET_DROP_DELAY_MS);
            return;
          }

          delay(SQUARE_PAUSE_MS);
        }
      }
      else {
        for (int fileIndex = 7; fileIndex >= 0; fileIndex--) {
          char file = 'a' + fileIndex;
          char rank = '1' + rankIndex;

          bool success = moveToSquare(file, rank);
          if (!success) {
            Magnet::forceOff();
            delay(MAGNET_DROP_DELAY_MS);
            return;
          }

          delay(SQUARE_PAUSE_MS);
        }
      }
    }

    Magnet::forceOff();
    delay(MAGNET_DROP_DELAY_MS);
    Serial.println(F("64-square test complete."));
  }

  bool movePiece(char fromFile, char fromRank, char toFile, char toRank) {
    return movePieceSafe(fromFile, fromRank, toFile, toRank);
  }

  bool movePieceSafe(char fromFile, char fromRank, char toFile, char toRank) {
    if (!boardCalibrated) {
      Serial.println(F("Board not calibrated yet."));
      Magnet::forceOff();
      delay(MAGNET_DROP_DELAY_MS);
      return false;
    }

    makeSureMagnetIsOff(F("movePieceSafe"));

    if (!isValidSquare(fromFile, fromRank) || !isValidSquare(toFile, toRank)) {
      Serial.println(F("Invalid move square."));
      Magnet::forceOff();
      delay(MAGNET_DROP_DELAY_MS);
      return false;
    }

    int fromFileIndex = fileToIndex(fromFile);
    int fromRankIndex = rankToIndex(fromRank);

    int toFileIndex = fileToIndex(toFile);
    int toRankIndex = rankToIndex(toRank);

    int dFile = toFileIndex - fromFileIndex;
    int dRank = toRankIndex - fromRankIndex;

    long fromX, fromY;
    long toX, toY;

    squareToPosition(fromFile, fromRank, fromX, fromY);
    squareToPosition(toFile, toRank, toX, toY);

    char movingPiece = BoardState::getPiece(fromFile, fromRank);

    if (movingPiece == '.' || movingPiece == '?') {
      Serial.print(F("ERR NO_PIECE_AT "));
      Serial.print(fromFile);
      Serial.println(fromRank);
      Magnet::forceOff();
      delay(MAGNET_DROP_DELAY_MS);
      return false;
    }

    Serial.print(F("Safe moving piece "));
    Serial.print(movingPiece);
    Serial.print(F(" from "));
    Serial.print(fromFile);
    Serial.print(fromRank);
    Serial.print(F(" to "));
    Serial.print(toFile);
    Serial.println(toRank);

    if (!Motion::moveTo(fromX, fromY)) {
      Magnet::forceOff();
      delay(MAGNET_DROP_DELAY_MS);
      return false;
    }

    if (!magnetPickupSequence()) {
      return false;
    }

    if (dFile != 0 && dRank != 0 && abs(dFile) == abs(dRank)) {
      Serial.println(F("Using direct diagonal path."));

      if (!isDiagonalPathClear(fromFileIndex, fromRankIndex,
                               toFileIndex, toRankIndex)) {
        Serial.println(F("ERR DIAGONAL_PATH_BLOCKED"));
        Magnet::forceOff();
        delay(MAGNET_DROP_DELAY_MS);
        return false;
      }

      if (!Motion::moveTo(toX, toY)) {
        Magnet::forceOff();
        delay(MAGNET_DROP_DELAY_MS);
        return false;
      }

      if (!moveToReleaseOffset(fromFileIndex, fromRankIndex,
                               toFileIndex, toRankIndex)) {
        Magnet::forceOff();
        delay(MAGNET_DROP_DELAY_MS);
        return false;
      }

      magnetReleaseSequence();

      BoardState::movePiece(fromFile, fromRank, toFile, toRank);

      Serial.println(F("Safe diagonal piece move complete."));
      return true;
    }

    if (abs(dRank) >= abs(dFile)) {
      float laneFile = fromFileIndex + 0.5;
      if (dFile < 0 || (dFile == 0 && fromFileIndex == 7)) {
        laneFile = fromFileIndex - 0.5;
      }

      float laneRank = toRankIndex - 0.5;
      if (dRank < 0 || (dRank == 0 && toRankIndex == 0)) {
        laneRank = toRankIndex + 0.5;
      }

      long sourceBorderX, sourceBorderY;
      long borderCornerX, borderCornerY;
      long destinationBorderX, destinationBorderY;

      gridToPosition(laneFile, fromRankIndex, sourceBorderX, sourceBorderY);
      gridToPosition(laneFile, laneRank, borderCornerX, borderCornerY);
      gridToPosition(toFileIndex, laneRank, destinationBorderX, destinationBorderY);

      Serial.println(F("Using vertical lane path."));

      if (!Motion::moveTo(sourceBorderX, sourceBorderY)) {
        Magnet::forceOff();
        delay(MAGNET_DROP_DELAY_MS);
        return false;
      }

      if (!Motion::moveTo(borderCornerX, borderCornerY)) {
        Magnet::forceOff();
        delay(MAGNET_DROP_DELAY_MS);
        return false;
      }

      if (!Motion::moveTo(destinationBorderX, destinationBorderY)) {
        Magnet::forceOff();
        delay(MAGNET_DROP_DELAY_MS);
        return false;
      }

      if (!Motion::moveTo(toX, toY)) {
        Magnet::forceOff();
        delay(MAGNET_DROP_DELAY_MS);
        return false;
      }

      if (!moveToReleaseOffset(toFileIndex, laneRank,
                               toFileIndex, toRankIndex)) {
        Magnet::forceOff();
        delay(MAGNET_DROP_DELAY_MS);
        return false;
      }
    }
    else {
      float laneRank = fromRankIndex + 0.5;
      if (dRank < 0 || (dRank == 0 && fromRankIndex == 7)) {
        laneRank = fromRankIndex - 0.5;
      }

      float laneFile = toFileIndex - 0.5;
      if (dFile < 0 || (dFile == 0 && toFileIndex == 0)) {
        laneFile = toFileIndex + 0.5;
      }

      long sourceBorderX, sourceBorderY;
      long borderCornerX, borderCornerY;
      long destinationBorderX, destinationBorderY;

      gridToPosition(fromFileIndex, laneRank, sourceBorderX, sourceBorderY);
      gridToPosition(laneFile, laneRank, borderCornerX, borderCornerY);
      gridToPosition(laneFile, toRankIndex, destinationBorderX, destinationBorderY);

      Serial.println(F("Using horizontal lane path."));

      if (!Motion::moveTo(sourceBorderX, sourceBorderY)) {
        Magnet::forceOff();
        delay(MAGNET_DROP_DELAY_MS);
        return false;
      }

      if (!Motion::moveTo(borderCornerX, borderCornerY)) {
        Magnet::forceOff();
        delay(MAGNET_DROP_DELAY_MS);
        return false;
      }

      if (!Motion::moveTo(destinationBorderX, destinationBorderY)) {
        Magnet::forceOff();
        delay(MAGNET_DROP_DELAY_MS);
        return false;
      }

      if (!Motion::moveTo(toX, toY)) {
        Magnet::forceOff();
        delay(MAGNET_DROP_DELAY_MS);
        return false;
      }

      if (!moveToReleaseOffset(laneFile, toRankIndex,
                               toFileIndex, toRankIndex)) {
        Magnet::forceOff();
        delay(MAGNET_DROP_DELAY_MS);
        return false;
      }
    }

    magnetReleaseSequence();

    BoardState::movePiece(fromFile, fromRank, toFile, toRank);

    Serial.println(F("Safe piece move complete."));
    return true;
  }

  bool capturePiece(char fromFile, char fromRank,
                    char toFile, char toRank,
                    char capturedColor) {
    (void)fromFile;
    (void)fromRank;
    (void)toFile;
    (void)toRank;
    (void)capturedColor;

    Serial.println(F("ERR AUTO_CAPTURE_DISABLED"));
    Serial.println(F("Automatic captured-piece parking is disabled."));
    Serial.println(F("Remove the captured piece manually, then use regular move command r e2e4."));

    Magnet::forceOff();
    delay(MAGNET_DROP_DELAY_MS);

    return false;
  }

  bool castleKingside(char color) {
    if (!boardCalibrated) {
      Serial.println(F("ERR NOT_CALIBRATED"));
      Magnet::forceOff();
      delay(MAGNET_DROP_DELAY_MS);
      return false;
    }

    makeSureMagnetIsOff(F("castleKingside"));

    bool success = false;

    if (color == 'w') {
      Serial.println(F("White kingside castle."));

      success = movePieceSafe('e', '1', 'g', '1');
      if (!success) return false;

      delay(MAGNET_BETWEEN_MOVE_BUFFER_MS);

      success = movePieceSafe('h', '1', 'f', '1');
      if (!success) return false;
    }
    else if (color == 'b') {
      Serial.println(F("Black kingside castle."));

      success = movePieceSafe('e', '8', 'g', '8');
      if (!success) return false;

      delay(MAGNET_BETWEEN_MOVE_BUFFER_MS);

      success = movePieceSafe('h', '8', 'f', '8');
      if (!success) return false;
    }
    else {
      Serial.println(F("ERR INVALID_COLOR"));
      Magnet::forceOff();
      delay(MAGNET_DROP_DELAY_MS);
      return false;
    }

    Serial.println(F("OK CASTLE_KINGSIDE"));
    return true;
  }

  bool castleQueenside(char color) {
    if (!boardCalibrated) {
      Serial.println(F("ERR NOT_CALIBRATED"));
      Magnet::forceOff();
      delay(MAGNET_DROP_DELAY_MS);
      return false;
    }

    makeSureMagnetIsOff(F("castleQueenside"));

    bool success = false;

    if (color == 'w') {
      Serial.println(F("White queenside castle."));

      success = movePieceSafe('e', '1', 'c', '1');
      if (!success) return false;

      delay(MAGNET_BETWEEN_MOVE_BUFFER_MS);

      success = movePieceSafe('a', '1', 'd', '1');
      if (!success) return false;
    }
    else if (color == 'b') {
      Serial.println(F("Black queenside castle."));

      success = movePieceSafe('e', '8', 'c', '8');
      if (!success) return false;

      delay(MAGNET_BETWEEN_MOVE_BUFFER_MS);

      success = movePieceSafe('a', '8', 'd', '8');
      if (!success) return false;
    }
    else {
      Serial.println(F("ERR INVALID_COLOR"));
      Magnet::forceOff();
      delay(MAGNET_DROP_DELAY_MS);
      return false;
    }

    Serial.println(F("OK CASTLE_QUEENSIDE"));
    return true;
  }

  bool promotePiece(char fromFile, char fromRank,
                    char toFile, char toRank,
                    char promotedPiece) {
    if (!boardCalibrated) {
      Serial.println(F("ERR NOT_CALIBRATED"));
      Magnet::forceOff();
      delay(MAGNET_DROP_DELAY_MS);
      return false;
    }

    makeSureMagnetIsOff(F("promotePiece"));

    if (!isValidSquare(fromFile, fromRank) || !isValidSquare(toFile, toRank)) {
      Serial.println(F("ERR INVALID_SQUARE"));
      Magnet::forceOff();
      delay(MAGNET_DROP_DELAY_MS);
      return false;
    }

    char movingPiece = BoardState::getPiece(fromFile, fromRank);

    if (movingPiece == '.') {
      Serial.println(F("ERR NO_PIECE_TO_PROMOTE"));
      Magnet::forceOff();
      delay(MAGNET_DROP_DELAY_MS);
      return false;
    }

    bool success = movePieceSafe(fromFile, fromRank, toFile, toRank);

    if (!success) {
      Serial.println(F("ERR PROMOTION_MOVE_FAILED"));
      Magnet::forceOff();
      delay(MAGNET_DROP_DELAY_MS);
      return false;
    }

    if (movingPiece >= 'A' && movingPiece <= 'Z') {
      if (promotedPiece >= 'a' && promotedPiece <= 'z') {
        promotedPiece = promotedPiece - 32;
      }
    }
    else {
      if (promotedPiece >= 'A' && promotedPiece <= 'Z') {
        promotedPiece = promotedPiece + 32;
      }
    }

    BoardState::setPiece(toFile, toRank, promotedPiece);

    Serial.print(F("OK PROMOTION "));
    Serial.print(toFile);
    Serial.print(toRank);
    Serial.print(F("="));
    Serial.println(promotedPiece);

    BoardState::print();

    return true;
  }

  bool enPassant(char fromFile, char fromRank,
                 char toFile, char toRank,
                 char capturedFile, char capturedRank,
                 char capturedColor) {
    if (!boardCalibrated) {
      Serial.println(F("ERR NOT_CALIBRATED"));
      Magnet::forceOff();
      delay(MAGNET_DROP_DELAY_MS);
      return false;
    }

    makeSureMagnetIsOff(F("enPassant"));

    if (!isValidSquare(fromFile, fromRank) ||
        !isValidSquare(toFile, toRank) ||
        !isValidSquare(capturedFile, capturedRank)) {
      Serial.println(F("ERR INVALID_SQUARE"));
      Magnet::forceOff();
      delay(MAGNET_DROP_DELAY_MS);
      return false;
    }

    if (!(capturedColor == 'w' || capturedColor == 'W' ||
          capturedColor == 'b' || capturedColor == 'B')) {
      Serial.println(F("ERR INVALID_CAPTURE_COLOR"));
      Magnet::forceOff();
      delay(MAGNET_DROP_DELAY_MS);
      return false;
    }

    Serial.println(F("Starting manual en passant."));
    Serial.println(F("Captured pawn should already be removed physically."));

    bool success = movePieceSafe(fromFile, fromRank, toFile, toRank);

    if (!success) {
      Serial.println(F("ERR EN_PASSANT_MOVE_FAILED"));
      Magnet::forceOff();
      delay(MAGNET_DROP_DELAY_MS);
      return false;
    }

    BoardState::clearSquare(capturedFile, capturedRank);

    Serial.println(F("OK EN_PASSANT"));
    BoardState::print();

    return true;
  }
}

// ---------------- INTERNAL HELPER FUNCTIONS ----------------

static void promptCalibrationStep() {
  Serial.println();

  if (calibrationStep == 0) {
    Serial.println(F("Move to center of a1, then press k."));
  }
  else if (calibrationStep == 1) {
    Serial.println(F("Move to center of a8, then press k."));
  }
  else if (calibrationStep == 2) {
    Serial.println(F("Move to center of h8, then press k."));
  }
  else if (calibrationStep == 3) {
    Serial.println(F("Move to center of h1, then press k."));
  }

  Serial.println();
}

static void finishFourCornerCalibration(bool saveToEeprom) {
  calibrationMode = false;
  boardCalibrated = true;

  boardMaxX = round(cornerH8.x);
  boardMaxY = round(cornerH8.y);

  squareSpacingX = ((cornerH1.x - cornerA1.x) + (cornerH8.x - cornerA8.x)) / 14.0;
  squareSpacingY = ((cornerA8.y - cornerA1.y) + (cornerH8.y - cornerH1.y)) / 14.0;

  Serial.println();
  Serial.println(F("4-corner calibration complete."));
  Serial.println(F("The board grid has been generated."));
  Serial.println();

  if (saveToEeprom) {
    saveCalibration();
    Serial.println(F("Calibration saved to EEPROM."));
  }

  Calibration::printGrid();
}

static void saveCalibration() {
  StoredCalibration stored = {};
  stored.magic = CALIBRATION_MAGIC;
  stored.version = CALIBRATION_VERSION;

  PointF corners[] = {cornerA1, cornerH1, cornerA8, cornerH8};

  for (int corner = 0; corner < 4; corner++) {
    stored.coordinates[corner * 2] = round(corners[corner].x);
    stored.coordinates[corner * 2 + 1] = round(corners[corner].y);
  }

  stored.checksum = calibrationChecksum(stored);
  EEPROM.put(CALIBRATION_EEPROM_ADDRESS, stored);
}

static void clearSavedCalibration() {
  uint32_t emptyMagic = 0;
  EEPROM.put(CALIBRATION_EEPROM_ADDRESS, emptyMagic);
}

static uint16_t calibrationChecksum(const StoredCalibration &stored) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&stored);
  const size_t checksumOffset = offsetof(StoredCalibration, checksum);
  uint16_t checksum = 0xFFFF;

  for (size_t i = 0; i < checksumOffset; i++) {
    checksum ^= bytes[i];

    for (uint8_t bit = 0; bit < 8; bit++) {
      checksum = (checksum & 1) ? (checksum >> 1) ^ 0xA001 : checksum >> 1;
    }
  }

  return checksum;
}

static PointF lerp(PointF a, PointF b, float t) {
  PointF result;

  result.x = a.x + (b.x - a.x) * t;
  result.y = a.y + (b.y - a.y) * t;

  return result;
}

static bool gridToPosition(float fileCoord, float rankCoord, long &x, long &y) {
  if (!boardCalibrated) {
    return false;
  }

  float u = fileCoord / 7.0;
  float v = rankCoord / 7.0;

  PointF bottom = lerp(cornerA1, cornerH1, u);
  PointF top = lerp(cornerA8, cornerH8, u);

  PointF point = lerp(bottom, top, v);

  x = round(point.x);
  y = round(point.y);

  return true;
}

static bool squareToPosition(char file, char rank, long &x, long &y) {
  if (!isValidSquare(file, rank)) {
    return false;
  }

  int fileIndex = fileToIndex(file);
  int rankIndex = rankToIndex(rank);

  return gridToPosition(fileIndex, rankIndex, x, y);
}

static bool magnetPickupSequence() {
  Magnet::forceOff();
  delay(MAGNET_BETWEEN_MOVE_BUFFER_MS);

  delay(BEFORE_MAGNET_ON_DELAY_MS);

  Magnet::on();
  delay(MAGNET_PICKUP_DELAY_MS);

  if (!Magnet::isOn()) {
    Serial.println(F("ERR MAGNET_FAILED_TO_TURN_ON"));
    Magnet::forceOff();
    delay(MAGNET_DROP_DELAY_MS);
    return false;
  }

  return true;
}

static void magnetReleaseSequence() {
  delay(BEFORE_MAGNET_OFF_DELAY_MS);

  Magnet::forceOff();
  delay(250);
  Magnet::forceOff();
  delay(250);
  Magnet::forceOff();

  delay(MAGNET_DROP_DELAY_MS);
  delay(MAGNET_BETWEEN_MOVE_BUFFER_MS);
}

static void makeSureMagnetIsOff(const __FlashStringHelper *reason) {
  if (Magnet::isOn()) {
    Serial.print(F("Warning: magnet was still on before "));
    Serial.println(reason);
  }

  Magnet::forceOff();
  delay(MAGNET_BETWEEN_MOVE_BUFFER_MS);
}

static bool moveToReleaseOffset(float approachFileCoord, float approachRankCoord,
                                float targetFileCoord, float targetRankCoord) {
  float dFile = targetFileCoord - approachFileCoord;
  float dRank = targetRankCoord - approachRankCoord;

  float distance = sqrt(dFile * dFile + dRank * dRank);

  if (distance < 0.001) {
    delay(MAGNET_RELEASE_HOLD_MS);
    return true;
  }

  float releaseFileCoord = targetFileCoord + (dFile / distance) * RELEASE_FORWARD_OFFSET_SQUARES;
  float releaseRankCoord = targetRankCoord + (dRank / distance) * RELEASE_FORWARD_OFFSET_SQUARES;

  long releaseX;
  long releaseY;

  if (!gridToPosition(releaseFileCoord, releaseRankCoord, releaseX, releaseY)) {
    Serial.println(F("ERR RELEASE_OFFSET_FAILED"));
    return false;
  }

  Serial.print(F("Release offset target: grid "));
  Serial.print(releaseFileCoord);
  Serial.print(F(", "));
  Serial.print(releaseRankCoord);
  Serial.print(F(" -> X="));
  Serial.print(releaseX);
  Serial.print(F(", Y="));
  Serial.println(releaseY);

  if (!Motion::moveTo(releaseX, releaseY)) {
    Magnet::forceOff();
    delay(MAGNET_DROP_DELAY_MS);
    return false;
  }

  delay(MAGNET_RELEASE_HOLD_MS);

  return true;
}

static bool isDiagonalPathClear(int fromFileIndex, int fromRankIndex,
                                int toFileIndex, int toRankIndex) {
  int dFile = toFileIndex - fromFileIndex;
  int dRank = toRankIndex - fromRankIndex;

  if (dFile == 0 || dRank == 0) {
    return false;
  }

  if (abs(dFile) != abs(dRank)) {
    return false;
  }

  int fileStep = dFile > 0 ? 1 : -1;
  int rankStep = dRank > 0 ? 1 : -1;

  int steps = abs(dFile);

  for (int i = 1; i < steps; i++) {
    char checkFile = 'a' + fromFileIndex + fileStep * i;
    char checkRank = '1' + fromRankIndex + rankStep * i;

    char piece = BoardState::getPiece(checkFile, checkRank);

    if (piece != '.') {
      Serial.print(F("Diagonal blocked by "));
      Serial.print(piece);
      Serial.print(F(" at "));
      Serial.print(checkFile);
      Serial.println(checkRank);
      return false;
    }
  }

  return true;
}

static bool isValidSquare(char file, char rank) {
  return file >= 'a' && file <= 'h' && rank >= '1' && rank <= '8';
}

static int fileToIndex(char file) {
  return file - 'a';
}

static int rankToIndex(char rank) {
  return rank - '1';
}