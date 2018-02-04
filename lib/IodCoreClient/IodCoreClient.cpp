//#define IODCLIENT_DEBUG_ON 1

#include "IodCoreClient.hpp"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

// MEMORY MAP
// 4 bytes magic number [8,1,19,21] to know if we  "have a UUID"
#define UUID_OFFSET 4
#define UUID_LEN 16
// 16 bytes UUID
#define CONFIG_LEN_OFFSET UUID_OFFSET + 16
// 4 bytes config length
#define CONFIG_OFFSET CONFIG_LEN_OFFSET + 4
// [...] config

IoDCoreClient::IoDCoreClient(char *wifiSsid, char *wifiPass, char *iodHost,
                             uint16_t iodPort, char *iodUser, char *iodPass) {
  _wifiSsid = wifiSsid;
  _wifiPass = wifiPass;
  _iodHost = iodHost;
  _iodPort = iodPort;
  _iodUser = iodUser;
  _iodPass = iodPass;
}

bool IoDCoreClient::hasUUID(EEPROMClass &eeprom) {
  uint8_t h = eeprom.read(0);
  uint8_t a = eeprom.read(1);
  uint8_t s = eeprom.read(2);
  uint8_t u = eeprom.read(3);
  return h == 'h' && a == 'a' && s == 's' && u == 'u';
}

void IoDCoreClient::createUUID(uint8_t *uuid) {
  randomSeed(micros()); // yes, I know... if this bugs you please add an issue
                        // here: https://github.com/uvwxy/iod-core/issues/new
  for (int i = 0; i < UUID_LEN; i++) {
    uuid[i] = random(256);
  }
}

void IoDCoreClient::getUUID(EEPROMClass &eeprom, uint8_t *uuid) {
  for (int i = 0; i < UUID_LEN; i++) {
    uuid[i] = eeprom.read(i + UUID_OFFSET);
  }
}

void IoDCoreClient::setUUID(EEPROMClass &eeprom, uint8_t *uuid) {
  eeprom.write(0, 'h');
  eeprom.write(1, 'a');
  eeprom.write(2, 's');
  eeprom.write(3, 'u');

  for (int i = 0; i < UUID_LEN; i++) {
    eeprom.write(i + UUID_OFFSET, uuid[i]);
  }
}

void IoDCoreClient::uuid2string(uint8_t *uuid, char *uuidString) {
  uint8_t dashOffset = 0;
  for (int i = 0; i < UUID_LEN; i++) {
    if (i == 4 || i == 6 || i == 8 || i == 10) {
      uuidString[i * 2 + dashOffset] = '-';
      dashOffset++;
    }
    uuidString[i * 2 + 0 + dashOffset] = "0123456789abcdef"[uuid[i] >> 4];
    uuidString[i * 2 + 1 + dashOffset] = "0123456789abcdef"[uuid[i] & 0x0F];
  }
  uuidString[16 * 2 + 4] = 0; // terminate string
}

uint32_t IoDCoreClient::getConfigLength(EEPROMClass &eeprom) {
  uint32_t len = (uint32_t)                                   //
                 (eeprom.read(CONFIG_LEN_OFFSET + 3) << 24)   //
                 | (eeprom.read(CONFIG_LEN_OFFSET + 2) << 16) //
                 | (eeprom.read(CONFIG_LEN_OFFSET + 1) << 8)  //
                 | eeprom.read(CONFIG_LEN_OFFSET + 0);
  return len;
}

void IoDCoreClient::setConfigLength(EEPROMClass &eeprom, uint32_t length) {
  eeprom.write(CONFIG_LEN_OFFSET + 0, (byte)length);
  eeprom.write(CONFIG_LEN_OFFSET + 1, (byte)(length >> 8));
  eeprom.write(CONFIG_LEN_OFFSET + 2, (byte)(length >> 16));
  eeprom.write(CONFIG_LEN_OFFSET + 3, (byte)(length >> 24));
}

void IoDCoreClient::getConfigString(EEPROMClass &eeprom, char *json,
                                    uint32_t length) {
  // TODO: don't run across MAX_CONFIG_SIZE
  for (uint32_t i = 0; i < length; i++) {
    json[i] = eeprom.read(i + CONFIG_OFFSET);
  }
  json[length] = 0; // terminate string
}

void IoDCoreClient::setConfigString(EEPROMClass &eeprom, const char *json,
                                    uint32_t length) {
  // TODO: don't run across MAX_CONFIG_SIZE
  for (uint32_t i = 0; i < length; i++) {
    eeprom.write(i + CONFIG_OFFSET, json[i]);
  }
}

