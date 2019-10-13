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
#include <signal.h>
#include <sys/time.h>
#include <setjmp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define RTT_weight (0.7)

char* allocate_sendstr(int size){
  char *s = calloc(sizeof(char), size);
  for(int i = 0; i < size; i++){
    s[i] = 3;
  }
  return s;
}

static jmp_buf env_alarm;
int timeout_count = 0;

void timeout_handler(int sig){
  timeout_count++;
  printf("sender: Timeout, resending last packet\n");
  fflush(stdout);

  siglongjmp(env_alarm, timeout_count);
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
  struct itimerval itime;
  struct timeval starttime, endtime;
  int rv;
  int numbytes;
  char *sendstr;
  char cli_ip[16];
  char cli_port[8];
  int filesize, blocksize;
  int num_sends;
  double timeout;
  int seqno = 0;

  if (argc != 6) {
    fprintf(stderr,"usage: filesize blocksize timeout cli-ip cli-port\n");
    exit(1);
  }

  filesize = atoi(argv[1]);
  blocksize = atoi(argv[2]);
  timeout = atof(argv[3]);
  printf("sender: filesize %d\n", filesize);
  printf("sender: blocksize %d\n", blocksize);
  printf("sender: timeout %f\n", timeout);
  /* printf("sender: timeout %f\n", (int) (timeout - ((int) timeout / 1000))); */

  if(blocksize > 1471){
    fprintf(stderr, "sender: blocksize must be smaller than 1471");
    exit(1);
  }
  strcpy(cli_ip, argv[4]);
  strcpy(cli_port, argv[5]);
  num_sends = filesize / blocksize + (filesize % blocksize != 0);
  printf("sender: starting connection with %s:%s\n", cli_ip, cli_port);
  fflush(stdout);
  sendstr = allocate_sendstr(filesize);

  // Setup itimer
  if (signal(SIGALRM, timeout_handler) == SIG_ERR) {
    perror("sender: unable to catch SIGALRM");
    exit(1);
  }

  itime.it_value.tv_sec = timeout / 1000;
  itime.it_value.tv_usec = (timeout - itime.it_value.tv_sec * 1000) * 1000;
  itime.it_interval = itime.it_value;

  printf("tv_sec: %ld\n", itime.it_value.tv_sec);
  printf("tv_usec: %ld\n", itime.it_value.tv_usec);

  if (setitimer(ITIMER_REAL, &itime, NULL) == -1) {
    perror("sender: error calling setitimer()");
    exit(1);
  }

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

  // printf("sender: Sending the following file:\n %s\n", sendstr);
  printf("sender: Sending file of size %d bytes in %d packets\n", filesize, num_sends+1);
  fflush(stdout);

  int addr_len = sizeof their_addr;
  char *buffer;
  buffer = calloc(sizeof(char), blocksize+1);
  char buf;

  for(int i = 0; i < num_sends; i++){
    // Puts sequence number at the head of the buffer
    buffer[0] = seqno;

    // Adjust last block size according the remaining bytes
    if(i == num_sends - 1 && (filesize % blocksize != 0))
      blocksize = filesize % blocksize;

    // Slice the correct block size of the string to be sent
    strncpy(&buffer[1], &sendstr[blocksize*i], blocksize);

    // If alarm goes off we restart the communication from here
    if (sigsetjmp(env_alarm, 1) > 2){
      printf("sender: tried to contact the server too much, dropping request\n");
      fflush(stdout);
      return 0;
    }

    // All set, we can send the packet now
    printf("\nsender: sending %d bytes seq %d\n", blocksize, buffer[0]);
    // printf("\nsender: sending seq %d'%s\n", buffer[0], &buffer[1]);
    gettimeofday(&starttime, NULL);

    if ((numbytes = sendto(sockfd, buffer, blocksize+1, 0,
                           p->ai_addr, p->ai_addrlen)) == -1) {
      perror("sender: sendto() failed");
      exit(1);
    }
    printf("sender: sent %d/%d packets to '%s:%s'\n", i+1, num_sends, cli_ip, cli_port);
    printf("sender: waiting for ACK...\n");
    if ((numbytes = recvfrom(sockfd, &buf, 1 , 0,
                             (struct sockaddr *)&their_addr, &addr_len)) == -1) {
      perror("recvfrom");
      exit(1);
    }
    gettimeofday(&endtime, NULL);

    struct timeval rtt;
    timersub(&endtime, &starttime, &rtt);

    double sec_rem =
      RTT_weight * itime.it_value.tv_sec
      + (1 - RTT_weight) * rtt.tv_sec;

    double usec_rem =
      RTT_weight * itime.it_value.tv_usec
      + (1 - RTT_weight) * rtt.tv_usec
      + ((sec_rem - (int) sec_rem) * 1000000);

    printf("sec_rem: %f\n", sec_rem);
    printf("usec_rem: %f\n", usec_rem);

    itime.it_value.tv_sec =
      (int) sec_rem
      + (int) (usec_rem / 1000000);

    itime.it_value.tv_usec =
      usec_rem - ((int) (usec_rem / 1000000) * 1000000);

    /* printf("tv_sec: %f\n", RTT_weight * itime.it_value.tv_sec); */
    printf("tv_sec: %ld\n", itime.it_value.tv_sec);
    printf("tv_usec: %ld\n", itime.it_value.tv_usec);
    fflush(stdout);

    itime.it_interval = itime.it_value;
    // Communication went well, reset timeout
    if (setitimer(ITIMER_REAL, &itime, NULL) == -1) {
      perror("sender: error calling setitimer()");
      exit(1);
    }
    timeout_count = 0;

    printf("sender: received ACK:%c\n", buf + '0');

    seqno = (seqno+1)%2;
  }

  // Sweet, all packages dully sent.
  // Let's send the end of transmission message now.
  // We retry twice by the way
  if (sigsetjmp(env_alarm, 1) > 2){
    printf("sender: Tried to contact the server too much, dropping request\n");
    printf("sender: Transfer was successful anyways though\n");
    printf("sender: Tearing down the server\n");
    printf("sender: Good night\n");
    fflush(stdout);
    return 0;
  }

  printf("\nsender: Sending end of transmission\n");
  buffer[0] = 2;
  if ((numbytes = sendto(sockfd, buffer, 1, 0,
                         p->ai_addr, p->ai_addrlen)) == -1) {
    perror("sender: sendto() failed");
    exit(1);
  }

  if ((numbytes = recvfrom(sockfd, &buf, 1 , 0,
                           (struct sockaddr *)&their_addr, &addr_len)) == -1) {
    perror("recvfrom");
    exit(1);
  }

  // Communication went well, reset timeout
  if (setitimer(ITIMER_REAL, NULL, NULL) == -1) {
    perror("sender: error calling setitimer()");
    exit(1);
  }

  printf("sender: End of transmission ACK received\n");
  printf("sender: Transfer was successful\n");
  printf("sender: Tearing down the server\n");
  printf("sender: Good night\n");

  // Closing sockets are always a good practice
  close(sockfd);
  freeaddrinfo(servinfo);

  // Don't forget to free the strings!
  free(sendstr);
  free(buffer);

  return 0;
}
