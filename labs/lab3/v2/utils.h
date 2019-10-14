#ifndef __UTILS_H_
#define __UTILS_H_

#include <sys/time.h>

#define MAXBUFLEN 1500

typedef struct ack {
  int seqno;
  struct timeval timestamp;
} ack_t;

typedef struct packet {
  int seqno;
  struct timeval timestamp;
  char buf[1343];
} packet_t;

#endif // __UTILS_H_
