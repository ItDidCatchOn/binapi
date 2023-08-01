#ifndef _TIMEUTILS_
#define _TIMEUTILS_

#include <time.h>

static inline void timespecdiff(const struct timespec* newer, const struct timespec* older, struct timespec* diff){
  struct timespec ret;
  ret.tv_sec = newer->tv_sec - older->tv_sec;

  if(newer->tv_nsec > older->tv_nsec) {
    ret.tv_nsec = newer->tv_nsec - older->tv_nsec;

  } else {
    --ret.tv_sec;
    ret.tv_nsec = 1000000000 - older->tv_nsec + newer->tv_nsec;
  } 
  *diff = ret;
}

static inline void timespecsum(const struct timespec* first, const struct timespec* second, struct timespec* sum){
  struct timespec ret;
  ret.tv_sec = first->tv_sec + second->tv_sec;

  if(first->tv_nsec + second->tv_nsec <= 999999999) {
    ret.tv_nsec = first->tv_nsec + second->tv_nsec;

  } else {
    ++ret.tv_sec;
    ret.tv_nsec = first->tv_nsec + second->tv_nsec - 1000000000;
  }
  *sum = ret;
}

static inline bool operator<(const struct timespec lhs, const struct timespec rhs){if(lhs.tv_sec<rhs.tv_sec || (lhs.tv_sec==rhs.tv_sec && lhs.tv_nsec<rhs.tv_nsec)) return true; return false;}
static inline bool operator<=(const struct timespec lhs, const struct timespec rhs){if(lhs.tv_sec<rhs.tv_sec || (lhs.tv_sec==rhs.tv_sec && lhs.tv_nsec<=rhs.tv_nsec)) return true; return false;}
static inline bool operator>(const struct timespec lhs, const struct timespec rhs){if(lhs.tv_sec>rhs.tv_sec || (lhs.tv_sec==rhs.tv_sec && lhs.tv_nsec>rhs.tv_nsec)) return true; return false;}
static inline bool operator>=(const struct timespec lhs, const struct timespec rhs){if(lhs.tv_sec>rhs.tv_sec || (lhs.tv_sec==rhs.tv_sec && lhs.tv_nsec>=rhs.tv_nsec)) return true; return false;}

/*
static inline bool operator<(const struct tm lhs, const struct tm rhs){if(lhs.tm_hour<rhs.tm_hour || (lhs.tm_hour==rhs.tm_hour && (lhs.tm_min<rhs.tm_min || (lhs.tm_min==rhs.tm_min && lhs.tm_sec<rhs.tm_sec)))) return true; return false;}
static inline bool operator<=(const struct tm lhs, const struct tm rhs){if(lhs.tm_hour<rhs.tm_hour || (lhs.tm_hour==rhs.tm_hour && (lhs.tm_min<=rhs.tm_min || (lhs.tm_min==rhs.tm_min && lhs.tm_sec<=rhs.tm_sec)))) return true; return false;}
static inline bool operator>(const struct tm lhs, const struct tm rhs){if(lhs.tm_hour>rhs.tm_hour || (lhs.tm_hour==rhs.tm_hour && (lhs.tm_min>rhs.tm_min || (lhs.tm_min==rhs.tm_min && lhs.tm_sec>rhs.tm_sec)))) return true; return false;}
static inline bool operator>=(const struct tm lhs, const struct tm rhs){if(lhs.tm_hour>rhs.tm_hour || (lhs.tm_hour==rhs.tm_hour && (lhs.tm_min>=rhs.tm_min || (lhs.tm_min==rhs.tm_min && lhs.tm_sec>=rhs.tm_sec)))) return true; return false;}
*/

#endif
