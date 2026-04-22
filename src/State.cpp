#include "State.h"
#include <ArduinoJson.h>

bool State::signalk_parse_ws(const char *data) {
    JsonDocument doc;
    if (deserializeJson(doc, data) != DeserializationError::Ok) return false;
    if (!doc["updates"].is<JsonArray>()) return false;

    for (JsonObject update : doc["updates"].as<JsonArray>()) {
        if (!update["values"].is<JsonArray>()) continue;
        for (JsonObject v : update["values"].as<JsonArray>()) {
            const char *path = v["path"];
            if (!path) continue;
            JsonVariant val = v["value"];

            if      (!strcmp(path, "environment.wind.angleApparent"))    windAngleApparent = val.as<float>();
            else if (!strcmp(path, "environment.wind.speedApparent"))    windSpeedApparent = val.as<float>();
            else if (!strcmp(path, "environment.wind.speedTrue"))        windSpeedTrue     = val.as<float>();
            else if (!strcmp(path, "environment.wind.directionTrue"))    windDirectionTrue = val.as<float>();
            else if (!strcmp(path, "navigation.headingMagnetic"))        skHeadingMagnetic = val.as<float>();
            else if (!strcmp(path, "navigation.headingTrue"))            skHeadingTrue     = val.as<float>();
            else if (!strcmp(path, "navigation.speedOverGround"))        skSog             = val.as<float>();
            else if (!strcmp(path, "navigation.courseOverGroundTrue"))   skCog             = val.as<float>();
            else if (!strcmp(path, "navigation.position") && val.is<JsonObject>()) {
                skLat = val["latitude"].as<double>();
                skLon = val["longitude"].as<double>();
            }
        }
    }
    return true;
}
