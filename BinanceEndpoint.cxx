#include "BinanceEndpoint.h"
#include "conv_utils.h"

BinanceEndpoint::BinanceEndpoint(const char* configfile): fCHandle(curl_easy_init()), fHeaders(NULL), fJSTok(json_tokener_new()), fJObj(NULL), fCode(NULL), fSigBuf(), fCodeLength(0), fNeedCleanup(false)
{
  int fid=open(configfile,0);

  if(fid<0) {
    perror(__func__);
    throw 0;
  }

  long flength=lseek(fid, 0,  SEEK_END);
  lseek(fid, 0,  SEEK_SET);

  char* fbuf=(char*)malloc(flength);

  if(read(fid, fbuf, flength)<0) {
    perror(__func__);
    throw 0;
  }
  close(fid);

  json_tokener_reset(fJSTok);
  json_object* jobj=json_tokener_parse_ex(fJSTok, fbuf, flength);
  free(fbuf);
  enum json_tokener_error jerr=json_tokener_get_error(fJSTok);

  if(!jobj || jerr!=json_tokener_success) {

    if(jerr==json_tokener_continue) fprintf(stderr,"%s: Incomplete JSON reply\n",__func__);

    else fprintf(stderr,"%s: Error parsing JSON reply: %s\n",__func__,json_tokener_error_desc(jerr));
    
    if(jobj) json_object_put(jobj);
    json_tokener_reset(fJSTok);
    throw 0;

  } else {
    json_object *val;

    if(json_object_object_get_ex(jobj, "apiKey", &val)) {
      const char* key=json_object_get_string(val);
      const size_t keylen=strlen(key);
      const size_t keystrlen=keylen+15;
      char* keystr=(char*)malloc(keystrlen);
      memcpy(keystr,"X-MBX-APIKEY: ",14);
      memcpy(keystr+14,key,keylen);
      keystr[keystrlen-1]=0;
      //printf("Key header is '%s'\n",keystr);
      fHeaders=curl_slist_append(NULL,keystr);
      free(keystr);

    } else {
      fprintf(stderr,"%s: Error: Could not read apiKey\n",__func__);
      json_object_put(jobj);
      json_tokener_reset(fJSTok);
      throw 0;
    }

    if(json_object_object_get_ex(jobj, "secretKey", &val)) {
      fCode=(unsigned char*)strdup(json_object_get_string(val));
      fCodeLength=strlen((char*)fCode);

    } else {
      fprintf(stderr,"%s: Error: Could not read secretKey\n",__func__);
      json_object_put(jobj);
      json_tokener_reset(fJSTok);
      throw 0;
    }
    json_object_put(jobj);
    json_tokener_reset(fJSTok);
  }

  curl_easy_setopt(fCHandle,CURLOPT_NOSIGNAL,1);
  curl_easy_setopt(fCHandle, CURLOPT_WRITEFUNCTION, CurlCB);
  curl_easy_setopt(fCHandle, CURLOPT_WRITEDATA, this);
  //curl_easy_setopt(fCHandle, CURLOPT_VERBOSE, 1L);
  //curl_easy_setopt(fCHandle, CURLOPT_DEBUGFUNCTION, debug_callback);
}

int BinanceEndpoint::debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr)
{
  printf("Data: %.*s\n",(int)size,data);
  return size;
}

size_t BinanceEndpoint::CurlCB(char *ptr, size_t size, size_t nmemb, void *instance)
{
  const size_t nbytes=size*nmemb;
  BinanceEndpoint& bep=*(BinanceEndpoint*)instance;

  //printf("%.*s\n",(int)nbytes,ptr);
  bep.fJObj=json_tokener_parse_ex(bep.fJSTok, ptr, nbytes);
  enum json_tokener_error jerr=json_tokener_get_error(bep.fJSTok);

  if(!bep.fJObj && jerr!=json_tokener_continue) {
    fprintf(stderr,"%s: Error parsing JSON reply: %s\n",__func__,json_tokener_error_desc(jerr));
    json_tokener_reset(bep.fJSTok);
    return 0;

  }
  return nbytes;
}

uint64_t BinanceEndpoint::GetServerTime(const bintype& btype)
{
  uint64_t ret=0;

  if(!Request(btype, bintime, binempty, false)) {
    json_object *val;

    if(json_object_object_get_ex(fJObj, "serverTime", &val)) ret=(uint64_t)json_object_get_int64(val);
  }
  return ret;
}

int BinanceEndpoint::PingServer(const bintype& btype)
{
  uint64_t mstime=getmstime();
  if(!Request(btype, binping, binempty, false)) {
    return getmstime()-mstime;
  }
  return -1;
}

