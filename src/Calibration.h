#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>

namespace Calibration {
  void zeroPosition();
  void printPosition();

  void setBoardMax();
  void printGrid();

  bool isCalibrated();

  long getBoardMaxX();
  long getBoardMaxY();

  float getSquareSpacingX();
  float getSquareSpacingY();

  bool moveToSquare(char file, char rank);
  void testAllSquares();

  bool movePiece(char fromFile, char fromRank, char toFile, char toRank);
}

#endif