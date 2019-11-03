#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "utils.h"

#define MAXBUFLEN 50

int main(int argc, char *argv[]) {
  struct sockaddr_in addr;
  socklen_t addrLen, addr_len;
  char vpn_ip[16], server_ip[16];
  int vpn_port, server_port;
  int sockfd, numbytes;
  char buf[MAXBUFLEN];

  if(argc != 5){
    fprintf(stderr, "usage: minigopher vpn-IP vpn-port server-IP server-port \n");
    exit(1);
  }
  strcpy(vpn_ip, argv[1]);
  strcpy(server_ip, argv[3]);
  vpn_port = atoi(argv[2]);
  server_port = atoi(argv[4]);

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sockfd == -1){
    printf("Failed to create socket\n");
    exit(1);
  }

  struct addrinfo *servinfo;
  struct addrinfo hints;
  struct sockaddr *their_addr;
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  their_addr = servinfo->ai_addr;
  request_t req;

  /* packet.sig = 3; */
  strcpy(req.server_ip, argv[3]);
  req.server_port = server_port;

  if ((numbytes = sendto(sockfd, &req, sizeof(request_t), 0,
                         their_addr, sizeof(*their_addr))) == -1) {
    perror("greet sendto");
    exit(1);
  }

  printf("Waiting for a server answer\n");
  answer_t ans;
  if((numbytes = recvfrom(sockfd, &ans, sizeof(answer_t) , 0,
                          (struct sockaddr *)&their_addr, &addr_len)) == -1){
    perror("recvfrom");
    exit(1);
  }

  printf("Server answered with %u, %u\n", ans.sig, htons(ans.tunnel_port));
  printf("minigopher was a success!\n");


  /* addr.sin_family = AF_INET; */
  /* addr.sin_port =  0; //htons(SERVERPORT); */
  /* addr.sin_addr.s_addr = INADDR_ANY; */
  /* if (bind(sockfd, (const struct sockaddr *) &addr, sizeof(addr)) == -1) { */
  /*   printf("Failed to bind\n"); */
  /*   exit(1); */
  /* } */
  /* addrLen = sizeof addr; */
  /* if (getsockname(sockfd, (struct sockaddr *)&addr, &addrLen) == -1) { */
  /*   printf("getsockname() failed\n"); */
  /*   exit(1); */
  /* } */

  /* printf("Waiting to recvfrom...\n"); */
  /* if((numbytes = recvfrom(sockfd, buf, MAXBUFLEN , 0, */
                          /* (struct sockaddr *)&their_addr, &addr_len)) == -1){ */
    /* perror("recvfrom"); */
    /* exit(1); */
  /* } */

  return 0;
}
