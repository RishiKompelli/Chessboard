#include "Magnet.h"
#include <Arduino.h>

const int MAGNET_PIN = 9;

// Change these only if your MOSFET module is active-low.
const int MAGNET_ON_LEVEL = HIGH;
const int MAGNET_OFF_LEVEL = LOW;

bool magnetState = false;

namespace Magnet {

  void begin() {
    pinMode(MAGNET_PIN, OUTPUT);
    forceOff();
  }

  void on() {
    digitalWrite(MAGNET_PIN, MAGNET_ON_LEVEL);
    magnetState = true;
    Serial.println(F("MAGNET ON"));
  }

  void off() {
    digitalWrite(MAGNET_PIN, MAGNET_OFF_LEVEL);
    magnetState = false;
    Serial.println(F("MAGNET OFF"));
  }

  void forceOff() {
    digitalWrite(MAGNET_PIN, MAGNET_OFF_LEVEL);
    delay(20);
    digitalWrite(MAGNET_PIN, MAGNET_OFF_LEVEL);
    delay(20);
    digitalWrite(MAGNET_PIN, MAGNET_OFF_LEVEL);

    magnetState = false;
    Serial.println(F("MAGNET FORCE OFF"));
  }

  bool isOn() {
    return magnetState;
  }
}