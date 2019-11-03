#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include "utils.h"

#define MAXBUFLEN 50

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
  struct sockaddr_in addr;

  struct sockaddr_storage their_addr;
  socklen_t addr_len;
  char buf[MAXBUFLEN], their_ip[20];
  int myport, sockfd, numbytes;

  if(argc != 2){
    fprintf(stderr, "usage: supergopher vpn-port \n");
    exit(1);
  }

  myport = atoi(argv[1]);

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sockfd == -1){
    fprintf(stderr, "Failed to create socket\n");
    exit(1);
  }
  addr.sin_family = AF_INET;
  addr.sin_port =  htons(myport);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sockfd, (const struct sockaddr *) &addr, sizeof(addr)) == -1) {
    fprintf(stderr, "Failed to bind\n");
    exit(1);
  }

  char name[50];
  struct addrinfo *result;

  if (gethostname(name, sizeof(name))) {
    perror("Invalid");
  }
  if (getaddrinfo(name, NULL, NULL, &result)) {
    perror("Invalid");
  }

  char realip[20];
  strcpy(realip, inet_ntoa(((struct sockaddr_in *)result->ai_addr)->sin_addr));
  printf("Running supergopher at %s %d\n", realip, myport);


  printf("Waiting to recvfrom...\n");
  request_t packet;

  if((numbytes = recvfrom(sockfd, &packet, sizeof(request_t) , 0,
                          (struct sockaddr *)&their_addr, &addr_len)) == -1){
    perror("recvfrom");
    exit(1);
  }

  inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            their_ip, sizeof their_ip);

  printf("Got packet from %s!\n", their_ip);
  printf("Packet is %d bytes long\n", numbytes);
  printf("It contains sig:'%d', %s:%d\n", packet.sig, packet.server_ip, packet.server_port);

  return 0;
}