bool IoDCoreClient::hasKey(JsonArray *jsonArray, const char *key) {
  // TODO: loop through array and compare each element
  for (uint32_t i = 0; i < jsonArray->size(); i++) {
    if (String(jsonArray->get<char *>(i)).equals(key)) {
      return true;
    }
  }

  return false;
}

bool IoDCoreClient::hasChanged(JsonObject &oldObject, JsonObject &newObject,
                               const char *entry) {
  if (!oldObject[entry].as<String>().equals(newObject[entry].as<String>())) {
#ifdef IODCLIENT_DEBUG_ON
    Serial.println(String(entry) +
                   " has changed: " + oldObject[entry].as<String>() + " -> " +
                   newObject[entry].as<String>());
#endif
    return true;
  }

  return false;
}

uint8_t IoDCoreClient::storeConfigIfNewer(EEPROMClass &eeprom, char *newConfig,
                                          char *uuidString) {
  String newConfigString = String(newConfig);
#ifdef IODCLIENT_DEBUG_ON
  Serial.println(String("Got new Config:") + newConfigString);
#endif

  if (newConfigString.indexOf("lastSeen") > 0) { // we have a valid config
    uint32_t len = this->getConfigLength(eeprom);
    char oldConfig[len + 1];
    this->getConfigString(eeprom, oldConfig, len);
#ifdef IODCLIENT_DEBUG_ON
    Serial.println("Loading from EEPROM");
    Serial.println(String("Length: ") + len);
    Serial.println(oldConfig);
#endif
    DynamicJsonBuffer oldJsonBuffer(1024);
    JsonObject &oldConfigJson = oldJsonBuffer.parseObject(oldConfig);

    DynamicJsonBuffer newJsonBuffer(1024);
    JsonObject &newConfigJson = newJsonBuffer.parseObject(newConfig);

    if (hasChanged(oldConfigJson, newConfigJson, "numberOfSamples") ||
        hasChanged(oldConfigJson, newConfigJson, "sleepTimeMillis") ||
        hasChanged(oldConfigJson, newConfigJson, "ipv4address") ||
        hasChanged(oldConfigJson, newConfigJson, "activeSensors") ||
        hasChanged(oldConfigJson, newConfigJson, "activeFeatures")) {

#ifdef IODCLIENT_DEBUG_ON
      Serial.println("Config has changed, writing to EEPROM");
      Serial.println(String("Length: ") + newConfigString.length());
      Serial.println(newConfigString);
#endif

      this->setConfigLength(eeprom, newConfigString.length());
      this->setConfigString(eeprom, newConfigString.c_str(),
                            newConfigString.length());

      if (String(newConfigJson["id"].as<char *>()).equals(uuidString)) {
        // only commit if config contained our ID
        if (eeprom.commit()) {
#ifdef IODCLIENT_DEBUG_ON
          Serial.println("Saved.");
#endif
          return 1; // SUCCESS (with saving)
        } else {
#ifdef IODCLIENT_DEBUG_ON
          Serial.println("FAILED.");
#endif
          return -3; // EEPROM commit failed
        }
      } else {
        return -2; // we got a wrong config...
      }
    } else {
      return 2; // SUCCESS (without saving)
    }

  } else {
    return -1; // invalid config
  }
}

uint8_t IoDCoreClient::updateConfig(EEPROMClass &eeprom, char *uuidString) {

#ifdef IODCLIENT_DEBUG_ON
  Serial.println("Fetching config...");
  Serial.println(uuidString);
#endif

  // config not set, get new config
  this->connectToWifi();

  char newConfig[1024];
  this->fetchConfigString(uuidString, newConfig);

  return storeConfigIfNewer(eeprom, newConfig, uuidString);
}

void IoDCoreClient::updateUUID(EEPROMClass &eeprom, uint8_t *uuid,
                               char *uuidString) {

#ifdef IODCLIENT_DEBUG_ON
  Serial.println();
  Serial.println("UUID missing...");
#endif

  // create UUID and store it
  this->createUUID(uuid);
  this->setUUID(eeprom, uuid);

#ifdef IODCLIENT_DEBUG_ON
  Serial.print("Created UUID: ");
  this->uuid2string(uuid, uuidString);
  Serial.println(uuidString);
#endif

  // create empty config and store it
  String emptyConfig = "{}";
  uint32_t len = emptyConfig.length();
  this->setConfigLength(eeprom, len);
  this->setConfigString(eeprom, emptyConfig.c_str(), len);

#ifdef IODCLIENT_DEBUG_ON
  Serial.print("Created empty config... ");
#endif
  eeprom.commit();
}

