#ifndef _BINANCEBASE_
#define _BINANCEBASE_

#include <cstdint>
#include <sys/time.h>

//#define BIN_TEST_NET
#ifndef BIN_TEST_NET
#define BINANCE_SPOT_BASEURI "https://api.binance.com/api/v3/"
#define BINANCE_SPOT_ALT_BASEURI "https://api.binance.com/sapi/v1/"
#define BINANCE_USDM_FUTURE_BASEURI "https://fapi.binance.com/fapi/v1/"
#define BINANCE_COINM_FUTURE_BASEURI "https://dapi.binance.com/dapi/v1/"

#define BINANCE_SPOT_WS_BASEURI "wss://stream.binance.com:9443/ws/"
#define BINANCE_USDM_FUTURE_WS_BASEURI "wss://fstream.binance.com/ws/"
#define BINANCE_COINM_FUTURE_WS_BASEURI "wss://dstream.binance.com/ws/"

#else
#define BINANCE_SPOT_BASEURI "https://testnet.binance.vision/api/v3/"
#define BINANCE_SPOT_ALT_BASEURI "https://testnet.binance.vision/sapi/v1/"
#define BINANCE_USDM_FUTURE_BASEURI "https://testnet.binancefuture.com/fapi/v1/"
#define BINANCE_COINM_FUTURE_BASEURI "https://testnet.binancefuture.com/dapi/v1/"

#define BINANCE_SPOT_WS_BASEURI "wss:///testnet.binance.vision/ws/"
#define BINANCE_USDM_FUTURE_WS_BASEURI "wss://stream.binancefuture.com/ws/"
#define BINANCE_COINM_FUTURE_WS_BASEURI "wss://dstream.binancefuture.com/ws/"
#endif

inline static uint64_t getmstime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  return (uint64_t)(tv.tv_sec)*1000 + (uint64_t)(tv.tv_usec)/1000;
}

inline static const char* getctime(const uint64_t* timestamp) {
  time_t timep=(time_t)*timestamp/1000;
  return ctime(&timep);
}

struct bintype
{
  bintype(const uint8_t& id, const char* ep, const char* ws):id(id), ep(ep), ws(ws){}
  uint8_t id;
  std::string ep;
  std::string ws;
};

const std::string binempty="";

const bintype bin_spot={0,BINANCE_SPOT_BASEURI,BINANCE_SPOT_WS_BASEURI}; 
const bintype bin_spot_alt={1,BINANCE_SPOT_ALT_BASEURI,BINANCE_SPOT_WS_BASEURI}; 
const bintype bin_usdm_future={2,BINANCE_USDM_FUTURE_BASEURI,BINANCE_USDM_FUTURE_WS_BASEURI}; 
const bintype bin_coinm_future={3,BINANCE_COINM_FUTURE_BASEURI,BINANCE_COINM_FUTURE_WS_BASEURI}; 

inline static bool operator==(const bintype& lhs, const bintype& rhs){return (lhs.id==rhs.id);}

enum binepsign {binepsign_false=false, binepsign_true=true, binepsign_apikey};

enum bineptype {bieneptype_get, bieneptype_post, bieneptype_put, bieneptype_delete};

struct binep
{
  std::string cmd;
  bineptype type;
};

const binep binping={"ping?",bieneptype_get};
const binep bintime={"time?",bieneptype_get};

const binep binaccount={"account?",bieneptype_get};
const binep binposition={"positionRisk?",bieneptype_get};
const binep bincommissionrate={"commissionRate?",bieneptype_get};
const binep binexchangeinfo={"exchangeInfo",bieneptype_get};
const binep binpositionrisk={"positionRisk?",bieneptype_get};
const binep binopenorders={"openOrders?",bieneptype_get};
const binep binorder={"order?",bieneptype_post};
const binep binfuturestransfer={"futures/transfer",bieneptype_post};
#endif
