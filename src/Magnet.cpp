#include "Magnet.h"
#include <Arduino.h>

const int MAGNET_PIN = 9;

// Normal MOSFET:
// D9 HIGH = magnet on
// D9 LOW  = magnet off
const int MAGNET_ON_LEVEL = HIGH;
const int MAGNET_OFF_LEVEL = LOW;

bool magnetState = false;

namespace Magnet {

  void begin() {
    pinMode(MAGNET_PIN, OUTPUT);

    digitalWrite(MAGNET_PIN, MAGNET_OFF_LEVEL);
    delay(200);

    forceOff();

    Serial.println(F("Magnet system ready."));
  }

  void on() {
    digitalWrite(MAGNET_PIN, MAGNET_ON_LEVEL);
    magnetState = true;

    Serial.println(F("MAGNET ON"));
  }

  void off() {
    forceOff();
  }

  void forceOff() {
    digitalWrite(MAGNET_PIN, MAGNET_OFF_LEVEL);
    delay(80);

    digitalWrite(MAGNET_PIN, MAGNET_OFF_LEVEL);
    delay(80);

    digitalWrite(MAGNET_PIN, MAGNET_OFF_LEVEL);

    magnetState = false;

    Serial.println(F("MAGNET FORCE OFF"));
  }

  bool isOn() {
    return magnetState;
  }
}