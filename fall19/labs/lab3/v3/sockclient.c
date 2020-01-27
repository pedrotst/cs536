#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>

#include <arpa/inet.h>
#include <stdbool.h>

#define MAXDATASIZE (100000) // max number of bytes we can get at once

// get sockaddr, IPV4 or IPV6:
void *get_in_addr(struct sockaddr *sa){
  if(sa->sa_family == AF_INET){
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int main(int argc, char *argv[]){
  int sockfd, numbytes;
  char buf[MAXDATASIZE];
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];
  char cmd[100];
  int select_ret = -1;
  int tries = 0;
  fd_set read_fds;

  struct timeval tv = {1, 0};

  if(argc < 3){
    fprintf(stderr, "usage: client hostname port message\n");
    exit(1);
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;


  do{
    if((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
      if((sockfd = socket(p->ai_family, p->ai_socktype,
                          p->ai_protocol)) == -1){
        perror("client: socket");
        continue;
      }

      if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
        close(sockfd);
        perror("client: connect");
        continue;
      }

      break;
    }

    if (p == NULL) {
      fprintf(stderr, "client: failed to connect \n");
      return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
              s, sizeof s);

    freeaddrinfo(servinfo); // all done with this structure

    // Build the command into a single string
    cmd[0] = '\0';
    strcat(cmd, argv[3]);

    for(int i = 4; i < argc; i++){
      strcat(cmd, " ");
      strcat(cmd, argv[i]);
    }

    // if(send(sockfd, cmd, strlen(cmd), 0) == -1)
    if(write(sockfd, cmd, strlen(cmd)) == -1)
      perror("send");
    if(tries == 0)
      printf("client:   sent '%s' to %s\n", cmd, s);
    else
      printf("client: resent '%s' to %s\n", cmd, s);
    fflush(stdout);

    tv.tv_sec = 2;
    tv.tv_usec = 0;;
    // Wait for an answer
    FD_ZERO(&read_fds);
    // Adds the socket file descriptor to the read_fds
    FD_SET(sockfd, &read_fds);
    select_ret = select(sockfd + 1, &read_fds, NULL, NULL, &tv);

    /* printf("client: selectret=%d\n", select_ret); */

    if(select_ret != 0)
      if((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1) {
        perror("recv");
        exit(1);
      }

    tries++;
  }while(select_ret == 0 && numbytes == 0 && tries < 3);

  if(tries == 3 && numbytes == 0){
    printf("client: Giving up to resend\n");
    fflush(stdout);
    close(sockfd);
    exit(1);
  }

  buf[numbytes] = '\0';

  /* printf("client: received '%s'\n", buf); */
  printf("%s", buf);

  close(sockfd);

  return 0;
}
