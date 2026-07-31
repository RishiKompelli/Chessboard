#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>

namespace Calibration {
  void zeroPosition();
  void printPosition();

  // Old quick calibration still works
  void setBoardMax();

  // New 4-corner calibration
  void startFourCornerCalibration();
  void recordCalibrationPoint();
  bool loadCalibration();

  void printGrid();

  bool isCalibrated();

  long getBoardMaxX();
  long getBoardMaxY();

  float getSquareSpacingX();
  float getSquareSpacingY();

  bool moveToSquare(char file, char rank);
  void testAllSquares();

  bool movePiece(char fromFile, char fromRank, char toFile, char toRank);
  bool movePieceSafe(char fromFile, char fromRank, char toFile, char toRank);

  bool loadCalibration();

  void setCurrentPositionAsA1();
  void printStatus();

  bool capturePiece(char fromFile, char fromRank, char toFile, char toRank, char capturedColor);

  bool castleKingside(char color);
  bool castleQueenside(char color);

  bool promotePiece(char fromFile, char fromRank, char toFile, char toRank, char promotedPiece);

  bool enPassant(char fromFile, char fromRank, char toFile, char toRank, char capturedFile, char capturedRank, char capturedColor);

  void resetCaptureParking();
}

#endif
