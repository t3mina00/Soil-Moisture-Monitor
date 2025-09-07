#include "secrets.h"
#include <Arduino.h>
#include <BlynkSimpleEsp32.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string>
#include <map>


const int sensor_pin = A0;
const int photo_register_pin = A7;
int moisture, sensor_analog, light_analog;

const float VREF   = 3.3; // Board reference
const int   ADCMAX = 4095; // 12-bit ADC on ESP32 (values 0-4095)
const float R_FIXED = 1000.0; // Fixed resistor (1 kΩ)
float K_cal = 50000.0; // calibration constant
float b_cal = 0.7; // calibration exponent

QueueSetHandle_t lightAmountQueue;
QueueSetHandle_t blynkSendQueue;


void measureSoilMoistureTask(void * pvParameters) {
  TickType_t moistureLastWakeTime = xTaskGetTickCount();
  const TickType_t moistureFrequency = 3600000;

  std::map<std::string, int> moistureDic;

  // Execute every hour
  for( ;; ) {
    // Read the soil moisture
    sensor_analog = analogRead(sensor_pin);

    // Calculate the moisture in percent (ADC value is from 0 to 4095)
    moisture = (100 - ((sensor_analog/4095.00) * 100));

    Serial.print("Moisture = ");
    Serial.print(moisture);
    Serial.println("%");

    // Send the moisture to the blynk send queue
    moistureDic["moisture"] = moisture;
    xQueueSend(blynkSendQueue, &moistureDic, 10);

    // Wait for the next cycle.
    vTaskDelayUntil( &moistureLastWakeTime, moistureFrequency );
  }

}

void measureLightTask(void * pvParameters) {
  TickType_t lightLastWakeTime = xTaskGetTickCount();
  const TickType_t lightFrequency = 1000;

  // Execute every second
  for( ;; ) {
    // Read the amount of light
    light_analog = analogRead(photo_register_pin);

    // Protect against divide-by-zero and saturation
    if (light_analog <= 0) light_analog = 1;
    if (light_analog >= ADCMAX) light_analog = ADCMAX - 1;

    // Convert ADC to voltage
    float vout = (float)light_analog * VREF / ADCMAX;

    // Wiring A formula: LDR on top, R_FIXED to GND
    float r_ldr = R_FIXED * ((VREF / vout) - 1.0);

    // Lux estimate using power-law calibration
    // L = (K / R)^ (1/b)
    float lux = pow((K_cal / r_ldr), (1.0 / b_cal));

    Serial.print("The amount of light: ");
    Serial.println(lux);

    // Send the amount of light to the light amount queue
    xQueueSend(lightAmountQueue, &lux, 10);

    // Wait for the next cycle.
    vTaskDelayUntil( &lightLastWakeTime, lightFrequency );

  }

}

void accumulateLightAmountTask(void * pvParameters) {
  std::map<std::string, int> lightDic;
  float lightAmount;
  int totalLightAmount;
  int secondCount = 0;

  for( ;; ) {
    // If a new item in the queue, accumulate that value
    if(xQueueReceive(lightAmountQueue, &lightAmount, 10) == pdPASS) {
      // Accumulate the light amount
      totalLightAmount += (int)lightAmount;
      
      // Accumulate the second count
      secondCount += 1;

      // Send the total amount of light once every day
      // TODO: want to set a certain time (like 0:00) as a time to send
      if(secondCount == 86400) {
        Serial.print("Send the total amount of light: ");
        Serial.println(totalLightAmount);

        // Send the total amount of light to the queue
        lightDic["light"] = totalLightAmount;
        xQueueSend(blynkSendQueue, &lightDic, 10);

        // Reset the total light amount and second count 
        totalLightAmount = 0;
        secondCount = 0;

      }

    }
    
  }

}

void blynkSendTask(void * pvParameters) {
  std::map<std::string, int> readData;
  int moisture, light;

  for( ;; ) {
    // If a new item in the queue, send that value to Blynk
    if(xQueueReceive(blynkSendQueue, &readData, 10) == pdPASS) { 
      // Moisture
      if(readData.count("moisture")) {
        Serial.println("Send moisture");
        
        // Send the moisture percentage to Blynk
        moisture = readData["moisture"];
        Blynk.virtualWrite(V5, moisture);  

        if (moisture < 30) {
          // If the moisture is less than 30%, send the notification to Blynk
          Blynk.logEvent("too-dry");
          Serial.println("Less than 30%. Need water");

        }

      // Light
      } else if(readData.count("light")) {
        Serial.println("Send light");

        // Send the total amount of light to Blynk
        light = readData["light"];
        Blynk.virtualWrite(V0, light);  

      }
    }

    Blynk.run();
    
  }
}

void setup() {
  Serial.begin(115200);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Create queues
  lightAmountQueue = xQueueCreate(3, sizeof(float));
  blynkSendQueue = xQueueCreate(3, sizeof(std::map<std::string, int>));

  // Create tasks
  xTaskCreate(measureSoilMoistureTask, "Measure Solid Moisture Task", 2000, NULL, 2, NULL);
  xTaskCreate(measureLightTask, "Measure Light Task", 1000, NULL, 2, NULL);
  xTaskCreate(accumulateLightAmountTask, "Accumulate Light Amount Task", 1000, NULL, 2, NULL);
  xTaskCreate(blynkSendTask, "Send Blynk Task", 4000, NULL, 1, NULL);

}

void loop() {

}

