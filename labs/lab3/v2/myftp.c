#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "utils.h"

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/*
$ myftp cli-ip dropwhen
  * cli-ip is the ip I will bind
  *
 */
int main(int argc, char *argv[]) {
  int sockfd, acksockfd;
  struct addrinfo;
  struct sockaddr_storage their_addr;
  struct sockaddr_in addr;
  struct sockaddr ack_addr;
  struct timeval starttime, endtime;
  int numbytes;
  /* char buf[MAXBUFLEN]; */
  packet_t pack;
  ack_t ack;

  socklen_t addr_len;
  char s[INET6_ADDRSTRLEN];
  socklen_t addrLen;
  int dropwhen, dup=0, packet_count = 0, packets_dropped = 0;
  int real_packets_count = 0;
  int total_bytes_recv = 0;
  int dup_bytes_recv = 0;
  int dup_bytes = 0;
  int fst_rec = 1;

  if(argc != 3){
    fprintf(stderr, "usage: cli-ip dropwhen\n");
    exit(1);
  }

  dropwhen = atoi(argv[2]);

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sockfd == -1){
    printf("socket() failed\n");
    exit(1);
  }

  addr.sin_family = AF_INET;
  addr.sin_port =  0;
  addr.sin_addr.s_addr = inet_addr(argv[1]); /* INADDR_ANY; */

  if (bind(sockfd, (const struct sockaddr *) &addr, sizeof(addr)) == -1) {
    printf("bind() failed\n");
    exit(1);
  }
  addrLen = sizeof addr;
  if (getsockname(sockfd, (struct sockaddr *)&addr, &addrLen) == -1) {
    printf("getsockname() failed\n");
    exit(1);
  }
  printf("receiver: Listening at %d\n", htons(addr.sin_port));
  fflush(stdout);

  addr_len = sizeof their_addr;


  acksockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(acksockfd == -1){
    printf("ack socket() failed\n");
    exit(1);
  }

  int expected_seq = 0;
  pack.seqno = 0;

  while(pack.seqno != 2){
    printf("\nreceiver: Ready to receive...\n");
    fflush(stdout);
    if ((numbytes = recvfrom(sockfd, &pack, sizeof(packet_t) , 0,
                             (struct sockaddr *)&their_addr, &addr_len)) == -1) {
      perror("recvfrom");
      exit(1);
    }


    if(fst_rec){
      // What time did receiving start? We'll need that for final report
      gettimeofday(&starttime, NULL);
      fst_rec = 0;
    }
    numbytes -= sizeof(int) + sizeof(struct timeval);

    printf("receiver: got packet from %s\n",
           inet_ntop(their_addr.ss_family,
                     get_in_addr((struct sockaddr *)&their_addr),
                     s, sizeof s));
    printf("receiver: this is packet #%d seq: %d size: %d\n", real_packets_count+1, pack.seqno, numbytes);

    // The server missed the last packet
    if(pack.seqno != expected_seq){
      printf("receiver: resending last packet was resent, we ack again\n");
      dup_bytes_recv += numbytes;
      packet_count--;
      /* continue; */
    }

    expected_seq = (expected_seq + 1) % 2;

    // Total bytes received does not account for header
    total_bytes_recv += numbytes - 1;

    ack.seqno = pack.seqno;
    ack.timestamp = pack.timestamp;

    packet_count++;
    real_packets_count = packet_count - packets_dropped;

    printf("receiver: dropwhen: %d, packet_count: %d, real_packets_sent: %d\n", dropwhen, packet_count, real_packets_count);
    if(dropwhen != -1 && packet_count % dropwhen == 0){
      // Workaround to fix corner case when the last package is dropped
      /* seqno = 0; */
      packets_dropped++;

      // dup is a flag to point that the next byte is duplicate
      /* dup = 1; */
      real_packets_count = packet_count - packets_dropped;
      printf("receiver: package droped!!\n");
      continue;
    }


    // We got everything alright and we acknowledge it
    if (sendto(acksockfd, &ack, sizeof(ack_t), 0,
                           (struct sockaddr*)&their_addr,
                           sizeof their_addr) == -1) {
      perror("receiver: sendto() failed");
      exit(1);
    }

    printf("receiver: Ack %d sent\n", ack.seqno);
    printf("receiver: Ack timestamp %lds%ldus sent\n", ack.timestamp.tv_sec, ack.timestamp.tv_usec);
    fflush(stdout);

  }
  // Cool, all went well. How much time did it take?
  gettimeofday(&endtime, NULL);
  double completion_time =
    (endtime.tv_sec + (endtime.tv_usec / 1000000.0)) - (starttime.tv_sec + (starttime.tv_usec / 1000000.0));

  printf("receiver: End of transmission received, tearing down communication\n");
  printf("\nreceiver: Total bytes received:\t %d\n", total_bytes_recv);
  printf("receiver: Duplicate Bytes:\t %d\n", dup_bytes_recv);
  printf("receiver: Completion Time:\t %.4f s\n", completion_time);
  printf("receiver: Speed:\t\t %.4f bps\n", total_bytes_recv / completion_time);

  close(sockfd);
  close(acksockfd);

  return 0;
}
