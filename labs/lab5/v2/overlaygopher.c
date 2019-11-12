#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>


#include "utils.h"

#define MAXBUFLEN 5000
#define TRANSITSOCKIND 20

#define TABLEUPDATE 1

int sock_arr[TRANSITSOCKIND];
int setupsock;
int sock_i = 0;
int fdmax;
fd_set master;

// get sockaddr, IPv4 or IPv6:
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


int update_table(int sock_i, char *pre_ip, int pre_port,
                 char *post_ip, int post_port){
  FILE *fp, *_fp;
  char _pre_ip[16], _post_ip[16];
  int _sock_i, _pre_port, _post_port;

  fp = fopen("forward_table.txt", "r");
  _fp = fopen("new_table.txt", "w");
  if(!fp || !_fp){
    fprintf(stderr, "forward table could not be created\n");
    exit(1);
  }

  _pre_ip[0] = '\0';
  _post_ip[0] = '\0';

#ifdef TABLEUPDATE
  printf("\n ============ Table Update ============ \n");
  printf("%d %s %d %s %d\n", sock_i, pre_ip, pre_port, post_ip, post_port);
  printf(" ====================================== \n");
#endif


  int added = 0;
  while(fscanf(fp, "%d %[^ ] %d %[^ ] %d\n", &_sock_i, _pre_ip, &_pre_port,
               _post_ip, &_post_port) == 5){

    if(strcmp(_pre_ip, pre_ip) == 0){
      added++;
      fprintf(_fp, "%d %s %d %s %d\n", sock_i, pre_ip, pre_port, post_ip,
              post_port);
    }
    else{
      fprintf(_fp, "%d %s %d %s %d\n", _sock_i, _pre_ip, _pre_port, _post_ip,
              _post_port);
    }
    _pre_ip[0] = '\0';
  }

  if(!added)
      fprintf(_fp, "%d %s %d %s %d\n", sock_i, pre_ip, pre_port, post_ip,
              post_port);

  fclose(fp);
  fclose(_fp);
  rename("new_table.txt", "forward_table.txt");

  return 0;
}

void create_table(){
  FILE *fp;

  fp = fopen("forward_table.txt", "w");
  if(fp == NULL){
    fprintf(stderr, "Failed to create forward table");
    exit(1);
  }

  fclose(fp);
}

// if src = foo#bar#x & start = 0, then dst = foo and return 3
int copy_pound(char *dst, char *src, int start){
  int j = start;
  int i = 0;

  while(src[j] != '\0' && src[j] != '#'){
    dst[i] = src[j];
    i++; j++;
  }
  dst[i] = '\0';

  return j;
}

