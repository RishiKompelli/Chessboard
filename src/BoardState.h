#ifndef BOARD_STATE_H
#define BOARD_STATE_H

#include <Arduino.h>

namespace BoardState {
  void reset();
  void print();

  char getPiece(char file, char rank);
  void setPiece(char file, char rank, char piece);

  bool movePiece(char fromFile, char fromRank, char toFile, char toRank);
}

#endif