#ifndef MAGNET_H
#define MAGNET_H

#include <Arduino.h>

namespace Magnet {
  void begin();

  void on();
  void off();
  void toggle();

  bool isOn();
}

#endif