int setup_tunnel(struct sockaddr_storage their_addr, char* recbuf){
  char their_ip[16];
  struct sockaddr_in naddr;
  socklen_t addrLen = sizeof naddr;
  answer_t ans;
  int numbytes;
  char my_ip[16], my_port[10];
  /* char post_ip[16], post_port[10]; */
  char num_overlays[2];
  char buf[1000];
  int i;
  struct in_addr pre_ip, post_ip;
  unsigned short pre_port, post_port;
  uint8_t ov;

  buf[0] = '\0';
  i = 0;

  num_overlays[0] = recbuf[0];
  num_overlays[1] = '\0';
  memcpy(&pre_ip, &recbuf[2], 4);
  memcpy(&pre_port, &recbuf[7], 2);
  ov = atoi(num_overlays - 1);
  buf[0] = ov + '0';

  memcpy(&post_ip, &recbuf[10], 4);
  memcpy(&post_port, &recbuf[15], 2);

  strcat(buf, &recbuf[16]);

  if(sock_i > TRANSITSOCKIND){
    return 0;
  }

  their_ip[0] = '\0';

  inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            their_ip, sizeof their_ip);

  int their_port = ntohs(get_in_port((struct sockaddr *)&their_addr));

  #ifdef TABLEUPDATE
  printf("Overlay request received from %s:%d!\n", their_ip, their_port);
  printf("It contains %s\n", recbuf);
  printf("num_overlays: %s\n", num_overlays);
  printf("pre_ip: %s\n", inet_ntoa(pre_ip));
  printf("pre_port: %d\n", pre_port);
  printf("post_ip: %s\n", inet_ntoa(post_ip));
  printf("post_port: %d\n", post_port);
  #endif


  // Open a new socket for the tunneling
  // at position sock_i
  sock_arr[sock_i] = socket(AF_INET, SOCK_DGRAM, 0);
  if(sock_arr[sock_i] == -1){
    fprintf(stderr, "Failed to create socket\n");
  }

  sock_arr[sock_i+1] = socket(AF_INET, SOCK_DGRAM, 0);
  if(sock_arr[sock_i+1] == -1){
    fprintf(stderr, "Failed to create socket\n");
  }

  // Bind the client
  naddr.sin_family = AF_INET;
  naddr.sin_port =  0;
  naddr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sock_arr[sock_i], (const struct sockaddr *) &naddr, sizeof(naddr)) == -1) {
    fprintf(stderr, "Failed to bind tunnel\n");
    exit(1);
  }
  if (getsockname(sock_arr[sock_i], (struct sockaddr *)&naddr, &addrLen) == -1) {
    printf("tunnel getsockname() failed\n");
    exit(1);
  }
  int tport1 = htons(naddr.sin_port);
  printf("Tunnel created for trans1:%d\n", tport1);
  ans.sig = 3;
  ans.transit_port = naddr.sin_port;

  // Bind the server
  naddr.sin_family = AF_INET;
  naddr.sin_port =  0;
  naddr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sock_arr[sock_i+1], (const struct sockaddr *) &naddr, sizeof(naddr)) == -1) {
    fprintf(stderr, "Failed to bind tunnel\n");
    exit(1);
  }
  if (getsockname(sock_arr[sock_i+1], (struct sockaddr *)&naddr, &addrLen) == -1) {
    printf("tunnel getsockname() failed\n");
    exit(1);
  }
  fdmax = sock_arr[sock_i+1];

  int tport2 = htons(naddr.sin_port);
  printf("Tunnel created for trans2:%d\n", tport2);


  if ((numbytes = sendto(setupsock, &ans, sizeof(answer_t), 0,
                         (struct sockaddr *)&their_addr, sizeof(their_addr))) == -1) {
    perror("create tunnel sendto");
  }

  update_table(sock_i, their_ip, tport1, inet_ntoa(post_ip), post_port);
  sock_i += 2;
  return 1;
}

int forward(uint8_t *buf, struct sockaddr_storage their_addr, int bufsize){
  struct sockaddr_in sendto_addr;
  FILE *fp, *_fp;
  char _cli_ip[16], _server_ip[16], their_ip[16];
  int _sock_i, _tport1, _tport2, _cli_port, _server_port;
  int numbytes;
  socklen_t sendtolen = sizeof(sendto_addr);

  inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            their_ip, sizeof their_ip);

  int their_port = ntohs(get_in_port((struct sockaddr *)&their_addr));

  fp = fopen("forward_table.txt", "r");
  if(fp == NULL){
    fprintf(stderr, "Something went wrong with forward table\n");
    exit(1);
  }

  sendto_addr.sin_family = AF_INET;
  while(fscanf(fp, "%d %d %d %[^ ] %d %[^ ] %d\n", &_sock_i, &_tport1, &_tport2,
               _cli_ip, &_cli_port, _server_ip, &_server_port) == 7){
    if(strcmp(their_ip, _cli_ip) == 0){
      printf("Got a packet from %s:%d\n",_cli_ip, their_port);
      printf("Sending to %s:%d\n", _server_ip, _server_port);
      //forward to _server_ip:_server_port
      sendto_addr.sin_port = ntohs(_server_port);
      inet_pton(AF_INET, _server_ip, &sendto_addr.sin_addr);
      if ((numbytes = sendto(sock_arr[_sock_i + 1], buf, bufsize, 0,
                             (struct sockaddr *) &sendto_addr, sendtolen)) == -1) {
        perror("sendto forward");
      }

      fclose(fp);
      if(_cli_port == -1)
        /* update_table(_sock_i, _tport1, _tport2, _cli_ip, their_port, _server_ip, _server_port); */
      return 1;
    }
    else if(strcmp(their_ip, _server_ip) == 0){
      // forward to _client_ip:_client_port
      printf("Got a packet from %s:%d\n", _server_ip, their_port);
      printf("Sending to %s:%d\n", _cli_ip, _cli_port);
      sendto_addr.sin_port = ntohs(_cli_port);
      inet_pton(AF_INET, _cli_ip, &sendto_addr.sin_addr);
      if ((numbytes = sendto(sock_arr[_sock_i], buf, bufsize, 0,
                             (struct sockaddr *) &sendto_addr, sendtolen)) == -1) {
        perror("sendto forward");
      }
      fclose(fp);
      return 1;
    }
  }
  // Getting here means we could not find the upcomming conection so the packet was dropped
  return 0;
}

