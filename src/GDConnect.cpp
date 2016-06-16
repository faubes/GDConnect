/*
 * GDConnect.cc
 *
 *  Module for accessing Google Drive
 *  Provides methods for authentication (OAuth2.0), listing, download and upload
 *
 *  Created on: Jun 1, 2016
 *      Author: Joël Faubert
 */

// #define DEBUG

#include "GDConnect.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <ctime>
#include <sstream>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <json/json.h>


GDConnect::GDConnect()
{
    GDConnect("config.json");
}

/*
Processes a client_secret JSON file obtained from Google Cloud Developper Console
Loads client_id, client_secret and other info required for authentication into memory
*/
GDConnect::GDConnect(const char * configFilename)
{
    ok = false;
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

GDConnect::~GDConnect()
{
    curl_global_cleanup();
}

int GDConnect::init(const char * configFilename)
{
    Json::Value config;
    Json::Reader reader;
    std::ifstream configFile;
    configFile.open(configFilename);
    if (configFile.is_open())
    {
        if (reader.parse(configFile, config))
        {
            clientID = config["installed"]["client_id"].asString();
            clientSecret = config["installed"]["client_secret"].asString();
            authURL = config["installed"]["auth_uri"].asString();
            tokenURL = config["installed"]["token_uri"].asString();
            authScope = "https://www.googleapis.com/auth/drive";
            redirectURI = config["installed"]["redirect_uris"][0].asString();
            ok = true;
        }
        else
        {
            std::cerr << "Unable to parse config file!" << std::endl;
            return -1;
        }
    }
    else
    {
        std::cerr << "Unable to open config file!" << std::endl;
        return -1;
    }


    Json::Value token;
    std::ifstream tokenFile;
    tokenFile.open("token.json");
    if (tokenFile.is_open())
    {
        if (reader.parse(tokenFile, token))
        {
            std::time_t currentTime;
            std::time(&currentTime);
            std::string stimestamp = token["timestamp"].asString();
            long int ts = atol(stimestamp.c_str());
            if (currentTime - ts < 3600)
            {
                accessToken = token["access_token"].asString();
                refreshToken = token["refresh_token"].asString();
                timestamp = (time_t) ts;
            }
        }
        else
        {
            timestamp = 0;
        }
        tokenFile.close();
    }
    return 0;
}

/* Callback function used by cURL to process responses into memory */
std::size_t GDConnect::callback(const char* in, std::size_t size, std::size_t num, std::string* out)
{
    const std::size_t totalBytes(size * num);
    out->append(in, totalBytes);
    return totalBytes;
}

/* Callback function used by cURL to write responses to file */
std::size_t GDConnect::write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}


long GDConnect::getFileSize(const char * filename)
{
    std::fstream file(filename, std::fstream::ate | std::fstream::binary);

    if(!file.is_open())
    {
        return -1;
    }
    int fileSize = file.tellg();
    file.close();

    return fileSize;
}
/* Function for sending HTTP Post */
std::pair<std::string, int> GDConnect::post(const char * endpoint, const char * msg, bool authorized=false)
{
    CURL *curlHandle;
    CURLcode res;
    struct curl_slist *slist=NULL;
    std::string response;
    std::pair<std::string, int> result;
    curlHandle = curl_easy_init(); // start up cURL with handle

    if (curlHandle)
    {
        curl_easy_setopt(curlHandle, CURLOPT_URL, endpoint);
#ifdef DEBUG
        curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 1); // for debugging
        curl_easy_setopt(curlHandle, CURLOPT_HEADER, 1); // for debugging
#endif
        curl_easy_setopt(curlHandle, CURLOPT_POST, 1);
        curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDSIZE, strlen(msg));
        curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, msg);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, callback);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &response);
        if (authorized)
        {
            const char * authHeader = ("Authorization: Bearer " + accessToken).c_str();
            slist = curl_slist_append(slist, authHeader);
            curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, slist);
        }
        res = curl_easy_perform(curlHandle);

        if(res != CURLE_OK)  // something went wrong
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            result.first = "curl_easy_perform() failed";
            result.second = -1;
        }
        else
        {
            long response_code;
            curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &response_code);
            if (response_code == 302)
            {
                char * redirectURL;
                res = curl_easy_getinfo(curlHandle, CURLINFO_REDIRECT_URL, &redirectURL);
                result.first = redirectURL;
            }
            else
            {
                result.first = response;
            }
            result.second = response_code;
        }
    }
    else
    {
        result.first = "Unable to start curl";
        result.second = -1;
    }
    if (authorized)
        curl_slist_free_all(slist);
    curl_easy_cleanup(curlHandle);
    return result;
}

