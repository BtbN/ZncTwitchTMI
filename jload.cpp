#include <curl/curl.h>
#include <memory>
#include <sstream>

#include "jload.h"

static bool wasCurlInit = false;

void initCurl()
{
    if(!wasCurlInit)
    {
        curl_global_init(CURL_GLOBAL_ALL);
        wasCurlInit = true;
    }
}

static size_t WriteCB(void *contents, size_t size, size_t nmemb, void *userp)
{
    std::string *userstr = (std::string*)userp;
    userstr->append((char*)contents, size * nmemb);
    return size * nmemb;
}

static size_t ReadCB(void *dest, size_t size, size_t nmemb, void *userp)
{
    std::istringstream *postSs = (std::istringstream*)userp;
    size_t bufsize = size * nmemb;
    postSs->read((char*)dest, bufsize);
    return postSs->gcount();
}

std::string getUrl(const std::string &url, const std::list<std::string> &extraHeaders, const std::string &postData)
{
    std::string resStr;

    CURL *curl;
    CURLcode res;

    initCurl();

    curl = curl_easy_init();
    if(!curl)
        return resStr;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resStr);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &WriteCB);

    struct curl_slist *hlist = nullptr;
    bool has_client_id = false;
    for(const std::string &extraHeader: extraHeaders)
    {
        hlist = curl_slist_append(hlist, extraHeader.c_str());
        if(extraHeader.substr(0, 10) == "Client-ID:")
            has_client_id = true;
    }
    if(!has_client_id)
        hlist = curl_slist_append(hlist, "Client-ID: kimne78kx3ncx6brgo4mv6wki5h1ko");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hlist);

    std::istringstream postSs(postData);
    if (!postData.empty())
    {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, &ReadCB);
        curl_easy_setopt(curl, CURLOPT_READDATA, &postSs);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)postData.size());
    }

    res = curl_easy_perform(curl);

    curl_slist_free_all(hlist);
    curl_easy_cleanup(curl);

    if(res != CURLE_OK)
        resStr.clear();

    return resStr;
}

Json::Value getJsonFromUrl(const std::string &url, const std::list<std::string> &extraHeaders, const std::string &postData)
{
    Json::Value res;
    Json::CharReaderBuilder rb;
    std::unique_ptr<Json::CharReader> reader(rb.newCharReader());

    std::string data = getUrl(url, extraHeaders, postData);

    if(data.empty())
        return res;

    bool ok = reader->parse(data.c_str(), data.c_str() + data.size(), &res, nullptr);

    if(!ok)
        return Json::Value();

    return res;
}
