#include "BinanceOrderBook.h"

BinanceOrderBook::BinanceOrderBook(WebSocketManager* manager, const int& btype, const char* symbol, const int& depthlimit): fManager(manager), fCHandle(curl_easy_init()), fJSTok(json_tokener_new()), fCheckFunction(NULL), fAsksPrice(), fBidsPrice(), fSocketCache(), fOBMutex(), fOBCond(), fLastUpdateID(0), fType(btype), fDepthLimit(), fSymbol(strdup(symbol)), fId(-1), fHasValidUpdate(0), fNewDataReady(false), fLastBidSum(-1), fLastAskSum(-1)
{
  pthread_mutex_init(&fOBMutex,NULL);
  pthread_cond_init(&fOBCond,NULL);
  curl_easy_setopt(fCHandle,CURLOPT_NOSIGNAL,1);
  curl_easy_setopt(fCHandle, CURLOPT_WRITEFUNCTION, GetSnapshotCB);
  curl_easy_setopt(fCHandle, CURLOPT_WRITEDATA, this);
  char wsuri[1024];
  char symb[128];

  if(depthlimit<=0) fDepthLimit=0;

  else if(depthlimit==5) fDepthLimit=5;

  else if(depthlimit==10) fDepthLimit=10;

  else if(depthlimit==20) fDepthLimit=20;

  else if(depthlimit==50) fDepthLimit=50;

  else if(depthlimit==100) fDepthLimit=100;

  else if(depthlimit==500) fDepthLimit=500;

  else {
    fprintf(stderr,"%s: Error: depth limit value %i is invalid!\n",__func__,depthlimit);
    throw 0;
  }

  size_t len=strlen(fSymbol);

  for(int i=len-1; i>=0; --i) symb[i]=toupper(fSymbol[i]);
  symb[len]=0;

  switch(fType) {
    case binance_spot:
      sprintf(wsuri,"%s%s%s%i",BINANCE_SPOT_URI,symb,DEPTH_CONF,(fDepthLimit?fDepthLimit:1000));
      fCheckFunction=CheckUpdateIDSpot;
      break;

    case binance_usdm_future:
      sprintf(wsuri,"%s%s%s%i",BINANCE_USDM_FUTURE_URI,symb,DEPTH_CONF,(fDepthLimit?fDepthLimit:1000));
      fCheckFunction=CheckUpdateIDFuture;
      break;

    case binance_coinm_future:
      sprintf(wsuri,"%s%s%s%i",BINANCE_COINM_FUTURE_URI,symb,DEPTH_CONF,(fDepthLimit?fDepthLimit:1000));
      fCheckFunction=CheckUpdateIDFuture;
      break;

    default:
      fprintf(stderr,"%s: Error: Invalid binance type\n",__func__);
      throw 0;
  }
  curl_easy_setopt(fCHandle, CURLOPT_URL, wsuri); 
  printf("Curl URI is '%s'\n",wsuri);
}

