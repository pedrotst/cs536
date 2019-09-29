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

#define MAXBUFLEN 1500

// FIXME: Don't forget to take this off!!
#define PORT 39140

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
  char buf[MAXBUFLEN];
  socklen_t addr_len;
  char s[INET6_ADDRSTRLEN];
  char seqno = 0;
  socklen_t addrLen;
  int dropwhen, dup=0, packet_count = 0, packets_dropped = 0;
  int real_packets_count = 0;
  int total_bytes_recv = 0;
  int dup_bytes_recv = 0;
  int dup_bytes = 0;

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
  addr.sin_port =  htons(PORT); //0; //htons(SERVERPORT);
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

  addr_len = sizeof their_addr;

  // What time did receiving start? We'll need that for final report
  gettimeofday(&starttime, NULL);

  while(seqno != 2){
    printf("\nreceiver: Ready to receive...\n");
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
                             (struct sockaddr *)&their_addr, &addr_len)) == -1) {
      perror("recvfrom");
      exit(1);
    }

    // Total bytes received does not account for header
    total_bytes_recv += numbytes - 1;

    // Countes duplicate bytes received
    if(dup){
      dup_bytes_recv += numbytes;
      dup = 0;
    }

    seqno = buf[0];

    printf("receiver: got packet from %s\n",
           inet_ntop(their_addr.ss_family,
                     get_in_addr((struct sockaddr *)&their_addr),
                     s, sizeof s));
    buf[numbytes] = '\0';
    printf("receiver: this is packet #%d seq: %d size: %d\n", real_packets_count+1, seqno, numbytes);
    // printf("receiver: this is packet #%d and contains %d\"%s\"\n", real_packets_count, seqno, &buf[1]);
    packet_count++;
    real_packets_count = packet_count - packets_dropped;

    printf("receiver: dropwhen: %d, packet_count: %d, real_packets_sent: %d\n", dropwhen, packet_count, real_packets_count);
    if(dropwhen != -1 && packet_count % dropwhen == 0){
      // Workaround to fix corner case when the last package is dropped
      seqno = 0;
      packets_dropped++;

      // dup is a flag to point that the next byte is duplicate
      dup = 1;
      real_packets_count = packet_count - packets_dropped;
      printf("receiver: package droped!!\n");
      continue;
    }

    // We got everything alright and we acknowledge it
    acksockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(acksockfd == -1){
      printf("ack socket() failed\n");
      exit(1);
    }
    if (sendto(sockfd, &seqno, 1, 0,
                           (struct sockaddr*)&their_addr,
                           sizeof their_addr) == -1) {
      perror("receiver: sendto() failed");
      exit(1);
    }

    printf("receiver: Ack %d sent\n", seqno);

  }
  // Cool, all went well. How much time did it take?
  gettimeofday(&endtime, NULL);
  float completion_time =
    (endtime.tv_sec - starttime.tv_sec) * 1000
    + (endtime.tv_usec - starttime.tv_usec) / 1000;

  printf("receiver: End of transmission received, tearing down communication\n");
  printf("\nreceiver: Total bytes received: %d\n", total_bytes_recv);
  printf("receiver: Duplicate Bytes: %d\n", dup_bytes_recv);
  printf("receiver: Completion Time: %d ms\n", (int) completion_time);
  printf("receiver: Speed: %.4f bps\n", ((total_bytes_recv / completion_time) * 1000));

  close(sockfd);

  return 0;
}
