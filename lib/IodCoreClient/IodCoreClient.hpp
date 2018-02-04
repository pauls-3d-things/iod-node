#ifndef IOD_CORE_CLIENT
#define IOD_CORE_CLIENT

#include <Arduino.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

#define MAX_CONFIG_SIZE                                                        \
  1024 // estimation via https://arduinojson.org/v5/assistant/

class IoDCoreClient {
private:
  char *_wifiSsid;
  char *_wifiPass;
  char *_iodHost;
  uint16_t _iodPort;
  char *_iodUser;
  char *_iodPass;

public:
  IoDCoreClient(char *wifiSsid, char *wifiPass, char *iodHost, uint16_t iodPort,
                char *iodUser, char *iodPass);

  bool hasUUID(EEPROMClass &eeprom);
  void createUUID(uint8_t *uuid);
  void getUUID(EEPROMClass &eeprom, uint8_t *uuid);
  void setUUID(EEPROMClass &eeprom, uint8_t *uuid);
  void uuid2string(uint8_t *uuid, char *uuidString);

  uint32_t getConfigLength(EEPROMClass &eeprom);
  void setConfigLength(EEPROMClass &eeprom, uint32_t length);

  void getConfigString(EEPROMClass &eeprom, char *json, uint32_t length);
  void setConfigString(EEPROMClass &eeprom, const char *json, uint32_t length);

  bool hasKey(JsonArray *jsonArray, const char *key);
  bool hasChanged(JsonObject &oldObject, JsonObject &newObject,
                  const char *entry);
  void updateUUID(EEPROMClass &eeprom, uint8_t *uuid, char *uuidString);
  uint8_t storeConfigIfNewer(EEPROMClass &eeprom, char *newConfig,
                             char *uuidString);
  uint8_t updateConfig(EEPROMClass &eeprom, char *uuidString);

  void connectToWifi();
  void fetchConfigString(char *nodeId, char *buf);
  void postValues(EEPROMClass &eeprom, JsonObject &values, char *uuidString);
};

#endif