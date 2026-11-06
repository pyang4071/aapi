#include <Arduino.h>

constexpr LED_PIN = 35;

int count = 0

voud setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  Serial.println(count);
  count = count + 1;

  static int last = millis();
  static bool on = true
  if (millis()-last >= 1){
    if (on == True) {
      digitalWrite(LED_PIN, LOW);
      on = false;
    } else {
      digitalWrite(LED_PIN, HIGH);
      on = true;
    }
    digitalWrite(LED_PIN, HIGH);
  }
}


