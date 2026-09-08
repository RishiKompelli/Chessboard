#ifndef MOTION_H
#define MOTION_H

namespace Motion {
  void begin();

  void zeroPosition();

  long getX();
  long getY();

  bool goTo(long targetX, long targetY);

  void jogStep(int xDir, int yDir);
}

#endif