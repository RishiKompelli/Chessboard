#include "Calibration.h"
#include "Motion.h"
#include "Magnet.h"
#include "BoardState.h"
#include <EEPROM.h>

struct PointF {
  float x;
  float y;
};

// Four calibrated board corners
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

const int SQUARE_PAUSE_MS = 500;
const int MAGNET_PICKUP_DELAY_MS = 300;
const int MAGNET_DROP_DELAY_MS = 300;

const uint32_t CALIBRATION_MAGIC = 0x43414C34UL; // "CAL4"
const uint16_t CALIBRATION_VERSION = 1;
const int CALIBRATION_EEPROM_ADDRESS = 0;

struct StoredCalibration {
  uint32_t magic;
  uint16_t version;
  int32_t coordinates[8];
  uint16_t checksum;
};

// Internal functions
static void promptCalibrationStep();
static void finishFourCornerCalibration(bool saveToEeprom = true);
static void saveCalibration();
static void clearSavedCalibration();
static uint16_t calibrationChecksum(const StoredCalibration &stored);

static PointF lerp(PointF a, PointF b, float t);
static bool gridToPosition(float fileCoord, float rankCoord, long &x, long &y);
static bool squareToPosition(char file, char rank, long &x, long &y);

static bool isValidSquare(char file, char rank);
static int fileToIndex(char file);
static int rankToIndex(char rank);

namespace Calibration {

  void zeroPosition() {
    Motion::zeroPosition();

    boardCalibrated = false;
    calibrationMode = false;
    calibrationStep = 0;
    clearSavedCalibration();

    Serial.println("Position zeroed.");
    Serial.println("Existing board calibration cleared.");
    printPosition();
  }

  void printPosition() {
    Serial.print("Current position: X = ");
    Serial.print(Motion::getX());
    Serial.print(", Y = ");
    Serial.println(Motion::getY());
  }

  void startFourCornerCalibration() {
    boardCalibrated = false;
    calibrationMode = true;
    calibrationStep = 0;

    Serial.println();
    Serial.println("Starting 4-corner calibration.");
    Serial.println("Use WASD to move to each square center.");
    Serial.println("Press x to stop before saving each point.");
    Serial.println("Then press k to save that corner.");
    Serial.println();

    promptCalibrationStep();
  }

  void recordCalibrationPoint() {
    if (!calibrationMode) {
      Serial.println("Not in calibration mode.");
      Serial.println("Press c to start 4-corner calibration.");
      return;
    }

    PointF currentPoint;
    currentPoint.x = Motion::getX();
    currentPoint.y = Motion::getY();

    if (calibrationStep == 0) {
      cornerA1 = currentPoint;
      Serial.print("Saved a1: ");
    }
    else if (calibrationStep == 1) {
      cornerH1 = currentPoint;
      Serial.print("Saved h1: ");
    }
    else if (calibrationStep == 2) {
      cornerA8 = currentPoint;
      Serial.print("Saved a8: ");
    }
    else if (calibrationStep == 3) {
      cornerH8 = currentPoint;
      Serial.print("Saved h8: ");
    }

    Serial.print("X = ");
    Serial.print(currentPoint.x);
    Serial.print(", Y = ");
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
      Serial.println("No saved calibration found.");
      return false;
    }

    PointF *corners[] = {&cornerA1, &cornerH1, &cornerA8, &cornerH8};

    for (int corner = 0; corner < 4; corner++) {
      corners[corner]->x = stored.coordinates[corner * 2];
      corners[corner]->y = stored.coordinates[corner * 2 + 1];
    }