/* Function for sending HTTP get */
std::pair<std::string, int> GDConnect::get(const char * endpoint, const char * msg = NULL, bool authorized=false)
{
    CURL *curlHandle;
    CURLcode res;
    struct curl_slist *slist=NULL;
    std::string response;
    std::pair<std::string, int> result;
    curlHandle = curl_easy_init(); // start up cURL with handle
    if (curlHandle)
    {
        std::string url = std::string(endpoint) + msg;
        curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
#ifdef DEBUG
        curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 1); // for debugging
        curl_easy_setopt(curlHandle, CURLOPT_HEADER, 1); // for debugging
#endif
        curl_easy_setopt(curlHandle, CURLOPT_HTTPGET, 1);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, callback);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &response);
        if (authorized)
        {
            const char * authHeader = ("Authorization: Bearer " + accessToken).c_str();
            slist = curl_slist_append(slist, authHeader);
            curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, slist);
        }
        res = curl_easy_perform(curlHandle);

        if(res != CURLE_OK)  // something went wrong
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            result.first = "curl_easy_perform() failed";
            result.second = -1;
        }
        else
        {
            long response_code;
            curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &response_code);
            if (response_code == 302)
            {
                char * redirectURL;
                res = curl_easy_getinfo(curlHandle, CURLINFO_REDIRECT_URL, &redirectURL);
                result.first = redirectURL;
            }
            else
            {
                result.first = response;
            }
            result.second = response_code;
        }
    }
    else
    {
        result.first = "Unable to start curl";
        result.second = -1;
    }
    if (authorized)
        curl_slist_free_all(slist);
    curl_easy_cleanup(curlHandle);
    return result;
}

/* Function for saving Token object to disk.
    Also adds a timestamp to verify validity
*/
int GDConnect::saveToken(Json::Value root)
{
    // Add timestamp and save JSON response to a file for reuse within the hour
    std::time_t timestamp;
    std::time(&timestamp);
    std::stringstream stimestamp;
    stimestamp << timestamp;
    root["timestamp"] = stimestamp.str(); // store seconds since unix epoch in the JSON object for expiry of token
    Json::StyledWriter writer;
    std::string output = writer.write(root);
    std::fstream tokenFile;
    tokenFile.open("token.json", std::fstream::out | std::fstream::trunc);
    if (tokenFile.is_open())
    {
        tokenFile << output;
        tokenFile.close();
    }
    else
    {
        std::cerr << "Error writing token file!" << std::endl;
        return -1;
    }
    return 0;
}

