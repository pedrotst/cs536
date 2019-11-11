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

#define MAXBUFLEN 1000

int main(int argc, char *argv[]) {
  struct sockaddr_in addr;
  socklen_t addrLen, addr_len;
  char vpn_ip[16], server_ip[16];
  int vpn_port, server_port;
  int sockfd, numbytes;
  char buf[MAXBUFLEN];

  if(argc < 5 || (argc - 1) % 2 != 0){
    fprintf(stderr, "usage: createoverlay router_1-IP router_1-port ... router_k-IP router_k-port dst-IP dst-port \n");
    exit(1);
  }
  buf[0] = '\0';
  sprintf(buf, "%u", (argc - 3) / 2);
  for(int i = 1; i < argc; i++){
    strcat(buf, "#");
    strcat(buf, argv[i]);
  }
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
  addr_len = sizeof *their_addr;
  printf("sending %s\n", buf);

  if ((numbytes = sendto(sockfd, buf, strlen(buf), 0,
                         their_addr, addr_len)) == -1) {
    perror("sendto");
    exit(1);
  }

  printf("Waiting for a server answer\n");
  answer_t ans;
  if((numbytes = recvfrom(sockfd, &ans, sizeof(answer_t) , 0,
                          (struct sockaddr *)&their_addr, &addr_len)) == -1){
    perror("recvfrom");
    exit(1);
  }

  printf("Overlay creation was a success!\nPlease use %s %d\n", argv[1], htons(ans.transit_port));
  return 0;
}