    finishFourCornerCalibration(false);
    Serial.println("Calibration loaded from EEPROM.");
    return true;
  }

  // Old quick calibration: assumes a1 is 0,0 and current position is h8
  void setBoardMax() {
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
      Serial.println("Board not calibrated yet.");
      Serial.println("Use c for 4-corner calibration.");
      return;
    }

    Serial.println();
    Serial.println("4-corner calibration data:");

    Serial.print("a1 = (");
    Serial.print(cornerA1.x);
    Serial.print(", ");
    Serial.print(cornerA1.y);
    Serial.println(")");

    Serial.print("h1 = (");
    Serial.print(cornerH1.x);
    Serial.print(", ");
    Serial.print(cornerH1.y);
    Serial.println(")");

    Serial.print("a8 = (");
    Serial.print(cornerA8.x);
    Serial.print(", ");
    Serial.print(cornerA8.y);
    Serial.println(")");

    Serial.print("h8 = (");
    Serial.print(cornerH8.x);
    Serial.print(", ");
    Serial.print(cornerH8.y);
    Serial.println(")");

    Serial.println();
    Serial.print("Average square spacing X = ");
    Serial.println(squareSpacingX);

    Serial.print("Average square spacing Y = ");
    Serial.println(squareSpacingY);

    Serial.println();
    Serial.println("Approx square centers:");

    for (int rank = 0; rank < 8; rank++) {
      for (int file = 0; file < 8; file++) {
        char fileChar = 'a' + file;
        char rankChar = '1' + rank;

        long squareX;
        long squareY;

        gridToPosition(file, rank, squareX, squareY);

        Serial.print(fileChar);
        Serial.print(rankChar);
        Serial.print("(");
        Serial.print(squareX);
        Serial.print(",");
        Serial.print(squareY);
        Serial.print(") ");
      }

      Serial.println();
    }

    Serial.println();
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
      Serial.println("Board not calibrated yet.");
      Serial.println("Use c for 4-corner calibration first.");
      return false;
    }

    long targetX;
    long targetY;

    if (!squareToPosition(file, rank, targetX, targetY)) {
      Serial.println("Invalid square.");
      return false;
    }

    Serial.print("Going to ");
    Serial.print(file);
    Serial.print(rank);
    Serial.print(" -> X=");
    Serial.print(targetX);
    Serial.print(", Y=");
    Serial.println(targetY);

    bool success = Motion::moveTo(targetX, targetY);

    printPosition();

    return success;
  }

  void testAllSquares() {
    if (!boardCalibrated) {
      Serial.println("Board not calibrated yet.");
      Serial.println("Use c for 4-corner calibration first.");
      return;
    }

    Serial.println();
    Serial.println("Starting 64-square test.");
    Serial.println("Send x during movement to abort.");
    Serial.println();

    for (int rankIndex = 0; rankIndex < 8; rankIndex++) {
      if (rankIndex % 2 == 0) {
        for (int fileIndex = 0; fileIndex < 8; fileIndex++) {
          char file = 'a' + fileIndex;
          char rank = '1' + rankIndex;

          bool success = moveToSquare(file, rank);
          if (!success) return;

          delay(SQUARE_PAUSE_MS);
        }
      }
      else {
        for (int fileIndex = 7; fileIndex >= 0; fileIndex--) {
          char file = 'a' + fileIndex;
          char rank = '1' + rankIndex;

          bool success = moveToSquare(file, rank);
          if (!success) return;

          delay(SQUARE_PAUSE_MS);
        }
      }
    }

    Serial.println("64-square test complete.");
  }

  bool movePiece(char fromFile, char fromRank, char toFile, char toRank) {
    if (!boardCalibrated) {
      Serial.println("Board not calibrated yet.");
      return false;
    }

    Serial.print("Moving piece from ");
    Serial.print(fromFile);
    Serial.print(fromRank);
    Serial.print(" to ");
    Serial.print(toFile);
    Serial.println(toRank);

    bool success = moveToSquare(fromFile, fromRank);

    if (!success) {
      Magnet::off();
      return false;
    }

    delay(200);

    Magnet::on();
    delay(MAGNET_PICKUP_DELAY_MS);

    success = moveToSquare(toFile, toRank);

    if (!success) {
      Magnet::off();
      return false;
    }

    delay(200);

    Magnet::off();
    delay(MAGNET_DROP_DELAY_MS);

    BoardState::movePiece(fromFile, fromRank, toFile, toRank);

    Serial.println("Piece move complete.");
    return true;
  }

  bool movePieceSafe(char fromFile, char fromRank, char toFile, char toRank) {
    if (!boardCalibrated) {
      Serial.println("Board not calibrated yet.");
      return false;
    }

    if (!isValidSquare(fromFile, fromRank) || !isValidSquare(toFile, toRank)) {
      Serial.println("Invalid move square.");
      return false;
    }

    int fromFileIndex = fileToIndex(fromFile);
    int fromRankIndex = rankToIndex(fromRank);

    int toFileIndex = fileToIndex(toFile);
    int toRankIndex = rankToIndex(toRank);

    long fromX, fromY;
    long toX, toY;

    squareToPosition(fromFile, fromRank, fromX, fromY);
    squareToPosition(toFile, toRank, toX, toY);

    Serial.print("Safe moving piece from ");
    Serial.print(fromFile);
    Serial.print(fromRank);
    Serial.print(" to ");
    Serial.print(toFile);
    Serial.println(toRank);

    if (!Motion::moveTo(fromX, fromY)) {
      Magnet::off();
      return false;
    }

    delay(200);

    Magnet::on();
    delay(MAGNET_PICKUP_DELAY_MS);

    int dFile = toFileIndex - fromFileIndex;
    int dRank = toRankIndex - fromRankIndex;

    // If mostly vertical, travel through a file lane between columns.
    if (abs(dRank) >= abs(dFile)) {
      float laneFile;

      if (fromFileIndex < 7) {
        laneFile = fromFileIndex + 0.5;
      }
      else {
        laneFile = fromFileIndex - 0.5;
      }

      long laneStartX, laneStartY;
      long laneEndX, laneEndY;

      gridToPosition(laneFile, fromRankIndex, laneStartX, laneStartY);
      gridToPosition(laneFile, toRankIndex, laneEndX, laneEndY);

      Serial.println("Using vertical lane path.");

      if (!Motion::moveTo(laneStartX, laneStartY)) {
        Magnet::off();
        return false;
      }

      if (!Motion::moveTo(laneEndX, laneEndY)) {
        Magnet::off();
        return false;
      }

      if (!Motion::moveTo(toX, toY)) {
        Magnet::off();
        return false;
      }
    }

    // If mostly horizontal, travel through a rank lane between rows.
    else {
      float laneRank;

      if (fromRankIndex < 7) {
        laneRank = fromRankIndex + 0.5;
      }
      else {
        laneRank = fromRankIndex - 0.5;
      }

      long laneStartX, laneStartY;
      long laneEndX, laneEndY;

      gridToPosition(fromFileIndex, laneRank, laneStartX, laneStartY);
      gridToPosition(toFileIndex, laneRank, laneEndX, laneEndY);

      Serial.println("Using horizontal lane path.");

      if (!Motion::moveTo(laneStartX, laneStartY)) {
        Magnet::off();
        return false;
      }

      if (!Motion::moveTo(laneEndX, laneEndY)) {
        Magnet::off();
        return false;
      }

      if (!Motion::moveTo(toX, toY)) {
        Magnet::off();
        return false;
      }
    }

    Magnet::off();
    delay(MAGNET_DROP_DELAY_MS);

    BoardState::movePiece(fromFile, fromRank, toFile, toRank);

    Serial.println("Safe piece move complete.");
    return true;
  }
}

// ---------------- INTERNAL HELPERS ----------------

static void promptCalibrationStep() {
  Serial.println();

  if (calibrationStep == 0) {
    Serial.println("Move to center of a1, then press k.");
  }
  else if (calibrationStep == 1) {
    Serial.println("Move to center of h1, then press k.");
  }
  else if (calibrationStep == 2) {
    Serial.println("Move to center of a8, then press k.");
  }
  else if (calibrationStep == 3) {
    Serial.println("Move to center of h8, then press k.");
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
  Serial.println("4-corner calibration complete.");
  Serial.println("The board grid has been generated.");
  Serial.println();

  if (saveToEeprom) {
    saveCalibration();
    Serial.println("Calibration saved to EEPROM.");
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

static bool isValidSquare(char file, char rank) {
  return file >= 'a' && file <= 'h' && rank >= '1' && rank <= '8';
}

static int fileToIndex(char file) {
  return file - 'a';
}

static int rankToIndex(char rank) {
  return rank - '1';
}
