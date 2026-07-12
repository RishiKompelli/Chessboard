#include "BoardState.h"

// board[rankIndex][fileIndex]
//
// rankIndex 0 = rank 1
// rankIndex 1 = rank 2
// ...
// rankIndex 7 = rank 8
//
// fileIndex 0 = a
// fileIndex 1 = b
// ...
// fileIndex 7 = h

char board[8][8];

bool isValidSquare(char file, char rank);
int fileToIndex(char file);
int rankToIndex(char rank);

namespace BoardState {

  void reset() {
    // Clear board first
    for (int rank = 0; rank < 8; rank++) {
      for (int file = 0; file < 8; file++) {
        board[rank][file] = '.';
      }
    }

    // White pieces
    board[0][0] = 'R';
    board[0][1] = 'N';
    board[0][2] = 'B';
    board[0][3] = 'Q';
    board[0][4] = 'K';
    board[0][5] = 'B';
    board[0][6] = 'N';
    board[0][7] = 'R';

    for (int file = 0; file < 8; file++) {
      board[1][file] = 'P';
    }

    // Black pieces
    board[7][0] = 'r';
    board[7][1] = 'n';
    board[7][2] = 'b';
    board[7][3] = 'q';
    board[7][4] = 'k';
    board[7][5] = 'b';
    board[7][6] = 'n';
    board[7][7] = 'r';

    for (int file = 0; file < 8; file++) {
      board[6][file] = 'p';
    }

    Serial.println("Board state reset to starting position.");
  }

  void print() {
    Serial.println();
    Serial.println("  +-----------------+");

    for (int rank = 7; rank >= 0; rank--) {
      Serial.print(rank + 1);
      Serial.print(" | ");

      for (int file = 0; file < 8; file++) {
        Serial.print(board[rank][file]);
        Serial.print(" ");
      }

      Serial.println("|");
    }

    Serial.println("  +-----------------+");
    Serial.println("    a b c d e f g h");
    Serial.println();
  }

  char getPiece(char file, char rank) {
    if (!isValidSquare(file, rank)) {
      return '?';
    }

    int fileIndex = fileToIndex(file);
    int rankIndex = rankToIndex(rank);

    return board[rankIndex][fileIndex];
  }

  void setPiece(char file, char rank, char piece) {
    if (!isValidSquare(file, rank)) {
      Serial.println("Invalid square.");
      return;
    }

    int fileIndex = fileToIndex(file);
    int rankIndex = rankToIndex(rank);

    board[rankIndex][fileIndex] = piece;
  }

  bool movePiece(char fromFile, char fromRank, char toFile, char toRank) {
    if (!isValidSquare(fromFile, fromRank) || !isValidSquare(toFile, toRank)) {
      Serial.println("Invalid move square.");
      return false;
    }

    int fromFileIndex = fileToIndex(fromFile);
    int fromRankIndex = rankToIndex(fromRank);

    int toFileIndex = fileToIndex(toFile);
    int toRankIndex = rankToIndex(toRank);

    char movingPiece = board[fromRankIndex][fromFileIndex];
    char capturedPiece = board[toRankIndex][toFileIndex];

    if (movingPiece == '.') {
      Serial.print("Warning: no piece at ");
      Serial.print(fromFile);
      Serial.println(fromRank);
      return false;
    }

    board[toRankIndex][toFileIndex] = movingPiece;
    board[fromRankIndex][fromFileIndex] = '.';

    Serial.print("Board updated: ");
    Serial.print(fromFile);
    Serial.print(fromRank);
    Serial.print(" -> ");
    Serial.print(toFile);
    Serial.println(toRank);

    if (capturedPiece != '.') {
      Serial.print("Captured piece logged: ");
      Serial.println(capturedPiece);
    }

    print();

    return true;
  }
}

bool isValidSquare(char file, char rank) {
  return file >= 'a' && file <= 'h' && rank >= '1' && rank <= '8';
}

int fileToIndex(char file) {
  return file - 'a';
}

int rankToIndex(char rank) {
  return rank - '1';
}