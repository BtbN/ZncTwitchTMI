#pragma once

#include <string>
#include <list>
#include "json/json.h"

void initCurl();

std::string getUrl(const std::string &url, const std::list<std::string> &extraHeaders, const std::string &postData);
inline std::string getUrl(const std::string &url, const std::list<std::string> &extraHeaders) { return getUrl(url, extraHeaders, std::string()); }
inline std::string getUrl(const std::string &url) { return getUrl(url, std::list<std::string>(), std::string()); }

Json::Value getJsonFromUrl(const std::string &url, const std::list<std::string> &extraHeaders, const std::string &postData);
inline Json::Value getJsonFromUrl(const std::string &url, const std::list<std::string> &extraHeaders) { return getJsonFromUrl(url, std::list<std::string>(), std::string()); }
inline Json::Value getJsonFromUrl(const std::string &url) { return getJsonFromUrl(url, std::list<std::string>(), std::string()); }
