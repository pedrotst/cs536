#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXBUFLEN 31

static int pad;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

char mydecoder(char x, unsigned int pubkey){
  return (x ^ (char) (pubkey & (0x000000FF << (pad++ % 4))));
}

// Implace decoder
void decode(char *s, unsigned int pubkey){
  pad = 0;
  for(int i = 0; i < strlen(s); i++){
    s[i] = mydecoder(s[i], pubkey);
  }
  pad = 0;
}

int assert_decode(char *s, char* ip1, char* ip2){
  if(strcmp(ip1, ip2) != 0)
    return 1;

  for(int i = 0; i < strlen(s); i++){
    if(!isascii(s[i])){
      return 1;
    }
  }
  return 0;
}

unsigned int get_privkey(char *their_ip){
  char privkey[30];
  char ip[16];
  int found = 0;

  FILE *fp = fopen("acl.txt", "r");
  if(fp == NULL){
    fprintf(stderr, "File acl.txt does not exists");
    exit(1);
  }

  while(found == 0 && (fscanf(fp, "%[^ ] %[^\n]\n", ip, privkey) != -1)){
    found = !strcmp(their_ip, ip);
  }
  fclose(fp);

  if(found){
    return(atoi(privkey));
  }
  else{
    fprintf(stderr, "Incoming connection not in ACL file\n");
    fprintf(stderr, "Terminating conection");
    exit(1);
  }

}

int main(void) {
    int sockfd;
    struct addrinfo;
    int numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    unsigned int privkey;
    char their_ip[16];
    int decode_fail;
    struct sockaddr_in addr;
    socklen_t addrLen;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd == -1){
      printf("Failed to create socket\n");
      exit(1);
    }
    addr.sin_family = AF_INET;
    addr.sin_port =  0; //htons(SERVERPORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sockfd, (const struct sockaddr *) &addr, sizeof(addr)) == -1) {
      printf("Failed to bind\n");
      exit(1);
    }
    addrLen = sizeof addr;
    if (getsockname(sockfd, (struct sockaddr *)&addr, &addrLen) == -1) {
      printf("getsockname() failed\n");
      exit(1);
    }
    printf("Listening at port %d\n", htons(addr.sin_port));


    addr_len = sizeof their_addr;

    do{
    printf("Waiting to recvfrom...\n");
      if((numbytes = recvfrom(sockfd, buf, MAXBUFLEN , 0,
                              (struct sockaddr *)&their_addr, &addr_len)) == -1){
        perror("recvfrom");
        exit(1);
      }
      if(numbytes > MAXBUFLEN - 1){
        printf("Command exceeded max size of %d, dropping package\n", MAXBUFLEN);
        continue;
      }

      int ip;
      strcpy(their_ip, inet_ntop(their_addr.ss_family,
                                 get_in_addr((struct sockaddr *)&their_addr),
                                 s, sizeof s));

      printf("Got packet from %s!\n", their_ip);
      printf("Packet is %d bytes long\n", numbytes);

      privkey = get_privkey(their_ip);
      /* printf("privkey: %d\n", privkey); */
      decode(buf, privkey);
      memcpy(&ip, buf, 4);
      struct in_addr ip_addr;
      ip_addr.s_addr = ip;
      char coded_ip[16];
      strcpy(coded_ip, inet_ntoa(ip_addr));

      /* printf("decoded msg: %u,%s:%s\n", ip, inet_ntoa(ip_addr), &buf[4]); */
      buf[numbytes] = '\0';
      strcpy(buf, &buf[4]);

      decode_fail = assert_decode(&buf[4], their_ip, coded_ip);
      if(decode_fail)
        fprintf(stderr, "Decoded message ended up in a bogus format, dropping request\n");

    }while(numbytes > MAXBUFLEN - 1 || decode_fail);
    /* printf("Packet contains \"%s\"\n", buf); */

    close(sockfd);

    int k = fork();
    char* args[10];
    int status;
   
    if (k==0) {
      int i = 0;

      // We first split the buffer into tokens
      args[0] = strtok(buf, " ");
      while(args[i] != NULL && i < 10){
        i++;
        args[i] = strtok(NULL, " ");
      }

      // Now we can call execvp
      if(execvp(args[0], args) == -1){	// if execution failed, terminate child
        printf("Error!\n");
        exit(1);
      }
    }
    else{
      waitpid(k, &status, 0);
    }

    return 0;
}
