#ifndef _BINANCEORDERBOOK_
#define _BINANCEORDERBOOK_

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cinttypes>
#include <ctype.h>

#include <unistd.h>

#include <vector>
#include <map>
#include <string>
#include <algorithm>

#include <pthread.h>

#include <curl/curl.h>
#include <json-c/json.h>

extern "C" {
#include "timeutils.h"
}

#include "binance_base.h"

#include "WebSocketManager.h"

enum {binance_spot, binance_usdm_future, binance_coinm_future};

#define BINANCE_SPOT_URI BINANCE_SPOT_BASEURI "depth?symbol="
#define BINANCE_USDM_FUTURE_URI BINANCE_USDM_FUTURE_BASEURI "depth?symbol="
#define BINANCE_COINM_FUTURE_URI BINANCE_COINM_FUTURE_BASEURI "depth?symbol="

#define BINANCE_SPOT_WS_URI BINANCE_SPOT_WS_BASEURI
#define BINANCE_USDM_FUTURE_WS_URI BINANCE_USDM_FUTURE_WS_BASEURI
#define BINANCE_COINM_FUTURE_WS_URI BINANCE_COINM_FUTURE_WS_BASEURI

#define DEPTH_CONF "&limit="
#define WS_DEPTH_CONF1 "@depth"
#define WS_DEPTH_CONF2 "@100ms"
//#define WS_DEPTH_CONF2 "@0ms"

/*
struct depth_compare {
  bool operator()(const std::pair<double, double>& value,
      const double& key)
  {
    return (value.first < key);
  }
  bool operator()(const double& key,
      const std::pair<double, double>& value)
  {
    return (key < value.first);
  }
};
*/
template<typename U, typename V, typename W> struct triplet {
  triplet(): x(), y(), z(){}
  triplet(const U& x, const V& y, const W& z): x(x), y(y), z(z){}
  const triplet& operator=(const triplet& rhs){x=rhs.x; y=rhs.y; z=rhs.z; return *this;}
  U x;
  V y;
  W z;
};
typedef triplet<double, double, double> bookentry;
typedef std::map<double, double, std::less<double> > askmap;
typedef std::map<double, double, std::greater<double> > bidmap;
typedef std::vector<bookentry> bookvec;


class BinanceOrderBook
{
  public:
  BinanceOrderBook(WebSocketManager* manager, const int& btype, const char* symbol, const int& depthlimit=0);
  ~BinanceOrderBook(){StopSocket(); json_tokener_free(fJSTok); curl_easy_cleanup(fCHandle); pthread_cond_destroy(&fOBCond); pthread_mutex_destroy(&fOBMutex); free(fSymbol);}

  static inline bool depth_compare(const bookentry& lhs, const double& rhs){return (lhs.z<rhs);}

  bool GetBookAtSum(const double& bidsum, const double& asksum, const struct timespec& waittime={1,0}, bookvec* bids=NULL, bookvec* asks=NULL);

  static inline double GetAverageAskPriceAtOrderQuantity(const bookvec& asks, const double& orderquantity){double dbuf=0; bookvec::const_iterator it; for(it=asks.begin(); it!=asks.end(); ++it) {dbuf+=it->x*it->y; if(dbuf>=orderquantity) {dbuf=it->z-(dbuf-orderquantity)/it->x; return orderquantity/dbuf;}} return INFINITY;}

  static inline double GetAverageAskPriceAtQuantity(const bookvec& asks, const double& quantity){if(quantity>asks.back().z) return INFINITY; double ret=0; bookvec::const_iterator it; for(it=asks.begin(); it->z<=quantity; ++it) ret+=it->x*it->y; ret+=it->x*(quantity-it->z+it->y); return ret/quantity;}

  static inline double GetAverageBidPriceAtOrderQuantity(const bookvec& bids, const double& orderquantity){double dbuf=0; bookvec::const_iterator it; for(it=bids.begin(); it!=bids.end(); ++it) {dbuf+=it->x*it->y; if(dbuf>=orderquantity) {dbuf=it->z-(dbuf-orderquantity)/it->x; return orderquantity/dbuf;}} return 0;}

  static inline double GetAverageBidPriceAtQuantity(const bookvec& bids, const double& quantity){double ret=0; bookvec::const_iterator it; for(it=bids.begin(); it!=bids.end() && it->z<=quantity; ++it) ret+=it->x*it->y; if(it!=bids.end()) ret+=it->x*(quantity-it->z+it->y); return ret/quantity;}

  void Init();

  int Launch(){Init(); StartSocket(); return ReloadBook();}

  void OnMessage(websocketpp::connection_hdl, client::message_ptr msg);

  void Print(const size_t limit=0);

  protected:
  static size_t GetSnapshotCB(char *ptr, size_t size, size_t nmemb, void *instance);
  int ReloadBook();
  void StartSocket();
  void StopSocket(){if(fId!=-1) fManager->Close(fId, websocketpp::close::status::normal, "");}
  int8_t _OnMessage(const std::string& msg);
  inline static int CheckUpdateIDSpot(const BinanceOrderBook& bob, const json_object* jobj, json_object* val){
    if(json_object_object_get_ex(jobj, "U", &val)) {

      if((uint64_t)json_object_get_int64(val) != bob.fLastUpdateID+1) {
        fprintf(stderr,"%s: Error: previous update id %" PRIu64 " does not match expected value %" PRIu64 "!\n",__func__,(uint64_t)json_object_get_int64(val),bob.fLastUpdateID+1);
        return -1;
      }

    } else return 0;
    return 1;
  }
  inline static int CheckUpdateIDFuture(const BinanceOrderBook& bob, const json_object* jobj, json_object* val)
  {
    if(json_object_object_get_ex(jobj, "pu", &val)) {

      if((uint64_t)json_object_get_int64(val) > bob.fLastUpdateID) {
	fprintf(stderr,"%s: Error: previous update id %" PRIu64 " does not match expected value %" PRIu64 "!\n",__func__,(uint64_t)json_object_get_int64(val),bob.fLastUpdateID);
	return -1;
      }

    } else return 0;
    return 1;
  }

  WebSocketManager* fManager;
  CURL* fCHandle;
  json_tokener* fJSTok;
  int (*fCheckFunction)(const BinanceOrderBook& bob, const json_object* jobj, json_object* val);
  askmap fAsksPrice;
  bidmap fBidsPrice;
  std::vector<std::string> fSocketCache;
  pthread_mutex_t fOBMutex;
  pthread_cond_t fOBCond;
  uint64_t fLastUpdateID;
  int fType;
  int fDepthLimit;
  char* fSymbol;
  int fId;
  int fHasValidUpdate;
  bool fNewDataReady;
  double fLastBidSum;
  double fLastAskSum;
  private:
};

#endif
