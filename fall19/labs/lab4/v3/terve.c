#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <setjmp.h>
#include <ncurses.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>

#define MAXMSGLEN 45

#define HNDSHK_REQUEST 5
#define HNDSHK_ACPT    6
#define HNDSHK_DCLN    7
#define MSG            8
#define TERM           9

void standby();
void greet();
void handshake();
void talk();
void receive_msg();

// This is the struct we use for comunication
// Since we want to send the least number of bytes
// We avoid padding with #pragma pack(1)
#pragma pack(1)
typedef struct message_s {
  uint8_t sig;
  unsigned int key;
  char msg[MAXMSGLEN];
} message_t;

// States of the communication
enum state { standby_st,
             greeting_st,
             handshake_st,
             talking_st
} state = standby_st;


// Curses variables
WINDOW *top, *top_txt;
WINDOW *bottom, *bot_txt;

// Global Variables
static sigjmp_buf sockio_alarm;
static sigjmp_buf resend_alarm;
struct sockaddr *their_addr;
unsigned int session_key = 0;
char their_ip[16], their_port[8];

int timeout_count = 0;
int myport;
int sockfd;

// get the IP address
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

void tear_down(){
  delwin(top_txt);
  delwin(top);
  delwin(bot_txt);
  delwin(bottom);
  endwin();
}

void refresh_top(){
  wrefresh(bot_txt);
  wrefresh(top_txt);
}

// Handler for SIGQUIT
void terve_quit(int sig){
  int numbytes;
  message_t msg;

  if(state == talking_st){
    msg.sig = TERM;
    msg.key = session_key;
    msg.msg[0] = '\0';


    if ((numbytes = sendto(sockfd, &msg, sizeof(msg.sig) + sizeof(msg.key), 0,
                           their_addr, sizeof(*their_addr))) == -1) {
      tear_down();
      perror("quit sendto");
      exit(1);
    }
  }

  tear_down();
  printf("Session Terminated Sucessfully\n");
  exit(0);
}

// Handler for SIGIO
void terve_msg_receive(){
  siglongjmp(sockio_alarm, 1);
}

// Handler for SIGALRM
void resend_handshake(){
  timeout_count++;
  fflush(stdout);
  siglongjmp(resend_alarm, timeout_count);
}


void talk(){
  message_t msg;
  int numbytes;
  // +1 for the \n and +1 for the \0
  char buf[MAXMSGLEN+2];

  state = talking_st;
  /* wprintw(top_txt, "Success!\nTalking to: %s:%s\n", their_ip, their_port); */
  /* refresh_top(); */

  while(1){
    wprintw(bot_txt, "\nyour msg: ");
    wrefresh(bot_txt);
    echo();
    wgetnstr(bot_txt, buf, MAXMSGLEN+1);

    strncpy(msg.msg, buf, MAXMSGLEN);
    msg.sig = MSG;
    msg.key = session_key;

    if ((numbytes = sendto(sockfd, &msg, sizeof(uint8_t)+sizeof(unsigned int)+strlen(buf), 0,
                           their_addr, sizeof(*their_addr))) == -1) {
      tear_down();
      perror("talk sendto");
      exit(1);
    }
    wprintw(top_txt, "your msg:\t%s\n", buf);
    /* refresh_top(); */
    wrefresh(top_txt);
  }
}

void standby(){
    state = standby_st;
    wprintw(top_txt, "Enter IP PORT to connect\n");
    refresh_top();
    wprintw(bot_txt, "\nIP PORT: ");
    wscanw(bot_txt, "%[^ ] %[^\n]", their_ip, their_port);
    wrefresh(bot_txt);
    greet();
}

