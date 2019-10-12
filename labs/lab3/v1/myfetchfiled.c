#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXBUFSZM (1000000)


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/*
  usage
  $ myfetchfiled blocksize srv-port
 */
int main(int argc, char *argv[]) {
  int sockfd;
  struct addrinfo;
  int numbytes;
  struct sockaddr_storage their_addr;
  socklen_t addr_len;
  char s[INET6_ADDRSTRLEN];
  struct sockaddr_in addr;
  socklen_t addrLen;
  int srv_port, blocksize;
  char filename[MAXBUFSZM];

  if(argc != 3){
    fprintf(stderr, "usage: myfetchfiled blocksize srv-port");
    exit(1);
  }

  blocksize = atoi(argv[1]);
  srv_port = atoi(argv[2]);

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sockfd == -1){
    fprintf(stderr, "Failed to create socket\n");
    exit(1);
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(srv_port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if(bind(sockfd, (const struct sockaddr *) &addr, sizeof(addr)) == -1){
    fprintf(stderr, "Failed to bind\n");
    exit(1);
  }

  addrLen = sizeof addr;
  if(getsockname(sockfd, (struct sockaddr *)&addr, &addrLen) == -1){
    fprintf(stderr, "getsockname() failed\n");
    exit(1);
  }

  addr_len = sizeof their_addr;
  if((numbytes = recvfrom(sockfd, filename, MAXBUFSZM-1, 0,
                          (struct sockaddr *)&their_addr, &addr_len)) == -1) {
    fprintf(stderr, "recvfrom() failed\n");
    exit(1);
  }

  printf("sender: got packet from %s\n",
         inet_ntop(their_addr.ss_family,
                   get_in_addr((struct sockaddr *)&their_addr),
                   s, sizeof s));
  printf("sender: packet is %d bytes long\n", numbytes);
  filename[numbytes] = '\0';
  printf("sender: packet contains '%s'\n", filename);

  close(sockfd);

  return 0;
}