void BinanceOrderBook::OnMessage(websocketpp::connection_hdl, client::message_ptr msg)
{
  //std::cout << msg->get_payload() << std::endl;
  //printf("%s\n",__func__);

  if(fHasValidUpdate < 0) return;

  if(!fHasValidUpdate) {

    //If does not have a valid update and the mutex is already locked
    if(pthread_mutex_trylock(&fOBMutex)) {
      //Mutex was locked 
      fSocketCache.push_back(msg->get_payload());
      return;
    }
    //The mutex is locked

  //Else if there is already an update, we enforce the lock
  } else pthread_mutex_lock(&fOBMutex);

  //If ReloadBook has not initialised the order book yet
  if(fAsksPrice.empty()) fSocketCache.push_back(msg->get_payload());

  //Otherwise if the order book has been initialised
  else {
    size_t i=0;
    size_t csize=fSocketCache.size();
    int8_t ret=0;
    size_t pos;
    uint64_t U, u;

    //If it is the first update since the order book was initialised
    if(csize) {
      printf("Reading from cache\n");

      for(i=0; i<csize; ++i) {
	const std::string& str=fSocketCache[i];
	pos=str.find("\"U\"");

	if(pos==std::string::npos || str.size()<pos+5) goto message_error;
	sscanf(str.c_str()+pos+4,"%" PRIu64,&U);

	pos=str.find("\"u\"",pos+5);

	if(pos==std::string::npos || str.size()<pos+5) goto message_error;
	sscanf(str.c_str()+pos+4,"%" PRIu64,&u);

	if(u>=fLastUpdateID) {
	  printf("First valid update has ID %" PRIu64 "\n",u);

	  if(U > fLastUpdateID) {
	    fprintf(stderr,"%s: Error: Missing update before snapshot!\n",__func__);
	    goto message_error;
	  }
	  fHasValidUpdate=1;
	  fLastUpdateID=U-1; //This is necessary for the spot price order book
	  ret=_OnMessage(str);

	  if(ret<=0) goto message_error; //Here we need to ensure that at least one update has been done

	  for(++i; i<csize; ++i) {
	    ret=_OnMessage(fSocketCache[i]);

	    if(ret<0) goto message_error;

	  }
	  break;

	} else printf("Skipping update ID %" PRIu64 "\n",u);
      }
      printf("Cache has been drained!\n");
      fSocketCache.clear();
    }

    if(fHasValidUpdate) {
      ret=_OnMessage(msg->get_payload());

      if(ret<0) goto message_error;

    } else {
      printf("Looking for valid update\n");
      const std::string& str=msg->get_payload();
      pos=str.find("\"U\"");

      if(pos==std::string::npos || str.size()<pos+5) goto message_error;
      sscanf(str.c_str()+pos+4,"%" PRIu64,&U);

      pos=str.find("\"u\"",pos+5);

      if(pos==std::string::npos || str.size()<pos+5) goto message_error;
      sscanf(str.c_str()+pos+4,"%" PRIu64,&u);

      if(u>=fLastUpdateID) {
	printf("First valid update has ID %" PRIu64 "\n",u);

	if(U > fLastUpdateID) {
	  fprintf(stderr,"%s: Error: Missing update before snapshot!\n",__func__);
	  goto message_error;
	}
	fHasValidUpdate=1;
	fLastUpdateID=U-1; //This is necessary for the spot price order book
	ret=_OnMessage(str);

	if(ret<=0) goto message_error; //Here we need to ensure that at least one update has been done

      } else printf("Skipping update ID %" PRIu64 "\n",u);
    }
  }
  pthread_mutex_unlock(&fOBMutex);
  return;

message_error:
  fprintf(stderr,"%s: Inconsistent data!\n",__func__);
  fHasValidUpdate=-1;
  fAsksPrice.clear();
  fBidsPrice.clear();
  fLastUpdateID=0;
  fNewDataReady=false;
  fLastBidSum=fLastAskSum=-1;
  pthread_cond_signal(&fOBCond);
  pthread_mutex_unlock(&fOBMutex);
}

