#include <curl/curl.h>

#include "jload.h"

static bool wasCurlInit = false;

static size_t WriteCB(void *contents, size_t size, size_t nmemb, void *userp)
{
	std::string *userstr = (std::string*)userp;
	userstr->append((char*)contents, size * nmemb);
	return size * nmemb;
}

std::string getUrl(const char *url, const char* extraHeader)
{
	std::string resStr;

	CURL *curl;
	CURLcode res;

	initCurl();

	curl = curl_easy_init();
	if(!curl)
		return resStr;

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resStr);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &WriteCB);

	struct curl_slist *hlist = nullptr;

	if(extraHeader)
	{
		hlist = curl_slist_append(hlist, extraHeader);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hlist);
	}

	res = curl_easy_perform(curl);

	if(hlist)
	{
		curl_slist_free_all(hlist);
	}

	curl_easy_cleanup(curl);

	if(res != CURLE_OK)
	{
		resStr.clear();
	}

	return resStr;
}

Json::Value getJsonFromUrl(const char* url, const char* extraHeader)
{
	Json::Value res;
	Json::Reader reader;

	std::string data = getUrl(url, extraHeader);

	if(data.empty())
		return res;

	bool ok = reader.parse(data, res);

	if(!ok)
		return Json::Value();

	return res;
}

void initCurl()
{
	if(!wasCurlInit)
	{
		curl_global_init(CURL_GLOBAL_ALL);
		wasCurlInit = true;
	}
}
