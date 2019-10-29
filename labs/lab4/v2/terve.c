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

#define MAXMSGLEN 45
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

void initiate_session();
void receive_msg();
void standby();

int sockfd;

// We want to avoid struct padding here
#pragma pack(1)
typedef struct message_s {
  uint8_t sig;
  unsigned int key;
  char msg[MAXMSGLEN];
} message_t;

int message_size(message_t msg){
  return (sizeof(msg.sig) + sizeof(msg.key) + strlen(msg.msg));
}

enum state { st_standby,
             request_initiated,
             handling_request,
             talking };

enum state state = st_standby;

static sigjmp_buf sockio_alarm;
static sigjmp_buf resend_alarm;
unsigned int session_key = 0;
char their_ip[16], their_port[8];
int myport;
struct sockaddr *their_addr;
socklen_t their_addrlen;


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

void terve_quit(int sig){
  int numbytes;
  message_t msg;

  if(state == talking){
    msg.sig = 9;
    msg.key = session_key;
    msg.msg[0] = '\0';


    if ((numbytes = sendto(sockfd, &msg, message_size(msg), 0,
                           their_addr, their_addrlen)) == -1) {
      perror("handshake sendto");
      exit(1);
    }
    printf("\n#let's terminate chat session\n");
    fflush(stdout);
  }
  exit(0);
}

void terve_resend_initiation_request(){
  static int timeout_count = 0;

  timeout_count++;
  fflush(stdout);
  siglongjmp(resend_alarm, timeout_count);
}

void talk(){
  message_t msg;
  int numbytes;
  char c;
  // +1 for the \n and +1 for the \0
  char buf[MAXMSGLEN+2];

  state = talking;

  while(1){
    printf("your msg: ");
    fgets(buf, MAXMSGLEN+2, stdin);
    // If fgets didn't read the \n it means there is more than
    // MAXMSGLEN in the stdin, in which case we flush them
    // Otherwise we strip that silly \n off
    if(buf[strlen(buf) - 1] != '\n')
      while((c = getchar()) != '\n');
    else
      buf[strlen(buf) - 1] = '\0';

    // My version of strncpy without the \0
    for(int i = 0; i < strlen(buf) - 1; i++){
      msg.msg[i] = buf[i];
      printf("%c", buf[i]);
    }

    msg.sig = 8;
    msg.key = session_key;

    if ((numbytes = sendto(sockfd, &msg, message_size(msg), 0,
                           their_addr, their_addrlen)) == -1) {
      perror("handshake sendto");
      exit(1);
    }
  }
}


void handle_session_request(unsigned int key){
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
  if ((numbytes = sendto(sockfd, &packet, sizeof(message_t), 0,
                         their_addr, their_addrlen)) == -1) {
    perror("handle_request sendto");
    exit(1);
  }

  if(ans != 'y'){
    standby();
  }
  else
    talk();
}

void standby(){
    state = st_standby;
    printf("ready: ");
    scanf("%[^ ] %[^\n]", their_ip, their_port);
    initiate_session();
}

void receive_msg(){
  int numbytes;
  message_t packet;
  struct sockaddr_storage theiraddr;
  char ip[INET6_ADDRSTRLEN];

  their_addrlen = sizeof their_addr;
  if((numbytes = recvfrom(sockfd, &packet, sizeof(message_t), 0,
                          (struct sockaddr *)&theiraddr, &their_addrlen)) == -1){
    perror("recvfrom");
    exit(1);
  }

  their_addr = (struct sockaddr *)&theiraddr;

  inet_ntop(theiraddr.ss_family,
            get_in_addr((struct sockaddr *)&theiraddr),
            ip, sizeof ip);
  int port = ntohs(get_in_port((struct sockaddr *)&theiraddr));

  // This is our state machine
  if(packet.sig == 5){
    printf("\nSession Request from %s %d\n", ip, port);
    fflush(stdout);
    // If we don't answer in a timely maner we end up here again
    if(state == st_standby || state == handling_request){
      printf("Handle Session Request\n");
      fflush(stdout);
      session_key = packet.key;
      handle_session_request(packet.key);
    }
  }
  else if(packet.sig == 6 && packet.key == session_key){
    printf("#success: %s %d\n", ip, port);
    fflush(stdout);
    if (setitimer(ITIMER_REAL, NULL, NULL) == -1) {
      perror("sender: error calling setitimer()");
      exit(1);
    }
    talk();
  }
  else if(packet.sig == 7 && packet.key == session_key){
    printf("#failure: %s %d\n", ip, port);
    if (setitimer(ITIMER_REAL, NULL, NULL) == -1) {
      perror("sender: error calling setitimer()");
      exit(1);
    }
    standby();
  }
  else if(packet.sig == 8 && packet.key == session_key){
    numbytes -= sizeof(packet.sig) + sizeof(packet.key);
    packet.msg[numbytes] = '\0';
    printf("\nreceived msg: '%s'\n", packet.msg);
  }
  else if(packet.sig == 9 && packet.key == session_key){
    printf("\n#session termination received\n");
    exit(0);
  }
}

void initiate_session(){
  struct addrinfo *servinfo, *p;
  struct addrinfo hints;
  struct itimerval itime;
  message_t packet;
  int numbytes, rv;

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

  // Do kind of a busy wait, otherwise the process can terminate
  // without registering an answer
  while(getchar());
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

  // Registers sigquit
  signal(SIGQUIT, &terve_quit);

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
    standby();
  }

  if(state == talking){
    talk();
  }


  return 0;
}
