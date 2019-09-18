#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAXBUFLEN 100

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
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

    printf("listener: waiting to recvfrom...\n");

    addr_len = sizeof their_addr;
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }

    printf("listener: got packet from %s\n",
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s));
    printf("listener: packet is %d bytes long\n", numbytes);
    buf[numbytes] = '\0';
    printf("listener: packet contains \"%s\"\n", buf);

    close(sockfd);

    return 0;
}