/* Function for OAuth 2.0 authentication flow
    Step 1: Obtain authentication URL
    Step 2: User follows URL, signs into Google and provides consent to GDConnect
    Step 3: Google provides validation code to user
    Step 4: Validation code is used to obtain access token and refresh token
*/
int GDConnect::getToken()
{
    std::time_t currentTime;
    std::time(&currentTime);
    if (currentTime - timestamp < 3600)
    {
        std::cout << "Current credentials are still valid!" << std::endl;
        return 0;
    }

    /* Step 1 - Obtain validation URL for user to provide consent */
    std::stringstream reqBuilder;
    reqBuilder << "scope=" << authScope << "&"
               << "redirect_uri=" << redirectURI << "&"
               << "response_type=code" << "&"
               << "client_id=" << clientID ;
    std::string req = reqBuilder.str();
#ifdef DEBUG
    std::cout << req << std::endl; // for debugging
#endif // DEBUG
    std::pair<std::string, int> response;
    response = post(authURL.c_str(), req.c_str());
    if (response.second != 302)
    {
        std::cerr << "Something went wrong! First request should return code 302: redirect to authorization URL" << std::endl;
        std::cerr << "Response code was: " << response.second << std::endl
                  << "and response was: " << response.first << std::endl;
        return -1;
    }

    /* Step 2 - User clicks on provided link and obtains validation code.
     * Validation code is used to obtain authentication token and refresh token */

    std::cout << "Navigate to the following URL to allow Dagda-Cloud to access your Google drive:" << std::endl;
    std::cout << response.first << std::endl;

    bool valid = false;
    do
    {
        std::cout << "Enter the validation code (or nothing to abort): ";
        getline(std::cin, validationCode);
        if (validationCode.empty())
        {
            std::cout << "Aborted" << std::endl;
            return -1;
        }
        if (strlen(validationCode.c_str()) != 45)
            std::cout << "Validation code should be 45 chars long" << std::endl;
        else
            valid = true;
    }
    while (!valid);

    /* Step 3 user-provided authentication code is sent to
    authentication server to obtain access and refresh tokens */

    reqBuilder.str(std::string()); // Empty reqBuilder
    reqBuilder << "code=" << validationCode << "&"
               << "client_id=" << clientID << "&"
               << "client_secret=" << clientSecret << "&"
               << "redirect_uri=" << redirectURI << "&"
               << "grant_type=authorization_code";

    req = reqBuilder.str();
    response = post(tokenURL.c_str(), req.c_str());

    if (response.second != 200)
    {
        std::cerr << "Something went wrong! Expected response was 200 OK with token info " << std::endl;
        std::cerr << "Response code was: " << response.second << std::endl
                  << "and response was: " << response.first << std::endl;
        return -1;
    }

    /* example response:
     * Response was: {
     * "access_token": "ya29.Ci_-...",
     * "token_type": "Bearer",
     * "expires_in": 3600,
     * "refresh_token": "1/oTWYnvxQYep1B6anQIRutiFlPVmDFqIcz1aolnplKxI"
     * }
     * */

    // Parse JSON response from Google API server into JSON Object for handling
    Json::Value root;
    Json::Reader reader;
    if (reader.parse(response.first, root))
    {
        accessToken = root["access_token"].asString();
        refreshToken = root["refresh_token"].asString();
    }
    else
    {
        std::cout << "Error parsing response!" << std::endl;
        return -1;
    }
    saveToken(root);
    return 0;
}

/* Function for renewing expired token */
int GDConnect::renewToken()
{
    std::stringstream reqBuilder;
    reqBuilder << "refresh_token=" << refreshToken << "&"
               << "client_id=" << clientID << "&"
               << "client_secret=" << clientSecret << "&"
               << "grant_type=refresh_token" ;

    std::string req = reqBuilder.str();

#ifdef DEBUG
    std::cout << req << std::endl; // for debugging
#endif // DEBUG

    std::pair<std::string, int> response;
    response = post(tokenURL.c_str(), req.c_str());
    if (response.second != 200)
    {
        std::cerr << "Something went wrong!" << std::endl
                  << "Refresh request should return 200 OK and new access token" << std::endl;
        std::cerr << "Response code was: " << response.second << std::endl
                  << "and response was: " << response.first << std::endl;
        return -1;
    }

    Json::Value root;
    Json::Reader reader;
    if (reader.parse(response.first, root))
    {
        accessToken = root["access_token"].asString();
    }
    else
    {
        std::cout << "Error parsing response!" << std::endl;
        return -1;
    }
    root["refresh_token"] = refreshToken; // need to re-add refresh token since not included in rewnew response.
    saveToken(root);
    std::cout << "Token renewed." << std::endl;
    return 0;
}

/* Function for listing files in Google Drive */
int GDConnect::listFiles()
{
    std::pair<std::string, int> response = get("https://www.googleapis.com/drive/v2/files", "", true);
    if (response.second != 200)
    {
        std::cerr << "Something went wrong!" << std::endl
                  << "Request for list of files should return 200 OK and JSON object" << std::endl;
        std::cerr << "Response code was: " << response.second << std::endl
                  << "and response was: " << response.first << std::endl;
        return -1;
    }
    Json::Value root;
    Json::Reader reader;
    if (reader.parse(response.first, root))
    {
        const Json::Value& items = root["items"];
        for(unsigned int i=0; i < items.size(); i++)
        {
            std::cout << i << "\t" << items[i]["id"].asString() << std::endl
                      << "\t" <<  items[i]["title"].asString() << std::endl
                      << "\t" <<  items[i]["originalFilename"].asString() << std::endl;
        }
    }
    else
    {
        std::cerr << "Error parsing file list JSON object" << std::endl;
        return -1;
    }
    return 0;
}

