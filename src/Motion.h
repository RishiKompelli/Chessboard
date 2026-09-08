#ifndef MOTION_H
#define MOTION_H

namespace Motion {
  void begin();

  void zeroPosition();

  long getX();
  long getY();

  bool moveTo(long targetX, long targetY);
}

#endif