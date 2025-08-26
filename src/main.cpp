#include "secrets.h"
#include <Arduino.h>
#include <BlynkSimpleEsp32.h>


const int sensor_pin = A0;
int moisture, sensor_analog;

BlynkTimer timer; 

void dataStreamTimer() {
  // Read the soil moisture
  sensor_analog = analogRead(sensor_pin);

  // Calculate the moisture in percent (ADC value is from 0 to 4095)
  moisture = (100 - ((sensor_analog/4095.00) * 100));

  // Print out to the serial monitor
  Serial.print("Moisture = ");
  Serial.print(moisture);
  Serial.println("%");
  
  // Send the moisture percentage to Blynk
  Blynk.virtualWrite(V5, moisture);  

  if (moisture < 30) {
    // If the moisture is less than 30%, send the notification to Blynk
    Blynk.logEvent("too-dry");
    Serial.println("Less than 30%. Need water");
  }

}

void setup() {
  Serial.begin(115200);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Call dataStreamTimer() every hour
  timer.setInterval(3600000L, dataStreamTimer); 
  
}

void loop() {
  Blynk.run();
  timer.run(); 

}