/* Function for obtaining metadata of Google Drive file using its ID */
Json::Value GDConnect::getFileMetadataById(const char * id)
{
    const char * endpoint = "https://www.googleapis.com/drive/v3/files/";
    std::pair<std::string, int> response = get(endpoint, id, true);
    Json::Value obj;
    Json::Reader reader;
    std::string filename;
    if (reader.parse(response.first, obj))
    {
        return obj;
    }
    else return 0;
}

/* Function for downloading a file from Google Drive using its ID */
int GDConnect::getFileById(const char * id)
{
    CURL *curlHandle;
    CURLcode res;
    FILE *fp;
    struct curl_slist *slist=NULL;
    long result;
    Json::Value obj = getFileMetadataById(id);
    if (!obj)
    {
        std::cerr << "Error retrieving file id " << id << std::endl;
        return -1;
    }
    std::string filename = obj["name"].asString();
    // int64_t filesize = obj["size"].asInt64();
    std::string url = std::string("https://www.googleapis.com/drive/v3/files/")
                      + id + "?alt=media";
    curlHandle = curl_easy_init(); // start up cURL with handle
    if (curlHandle)
    {
        fp = fopen(filename.c_str(), "wb");
        curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
#ifdef DEBUG
        curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 1); // for debugging
        curl_easy_setopt(curlHandle, CURLOPT_HEADER, 1); // for debugging
#endif
        curl_easy_setopt(curlHandle, CURLOPT_HTTPGET, 1);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, fp);
        const char * authHeader = ("Authorization: Bearer " + accessToken).c_str();
        slist = curl_slist_append(slist, authHeader);
        curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, slist);
        res = curl_easy_perform(curlHandle);
        if(res != CURLE_OK)  // something went wrong
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            result = -1;
        }
        else
        {
            curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &result);
        }
    }
    else
    {
        result = -1;
    }
    curl_slist_free_all(slist);
    curl_easy_cleanup(curlHandle);
    return result;
    return 0;
}

/* Function for obtaining a Google Drive file's ID using an exact name search */
std::string GDConnect::getFileId(const char * filename)
{
    const char * url = "https://www.googleapis.com/drive/v3/files?";
    std::string searchString = std::string("name=\"") + filename + "\"";

    CURL *curl = curl_easy_init(); // use cURL for url-encoding
    char * output;
    if(curl)
    {
        output = curl_easy_escape(curl, searchString.c_str(), 0);

    }
    std::string msg = std::string("q=") + output;
    curl_free(output);
    curl_easy_cleanup(curl);

    std::pair<std::string, int> response = get(url, msg.c_str(), true);
    std::string id;
    Json::Value obj;
    Json::Reader reader;
    if (reader.parse(response.first, obj))
    {
        Json::Value files = obj["files"];
        if (files.size() == 0 )
        {
            std::cout << "Error getting file ID: no files found with name " << filename << std::endl;
        }
        if (files.size() > 1)
        {
            std::cout << "Error getting file ID: " << files.size() << " files match name " << filename << std::endl;
        }
        if (files.size() == 1)
        {
            id = files[0]["id"].asString();
        }
    }
    return id;
}