int BinanceEndpoint::Request(const bintype& btype, const binep& ep, const std::string& args, const int& sign)
{
  if(fNeedCleanup) {
    json_object_put(fJObj);
    json_tokener_reset(fJSTok);
    fNeedCleanup=false;
  }
  char* url=(char*)malloc(btype.ep.size()+ep.cmd.size()+43+args.size()+2*EVP_MAX_MD_SIZE);
  memcpy(url, btype.ep.c_str(), btype.ep.size());
  size_t urllength=btype.ep.size();
  memcpy(url+urllength, ep.cmd.c_str(), ep.cmd.size());
  urllength+=ep.cmd.size();
  char* buf;
  int arglength;

  if(sign==binepsign_true) {
    uint64_t mstime=getmstime();
    int len;

    if(!args.empty()) {
      buf=(char*)malloc(args.size()+42+2*EVP_MAX_MD_SIZE);
      memcpy(buf,args.c_str(),args.size());
      len=args.size()+sprintf(buf+args.size(),"&timestamp=%" PRIu64 "&signature=",mstime);
      //len=args.size()+sprintf(buf+args.size(),"&signature=");

    } else {
      buf=(char*)malloc(42+2*EVP_MAX_MD_SIZE);
      len=sprintf(buf,"timestamp=%" PRIu64 "&signature=",mstime);
      //len=sprintf(buf,"&signature=");
    }
    printf("String to be signed (%i): '%.*s'\n",len,len-11,buf);
    unsigned int len2=EVP_MAX_MD_SIZE;
    mx_hmac_sha256(fCode,fCodeLength,buf,len-11,fSigBuf,&len2);

    for(unsigned int i=0; i<len2; ++i) {
      printf("%02x",fSigBuf[i]);
      buf[len+2*i]=uint4toasciihex(fSigBuf[i]>>4);
      buf[len+2*i+1]=uint4toasciihex(fSigBuf[i] & 0xF);
    }
    printf("\n");
    arglength=len+2*len2;
    printf("Full is '%.*s'\n",arglength,buf);
    curl_easy_setopt(fCHandle, CURLOPT_HTTPHEADER, fHeaders);

  } else {
    buf=(char*)malloc(args.size());

    if(args.empty()) arglength=0;
    else {
      arglength=args.size();
      memcpy(buf, args.c_str(), arglength);
    }

    if(sign==binepsign_apikey) curl_easy_setopt(fCHandle, CURLOPT_HTTPHEADER, fHeaders);

    else curl_easy_setopt(fCHandle, CURLOPT_HTTPHEADER, NULL);
  }

  switch(ep.type) {

    case bieneptype_get:

      if(arglength) {
	memcpy(url+urllength,buf,arglength);
	url[urllength+arglength]=0;

      } else url[urllength]=0;
      printf("URL is '%s'\n",url);
      curl_easy_setopt(fCHandle, CURLOPT_URL, url); 
      free(url);

      if(curl_easy_perform(fCHandle)) {
	fprintf(stderr,"curl_easy_perform: An error was returned!\n");
	free(buf);
	return -1;
      }
      break;

    case bieneptype_post:
      url[urllength]=0;

      if(arglength) {
	curl_easy_setopt(fCHandle, CURLOPT_POSTFIELDS, buf);
	curl_easy_setopt(fCHandle, CURLOPT_POSTFIELDSIZE, arglength);

      } else {
	curl_easy_setopt(fCHandle, CURLOPT_POST,1L);
	curl_easy_setopt(fCHandle, CURLOPT_POSTFIELDSIZE, 0);
      }
      printf("URL is '%s'\n",url);
      curl_easy_setopt(fCHandle, CURLOPT_URL, url); 
      free(url);

      if(curl_easy_perform(fCHandle)) {
	fprintf(stderr,"curl_easy_perform: An error was returned!\n");
	free(buf);
	curl_easy_setopt(fCHandle, CURLOPT_POST,0);
	return -1;
      }
      curl_easy_setopt(fCHandle, CURLOPT_POST,0);
      break;

    case bieneptype_put:

      if(arglength) {
	memcpy(url+urllength,buf,arglength);
	url[urllength+arglength]=0;

      } else url[urllength]=0;
      curl_easy_setopt(fCHandle, CURLOPT_UPLOAD, 1L);
      curl_easy_setopt(fCHandle, CURLOPT_INFILESIZE, 0);
      printf("URL is '%s'\n",url);
      curl_easy_setopt(fCHandle, CURLOPT_URL, url); 
      free(url);

      if(curl_easy_perform(fCHandle)) {
	fprintf(stderr,"curl_easy_perform: An error was returned!\n");
	free(buf);
        curl_easy_setopt(fCHandle, CURLOPT_UPLOAD, 0L);
	return -1;
      }
      curl_easy_setopt(fCHandle, CURLOPT_UPLOAD, 0L);

      break;

    case bieneptype_delete:

      if(arglength) {
	memcpy(url+urllength,buf,arglength);
	url[urllength+arglength]=0;

      } else url[urllength]=0;
      curl_easy_setopt(fCHandle, CURLOPT_CUSTOMREQUEST, "DELETE");
      printf("URL is '%s'\n",url);
      curl_easy_setopt(fCHandle, CURLOPT_URL, url); 
      free(url);

      if(curl_easy_perform(fCHandle)) {
	fprintf(stderr,"curl_easy_perform: An error was returned!\n");
	free(buf);
        curl_easy_setopt(fCHandle, CURLOPT_CUSTOMREQUEST, NULL);
	return -1;
      }
      curl_easy_setopt(fCHandle, CURLOPT_CUSTOMREQUEST, NULL);
  }
  free(buf);
  fNeedCleanup=true;
  return 0;
}
