#ifndef _BINANCEOUSERDATASTREAM_
#define _BINANCEUSERDATASTREAMK_

#include <cstdio>
#include <cstring>
#include <cstdint>

#include <pthread.h>
#include <signal.h>

#include <string>
#include <queue>

extern "C" {
#include "timeutils.h"
}

#include "binance_base.h"

#include "BinanceEndpoint.h"
#include "WebSocketManager.h"

class BinanceUserDataStream
{
  public:
  BinanceUserDataStream(WebSocketManager* manager, const char* configfile, const bintype& btype);
  ~BinanceUserDataStream();

  void Init();

  int Launch();

  void OnMessage(websocketpp::connection_hdl, client::message_ptr msg)
  {
    std::cout << msg->get_payload() << std::endl;
    pthread_mutex_lock(&fQueueMutex);
    fQueue.push(new std::string(msg->get_payload()));
    pthread_cond_signal(&fQueueCond);
    pthread_mutex_unlock(&fQueueMutex);
  }

  inline std::string* GetMessageWait() //User owns the returned allocated memory!
  {
    std::string* ret;
    pthread_mutex_lock(&fQueueMutex);

    while(fQueue.empty()) pthread_cond_wait(&fQueueCond, &fQueueMutex);
    ret=fQueue.front();
    fQueue.pop();
    pthread_mutex_unlock(&fQueueMutex);
    return ret;
  }

  inline std::string* GetMessageNoWait() //User owns the returned allocated memory!
  {
    std::string* ret;
    pthread_mutex_lock(&fQueueMutex);

    if(fQueue.empty()) {
      pthread_mutex_unlock(&fQueueMutex);
      return NULL;
    }
    ret=fQueue.front();
    fQueue.pop();
    pthread_mutex_unlock(&fQueueMutex);
    return ret;
  }

  inline std::string* GetMessageTimedWait(const struct timespec& waittime) //User owns the returned allocated memory!
  {
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timespecsum(&timeout, &waittime, &timeout);

    if(pthread_mutex_timedlock(&fQueueMutex,&timeout)) return NULL;

    while(fQueue.empty()) if(pthread_cond_timedwait(&fQueueCond, &fQueueMutex, &timeout)==ETIMEDOUT) {
      pthread_mutex_unlock(&fQueueMutex);
      return NULL;
    }
    std::string* ret=fQueue.front();
    fQueue.pop();
    pthread_mutex_unlock(&fQueueMutex);
    return ret;
  }

  protected:
  static void* PingThread(void* instance);
  static void sig_handler(int signum){}
  int StartSocket();
  void StopSocket(){if(fId!=-1) fManager->Close(fId, websocketpp::close::status::normal, "");}

  BinanceEndpoint fEP;
  WebSocketManager* fManager;
  pthread_t fPingThread;
  pthread_mutex_t fQueueMutex;
  pthread_cond_t fQueueCond;
  std::queue<std::string*> fQueue;
  const bintype& fBType;
  const char* fUDSName;
  char* fListenKey;
  char* fWSURI;
  int fId;
  bool fKeepPinging;
  private:
};

#endif
