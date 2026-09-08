#ifndef CALIBRATION_H
#define CALIBRATION_H

namespace Calibration {
  void zeroPosition();
  void setCurrentPositionAsA1();
  void printPosition();

  void startFourCornerCalibration();
  void recordCalibrationPoint();
  bool loadCalibration();

  void setBoardMax();
  void printGrid();
  void printStatus();

  void resetCaptureParking();

  bool isCalibrated();

  long getBoardMaxX();
  long getBoardMaxY();

  float getSquareSpacingX();
  float getSquareSpacingY();

  bool moveToSquare(char file, char rank);

  void testAllSquares();

  bool movePiece(char fromFile, char fromRank, char toFile, char toRank);
  bool movePieceSafe(char fromFile, char fromRank, char toFile, char toRank);

  bool capturePiece(char fromFile, char fromRank,
                    char toFile, char toRank,
                    char capturedColor);

  bool castleKingside(char color);
  bool castleQueenside(char color);

  bool promotePiece(char fromFile, char fromRank,
                    char toFile, char toRank,
                    char promotedPiece);

  bool enPassant(char fromFile, char fromRank,
                 char toFile, char toRank,
                 char capturedFile, char capturedRank,
                 char capturedColor);
}

#endif