#include "BME280Handler.hpp"
#include "IodCoreClient.hpp"
#include <ArduinoJson.h>
#include <BME280I2C.h>
#include <EnvironmentCalculations.h>
#include <Wire.h>
#define IODCLIENT_DEBUG_ON 1

BME280I2C bme;

void addEntry(JsonBuffer &jsonBuffer, JsonObject &sensorData, const char *t,
              String v) {
  sensorData[t] = v;
#ifdef IODCLIENT_DEBUG_ON
  Serial.println("Adding entry");
#endif
}

void handleBME280(IoDCoreClient *client, JsonBuffer &jsonBuffer,
                  JsonObject &sensorData, JsonArray &activeSensors,
                  JsonArray &activeFeatures) {

  if (client->hasKey(&activeSensors, "BME280_TEMP")     //
      || client->hasKey(&activeSensors, "BME280_HYGRO") //
      || client->hasKey(&activeSensors, "BME280_BARO")  //
      || client->hasKey(&activeSensors, "BME280_ALTI")  //
      || client->hasKey(&activeSensors, "BME280_DEW")) {

    while (!bme.begin()) {
#ifdef IODCLIENT_DEBUG_ON
      Serial.println("Could not find BME280 sensor!");
#endif

      delay(1000);
    }

#ifdef IODCLIENT_DEBUG_ON
    Serial.println("Reading Sensors");
#endif

    bool metric = true;

    float pres = NAN;
    float temp = NAN;
    float hum = NAN;
    float alt = NAN;

    // unit: B000 = Pa,  B001 = hPa,  B010 = Hg,    B011 = atm,
    //       B100 = bar, B101 = torr, B110 = N/m^2, B111 = psi
    bme.read(pres, temp, hum, BME280::TempUnit_Celsius, BME280::PresUnit_hPa);

    if (client->hasKey(&activeFeatures, "I2C_DEVICE_ON_IO13") ||
        client->hasKey(&activeFeatures, "I2C_DEVICE_ON_IO0")) {
      delay(50); // read twice if the feature is set, otherwise we get bogus
                 // values
      bme.read(pres, temp, hum, BME280::TempUnit_Celsius, BME280::PresUnit_hPa);
    }

    // BME280_TEMP: { id: "BME280_TEMP", icon: "thermometer-half", descr:
    // "Thermometer" },
    if (client->hasKey(&activeSensors, "BME280_TEMP")) {
      addEntry(jsonBuffer, sensorData, "BME280_TEMP", String(temp));
    }
    // BME280_HYGRO: { id: "BME280_HYGRO", icon: "tint", descr: "Hygrometer" }
    if (client->hasKey(&activeSensors, "BME280_HYGRO")) {
      addEntry(jsonBuffer, sensorData, "BME280_HYGRO", String(hum));
    }
    // BME280_BARO: { id: "BME280_BARO", icon: "cloud", descr: "Barometer" }
    if (client->hasKey(&activeSensors, "BME280_BARO")) {
      addEntry(jsonBuffer, sensorData, "BME280_BARO", String(pres));
    }
    // BME280_ALTI: { id: "BME280_ALTI", icon: "arrows-v", descr: "Altimeter" }
    if (client->hasKey(&activeSensors, "BME280_ALTI")) {
      // Using ISA standards, the defaults for pressure and temperature at sea
      // level are 101,325 Pa and 288 K.
      addEntry(
          jsonBuffer, sensorData, "BME280_ALTI",
          String(EnvironmentCalculations::Altitude(pres, metric, 1013.25)));
    }
    // BME280_DEW: { id: "BME280_DEW", icon: "filter", descr: "Dewpoint" }
    if (client->hasKey(&activeSensors, "BME280_DEW")) {
      addEntry(jsonBuffer, sensorData, "BME280_DEW",
               String(EnvironmentCalculations::DewPoint(temp, hum, metric)));
    }
  }
  return;
}