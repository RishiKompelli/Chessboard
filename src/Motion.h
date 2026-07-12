#ifndef MOTION_H
#define MOTION_H

#include <Arduino.h>

namespace Motion {
  void begin();
  void update();

  void setJog(int xDirection, int yDirection);
  void stop();

  void speedUp();
  void slowDown();

  void zeroPosition();

  long getX();
  long getY();

  int getStepDelay();

  void rawMotorTest(char motor, int direction, long steps = 300);

  // automatic movement
  bool moveTo(long targetX, long targetY);
}

#endif