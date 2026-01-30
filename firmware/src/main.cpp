#include <Arduino.h>

// ----- Board-independent LED selection -----
#if defined(ARDUINO_XIAO_ESP32S3)
  #define LED_PIN 21        // XIAO ESP32-S3 built-in LED
#elif defined(ARDUINO_ESP32_DEV)
  #define LED_PIN 2         // Typical ESP32 dev board LED
#else
  #define LED_PIN LED_BUILTIN // Fallback (may be undefined)
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32 portable firmware starting...");
  Serial.print("CPU cores: ");
  Serial.println(ESP.getChipCores());
  Serial.print("Flash size: ");
  Serial.println(ESP.getFlashChipSize());

  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED ON");
  delay(500);

  digitalWrite(LED_PIN, LOW);
  Serial.println("LED OFF");
  delay(500);
}