void get_theiraddr(){
  struct addrinfo *servinfo;
  struct addrinfo hints;
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  if ((rv = getaddrinfo(their_ip, their_port, &hints, &servinfo)) != 0) {
    tear_down();
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  their_addr = servinfo->ai_addr;
}

void greet(){
  /* struct addrinfo *servinfo, *p; */
  /* struct addrinfo hints; */
  /* int rv; */
  struct itimerval itime;
  message_t packet;
  int numbytes;

  state = greeting_st;

  get_theiraddr();

  // Generate our random key
  srand(time(0));
  packet.sig = HNDSHK_REQUEST;
  session_key = rand();
  packet.key = session_key;

  // Setup message send timeout
  if (signal(SIGALRM, &resend_handshake) == SIG_ERR) {
    tear_down();
    perror("sender: unable to catch SIGALRM");
    exit(1);
  }

  itime.it_value.tv_sec = 5;
  itime.it_value.tv_usec = 0;
  itime.it_interval = itime.it_value;

  // Set send timeout waiting for a response
  if (setitimer(ITIMER_REAL, &itime, NULL) == -1) {
    tear_down();
    perror("error calling setitimer()");
    exit(1);
  }

  // Send the initial handshake
  if ((numbytes = sendto(sockfd, &packet, sizeof(message_t), 0,
                         their_addr, sizeof(*their_addr))) == -1) {
    tear_down();
    perror("greet sendto");
    exit(1);
  }
  /* their_addr = p->ai_addr; */

  noecho();
  wprintw(top_txt, "Request sent to %s:%s\n", their_ip, their_port);
  wprintw(bot_txt, "\nwaiting response...");
  refresh_top();

  state = handshake_st;

  sleep(30);
}

void handshake(unsigned int key){
  char ans = 'x';
  char buf[200];
  int numbytes;
  message_t packet;

  state = handshake_st;
  session_key = key;
  wprintw(top_txt, "Enter 'y' to accept or 'n' do decline\n");
  refresh_top();

  while(ans != 'y' && ans != 'n'){
    echo();
    wprintw(bot_txt, "\n[y/n]: ");
    wrefresh(bot_txt);
    wscanw(bot_txt, "%s", buf);
    ans = buf[0];

    if(ans == 'y'){
      packet.sig = HNDSHK_ACPT;
    }
    if(ans == 'n'){
      packet.sig = HNDSHK_DCLN;
    }
  }
  packet.key = key;

  if ((numbytes = sendto(sockfd, &packet, sizeof(message_t), 0,
                         their_addr, sizeof(*their_addr))) == -1) {
    tear_down();
    perror("handle request sendto");
    exit(1);
  }

  if(ans != 'y'){
    standby();
  }
  else{
    wprintw(top_txt, "Success!\nTalking to: %s:%s\n", their_ip, their_port);
    refresh_top();
    talk();
  }
}

// Assyncronous function that will be called each time we receive a message
// It encodes the state machine of the communication
void receive_msg(){
  int numbytes;
  message_t packet;
  struct sockaddr_storage theiraddr;
  char ip[INET6_ADDRSTRLEN];
  socklen_t their_addrlen;

  their_addrlen = sizeof(theiraddr);
  if((numbytes = recvfrom(sockfd, &packet, sizeof(message_t), 0,
                          (struct sockaddr *)&theiraddr, &their_addrlen)) == -1){
    tear_down();
    perror("recvfrom");
    exit(1);
  }

  inet_ntop(theiraddr.ss_family,
            get_in_addr((struct sockaddr *)&theiraddr),
            ip, sizeof ip);
  int port = ntohs(get_in_port((struct sockaddr *)&theiraddr));
  /* their_port = atoi */
  /* sprintf("% */

  // The State Machine of the communication
  if(packet.sig == HNDSHK_REQUEST){
    wprintw(top_txt, "Session Request from %s:%d\n", ip, port);
    refresh_top();
    // If we don't answer in a timely maner we end up here again
    if(state == standby_st || state == handshake_st){
      strcpy(their_ip, ip);
      sprintf(their_port, "%d", port);
      get_theiraddr();
      handshake(packet.key);
    }
  }
  else if(state == handshake_st && packet.sig == HNDSHK_ACPT && packet.key == session_key){
    if (setitimer(ITIMER_REAL, NULL, NULL) == -1) {
      tear_down();
      perror("sender: error calling setitimer()");
      exit(1);
    }

    wprintw(top_txt, "Success!\nTalking to: %s:%s\n", their_ip, their_port);
    refresh_top();
    talk();
  }
  else if(state == handshake_st && packet.sig == HNDSHK_DCLN && packet.key == session_key){
    wprintw(top_txt, "#failure: %s %d\n", ip, port);
    refresh_top();
    if (setitimer(ITIMER_REAL, NULL, NULL) == -1) {
      tear_down();
      perror("sender: error calling setitimer()");
      exit(1);
    }
    standby();
  }
  else if(state == talking_st && packet.sig == MSG && packet.key == session_key){
    char buf[MAXMSGLEN+1];

    numbytes -= sizeof(packet.sig) + sizeof(packet.key);
    for(int i=0; i < numbytes; i++)
      buf[i] = packet.msg[i];

    buf[numbytes] = '\0';
    wprintw(top_txt, "received msg:\t%s\n", buf);
    refresh_top();
  }
  else if(packet.sig == TERM && packet.key == session_key){
    tear_down();
    printf("Session Termination Received\n");
    exit(0);
  }
}

int main(int argc, char *argv[]) {
  struct sockaddr_in addr;
  int on = 1;
  pid_t pgrp;
  int maxx,maxy; // Screen dimensions

  if(argc != 2){
    tear_down();
    fprintf(stderr, "usage: terve port\n");
    exit(0);
  }

  myport = atoi(argv[1]);
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sockfd == -1){
    tear_down();
    fprintf(stderr, "Failed to create socket\n");
    exit(1);
  }
  addr.sin_family = AF_INET;
  addr.sin_port =  htons(myport);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sockfd, (const struct sockaddr *) &addr, sizeof(addr)) == -1) {
    tear_down();
    fprintf(stderr, "Failed to bind\n");
    exit(1);
  }

  // Set up windows
  initscr();
  cbreak();
  refresh();

  getmaxyx(stdscr,maxy,maxx);

  top = newwin(maxy-3,maxx,0,0);
  top_txt = derwin(top, maxy-5, maxx - 2, 1, 1);
  bottom= newwin(3,maxx,maxy-3,0);
  bot_txt = derwin(bottom, 1, maxx - 2, 1, 1);

  box(top,'|','-');
  box(bottom,'|','-');
  wrefresh(top);
  wrefresh(bottom);

  // This is to allow scrolling
  scrollok(top_txt, TRUE);
  scrollok(bot_txt, TRUE);
  wsetscrreg(top_txt,0,0);
  wsetscrreg(bot_txt,0,0);
  wrefresh(top_txt);
  wrefresh(bot_txt);

  wprintw(top_txt, "Listening at port %d\n", htons(addr.sin_port));
  refresh_top();

  // And to raise a SIGIO upon data being received
  signal(SIGIO, &terve_msg_receive);

  // Register the socket to be non blocking
  pgrp=getpid();
  if (ioctl(sockfd, SIOCSPGRP, &pgrp) < 0) {
    tear_down();
    perror("ioctl F_SETOWN");
    exit(1);
  }
  if (ioctl(sockfd, FIOASYNC, &on) < 0) {
    tear_down();
    perror("ioctl F_SETFL, FASYNC");
    exit(1);
  }

  // Registers sigquit
  signal(SIGQUIT, &terve_quit);

  int jmpret = sigsetjmp(resend_alarm, 1);
  if (jmpret > 2){
    /* tear_down(); */

    if (setitimer(ITIMER_REAL, NULL, NULL) == -1) {
      tear_down();
      perror("error calling setitimer()");
      exit(1);
    }
    timeout_count = 0;

    wprintw(top_txt, "Tried sending message too much, dropping request\n");
    refresh_top();
    standby();
    /* exit(1); */
  }
  else if (jmpret != 0){
    wprintw(top_txt, "Timeout\n");
    refresh_top();
    greet();
  }
  else if (sigsetjmp(sockio_alarm, 1) > 0){
    receive_msg();
  }
  else{
    standby();
  }

  if(state == talking_st){
    talk();
  }

  return 0;
}
