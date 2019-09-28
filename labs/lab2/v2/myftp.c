#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXBUFLEN 100

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
  int numbytes;
  struct sockaddr_storage their_addr;
  struct sockaddr ack_addr;
  char buf[MAXBUFLEN];
  socklen_t addr_len;
  char s[INET6_ADDRSTRLEN];
  char seq_no;

  struct sockaddr_in addr;
  socklen_t addrLen;
  int dropwhen;

  if(argc != 3){
    fprintf(stderr, "usage: cli-ip dropwhen\n");
    exit(1);
  }

  dropwhen = atoi(argv[1]);

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sockfd == -1){
    printf("socket() failed\n");
    exit(1);
  }

  addr.sin_family = AF_INET;
  addr.sin_port =  0; //htons(SERVERPORT);
  addr.sin_addr.s_addr =
    inet_addr(argv[1]);
  /* INADDR_ANY; */
  // inet_pton(AF_INET, argv[0], &addr.sin_addr);


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

  while(1){
    printf("receiver: Ready to receive...\n");

    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
                             (struct sockaddr *)&their_addr, &addr_len)) == -1) {
      perror("recvfrom");
      exit(1);
    }
    seq_no = buf[0];

    printf("listener: got packet from %s\n",
           inet_ntop(their_addr.ss_family,
                     get_in_addr((struct sockaddr *)&their_addr),
                     s, sizeof s));
    printf("listener: packet is %d bytes long\n", numbytes);
    buf[numbytes] = '\0';
    printf("listener: packet contains \"%s\"\n", buf);

    acksockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(acksockfd == -1){
      printf("ack socket() failed\n");
      exit(1);
    }


    if (sendto(sockfd, &seq_no, 1, 0,
                           (struct sockaddr*)&their_addr,
                           sizeof their_addr) == -1) {
      perror("sender: sendto() failed");
      exit(1);
    }

    /* printf("sender: sent %s, %d bytes to '%s:%s'\n", sendstr, numbytes, cli_ip, cli_port); */
    printf("listener: Ack sent\n");

  }

  close(sockfd);

  return 0;
}
