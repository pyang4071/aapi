#include <Arduino.h>

constexpr int LED_PIN = 35;

int count = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  static int last = millis();
  static bool on = true;
  if (millis()-last >= 1000){
    on = !on;
    digitalWrite(LED_PIN, on);
    last = millis();
    Serial.println(count);
    count = count + 1;
  }
}


