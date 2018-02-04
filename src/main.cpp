//#define IODCLIENT_DEBUG_ON 1
//#define ESP8285 // also switch to board=esp8285 in platformio.ini

#include "BME280Handler.hpp"
#include "FeatureHandler.hpp"
#include "IodCoreClient.hpp"
#include "defines.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <Wire.h>

IoDCoreClient client = IoDCoreClient(WIFI_SSID, WIFI_PASS, IOD_CORE_HOST,
                                     IOD_CORE_PORT, IOD_USER, IOD_PASS);

// 0. Boot/Wakeup
void setup() {
#ifdef ESP8285
  Wire.begin(4, 14);
#else
  Wire.begin();
#endif

#ifdef IODCLIENT_DEBUG_ON
  Serial.begin(9600);
  delay(3000);
  Serial.println("DEBUG mode!");
#endif

  EEPROM.begin(MAX_CONFIG_SIZE);

  uint8_t uuid[16];
  char uuidString[16 * 2 + 4 + 1];

  // 1. Check if node has UUID
  if (!client.hasUUID(EEPROM)) {
    client.updateUUID(EEPROM, uuid, uuidString);
  }

  // load UUID
  client.getUUID(EEPROM, uuid);
  client.uuid2string(uuid, uuidString);

#ifdef IODCLIENT_DEBUG_ON
  Serial.print("Loaded UUID: ");
  Serial.println(uuidString);
#endif

  // read configuration
  uint32_t len = client.getConfigLength(EEPROM);
  char bootConfig[len + 1];
  client.getConfigString(EEPROM, bootConfig, len);

#ifdef IODCLIENT_DEBUG_ON
  Serial.print("Loaded config: ");
  Serial.println(bootConfig);
#endif

  DynamicJsonBuffer jsonBuffer(2048);
  JsonObject &bootConfigJson = jsonBuffer.parseObject(bootConfig);

  // 2. Check if there is a local config
  if (String(bootConfigJson["id"].as<char *>()).equals(uuidString)) {

    JsonArray &features = bootConfigJson["activeFeatures"];
    JsonArray &sensors = bootConfigJson["activeSensors"];

    // first check if we have something todo:
#ifdef IODCLIENT_DEBUG_ON
    for (JsonVariant feature : features) {
      Serial.print("Feature: ");
      Serial.println(feature.as<char *>());
    }
    for (JsonVariant sensor : sensors) {
      Serial.print("Sensor: ");
      Serial.println(sensor.as<char *>());
    }
#endif

    if (features.size() == 0 && sensors.size() == 0) {
      // in case the config is empty, try to get an updated one.
      if (client.updateConfig(EEPROM, uuidString)) {
#ifdef IODCLIENT_DEBUG_ON
        Serial.println("Got updated config from server");
        Serial.println(String("Will sleep now for ") +
                       String(DEEP_SLEEP_MINUTES) + " minutes");
#endif
        ESP.deepSleep(1000 * 1000 * 60 * DEEP_SLEEP_MINUTES, WAKE_RF_DEFAULT);
      } else {
#ifdef IODCLIENT_DEBUG_ON
        Serial.println(String("Update failed, trying again in ") +
                       String(DEEP_SLEEP_MINUTES) + " minutes");
#endif
        ESP.deepSleep(1000 * 1000 * 60 * DEEP_SLEEP_MINUTES, WAKE_RF_DEFAULT);
      }
    }
    // handle tasks

    JsonObject &payLoad = jsonBuffer.createObject();
    payLoad["dataId"] = bootConfigJson["dataId"];
    JsonObject &sensorData = payLoad.createNestedObject("values");

    handleFeaturesBeforeSensors(&client, jsonBuffer, features);
    handleBME280(&client, jsonBuffer, sensorData, sensors, features);
    handleFeaturesAfterSensors(&client, jsonBuffer, features);

#ifdef IODCLIENT_DEBUG_ON
    String output;
    sensorData.printTo(output);
    Serial.println("SensorData: " + output);
#endif

    client.connectToWifi();
    client.postValues(EEPROM, payLoad, uuidString);

#ifdef IODCLIENT_DEBUG_ON
    Serial.println("GoodNight");
#endif

    ESP.deepSleep(1000 * bootConfigJson["sleepTimeMillis"].as<uint32_t>(),
                  WAKE_RF_DEFAULT);

    // TODO: advanced implementation ( |: measure, cache :| and send)
  } else {
    if (client.updateConfig(EEPROM, uuidString)) {
#ifdef IODCLIENT_DEBUG_ON
      Serial.println("Got initial config from server");
      Serial.println("Will sleep now for 10 seconds ");
#endif
      ESP.deepSleep(1000 * 1000 * 10, WAKE_RF_DEFAULT);
    } else {
#ifdef IODCLIENT_DEBUG_ON
      Serial.println(String("Fetch failed, trying again in ") +
                     String(DEEP_SLEEP_MINUTES) + " minutes");
#endif
      ESP.deepSleep(1000 * 1000 * 60 * DEEP_SLEEP_MINUTES, WAKE_RF_DEFAULT);
    }
  }

  // TODO: sleep for 3 minutes
}

void loop(void) {
  // There is no loop, this program will only execute once and go back to
  // sleep.
}