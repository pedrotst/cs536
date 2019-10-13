#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
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
  char buf[MAXBUFSZM];

  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv, numbytes;
  struct timeval start_time, end_time;

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

  freeaddrinfo(servinfo);

  // Send the name of the file to be fetched from the server
  if((numbytes = write(sockfd, filename, strlen(filename))) == -1)
    perror("send");

  printf("receiver: sent %d bytes to '%s:%s'\n", numbytes, srv_ip, srv_port);


  char c;
  // Receive message code
  if(read(sockfd, &c, 1) == -1)
    perror("read");

  if(c == '0'){
    printf("receiver: file %s does not exist\n", filename);
    close(sockfd);
    return 0;
  }
  else if(c == '1'){
    printf("receiver: file %s is empty\n", filename);
    close(sockfd);
    return 0;
  }
  else if(c != '2'){
    printf("receiver: received weird control number, terminating\n");
    close(sockfd);
    return 0;
  }

  gettimeofday(&start_time, NULL);

  printf("receiver: begining to receive file\n");

  FILE *fptr;
  char writefile[150];

  strcpy(writefile, "/tmp/");
  strcat(writefile, filename);

  // Overwrite old file
  fptr = fopen(writefile, "w");
  fclose(fptr);

  fprintf(stderr, "receiver: setting up file at %s\n", writefile);
  int totsize = 0;
  do{
    numbytes = recv(sockfd, buf, MAXBUFSZM, 0);
    totsize += numbytes;
    buf[numbytes] = '\0';
    /* fprintf(stderr, "receiver: received %d bytes\n", numbytes); */

    fptr = fopen(writefile, "a");
    fwrite(buf, sizeof(char), numbytes, fptr);
    fclose(fptr);
  }while(numbytes != 0);
  gettimeofday(&end_time, NULL);
  double time;

  time = (end_time.tv_sec * 1000 + end_time.tv_usec / 1000.0) - (start_time.tv_sec * 1000+ start_time.tv_usec / 1000.0);
  /* double completion_time = */
    /* (endtime.tv_sec + (endtime.tv_usec / 1000000.0)) - (starttime.tv_sec + (starttime.tv_usec / 1000000.0)); */

  printf("receiver: file successfuly written to %s\n", writefile);
  printf("receiver: completion time: %fms\n", time);
  printf("receiver: file size: %d bytes\n", totsize);
  printf("receiver: bps: %f\n", (totsize / (time / 1000)));
  /* printf("receiver: Mbps: %f\n", (totsize / (time / 1000)) / 1000000); */
  printf("receiver: Shutting Down\n");
  printf("receiver: Good Night!\n");

  close(sockfd);

  return 0;
}
