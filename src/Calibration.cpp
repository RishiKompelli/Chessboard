#include "Calibration.h"
#include "Motion.h"
#include "Magnet.h"
#include "BoardState.h"

long boardMaxX = 0;
long boardMaxY = 0;

float squareSpacingX = 0;
float squareSpacingY = 0;

bool boardCalibrated = false;

const int SQUARE_PAUSE_MS = 500;
const int MAGNET_PICKUP_DELAY_MS = 300;
const int MAGNET_DROP_DELAY_MS = 300;

bool squareToPosition(char file, char rank, long &x, long &y);

namespace Calibration {

  void zeroPosition() {
    Motion::zeroPosition();

    Serial.println("Position zeroed.");
    Serial.println("This should be a1 / bottom-left.");
    printPosition();
  }

  void printPosition() {
    Serial.print("Current position: X = ");
    Serial.print(Motion::getX());
    Serial.print(", Y = ");
    Serial.println(Motion::getY());
  }

  void setBoardMax() {
    boardMaxX = Motion::getX();
    boardMaxY = Motion::getY();

    squareSpacingX = boardMaxX / 7.0;
    squareSpacingY = boardMaxY / 7.0;

    boardCalibrated = true;

    Serial.println("Board max saved.");
    Serial.println("This should be h8 / top-right.");

    Serial.print("boardMaxX = ");
    Serial.println(boardMaxX);

    Serial.print("boardMaxY = ");
    Serial.println(boardMaxY);

    printGrid();
  }

  void printGrid() {
    if (!boardCalibrated) {
      Serial.println("Board not calibrated yet.");
      Serial.println("Move to h8 / top-right and press m first.");
      return;
    }

    Serial.println();
    Serial.println("Grid info:");

    Serial.print("Square spacing X = ");
    Serial.println(squareSpacingX);

    Serial.print("Square spacing Y = ");
    Serial.println(squareSpacingY);

    Serial.println();
    Serial.println("Approx square centers:");

    for (int rank = 0; rank < 8; rank++) {
      for (int file = 0; file < 8; file++) {
        char fileChar = 'a' + file;
        char rankChar = '1' + rank;

        long squareX = round(file * squareSpacingX);
        long squareY = round(rank * squareSpacingY);

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
      Serial.println("Use z at a1, then m at h8 first.");
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
      Serial.println("Use z at a1, then m at h8 first.");
      return;
    }

    Serial.println();
    Serial.println("Starting 64-square test.");
    Serial.println("Send x during movement to abort.");
    Serial.println();

    // Serpentine path:
    // a1 b1 c1 ... h1
    // h2 g2 f2 ... a2
    // a3 b3 ...
    // This avoids long jumps at the end of every row.

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
    Serial.println("Use z at a1, then m at h8 first.");
    return false;
  }

  Serial.print("Moving piece from ");
  Serial.print(fromFile);
  Serial.print(fromRank);
  Serial.print(" to ");
  Serial.print(toFile);
  Serial.println(toRank);

  // 1. Go to starting square
  bool success = moveToSquare(fromFile, fromRank);

  if (!success) {
    Serial.println("Could not reach starting square.");
    Magnet::off();
    return false;
  }

  delay(200);

  // 2. Pick up piece with electromagnet
  Magnet::on();
  delay(MAGNET_PICKUP_DELAY_MS);

  // 3. Drag piece to destination square
  success = moveToSquare(toFile, toRank);

  if (!success) {
    Serial.println("Move interrupted while carrying piece.");
    Magnet::off();
    return false;
  }

  delay(200);

  // 4. Drop piece
  Magnet::off();
  delay(MAGNET_DROP_DELAY_MS);

  BoardState::movePiece(fromFile, fromRank, toFile, toRank);

  Serial.println("Piece move complete.");
  return true;
}
}

bool squareToPosition(char file, char rank, long &x, long &y) {
  if (file < 'a' || file > 'h') {
    return false;
  }

  if (rank < '1' || rank > '8') {
    return false;
  }

  int fileIndex = file - 'a';
  int rankIndex = rank - '1';

  x = round(fileIndex * squareSpacingX);
  y = round(rankIndex * squareSpacingY);

  return true;
}