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

char* allocate_sendstr(int size){
  char *s = calloc(sizeof(char), size);
  for(int i = 0; i < size; i++)
    s[i] = '3';
  /* s[size - 1] = '\0'; */
  return s;
}

/*
$ myftpd filesize blocksize timeout cli-ip cli-port
* filesize - total byte to be sent
* blocksize - max of 1471
* timeout - in ms
* cli-port cli-ip - obvious
*/

int main(int argc, char *argv[]) {
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;
  int rv;
  int numbytes;
  char *sendstr;
  char cli_ip[16];
  char cli_port[8];
  int filesize, blocksize;
  int num_sends, timeout;
  int seqno = 0;

  if (argc != 6) {
    fprintf(stderr,"usage: filesize blocksize timeout cli-ip cli-port\n");
    exit(1);
  }

  filesize = atoi(argv[1]);
  blocksize = atoi(argv[2]);

  if(blocksize > 1471){
    fprintf(stderr, "blocksize must be smaller than 1471");
    exit(1);
  }

  timeout = atoi(argv[3]);
  strcpy(cli_ip, argv[4]);
  strcpy(cli_port, argv[5]);
  num_sends = filesize / blocksize + (filesize % blocksize != 0);
  printf("sender: Starting connection with %s:%s\n", cli_ip, cli_port);
  fflush(stdout);


  sendstr = allocate_sendstr(blocksize);
  sendstr[0] = seqno;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  if ((rv = getaddrinfo(cli_ip, cli_port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "sender: failed getaddrinfo(): %s\n", gai_strerror(rv));
    return 1;
  }

  p = servinfo;
  sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

  if (p == NULL) {
    fprintf(stderr, "sender: socket() failed\n");
    return 2;
  }

  printf("sender: Sending file of size %d bytes in %d packets\n", filesize, num_sends);
  fflush(stdout);

  int addr_len = sizeof their_addr;
  for(int i = 0; i <= num_sends; i++){
    if ((numbytes = sendto(sockfd, sendstr, blocksize, 0,
                           p->ai_addr, p->ai_addrlen)) == -1) {
      perror("sender: sendto() failed");
      exit(1);
    }

    printf("\nsender: sending packet seq: %d...\n", seqno);
    printf("sender: sent %d/%d packets to '%s:%s'\n", i, num_sends, cli_ip, cli_port);
    printf("sender: waiting for ACK...'\n");
    char buf;
    if ((numbytes = recvfrom(sockfd, &buf, 1 , 0,
                             (struct sockaddr *)&their_addr, &addr_len)) == -1) {
      perror("recvfrom");
      exit(1);
    }

    printf("sender: received ACK:%c\n", buf + '0');

    seqno = (seqno+1)%2;
    sendstr[0] = seqno;
    /* sleep(1); */
  }

  freeaddrinfo(servinfo);

  // Closing sockets are always a good practice
  close(sockfd);

  // Don't forget to free the string!
  free(sendstr);

  return 0;
}