int main(int argc, char *argv[]) {
  struct sockaddr_in addr;
  struct sockaddr_storage their_addr;
  struct addrinfo *result;
  socklen_t addr_len;
  int myport, numbytes;
  char name[50];
  char realip[20];
  request_t packet;

  if(argc != 2){
    fprintf(stderr, "usage: supergopher vpn-port \n");
    exit(1);
  }

  create_table();
  myport = atoi(argv[1]);

  setupsock = socket(AF_INET, SOCK_DGRAM, 0);
  if(setupsock == -1){
    fprintf(stderr, "Failed to create socket\n");
    exit(1);
  }
  addr.sin_family = AF_INET;
  addr.sin_port =  htons(myport);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(setupsock, (const struct sockaddr *) &addr, sizeof(addr)) == -1) {
    fprintf(stderr, "Failed to bind\n");
    exit(1);
  }

  if (gethostname(name, sizeof(name))) {
    perror("Invalid");
  }
  if (getaddrinfo(name, NULL, NULL, &result)) {
    perror("Invalid");
  }

  strcpy(realip, inet_ntoa(((struct sockaddr_in *)result->ai_addr)->sin_addr));
  printf("Running supergopher at %s %d\n", realip, myport);
  char buf[MAXBUFLEN];

  addr_len = sizeof(their_addr);
  fd_set readfds;

  FD_ZERO(&readfds);
  FD_ZERO(&master);
  FD_SET(setupsock, &master);
  fdmax = setupsock;

  while(1){
    readfds = master;
    if (select(fdmax+1, &readfds, NULL, NULL, NULL) == -1){
      perror("select");
      exit(1);
    }

    for(int fd_i = 0; fd_i <= fdmax; fd_i++){
      if(FD_ISSET(fd_i, &readfds)){ // fd_i is ready to read!
        /* if(fd_i == setupsock){ */
        if((numbytes = recvfrom(fd_i, &buf, MAXBUFLEN , 0,
                                (struct sockaddr *)&their_addr, &addr_len)) == -1){
          perror("recvfrom");
          exit(1);
        }

        // If it came through the setupsock we setup a new tunnel
        if(fd_i == setupsock){
          // If tunnel is created we add the new sockets to the set that we are listening to
          if(setup_tunnel(their_addr, buf)){
            FD_SET(sock_arr[sock_i - 2], &master);
            FD_SET(sock_arr[sock_i - 1], &master);
            printf("clisockfd: %d\n", sock_arr[sock_i - 2]);
            printf("servsockfd: %d\n", sock_arr[sock_i - 1]);
            fdmax = sock_arr[sock_i - 1];
          }
          else
            printf("Could not create tunnel, max tunnel conections reached\n");
        }
        // Otherwise it's a forward packet
        else{
          printf("Got data!\n");
          /* forward(buf, their_addr, numbytes); */
        }
      }
    }

  }

  return 0;
}
