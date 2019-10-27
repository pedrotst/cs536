#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAXBUFLEN 51

int sockfd;

typedef struct packet_s {
  char sig;
  unsigned int key;
} packet_t;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void terve_msg_receive(){
  int numbytes;
  packet_t packet;
  struct sockaddr_storage their_addr;
  socklen_t their_addrlen;

  their_addrlen = sizeof their_addr;
  if((numbytes = recvfrom(sockfd, &packet, sizeof(packet_t), 0,
                          (struct sockaddr *)&their_addr, &their_addrlen)) == -1){
    perror("recvfrom");
    exit(1);
  }
  printf("Received %c:%d\n", packet.sig, packet.key);

}

void initiate_session(){
  struct addrinfo *servinfo, *p;
  struct addrinfo hints;
  packet_t packet;
  char their_ip[16], their_port[8];
  int numbytes, rv;

  printf("#ready: ");
  scanf("%[^ ] %[^\n]", their_ip, their_port);
  printf("sending to '1' to %s:%s\n", their_ip, their_port);


  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  if ((rv = getaddrinfo(their_ip, their_port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  p = servinfo;
  sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

  if (p == NULL) {
    fprintf(stderr, "failed to create socket\n");
    exit(1);
  }

  // Generate our random key
  srand(time(0));
  packet.sig = '1';
  packet.key = rand();

  printf("sending '%c:%d'\n", packet.sig, packet.key);
  if ((numbytes = sendto(sockfd, &packet, sizeof(packet_t), 0,
                         p->ai_addr, p->ai_addrlen)) == -1) {
    perror("talker: sendto");
    exit(1);
  }

  printf("could send just fine, terminating\n");

}

int main(int argc, char *argv[]) {
  struct sockaddr_in addr;
  int port;
  int on = 1;
  pid_t pgrp;

  if(argc != 2){
    fprintf(stderr, "usage: terve port\n");
    exit(0);
  }
  port = atoi(argv[1]);

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sockfd == -1){
    printf("Failed to create socket\n");
    exit(1);
  }
  addr.sin_family = AF_INET;
  addr.sin_port =  htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sockfd, (const struct sockaddr *) &addr, sizeof(addr)) == -1) {
    printf("Failed to bind\n");
    exit(1);
  }

  printf("Listening at port %d\n", htons(addr.sin_port));

  // Register the socket to be non blocking
  // And to raise a SIGIO upon data being received
  /* fcntl(sockfd, F_SETFL, O_NONBLOCK); */
  signal(SIGIO, &terve_msg_receive);

  pgrp=getpid();
  if (ioctl(sockfd, SIOCSPGRP, &pgrp) < 0) {
    perror("ioctl F_SETOWN");
    exit(1);
  }
  if (ioctl(sockfd, FIOASYNC, &on) < 0) {
    perror("ioctl F_SETFL, FASYNC");
    exit(1);
  }

  initiate_session();


  return 0;
}
