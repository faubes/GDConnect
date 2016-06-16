
/*
 * GDConnect.hh
 *
 *  Created on: Jun 1, 2016
 *      Author: Joël Faubert
 */

#ifndef GDCONNECT_H
#define GDCONNECT_H
#include <string>
#include <utility>
#include <json/json.h>

class GDConnect {
private:
	std::string clientID;
	std::string clientSecret;
	std::string authURL;
	std::string tokenURL;
	std::string redirectURI;
	std::string authScope;
	std::string validationCode;
	std::string accessToken;
	std::string refreshToken;
	time_t timestamp;
	bool ok;

	static std::size_t callback(const char* in, std::size_t size, std::size_t num, std::string* out);
	static std::size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream);
    static long getFileSize(const char * filename);
    std::pair<std::string, int> post(const char * endpoint, const char * msg, bool authorized);
    std::pair<std::string, int> get(const char * endpoint, const char * msg, bool authorized);
    std::pair<std::string, int> initUpload(const char * filename, long fileSize);
    Json::Value getFileMetadataById(const char * id);

public:
	GDConnect();
	GDConnect(const char * configFile);
    virtual ~GDConnect();
    int init(const char * configFilename);
    bool valid() { return ok; }
	int getToken();
	int renewToken();
	int saveToken(Json::Value root);
	int listFiles();
	int getFileById(const char * filename);
	std::string getFileId(const char * filename);
	int putFile(const char * filename);

};

#endif // GDCONNECT_H
