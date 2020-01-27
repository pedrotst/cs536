#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <setjmp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "utils.h"

#define RTT_weight (0.7)

#define RTTPRINT

typedef struct missed_acks_s {
  ack_t ack;
  struct missed_acks_s *next;
} missed_acks;

char* allocate_sendstr(int size){
  char *s = calloc(sizeof(char), size);
  for(int i = 0; i < size; i++){
    s[i] = 3;
  }
  return s;
}

static jmp_buf env_alarm;
int timeout_count = 0;
missed_acks *ack_arr;

void print_list(){
  missed_acks *l;
  l = ack_arr;
  printf("\n\n-------------- Printing Ack List ---------------\n\n");
  while(l != NULL){
    printf("addr:\t\t%p\n", l);
    printf("timestamp:\t%lds%ldus\n\n", l->ack.timestamp.tv_sec, l->ack.timestamp.tv_usec);
    l = l->next;
  }
  printf("-------------- ----------------- ---------------\n\n");
}

void add_to_list(ack_t ack){
  missed_acks *last_ack = ack_arr;
  missed_acks *next_ack = ack_arr == NULL ? NULL : ack_arr->next;

  printf("sender: adding seqno %d to missed ack arr\n", ack.seqno);
  printf("sender: adding timestamp %lds%ldus to missed ack arr\n", ack.timestamp.tv_sec, ack.timestamp.tv_usec);
  while(next_ack != NULL){
    last_ack = next_ack;
    next_ack = next_ack->next;
  }
  next_ack = malloc(sizeof(missed_acks));
  next_ack->ack = ack;
  next_ack->next = NULL;

  if(last_ack == NULL)
    ack_arr = next_ack;
  else
    last_ack->next = next_ack;
}

void timeout_handler(int sig){
  timeout_count++;
  printf("sender: Timeout, resending last packet\n");
  fflush(stdout);

  siglongjmp(env_alarm, timeout_count);
}

