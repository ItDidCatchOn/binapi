#include "BinanceUserDataStream.h"

BinanceUserDataStream::BinanceUserDataStream(WebSocketManager* manager, const char* configfile, const bintype& btype): fEP(configfile), fManager(manager), fPingThread(), fQueueMutex(), fQueueCond(), fQueue(), fBType(btype), fUDSName(NULL), fListenKey(NULL), fWSURI(NULL), fId(-1), fKeepPinging(false)
{
  pthread_mutex_init(&fQueueMutex,NULL);
  pthread_cond_init(&fQueueCond,NULL);

  if(fBType==bin_spot) fUDSName="userDataStream?";
  else fUDSName="listenKey?";
}

BinanceUserDataStream::~BinanceUserDataStream()
{
  if(fKeepPinging) {
    fKeepPinging=false;
    struct timespec jt={0,1000000};

    do pthread_kill(fPingThread,SIGALRM);

    while(pthread_timedjoin_np(fPingThread, NULL, &jt));
  }

  if(fListenKey) {

    if(fEP.Request(fBType, {fUDSName, bieneptype_delete}, fListenKey, binepsign_apikey)) {
      fprintf(stderr,"%s: Warning: Could not delete user data stream!\n",__func__);

    }
  }
  StopSocket();

  if(fListenKey) {
    free(fListenKey);
    free(fWSURI);
  }

  while(!fQueue.empty()) {
    free(fQueue.front());
    fQueue.pop();
  }

  pthread_mutex_destroy(&fQueueMutex);
  pthread_cond_destroy(&fQueueCond);
}

void BinanceUserDataStream::Init()
{
  if(fListenKey) {
    free(fListenKey);
    free(fWSURI);
    fListenKey=NULL;
  }
}

int BinanceUserDataStream::Launch()
{
  Init();
  int ret=fEP.Request(fBType, {fUDSName,bieneptype_post}, binempty, binepsign_apikey);

  if(ret) return ret;
  json_object *val;
  const char* key;
  size_t keylen;

  if(json_object_object_get_ex(fEP.GetJObj(), "listenKey", &val)) {
    key=json_object_get_string(val);
    keylen=strlen(key);
    fListenKey=(char*)malloc(keylen+11);
    memcpy(fListenKey,"listenKey=",10);
    memcpy(fListenKey+10,key,keylen);
    fListenKey[keylen+10]=0;
    fWSURI=(char*)malloc(fBType.ws.size()+keylen+1);
    memcpy(fWSURI,fBType.ws.c_str(),fBType.ws.size());
    memcpy(fWSURI+fBType.ws.size(),key,keylen);
    fWSURI[fBType.ws.size()+keylen]=0;

  } else return -1;
  fKeepPinging=true;
  pthread_create(&fPingThread,NULL,PingThread,this);
  return StartSocket();
}

void* BinanceUserDataStream::PingThread(void* instance)
{
  signal(SIGALRM, sig_handler);
  BinanceUserDataStream& buds=*(BinanceUserDataStream*)instance;
  struct timespec starttime;
  struct timespec waketime;
  struct timespec loopsleeptime={60*30, 0};
  struct timespec errloopsleeptime={60, 0};
  clock_gettime(CLOCK_REALTIME, &starttime);
  timespecsum(&starttime, &loopsleeptime, &waketime);
  clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &waketime, NULL);
  clock_gettime(CLOCK_REALTIME, &starttime);

  while(buds.fKeepPinging) {

    if(buds.fEP.Request(buds.fBType, {buds.fUDSName, bieneptype_put}, buds.fListenKey, binepsign_apikey)) {
      fprintf(stderr,"%s: Warning: Could not ping user data stream!\n",__func__);
      timespecsum(&starttime, &errloopsleeptime, &waketime);

    } else timespecsum(&starttime, &loopsleeptime, &waketime);
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &waketime, NULL);
    clock_gettime(CLOCK_REALTIME, &starttime);
  }
  return NULL;
}

int BinanceUserDataStream::StartSocket()
{
  printf("Socket URI is '%s'\n",fWSURI);
  fId=fManager->Connect(fWSURI, websocketpp::lib::bind(&BinanceUserDataStream::OnMessage, this, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));
  printf("Socket ID is %i\n",fId);
  return (fId<0);
}
