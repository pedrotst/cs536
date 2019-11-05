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
#define TRANSITSOCKIND 10

int clisock_arr[TRANSITSOCKIND];
int servsock_arr[TRANSITSOCKIND];
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


int update_table(int sock_i, int transit_port_1, int transit_port_2,
                 char *cli_ip, int cli_port, char *server_ip, int server_port){
  FILE *fp, *_fp;
  char _cli_ip[16], _server_ip[16];
  int _sock_i, _tport1, _tport2, _cli_port, _server_port;

  fp = fopen("forward_table.txt", "r");
  _fp = fopen("new_table.txt", "w");
  if(!fp || !_fp){
    fprintf(stderr, "forward table could not be created\n");
    exit(1);
  }

  _cli_ip[0] = '\0';
  _server_ip[0] = '\0';

  printf("%d %d %d %s %d %s %d\n", sock_i, transit_port_1, transit_port_2,
          cli_ip, cli_port, server_ip, server_port);

  int added = 0;
  while(fscanf(fp, "%d %d %d %[^ ] %d %[^ ] %d\n", &_sock_i, &_tport1, &_tport2,
               _cli_ip, &_cli_port, _server_ip, &_server_port) == 7){

    if(strcmp(_cli_ip, cli_ip) == 0){
      added++;
      fprintf(_fp, "%d %d %d %s %d %s %d\n", sock_i, transit_port_1, transit_port_2,
              cli_ip, cli_port, server_ip, server_port);
    }
    else{
      fprintf(_fp, "%d %d %d %s %d %s %d\n", _sock_i, _tport1, _tport2,
              _cli_ip, _cli_port, _server_ip, _server_port);
    }
    _cli_ip[0] = '\0';
  }

  if(!added)
      fprintf(_fp, "%d %d %d %s %d %s %d\n", sock_i, transit_port_1, transit_port_2,
              cli_ip, cli_port, server_ip, server_port);

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

int setup_tunnel(struct sockaddr_storage their_addr, uint8_t* buf){
  char their_ip[16];
  struct sockaddr_in naddr;
  socklen_t addrLen = sizeof naddr;
  answer_t ans;
  int numbytes;
  request_t packet;

  memcpy(&packet, buf, sizeof(request_t));

  if(sock_i > TRANSITSOCKIND){
    return 0;
  }

  their_ip[0] = '\0';

  inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            their_ip, sizeof their_ip);

  int their_port = ntohs(get_in_port((struct sockaddr *)&their_addr));

  printf("Tunnel request received from %s:%d!\n", their_ip, their_port);
  printf("It contains %s:%d\n", packet.server_ip, packet.server_port);
  /* printf("Packet is %d bytes long\n", numbytes); */

  // Open a new socket for the tunneling
  // at position sock_i
  clisock_arr[sock_i] = socket(AF_INET, SOCK_DGRAM, 0);
  if(clisock_arr[sock_i] == -1){
    fprintf(stderr, "Failed to create socket\n");
  }

  servsock_arr[sock_i] = socket(AF_INET, SOCK_DGRAM, 0);
  if(clisock_arr[sock_i] == -1){
    fprintf(stderr, "Failed to create socket\n");
  }

  naddr.sin_family = AF_INET;
  naddr.sin_port =  0;
  naddr.sin_addr.s_addr = INADDR_ANY;
  if (bind(clisock_arr[sock_i], (const struct sockaddr *) &naddr, sizeof(naddr)) == -1) {
    fprintf(stderr, "Failed to bind tunnel\n");
    exit(1);
  }
  if (getsockname(clisock_arr[sock_i], (struct sockaddr *)&naddr, &addrLen) == -1) {
    printf("tunnel getsockname() failed\n");
    exit(1);
  }
  int tport1 = htons(naddr.sin_port);
  printf("Tunnel created for trans1:%d\n", tport1);

  naddr.sin_family = AF_INET;
  naddr.sin_port =  0;
  naddr.sin_addr.s_addr = INADDR_ANY;
  if (bind(servsock_arr[sock_i], (const struct sockaddr *) &naddr, sizeof(naddr)) == -1) {
    fprintf(stderr, "Failed to bind tunnel\n");
    exit(1);
  }
  if (getsockname(servsock_arr[sock_i], (struct sockaddr *)&naddr, &addrLen) == -1) {
    printf("tunnel getsockname() failed\n");
    exit(1);
  }
  fdmax = servsock_arr[sock_i];

  int tport2 = htons(naddr.sin_port);
  printf("Tunnel created for trans2:%d\n", tport2);

  ans.sig = 3;
  ans.transit_port = tport1;

  if ((numbytes = sendto(clisock_arr[0], &ans, sizeof(answer_t), 0,
                         (struct sockaddr *)&their_addr, sizeof(their_addr))) == -1) {
    perror("create tunnel sendto");
  }

  update_table(sock_i, tport1, tport2, their_ip, -1, packet.server_ip, packet.server_port);
  sock_i++;
}

int forward(uint8_t *buf, struct sockaddr_storage their_addr, int numbytes){
  FILE *fp, *_fp;
  char _cli_ip[16], _server_ip[16], their_ip[16];
  int _sock_i, _tport1, _tport2, _cli_port, _server_port;

  inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            their_ip, sizeof their_ip);


  fp = fopen("forward_table.txt", "r");
  if(fp == NULL){
    fprintf(stderr, "Something went wrong with forward table\n");
    exit(1);
  }


  while(fscanf(fp, "%d %d %d %[^ ] %d %[^ ] %d\n", &_sock_i, &_tport1, &_tport2,
               _cli_ip, &_cli_port, _server_ip, &_server_port) == 7){
    if(strcmp(their_ip, _cli_ip) == 0){
      printf("Got a packet from %s:%d\n", _cli_ip, _cli_port);
      //forward to _server_ip
      fclose(fp);
      return 1;
    }
    else if(strcmp(their_ip, _server_ip) == 0){
      // forward to _client_ip
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
  int setupsock;
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
  uint8_t buf[MAXBUFLEN];

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
            FD_SET(clisock_arr[sock_i - 1], &master);
            FD_SET(servsock_arr[sock_i - 1], &master);
            printf("clisockfd: %d\n", clisock_arr[sock_i - 1]);
            printf("servsockfd: %d\n", servsock_arr[sock_i - 1]);
            fdmax = servsock_arr[sock_i - 1];
          }
          else
            printf("Could not create tunnel, max tunnel conections reached\n");
        }
        // Otherwise it's a forward packet
        else{
          printf("Got data!\n");
          forward(buf, their_addr, numbytes);
        }
      }
    }

  }

  return 0;
}
