#include "Magnet.h"

// MOSFET switch input pin
const int MAGNET_PIN = 9;

// Most MOSFET modules turn ON when the input pin is HIGH.
const bool MAGNET_ACTIVE_HIGH = true;

bool magnetState = false;

namespace Magnet {

  void begin() {
    pinMode(MAGNET_PIN, OUTPUT);
    off(); // always start with magnet off
  }

  void on() {
    magnetState = true;

    if (MAGNET_ACTIVE_HIGH) {
      digitalWrite(MAGNET_PIN, HIGH);
    } else {
      digitalWrite(MAGNET_PIN, LOW);
    }

    Serial.println("Electromagnet ON");
  }

  void off() {
    magnetState = false;

    if (MAGNET_ACTIVE_HIGH) {
      digitalWrite(MAGNET_PIN, LOW);
    } else {
      digitalWrite(MAGNET_PIN, HIGH);
    }

    Serial.println("Electromagnet OFF");
  }

  void toggle() {
    if (magnetState) {
      off();
    } else {
      on();
    }
  }

  bool isOn() {
    return magnetState;
  }
}