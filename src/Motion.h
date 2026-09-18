#ifndef MOTION_H
#define MOTION_H

namespace Motion {
  void begin();

  void zeroPosition();

  long getX();
  long getY();

  bool goTo(long targetX, long targetY);

  void setPosition(long x, long y);

  void jogStep(int xDir, int yDir);

  bool goLine(long targetX, long targetY);

  bool testGoLineRelative(long dx, long dy);
}

#endif