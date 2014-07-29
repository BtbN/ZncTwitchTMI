#include <curl/curl.h>

#include "jload.h"

static bool wasCurlInit = false;

static size_t WriteCB(void *contents, size_t size, size_t nmemb, void *userp)
{
	std::string *userstr = (std::string*)userp;
	userstr->append((char*)contents, size * nmemb);
	return size * nmemb;
}

std::string getUrl(const char *url)
{
	std::string resStr;

	CURL *curl;
	CURLcode res;

	if(!wasCurlInit)
	{
		curl_global_init(CURL_GLOBAL_ALL);
		wasCurlInit = true;
	}

	curl = curl_easy_init();
	if(!curl)
		return resStr;

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resStr);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &WriteCB);

	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	if(res != CURLE_OK)
	{
		resStr.clear();
	}

	return resStr;
}

Json::Value getJsonFromUrl(const char* url)
{
	Json::Value res;
	Json::Reader reader;

	std::string data = getUrl(url);

	if(data.empty())
		return res;

	bool ok = reader.parse(data, res);

	if(!ok)
		return Json::Value();

	return res;
}
