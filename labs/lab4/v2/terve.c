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

void initiate_session();
void receive_msg();

int sockfd;

// We want to avoid struct padding here
#pragma pack(1)
typedef struct message_s {
  uint8_t sig;
  unsigned int key;
  char msg[45];
} message_t;

int message_size(message_t msg){
  return (sizeof(msg.sig) + sizeof(msg.key) + strlen(msg.msg));
}

enum state { standby,
             request_initiated,
             handling_request,
             talking,
             closed };

enum state state = standby;

static sigjmp_buf sockio_alarm;
static sigjmp_buf resend_alarm;
unsigned int session_key = 0;
char their_ip[16], their_port[8];
int myport;

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
  /* printf("SIGIO!\n"); */
  /* fflush(stdout); */
  siglongjmp(sockio_alarm, 1);
}

void terve_resend_initiation_request(){
  static int timeout_count = 0;

  timeout_count++;
  fflush(stdout);

  siglongjmp(resend_alarm, timeout_count);
}

void talk(struct sockaddr *their_addr, socklen_t their_addrlen){
  message_t msg;
  int numbytes;
  state = talking;

  while(1){
    printf("your msg: ");
    /* scanf("%[^\n]", msg.msg); */
    /* fflush(stdin); */

    fgets(msg.msg,50,stdin);
    // Strip \n
    msg.msg[strlen(msg.msg) - 1] = '\0';
    msg.sig = 8;
    msg.key = session_key;
    printf("msg: %s, %d\n", msg.msg, message_size(msg));

    if ((numbytes = sendto(sockfd, &msg, message_size(msg), 0,
                           their_addr, their_addrlen)) == -1) {
      perror("handshake sendto");
      exit(1);
    }

  }
}


void handle_session_request(struct sockaddr *their_addr, socklen_t their_addrlen, unsigned int key){
  char ans = 'x';
  int numbytes;
  message_t packet;

  state = handling_request;

  while(ans != 'y' && ans != 'n'){
    printf("ready: ");
    scanf("%c", &ans);
    getchar();

    if(ans == 'y'){
      packet.sig = 6;
    }
    if(ans == 'n'){
      packet.sig = 7;
    }
  }
  packet.key = key;

  // Send answer
  printf("sending '%d:%d'\n", packet.sig, packet.key);
  if ((numbytes = sendto(sockfd, &packet, sizeof(message_t), 0,
                         their_addr, their_addrlen)) == -1) {
    perror("talker: sendto");
    exit(1);
  }
  talk(their_addr, their_addrlen);
}

void receive_msg(){
  int numbytes;
  message_t packet;
  struct sockaddr_storage their_addr;
  socklen_t their_addrlen;
  char s[INET6_ADDRSTRLEN];

  their_addrlen = sizeof their_addr;
  if((numbytes = recvfrom(sockfd, &packet, sizeof(message_t), 0,
                          (struct sockaddr *)&their_addr, &their_addrlen)) == -1){
    perror("recvfrom");
    exit(1);
  }

  inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
  int port = ntohs(get_in_port((struct sockaddr *)&their_addr));
  /* printf("\nMessage Received '%d:%d' from %s %d\n", packet.sig, packet.key, s, port); */
  /* fflush(stdout); */

  // This is our state machine
  if(packet.sig == 5){
    // FIXME: uncomment next line
    printf("\nSession Request from %s %d\n", s, port);
    fflush(stdout);
    // If we don't answer in a timely maner we end up here again
    if(state == standby || state == handling_request){
      printf("Handle Session Request\n");
      fflush(stdout);
      session_key = packet.key;
      handle_session_request((struct sockaddr *)&their_addr, their_addrlen, packet.key);
    }
  }
  else if(packet.sig == 6 && packet.key == session_key){
    printf("Handshake Completed\n");
    fflush(stdout);
    // Received answer we were expecting, turns off timer
    if (setitimer(ITIMER_REAL, NULL, NULL) == -1) {
      perror("sender: error calling setitimer()");
      exit(1);
    }
    talk((struct sockaddr *)&their_addr, their_addrlen);
  }
  else if(packet.sig == 8 && packet.key == session_key){
    printf("numbytes: %d\n", numbytes);
    numbytes -= sizeof(packet.sig) + sizeof(packet.key);
    printf("numbytes: %d\n", numbytes);
    packet.msg[numbytes] = '\0';
    printf("\nreceived msg: '%s'\n", packet.msg);
  }

  /* printf("Exit receivemsg\n"); */

}

void initiate_session(){
  struct sockaddr_in addr;
  struct addrinfo *servinfo, *p;
  struct addrinfo hints;
  struct itimerval itime;
  message_t packet;

  int numbytes, rv;
  char s[INET6_ADDRSTRLEN];

  // Now we reopen the socket with the information to where we are sending the data
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  if ((rv = getaddrinfo(their_ip, their_port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  p = servinfo;

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

  itime.it_value.tv_sec = 5;
  itime.it_value.tv_usec = 0;
  itime.it_interval = itime.it_value;

  // Set send timeout waiting for a response
  if (setitimer(ITIMER_REAL, &itime, NULL) == -1) {
    perror("error calling setitimer()");
    exit(1);
  }

  // Send the initial handshake
  /* printf("sending '%d:%d'\n", packet.sig, packet.key); */
  if ((numbytes = sendto(sockfd, &packet, sizeof(message_t), 0,
                          p->ai_addr, p->ai_addrlen)) == -1) {
    perror("handshake sendto");
    exit(1);
  }
  state = request_initiated;

  /* sleep(100); */
  /* printf("Exit initiate_session\n"); */
}

int main(int argc, char *argv[]) {
  struct sockaddr_in addr;
  int on = 1;
  pid_t pgrp;

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

  int jmpret = sigsetjmp(resend_alarm, 1);
  if (jmpret > 2){
    fprintf(stderr, "tried sending message too much, dropping request\n");
    exit(1);
  }
  else if (jmpret != 0){
    printf("\nTimeout, resending communication request\n");
    initiate_session();
  }
  else if (sigsetjmp(sockio_alarm, 1) > 0){
    receive_msg();
  }
  else{
    printf("ready: ");
    scanf("%[^ ] %[^\n]", their_ip, their_port);
    initiate_session();
  }

  while(getchar()){
    printf(".");
  };

  return 0;
}
