#ifndef BME280HANDLER
#define BME280HANDLER

#include <ArduinoJson.h>
#include <IodCoreClient.hpp>

void handleBME280(IoDCoreClient *client, JsonBuffer &jsonBuffer,
                  JsonObject &sensorData, JsonArray &activeSensors,
                  JsonArray &activeFeatures);

#endif