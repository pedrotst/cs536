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

#define MAXBUFLEN 50
#define TRANSITSOCKIND 10

int in_sock_arr[TRANSITSOCKIND];
int out_sock_arr[TRANSITSOCKIND];
int sock_i = 0;

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

void setup_tunnel(struct sockaddr_storage their_addr, request_t packet){
  char their_ip[16];
  struct sockaddr_in naddr;
  socklen_t addrLen = sizeof naddr;
  answer_t ans;
  int numbytes;


  inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            their_ip, sizeof their_ip);

  int their_port = ntohs(get_in_port((struct sockaddr *)&their_addr));

  printf("Got packet from %s:%d!\n", their_ip, their_port);
  printf("It contains %s:%d\n", packet.server_ip, packet.server_port);
  /* printf("Packet is %d bytes long\n", numbytes); */

  // Open a new socket for the tunneling
  // at position sock_i
  in_sock_arr[sock_i] = socket(AF_INET, SOCK_DGRAM, 0);
  if(in_sock_arr[sock_i] == -1){
    fprintf(stderr, "Failed to create socket\n");
  }

  naddr.sin_family = AF_INET;
  naddr.sin_port =  0;
  naddr.sin_addr.s_addr = INADDR_ANY;
  if (bind(in_sock_arr[sock_i], (const struct sockaddr *) &naddr, sizeof(naddr)) == -1) {
    fprintf(stderr, "Failed to bind tunnel\n");
    exit(1);
  }
  if (getsockname(in_sock_arr[sock_i], (struct sockaddr *)&naddr, &addrLen) == -1) {
    printf("tunnel getsockname() failed\n");
    exit(1);
  }

  printf("Tunnel created on port %d\n", htons(naddr.sin_port));
  int transit_port = naddr.sin_port;
  ans.sig = 3;
  ans.transit_port = transit_port;

  if ((numbytes = sendto(in_sock_arr[0], &ans, sizeof(answer_t), 0,
                         (struct sockaddr *)&their_addr, sizeof(their_addr))) == -1) {
    perror("create tunnel sendto");
  }

  update_table(sock_i, htons(transit_port), -1, their_ip, -1, packet.server_ip, packet.server_port);
}

int main(int argc, char *argv[]) {
  struct sockaddr_in addr;

  struct sockaddr_storage their_addr;
  socklen_t addr_len;
  char buf[MAXBUFLEN], their_ip[20];
  int myport, numbytes;
  int setupsock;

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

  char name[50];
  struct addrinfo *result;

  if (gethostname(name, sizeof(name))) {
    perror("Invalid");
  }
  if (getaddrinfo(name, NULL, NULL, &result)) {
    perror("Invalid");
  }

  char realip[20];
  strcpy(realip, inet_ntoa(((struct sockaddr_in *)result->ai_addr)->sin_addr));
  printf("Running supergopher at %s %d\n", realip, myport);

  while(1){
    request_t packet;

    if((numbytes = recvfrom(setupsock, &packet, sizeof(request_t) , 0,
                            (struct sockaddr *)&their_addr, &addr_len)) == -1){
      perror("recvfrom");
      exit(1);
    }
    setup_tunnel(their_addr, packet);

  }

  return 0;
}