int8_t BinanceOrderBook::_OnMessage(const std::string& msg)
{
  //fOBMutex must be locked before calling this function!
  
  //std::cout << msg << std::endl;
  json_object* jobj=json_tokener_parse_ex(fJSTok, msg.c_str(), msg.size());
  enum json_tokener_error jerr=json_tokener_get_error(fJSTok);
  int8_t ret=0;

  if(!jobj || jerr!=json_tokener_success) {

    if(jerr==json_tokener_continue) fprintf(stderr,"%s: Incomplete JSON reply\n",__func__);

    else fprintf(stderr,"%s: Error parsing JSON reply: %s\n",__func__,json_tokener_error_desc(jerr));
    
    if(jobj) json_object_put(jobj);
    json_tokener_reset(fJSTok);
    return 0;

  } else {
    json_object *val, *obj;
    double price, quantity;

    ret=fCheckFunction(*this, jobj, val);

    if(ret!=1) {
      json_object_put(jobj);
      json_tokener_reset(fJSTok);
      return ret;
    }

    if(json_object_object_get_ex(jobj, "u", &val)) {
      fLastUpdateID = json_object_get_int64(val);
      fNewDataReady=true;
      pthread_cond_signal(&fOBCond);
      //printf("Update ID is %" PRIu64 "\n",fLastUpdateID);

    } else {
      json_object_put(jobj);
      json_tokener_reset(fJSTok);
      return 0;
    }

    if(json_object_object_get_ex(jobj, "b", &val)) {

      if(json_object_get_type(val)!=json_type_array) {
	fprintf(stderr,"%s: Error: Returned JSON object is invalid!\n",__func__);
	json_object_put(jobj);
	json_tokener_reset(fJSTok);
	return -1;
      }

      const size_t alength=json_object_array_length(val);

      if(alength) {

	for(size_t i=0; i<alength; ++i) {
	  obj=json_object_array_get_idx(val,i);
	  price=json_object_get_double(json_object_array_get_idx(obj,0));
	  quantity=json_object_get_double(json_object_array_get_idx(obj,1));

	  if(quantity) {
	    //printf("Quantity %f set for bid price %f\n",quantity,price);
	    fBidsPrice[price]=quantity;
	  } else {
	    //printf("Erasing bid price %f\n",price);
	    fBidsPrice.erase(price);
	  }
	}
	ret+=1;
      }
    }

    if(json_object_object_get_ex(jobj, "a", &val)) {

      if(json_object_get_type(val)!=json_type_array) {
	fprintf(stderr,"%s: Error: Returned JSON object is invalid!\n",__func__);
	json_object_put(jobj);
	json_tokener_reset(fJSTok);
	return -1;
      }

      const size_t alength=json_object_array_length(val);

      if(alength) {

	for(size_t i=0; i<alength; ++i) {
	  obj=json_object_array_get_idx(val,i);
	  price=json_object_get_double(json_object_array_get_idx(obj,0));
	  quantity=json_object_get_double(json_object_array_get_idx(obj,1));

	  if(quantity) {
	    //printf("Quantity %f set for ask price %f\n",quantity,price);
	    fAsksPrice[price]=quantity;
	  } else {
	    //printf("Erasing ask price %f\n",price);
	    fAsksPrice.erase(price);
	  }
	}
	ret+=2;
      }
    }
  }
  json_object_put(jobj);
  json_tokener_reset(fJSTok);
  return ret;
}

bool BinanceOrderBook::GetBookAtSum(const double& bidsum, const double& asksum, const struct timespec& waittime, bookvec* bids, bookvec* asks)
{
  //Can return additional book entries if more entries were requested the
  //previous time and that has not been any update since
  struct timespec timeout;
  clock_gettime(CLOCK_REALTIME, &timeout);
  timespecsum(&timeout, &waittime, &timeout);
  pthread_mutex_lock(&fOBMutex);

  if(fHasValidUpdate < 0) {
    //printf("No valid update\n");

    if(ReloadBook()) {
      pthread_mutex_unlock(&fOBMutex);
      //printf("Reloadbook failed\n");
      return false;
    }
  }

  if(!fNewDataReady && bidsum<=fLastBidSum && asksum<=fLastAskSum) {
    //printf("Waiting...\n");
    
    if(pthread_cond_timedwait(&fOBCond, &fOBMutex, &timeout)==ETIMEDOUT) {
      pthread_mutex_unlock(&fOBMutex);
      //printf("Returning false\n");
      return false;
    }
  }
  pthread_mutex_unlock(&fOBMutex);

  if(bidsum>0) {
    double sum=0;
    bids->clear();

    for(bidmap::const_iterator it=fBidsPrice.begin(); it!=fBidsPrice.end(); ++it) {
      sum+=it->second;
      bids->push_back({it->first, it->second, sum});

      if(sum >= bidsum) break;
    }
  }

  if(asksum>0) {
    double sum=0;
    asks->clear();

    for(askmap::const_iterator it=fAsksPrice.begin(); it!=fAsksPrice.end(); ++it) {
      sum+=it->second;
      asks->push_back({it->first, it->second, sum});

      if(sum >= asksum) break;
    }
  }
  fNewDataReady=false;
  fLastBidSum=bidsum;
  fLastAskSum=asksum;
  pthread_mutex_unlock(&fOBMutex);
  return true;
}