/* Function for initiating resumable upload of a local file
    On success, returns location in result.first:
    HTTP/1.1 200 OK
    Location: https://www.googleapis.com/upload/drive/v3/files?uploadType=resumable&upload_id=xa298sd_sdlkj2
    Content-Length: 0
    The Location header provides a URI for actual upload of file
*/
std::pair<std::string, int> GDConnect::initUpload(const char * filename, long fileSize)
{
    CURL *curlHandle;
    CURLcode res;
    struct curl_slist *slist=NULL;
    std::string header;
    std::string response;
    std::pair<std::string, int> result;
    const char * url = "https://www.googleapis.com/upload/drive/v3/files?uploadType=resumable";

    Json::Value root;
    root["name"] = std::string(filename);
    Json::FastWriter fastWriter;
    std::string body = fastWriter.write(root);

    curlHandle = curl_easy_init(); // start up cURL with handle
    if (curlHandle)
    {
        curl_easy_setopt(curlHandle, CURLOPT_URL, url);
#ifdef DEBUG
        curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 1); // for debugging
        curl_easy_setopt(curlHandle, CURLOPT_HEADER, 1); // for debugging
#endif
        curl_easy_setopt(curlHandle, CURLOPT_POST, 1);
        curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDSIZE, strlen(body.c_str()));
        curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curlHandle, CURLOPT_HEADERFUNCTION, callback);
        curl_easy_setopt(curlHandle, CURLOPT_HEADERDATA, &header);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, callback);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &response);

        std::stringstream sbuilder;
        sbuilder << "Authorization: Bearer " << accessToken;
        std::string authHeader = sbuilder.str();
        slist = curl_slist_append(slist, authHeader.c_str());
        sbuilder.str(std::string());

        std::string contentType("Content-Type: application/json; charset=UTF-8");
        slist = curl_slist_append(slist, contentType.c_str());

        std::string uploadContentType("X-Upload-Content-Type: application/octet-stream"); // assume binary file
        slist = curl_slist_append(slist, uploadContentType.c_str());

        sbuilder << "X-Upload-Content-Length: " << fileSize;
        std::string uploadContentLength = sbuilder.str();
        slist = curl_slist_append(slist, uploadContentLength.c_str());

        curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, slist);

        res = curl_easy_perform(curlHandle);
        if (res != CURLE_OK)  // something went wrong
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            result.first = "curl_easy_perform() failed";
            result.second = -1;
        }
        else
        {
            long response_code;
            curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &response_code);
            if (response_code == 200)
            {
                // if upload init request successful, Google returns URI for resumable upload in Location header
                std::size_t start = header.find("Location: ");
                std::size_t endline = header.find('\n', start);
                std::string uri = header.substr(start +10, endline - start - 11); // trim URI from header
                result.first = uri;
            }
            else
            {
                result.first = response;
            }
            result.second = response_code;
        }
    }
    else
    {
        result.first = "Unable to start curl";
        result.second = -1;
    }
    curl_slist_free_all(slist);
    curl_easy_cleanup(curlHandle);
    return result;
}

int GDConnect::putFile(const char * filename)
{
    std::cout << "Uploading file " << filename << std::endl;

    struct stat fileInfo;
    double speedUpload, totalTime;
    FILE *fd;

    fd = fopen(filename, "rb"); /* open file to upload */
    if(!fd)
    {
        return 1; /* can't continue */
    }

    /* to get the file size */
    if(fstat(fileno(fd), &fileInfo) != 0)
    {
        return 1; /* can't continue */
    }

    std::cout << "Initiating upload" << std::endl;

    std::pair<std::string, int> initResponse;
    initResponse = initUpload(filename, reinterpret_cast<long>(fileInfo.st_size));
    std::cout << "Upload URI is " << std::endl << initResponse.first << std::endl;

    if (initResponse.second != 200)
    {
        std::cerr << "Something went wrong!" << std::endl
                  << "Request for upload URI should return 200 OK and location" << std::endl;
        std::cerr << "Response code was: " << initResponse.second << std::endl
                  << "and response was: " << initResponse.first << std::endl;
        return -1;
    }

    CURL * curlHandle;
    CURLcode res;
    std::string url = initResponse.first;
    std::string uploadResponse;
    long uploadResponseCode;
    curlHandle = curl_easy_init();
    if(curlHandle)
    {
        /* upload to this place */
        curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
        /* tell it to "upload" to the URL */
        curl_easy_setopt(curlHandle, CURLOPT_UPLOAD, 1L);
        /* set where to read from (on Windows you need to use READFUNCTION too) */
        curl_easy_setopt(curlHandle, CURLOPT_READDATA, fd);
        /* and give the size of the upload (optional) */
        curl_easy_setopt(curlHandle, CURLOPT_INFILESIZE_LARGE, (curl_off_t)fileInfo.st_size);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, callback);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &uploadResponse);
        /* enable verbose for easier tracing */
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        res = curl_easy_perform(curlHandle);
        /* Check for errors */
        if(res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));
            return -1;
        }
        else
        {
            /* now extract transfer info */
            curl_easy_getinfo(curlHandle, CURLINFO_SPEED_UPLOAD, &speedUpload);
            curl_easy_getinfo(curlHandle, CURLINFO_TOTAL_TIME, &totalTime);
            curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &uploadResponseCode);
            fprintf(stderr, "Speed: %.3f bytes/sec during %.3f seconds\n",
                    speedUpload, totalTime);
        }
        /* always cleanup */
        curl_easy_cleanup(curlHandle);
    }
    return 0;
}
