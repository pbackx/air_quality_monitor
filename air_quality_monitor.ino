#include <Adafruit_SGP30.h>
#include <EEPROM.h>
#include <SSD1306.h>
#include <Ticker.h>
#include <WiFi.h>
#include <Wire.h>

SSD1306  display(0x3c, 5, 4);
Adafruit_SGP30 sgp;

const int baseline_reset_button_pin = 13; // optionally, you can connect a button to reset the baseline
const int eeprom_address = 0; // Location where to store the baseline values in EEPROM. Needs 4 bytes

// Copy config.h.example into config.h and fill out your data
#include "config.h"
const char* wifi_ssid = WIFI_SSID;
const char* wifi_password = WIFI_PASSWORD;

// Great conversion trick by Tom Carpenter https://forum.arduino.cc/index.php?topic=119262.0
union BaselineValue {
  byte array[2];
  uint16_t integer;
};

BaselineValue TVOC_base;
BaselineValue eCO2_base;
Ticker baseline_writer;

void writeBaseline() {
  EEPROM.write(eeprom_address,   TVOC_base.array[0]);
  EEPROM.write(eeprom_address+1, TVOC_base.array[1]);
  EEPROM.write(eeprom_address+2, eCO2_base.array[0]);
  EEPROM.write(eeprom_address+3, eCO2_base.array[1]);
  EEPROM.commit();
  Serial.println("Wrote baseline values");
}

void readBaseline() {
  TVOC_base.array[0] = EEPROM.read(eeprom_address);
  TVOC_base.array[1] = EEPROM.read(eeprom_address+1);
  eCO2_base.array[0] = EEPROM.read(eeprom_address+2);
  eCO2_base.array[1] = EEPROM.read(eeprom_address+3);
}

void setup() {
  pinMode(baseline_reset_button_pin, INPUT);

  Serial.begin(115200);
  EEPROM.begin(4);

  readBaseline();
  Serial.print("****Read baseline values from EEPROM: eCO2: 0x"); Serial.print(eCO2_base.integer, HEX);
  Serial.print(" & TVOC: 0x"); Serial.println(TVOC_base.integer, HEX);

  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");

  Wire.begin(5, 4);
  if (! sgp.begin()){
    Serial.println("Sensor not found :(");
    while (1);
  }
  Serial.print("Found SGP30 serial #");
  Serial.print(sgp.serialnumber[0], HEX);
  Serial.print(sgp.serialnumber[1], HEX);
  Serial.println(sgp.serialnumber[2], HEX);

  if (! sgp.setIAQBaseline(eCO2_base.integer, TVOC_base.integer)) { // eco2, tvoc
    Serial.println("Unable to set baseline value.");
  }

  baseline_writer.attach(3600, writeBaseline);
}

int counter = 0;

void loop() {
  int switchState = digitalRead(baseline_reset_button_pin);
  if (switchState == HIGH) {
    sgp.IAQinit();
  }
  
  sgp.IAQmeasure();
  Serial.print("TVOC "); Serial.print(sgp.TVOC); Serial.print("\t");
  Serial.print("eCO2 "); Serial.println(sgp.eCO2);

  display.clear();
  String str = "TVOC ";
  str = str + sgp.TVOC;
  str = str + "\neCO2 ";
  str = str + sgp.eCO2;
  str += "\nbaseline TVOC 0x";
  str += String(TVOC_base.integer, HEX);
  str += "\nbaseline eCO2 0x";
  str += String(eCO2_base.integer, HEX);

  //to prevent OLED screen burn-in
  display.drawString(random(20), random(20), str);
  display.display();

  delay(1000);

  counter++;
  if (counter == 30) {
    counter = 0;
    if (! sgp.getIAQBaseline(&eCO2_base.integer, &TVOC_base.integer)) {
      Serial.println("Failed to get baseline readings");
      return;
    }
    Serial.print("****Baseline values: eCO2: 0x"); Serial.print(eCO2_base.integer, HEX);
    Serial.print(" & TVOC: 0x"); Serial.println(TVOC_base.integer, HEX);
  }
}