void BinanceOrderBook::Init()
{
  fSocketCache.clear();
  fAsksPrice.clear();
  fBidsPrice.clear();
  fHasValidUpdate=0;
  fLastUpdateID=0;
  fNewDataReady=false;
  fLastBidSum=fLastAskSum=-1;
}

void BinanceOrderBook::Print(const size_t limit)
{
  pthread_mutex_lock(&fOBMutex);
  printf("\nBids for update %" PRIu64 ":\n",fLastUpdateID);
  size_t i=0;

  for(bidmap::const_iterator it=fBidsPrice.begin(); it!=fBidsPrice.end(); ++it) {
    printf("%22f:\t%22f\n",it->first,it->second);
    ++i;

    if(limit && i==limit) break;
  }
  printf("\nAsk for update %" PRIu64 ":\n",fLastUpdateID);
  i=0;

  for(askmap::const_iterator it=fAsksPrice.begin(); it!=fAsksPrice.end(); ++it) {
    printf("%22f:\t%22f\n",it->first,it->second);
    ++i;

    if(limit && i==limit) break;
  }
  pthread_mutex_unlock(&fOBMutex);
}

size_t BinanceOrderBook::GetSnapshotCB(char *ptr, size_t size, size_t nmemb, void *instance)
{
  const size_t nbytes=size*nmemb;
  printf("Curl returned '%.*s'\n",(int)nbytes,ptr);
  BinanceOrderBook& bob=*(BinanceOrderBook*)instance;

  json_object* jobj=json_tokener_parse_ex(bob.fJSTok, ptr, nbytes);
  enum json_tokener_error jerr=json_tokener_get_error(bob.fJSTok);
  printf("jobj=%p\n",jobj);

  if(!jobj && jerr!=json_tokener_continue) {
    fprintf(stderr,"%s: Error parsing JSON reply: %s\n",__func__,json_tokener_error_desc(jerr));
    
    if(jobj) json_object_put(jobj);
    json_tokener_reset(bob.fJSTok);
    return 0;

  } else if(jerr==json_tokener_success) {
    json_object *val, *obj;
    double price, quantity;

    if(json_object_object_get_ex(jobj, "lastUpdateId", &val)) {
      bob.fLastUpdateID=json_object_get_int64(val)+(bob.fType==binance_spot); //fLastUpdateID+1 is used for spot!!
      printf("Order book lastUpdateID is %" PRIu64 "\n",bob.fLastUpdateID);

    } else {
      fprintf(stderr,"%s: Error: Could not read lastUpdateID!\n",__func__);
      json_object_put(jobj);
      json_tokener_reset(bob.fJSTok);
      return 0;
    }

    if(json_object_object_get_ex(jobj, "bids", &val)) {

	if(json_object_get_type(val)!=json_type_array) {
	  fprintf(stderr,"%s: Error: Returned JSON object is invalid!\n",__func__);
	  json_object_put(jobj);
          json_tokener_reset(bob.fJSTok);
	  return 0;
	}

	const size_t alength=json_object_array_length(val);

	for(size_t i=0; i<alength; ++i) {
	  obj=json_object_array_get_idx(val,i);
	  price=json_object_get_double(json_object_array_get_idx(obj,0));
	  quantity=json_object_get_double(json_object_array_get_idx(obj,1));
	  bob.fBidsPrice.insert(std::pair<double, double>(price, quantity));
	  //printf("price=%f, quantity=%f\n",price,quantity);
	}

    } else {
      fprintf(stderr,"%s: Error: Could not read bids!\n",__func__);
      json_object_put(jobj);
      json_tokener_reset(bob.fJSTok);
      return 0;
    }

    if(json_object_object_get_ex(jobj, "asks", &val)) {

	if(json_object_get_type(val)!=json_type_array) {
	  fprintf(stderr,"%s: Error: Returned JSON object is invalid!\n",__func__);
	  json_object_put(jobj);
          json_tokener_reset(bob.fJSTok);
	  return 0;
	}

	const size_t alength=json_object_array_length(val);

	for(size_t i=0; i<alength; ++i) {
	  obj=json_object_array_get_idx(val,i);
	  price=json_object_get_double(json_object_array_get_idx(obj,0));
	  quantity=json_object_get_double(json_object_array_get_idx(obj,1));
	  bob.fAsksPrice.insert(std::pair<double, double>(price, quantity));
	  //printf("price=%f, quantity=%f\n",price,quantity);
	}

    } else {
      fprintf(stderr,"%s: Error: Could not read asks!\n",__func__);
      json_object_put(jobj);
      json_tokener_reset(bob.fJSTok);
      return 0;
    }
    bob.fNewDataReady=true;
    json_object_put(jobj);
    json_tokener_reset(bob.fJSTok);
  }
  return nbytes;
}

