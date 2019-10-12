#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXBUFSZM (1000000)
#define STRSZ (100)

/*
  usage
  $ myfetchfile filename srv-ip srv-port
 */
int main(int argc, char *argv[]) {
  char filename[STRSZ];
  char srv_ip[16];
  char srv_port[8];

  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv, numbytes;

  if(argc != 4){
    fprintf(stderr, "usage: myfetchfile filename srv-ip srv-port\n");
    exit(1);
  }

  strcpy(filename, argv[1]);
  strcpy(srv_ip, argv[2]);
  strcpy(srv_port, argv[3]);


  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if ((rv = getaddrinfo(srv_ip, srv_port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  p = servinfo;
  sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

  if(p == NULL) {
    fprintf(stderr, "receiver: failed to create socket\n");
    return 2;
  }


  if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
    close(sockfd);
    perror("receiver: connect\n");
  }

  if (p == NULL) {
    fprintf(stderr, "client: failed to connect \n");
    return 2;
  }


  /* if((numbytes = sendto(sockfd, filename, strlen(filename), 0, */
                        /* p->ai_addr, p->ai_addrlen)) == -1) { */
    /* perror("receiver: sendto"); */
    /* exit(1); */
  /* } */

  freeaddrinfo(servinfo);

  if((numbytes = write(sockfd, filename, strlen(filename))) == -1)
    perror("send");

  printf("receiver: sent %d bytes to '%s:%s'\n", numbytes, srv_ip, srv_port);

  close(sockfd);

  return 0;
}
