#pragma once

#include <string>
#include "json/json.h"

std::string getUrl(const char *url);
Json::Value getJsonFromUrl(const char *url);
