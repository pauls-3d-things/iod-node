#ifndef FEATURE_HANDLER
#define FEATURE_HANDLER

#include "IodCoreClient.hpp"
#include <ArduinoJson.h>

void handleFeaturesBeforeSensors(IoDCoreClient *client, JsonBuffer &jsonBuffer,
                                 JsonArray &activeFeatures);

void handleFeaturesAfterSensors(IoDCoreClient *client, JsonBuffer &jsonBuffer,
                                JsonArray &activeFeatures);
#endif