int BinanceOrderBook::ReloadBook()
{

  while(fSocketCache.empty()) usleep(1);

  pthread_mutex_lock(&fOBMutex);

  if(curl_easy_perform(fCHandle)) {
    fprintf(stderr,"curl_easy_perform: An error was returned!\n");
    pthread_mutex_unlock(&fOBMutex);
    return -1;
  }
  fHasValidUpdate=0;
  pthread_mutex_unlock(&fOBMutex);
  return 0;
}

void BinanceOrderBook::StartSocket()
{
  char wsuri[1024];

  switch(fType) {
    case binance_spot:

      /*if(fDepthLimit) sprintf(wsuri,"%s%s%s%i%s",BINANCE_SPOT_WS_URI,fSymbol,WS_DEPTH_CONF1,fDepthLimit,WS_DEPTH_CONF2);

      else*/ sprintf(wsuri,"%s%s%s",BINANCE_SPOT_WS_URI,fSymbol,WS_DEPTH_CONF1 WS_DEPTH_CONF2);
      break;

    case binance_usdm_future:

      /*if(fDepthLimit) sprintf(wsuri,"%s%s%s%i%s",BINANCE_USDM_FUTURE_WS_URI,fSymbol,WS_DEPTH_CONF1,fDepthLimit,WS_DEPTH_CONF2);

      else*/ sprintf(wsuri,"%s%s%s",BINANCE_USDM_FUTURE_WS_URI,fSymbol,WS_DEPTH_CONF1 WS_DEPTH_CONF2);
      break;

    case binance_coinm_future:

      /*if(fDepthLimit) sprintf(wsuri,"%s%s%s%i%s",BINANCE_COINM_FUTURE_WS_URI,fSymbol,WS_DEPTH_CONF1,fDepthLimit,WS_DEPTH_CONF2);

      else*/ sprintf(wsuri,"%s%s%s",BINANCE_COINM_FUTURE_WS_URI,fSymbol,WS_DEPTH_CONF1 WS_DEPTH_CONF2);
      break;

    default:
      fprintf(stderr,"%s: Error: Invalid binance type\n",__func__);
      throw 0;
  }

  printf("Socket URI is %s\n",wsuri);
  fId=fManager->Connect(wsuri, websocketpp::lib::bind(&BinanceOrderBook::OnMessage, this, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));
}
