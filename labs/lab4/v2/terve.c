#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <setjmp.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>

#define MAXBUFLEN 51

int sockfd;

typedef struct handshake_s {
  char sig;
  unsigned int key;
} handshake_t;

enum state { standby,
             initiate_request,
             talking,
             closed };

enum state state = standby;

static jmp_buf sockio_alarm;
static jmp_buf resend_alarm;
unsigned int session_key = 0;

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// get port, IPv4 or IPv6:
in_port_t get_in_port(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return (((struct sockaddr_in*)sa)->sin_port);
    }

    return (((struct sockaddr_in6*)sa)->sin6_port);
}

void terve_msg_receive(){
  siglongjmp(sockio_alarm, 1);
}

void terve_resend_initiation_request(){
  static int timeout_count = 0;

  timeout_count++;
  printf("\nTimeout, resending communication request\n");
  fflush(stdout);

  siglongjmp(resend_alarm, timeout_count);
}

void talk(){
  printf("Talking was a success, terminating\n");
}

void receive_msg(){
  int numbytes;
  handshake_t packet;
  struct sockaddr_storage their_addr;
  socklen_t their_addrlen;
  char s[INET6_ADDRSTRLEN];
  char ans = 'x';

  their_addrlen = sizeof their_addr;
  if((numbytes = recvfrom(sockfd, &packet, sizeof(handshake_t), 0,
                          (struct sockaddr *)&their_addr, &their_addrlen)) == -1){
    perror("recvfrom");
    exit(1);
  }
  // Maybe I should be smarter about my state
  if(packet.sig != 5){
    return;
  }

  inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
  int port = ntohs(get_in_port((struct sockaddr *)&their_addr));
  printf("\nSession Request from %s %d\n", s, port);

  while(ans != 'y' && ans != 'n'){
    printf("ready: ");
    scanf("%c", &ans);
    // Flush the buffer
    /* fseek(stdin,0,SEEK_END); */
    getchar();

    if(ans == 'y'){
      packet.sig = 6;
      session_key = packet.key;
    }
    if(ans == 'n'){
      packet.sig = 7;
      session_key = packet.key;
    }
  }

  // Send answer
  printf("sending '%d:%d'\n", packet.sig, packet.key);
  if ((numbytes = sendto(sockfd, &packet, sizeof(handshake_t), 0,
                         (struct sockaddr *)&their_addr, their_addrlen)) == -1) {
    perror("talker: sendto");
    exit(1);
  }
  talk();
}



void initiate_session(char *their_ip, char *their_port, int port){
  struct sockaddr_in addr;
  struct addrinfo *servinfo, *p;
  struct addrinfo hints;
  struct itimerval itime;
  struct sockaddr_storage their_addr;
  socklen_t their_addrlen;
  handshake_t packet;
  int numbytes, rv, true;
  char s[INET6_ADDRSTRLEN];

  // Correctly close the old socket before opening it again
  true = 1;
  close(sockfd);
  setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&true,sizeof(int));

  // Now we reopen the socket with the information to where we are sending the data
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  if ((rv = getaddrinfo(their_ip, their_port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  p = servinfo;
  sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

  if (p == NULL) {
    fprintf(stderr, "failed to create socket\n");
    exit(1);
  }

  addr.sin_family = AF_INET;
  addr.sin_port =  htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sockfd, (const struct sockaddr *) &addr, sizeof(addr)) == -1) {
    printf("Failed to bind\n");
    exit(1);
  }

  // Generate our random key
  srand(time(0));
  // Sig to send will be "Let's Talk" - i.e. 5
  packet.sig = 5;
  session_key = rand();
  packet.key = session_key;

  // Setup message send timeout
  if (signal(SIGALRM, &terve_resend_initiation_request) == SIG_ERR) {
    perror("sender: unable to catch SIGALRM");
    exit(1);
  }

  if (sigsetjmp(resend_alarm, 1) > 2){
    fprintf(stderr, "tried sending message too much, dropping request\n");
    exit(1);
  }

  itime.it_value.tv_sec = 5;
  itime.it_value.tv_usec = 0;
  itime.it_interval = itime.it_value;

  // Set send timeout waiting for a response
  if (setitimer(ITIMER_REAL, &itime, NULL) == -1) {
    perror("sender: error calling setitimer()");
    exit(1);
  }

  // Send the initial handshake
  printf("sending '%d:%d'\n", packet.sig, packet.key);
  if ((numbytes = sendto(sockfd, &packet, sizeof(handshake_t), 0,
                         p->ai_addr, p->ai_addrlen)) == -1) {
    perror("talker: sendto");
    exit(1);
  }

  while(state != talking){
    // Wait for response
    if((numbytes = recvfrom(sockfd, &packet, sizeof(handshake_t), 0,
                            (struct sockaddr *)&their_addr, &their_addrlen)) == -1){
      perror("recvfrom");
      exit(1);
    }
    inet_ntop(their_addr.ss_family,
              get_in_addr((struct sockaddr *)&their_addr),
              s, sizeof s);

    if(packet.sig == 6 && packet.key == session_key){
      int port = ntohs(get_in_port((struct sockaddr *)&their_addr));
      printf("\nsuccess: %s %d\n", s, port);
      state = talking;
    }
  }

  if (setitimer(ITIMER_REAL, NULL, NULL) == -1) {
    perror("sender: error calling setitimer()");
    exit(1);
  }

  talk();
}

int main(int argc, char *argv[]) {
  struct sockaddr_in addr;
  int myport;
  int on = 1;
  pid_t pgrp;
  char their_ip[16], their_port[8];

  if(argc != 2){
    fprintf(stderr, "usage: terve port\n");
    exit(0);
  }
  myport = atoi(argv[1]);

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sockfd == -1){
    printf("Failed to create socket\n");
    exit(1);
  }
  addr.sin_family = AF_INET;
  addr.sin_port =  htons(myport);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sockfd, (const struct sockaddr *) &addr, sizeof(addr)) == -1) {
    printf("Failed to bind\n");
    exit(1);
  }

  printf("Listening at port %d\n", htons(addr.sin_port));

  // Register the socket to be non blocking
  // And to raise a SIGIO upon data being received
  signal(SIGIO, &terve_msg_receive);

  pgrp=getpid();
  if (ioctl(sockfd, SIOCSPGRP, &pgrp) < 0) {
    perror("ioctl F_SETOWN");
    exit(1);
  }
  if (ioctl(sockfd, FIOASYNC, &on) < 0) {
    perror("ioctl F_SETFL, FASYNC");
    exit(1);
  }


  if (sigsetjmp(sockio_alarm, 0) > 0){
    receive_msg();
  }
  else{
    printf("ready: ");
    scanf("%[^ ] %[^\n]", their_ip, their_port);
    initiate_session(their_ip, their_port, myport);
  }

  return 0;
}
