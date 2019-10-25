/*
** Run The Server, get the port and then invoke this program as
** $ gcc -o client datagramclient.c && ./client hostname port message
** hostname will probably be data.cs.purdue.edu
** port is fetched from the output of the server
** and message is whatever you feel like, have fun
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAXBUF 30

char myencoder(char x, unsigned int pubkey){
  static int pad = 0;
  return (x ^ (pubkey & (0x000000FF << (pad++ % 4))));
}

// Implace decoder
void encode(char *s, unsigned int pubkey){
  for(int i = 0; i < strlen(s); i++){
    s[i] = myencoder(s[i], pubkey);
  }
}

int main(int argc, char *argv[]) {
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  int numbytes;
  char cmd[1000];

  if (argc < 4) {
    fprintf(stderr,"usage: talker-hostname port private-key message\n");
    exit(1);
  }

  int privkey = atoi(argv[3]);

  // Datagram must send one single package, so let's concatenate
  // the command into a single string
  cmd[0] = '\0';
  strcat(cmd, argv[4]);

  for(int i = 5; i < argc; i++){
    strcat(cmd, " ");
    strcat(cmd, argv[i]);
  }
  encode(cmd, privkey);
  printf("Encoded message: %s\n", cmd);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  p = servinfo;
  sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

  if (p == NULL) {
    fprintf(stderr, "Failed to create socket\n");
    return 2;
  }

  /* int sendsize; */
  /* if(strlen(cmd) < MAXBUF) */
  /*   sendsize = strlen(cmd); */
  /* else */
  /*   sendsize = MAXBUF; */

  if ((numbytes = sendto(sockfd, cmd, strlen(cmd), 0,
                         p->ai_addr, p->ai_addrlen)) == -1) {
    perror("Sendto");
    exit(1);
  }

  freeaddrinfo(servinfo);

  printf("Sent %d bytes to '%s:%s'\n", numbytes, argv[1], argv[2]);
  close(sockfd);

  return 0;
}
