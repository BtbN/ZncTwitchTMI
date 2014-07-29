#pragma once

#include <string>
#include "json/json.h"

void initCurl();
std::string getUrl(const char *url, const char *extraHeader = 0);
Json::Value getJsonFromUrl(const char *url, const char *extraHeader = 0);
