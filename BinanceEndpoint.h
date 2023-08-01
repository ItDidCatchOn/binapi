#ifndef _BINANCEENDPOINT_
#define _BINANCEENDPOINT_

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cinttypes>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>


#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <curl/curl.h>
#include <json-c/json.h>

#include "binance_base.h"

inline unsigned char *mx_hmac_sha256(const unsigned char* code, int codelen,
    const void *data, int datalen,
    void *result, unsigned int *resultlen) {
  //printf("Code length is %i, data is '%.*s' of length %i\n",codelen,datalen,(char*)data,datalen);
  return HMAC(EVP_sha256(), code, codelen, (const unsigned char*)data, datalen, (unsigned char*)result, resultlen); //result must have a length of EVP_MAX_MD_SIZE
}

class BinanceEndpoint
{
  public:
    BinanceEndpoint(const char* configfile);
    ~BinanceEndpoint(){if(fNeedCleanup) {json_object_put(fJObj); json_tokener_reset(fJSTok);} if(fCode) free(fCode); json_tokener_free(fJSTok); if(fHeaders) curl_slist_free_all(fHeaders); curl_easy_cleanup(fCHandle);}

    inline json_object*& GetJObj(){return fJObj;}

    int Request(const bintype& btype, const binep& ep, const std::string& args=binempty, const int& sign=binepsign_false);

    uint64_t GetServerTime(const bintype& btype);
    int PingServer(const bintype& btype);

    static size_t CurlCB(char *ptr, size_t size, size_t nmemb, void *instance);
    static int debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr);

  protected:
    CURL* fCHandle;
    struct curl_slist *fHeaders;
    json_tokener* fJSTok;
    json_object* fJObj;
    unsigned char* fCode;
    unsigned char fSigBuf[EVP_MAX_MD_SIZE];
    int fCodeLength;
    bool fNeedCleanup;
  public:
};

#endif