int main(int argc, char *argv[]) {
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;
  struct itimerval itime;
  struct timeval starttime, endtime;
  ack_t ack;
  packet_t pack;

  int rv;
  int numbytes;
  char *sendstr;
  char cli_ip[16];
  char cli_port[8];
  int filesize, blocksize;
  int num_sends;
  double timeout;
  int seqno = 0, getnewack = 0;
  double old_rtt_ms = 0;

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

  old_rtt_ms = (itime.it_value.tv_usec / 1000 + (itime.it_value.tv_sec * 1000.0));

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
    pack.seqno = seqno;
    /* buffer[0] = seqno; */

    // Adjust last block size according the remaining bytes
    if(i == num_sends - 1 && (filesize % blocksize != 0))
      blocksize = filesize % blocksize;

    // Slice the correct block size of the string to be sent
    /* strncpy(&buffer[1], &sendstr[blocksize*i], blocksize); */
    strncpy(pack.buf, &sendstr[blocksize*i], blocksize);
    pack.buf[blocksize] = '\0';

    // If alarm goes off we restart the communication from here
    if (sigsetjmp(env_alarm, 1) > 2){
      printf("sender: tried to contact the server too much, dropping request\n");
      fflush(stdout);
      return 0;
    }

    // All set, we can send the packet now
    printf("\nsender: sending %d bytes seq %d\n", blocksize, pack.seqno);
    // printf("\nsender: sending seq %d'%s\n", buffer[0], &buffer[1]);
    gettimeofday(&starttime, NULL);
    pack.timestamp = starttime;

    // Mount missed_ack. if recvfrom timesout then the handler will add this to the array
    ack_t expected_ack;
    expected_ack.seqno = pack.seqno;
    expected_ack.timestamp = starttime;
    add_to_list(expected_ack);

    /* if ((numbytes = sendto(sockfd, buffer, blocksize+1, 0, */
                           /* p->ai_addr, p->ai_addrlen)) == -1) { */
    if ((numbytes = sendto(sockfd, &pack, sizeof(struct timeval) + sizeof(int) + blocksize, 0,
                           p->ai_addr, p->ai_addrlen)) == -1) {
      perror("sender: sendto() failed");
      exit(1);
    }

    printf("sender: sent %d/%d packets to '%s:%s'\n", i+1, num_sends, cli_ip, cli_port);
    printf("sender: sent timestamp %lds%ldus\n", expected_ack.timestamp.tv_sec, expected_ack.timestamp.tv_usec);
    printf("sender: waiting for ACK...\n");
    getnewack = 0;
GETACK:
    if ((numbytes = recvfrom(sockfd, &ack, sizeof(ack_t), 0,
                             (struct sockaddr *)&their_addr, &addr_len)) == -1) {
      perror("recvfrom");
      exit(1);
    }

    if (setitimer(ITIMER_REAL, NULL, NULL) == -1) {
      perror("sender: error calling setitimer()");
      exit(1);
    }
    gettimeofday(&endtime, NULL);
    printf("sender: received ACK:%d\n", ack.seqno);

    // Check if this ack is the latest one, otherwise it's a missed one
    // In which case we iterate through missed_acks array to find when it was sent
    missed_acks *_ack_arr = ack_arr;
    missed_acks *last_ack = ack_arr;
    fprintf(stdout, "\n\nsender: got timestamp %lds%ldus\n", ack.timestamp.tv_sec, ack.timestamp.tv_usec);
    /* print_list(); */
    while(_ack_arr != NULL && (timercmp(&ack.timestamp, &_ack_arr->ack.timestamp, !=) == 1)){
      last_ack = _ack_arr;
      _ack_arr = _ack_arr->next;
      fprintf(stdout, ".");
    }
    // If the element was found then get the timestamp and free the node
    if(_ack_arr != NULL){
      starttime = _ack_arr->ack.timestamp;
      fflush(stdout);
      last_ack->next = _ack_arr->next;

      // if the found element is the head
      if(_ack_arr == ack_arr){
        ack_arr = _ack_arr->next;
      }
      free(_ack_arr);
    }
    else{
      goto RESET;
    }

    if(timercmp(&ack.timestamp, &starttime, !=) == 1)
      getnewack = 1;

    struct timeval new_rtt;
    timersub(&endtime, &starttime, &new_rtt);

    // What we want is newrtt = A * oldrtt + (1 - A) * newrtt
    // First we convert everything to ms
    double new_rtt_ms = new_rtt.tv_usec / 1000.0 + (new_rtt.tv_sec * 1000.0);


    // Ok cool, we have the updated rtt in ms, let's convert to ms and us
    double updated_rtt_ms = RTT_weight * old_rtt_ms + (1 - RTT_weight) * new_rtt_ms;

    #ifdef RTTPRINT
    printf("Old RTT:\t%.5fms\n", old_rtt_ms);
    printf("New RTT:\t%.5fms\n", new_rtt_ms);
    printf("Updated RTT:\t%.5fms\n", updated_rtt_ms);
    printf("Slack RTT:\t%.5fms\n", updated_rtt_ms * 1.2);
    fflush(stdout);
    #endif

    old_rtt_ms = updated_rtt_ms;
    double fullupdated = 1.2 * updated_rtt_ms / 1000.0;
    itime.it_value.tv_sec = fullupdated;
    itime.it_value.tv_usec = (fullupdated - itime.it_value.tv_sec) * 1000000;

    itime.it_interval = itime.it_value;
    // Communication went well, reset timeout
  RESET:
    if (setitimer(ITIMER_REAL, &itime, NULL) == -1) {
      perror("sender: error calling setitimer()");
      exit(1);
    }
    timeout_count = 0;

    if(getnewack)
      goto GETACK;

    seqno = (pack.seqno+1)%2;
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
  pack.seqno = 2;
  if ((numbytes = sendto(sockfd, &pack, sizeof(int), 0,
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
  shutdown(sockfd, SHUT_WR);
  close(sockfd);
  freeaddrinfo(servinfo);

  // Don't forget to free the strings!
  free(sendstr);
  free(buffer);

  return 0;
}
