
#include "FeatureHandler.hpp"
#include "IodCoreClient.hpp"
#include <ArduinoJson.h>
void handleFeaturesBeforeSensors(IoDCoreClient *client, JsonBuffer &jsonBuffer,
                                 JsonArray &activeFeatures) {

  if (client->hasKey(&activeFeatures, "I2C_DEVICE_ON_IO13")) {
    pinMode(13, OUTPUT);
    digitalWrite(13, HIGH); // provide 3V3
    delay(200);             // give sensor(s) some time to startup
  }

  if (client->hasKey(&activeFeatures, "I2C_DEVICE_ON_IO0")) {
    pinMode(0, OUTPUT);
    digitalWrite(0, HIGH); // provide 3V3
    delay(200);             // give sensor(s) some time to startup
  }
}

void handleFeaturesAfterSensors(IoDCoreClient *client, JsonBuffer &jsonBuffer,
                                JsonArray &activeFeatures) {

  if (client->hasKey(&activeFeatures, "I2C_DEVICE_ON_IO13")) {
    pinMode(13, OUTPUT);
    digitalWrite(13, LOW); // remove 3V3
  }

  if (client->hasKey(&activeFeatures, "I2C_DEVICE_ON_IO0")) {
    pinMode(0, OUTPUT);
    digitalWrite(0, LOW); // remove 3V3
  }
}