void IoDCoreClient::connectToWifi() {
  // TODO: add possibility to use fixed ip:
  // https://github.com/esp8266/Arduino/issues/1959
#ifdef IODCLIENT_DEBUG_ON
  Serial.println("Enabling WIFI");
#endif

  uint8_t tries = 0;
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);

  while (WiFi.status() != WL_CONNECTED) {

#ifdef IODCLIENT_DEBUG_ON
    Serial.println("Connecting to WIFI");
#endif

    if (tries % 10 == 0) {
      WiFi.begin(_wifiSsid, _wifiPass);
    }

    delay(500);
    tries++;
  }

#ifdef IODCLIENT_DEBUG_ON
  Serial.println("Connected to WIFI");
  Serial.println(WiFi.localIP());
#endif
}

void IoDCoreClient::fetchConfigString(char *nodeId, char *buf) {

  if (WiFi.status() == WL_CONNECTED) {
    String url = "http://" + String(_iodHost) + ":" + String(_iodPort) +
                 "/api/node/" + String(nodeId) + "/config";

#ifdef IODCLIENT_DEBUG_ON
    Serial.println(url);
#endif

    HTTPClient http;

#ifdef IODCLIENT_DEBUG_ON
    Serial.println("Beginning");
#endif

    http.begin(url);

#ifdef IODCLIENT_DEBUG_ON
    Serial.println("Setting auth");
#endif

    http.setAuthorization(_iodUser, _iodPass);

#ifdef IODCLIENT_DEBUG_ON
    Serial.println("Calling GET");
#endif
    int code = http.GET();

    if (code == 200) {
#ifdef IODCLIENT_DEBUG_ON
      Serial.println("GET successful");
#endif
      String payload = http.getString();

#ifdef IODCLIENT_DEBUG_ON
      Serial.println(payload);
#endif

      payload.toCharArray(buf, payload.length() + 1);
      return;
    } else {
      if (code == 404) {
        http.end();
#ifdef IODCLIENT_DEBUG_ON
        Serial.println("Registering");
#endif
        http.begin(url);
        code = http.POST(String(""));

        if (code == 200) {
          String regPayload = http.getString();
#ifdef IODCLIENT_DEBUG_ON
          Serial.println("OK");
          Serial.println(regPayload);
#endif
          regPayload.toCharArray(buf, regPayload.length() + 1);
          return;
        }
      }
#ifdef IODCLIENT_DEBUG_ON
      Serial.println(String("error: ") + code);
#endif
    }

    http.end(); // Close connection
  }
}

void IoDCoreClient::postValues(EEPROMClass &eeprom, JsonObject &payload,
                               char *uuidString) {

  if (WiFi.status() == WL_CONNECTED) {
    String url = "http://" + String(_iodHost) + ":" + String(_iodPort) +
                 "/api/node/" + String(uuidString) + "/values";

#ifdef IODCLIENT_DEBUG_ON
    Serial.println(url);
#endif

    HTTPClient http;

#ifdef IODCLIENT_DEBUG_ON
    Serial.println("Beginning");
#endif

    http.begin(url);
    http.addHeader("Content-Type", "application/json");

#ifdef IODCLIENT_DEBUG_ON
    Serial.println("Setting auth");
#endif

    http.setAuthorization(_iodUser, _iodPass);

    String body;
    payload.printTo(body);

#ifdef IODCLIENT_DEBUG_ON
    Serial.println("Caling POST");
    Serial.println(body);
#endif

    int code = http.POST(body);

    if (code == 200) {
#ifdef IODCLIENT_DEBUG_ON
      Serial.println("POST successful");
#endif
      String payload = http.getString();

#ifdef IODCLIENT_DEBUG_ON
      Serial.println(payload);
#endif
      char newConfig[1024];
      payload.toCharArray(newConfig, payload.length() + 1);
      storeConfigIfNewer(eeprom, newConfig, uuidString);

      return;
    } else {
#ifdef IODCLIENT_DEBUG_ON
      Serial.println(String("error: ") + code);
#endif
      if (code == 500) {
        // this can happen if the device has been moved to the wrong server
        char newConfig[1024];
        fetchConfigString(uuidString, newConfig); // will register if not registered
        storeConfigIfNewer(eeprom, newConfig, uuidString);
      }
    }

    http.end(); // Close connection
  }
}
