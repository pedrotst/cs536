#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXBUFSZM (1000000)
#define MAXCHARSIZE (100)
#define BACKLOG 10 // how many pending connections queue will hold


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/*
  usage
  $ myfetchfiled blocksize srv-port
 */
int main(int argc, char *argv[]) {
  int sockfd, new_fd;
  struct addrinfo;
  int numbytes;
  struct sockaddr_storage their_addr;
  socklen_t addr_len;
  char s[INET6_ADDRSTRLEN];
  struct sockaddr_in addr;
  socklen_t addrLen;
  socklen_t sin_size;
  int srv_port, blocksize;
  char filename[100];

  if(argc != 3){
    fprintf(stderr, "usage: myfetchfiled blocksize srv-port\n");
    exit(1);
  }

  blocksize = atoi(argv[1]);
  srv_port = atoi(argv[2]);

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd == -1){
    fprintf(stderr, "Failed to create socket\n");
    exit(1);
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(srv_port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if(bind(sockfd, (const struct sockaddr *) &addr, sizeof(addr)) == -1){
    fprintf(stderr, "Failed to bind\n");
    exit(1);
  }

  addrLen = sizeof addr;
  if(getsockname(sockfd, (struct sockaddr *)&addr, &addrLen) == -1){
    fprintf(stderr, "getsockname() failed\n");
    exit(1);
  }

  addr_len = sizeof their_addr;

  if(listen(sockfd, BACKLOG) == -1){
    perror("listen");
    exit(1);
  }


  char hostnamebuf[1000];
  gethostname(hostnamebuf, 1000);
  printf("sender: Listening at %s:%d\n", hostnamebuf, srv_port);

  while(1){
    sin_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

    if(new_fd == -1){
      perror("accept");
      continue;
    }

    inet_ntop(their_addr.ss_family,
              get_in_addr((struct sockaddr *) &their_addr),
              s, sizeof s);
    printf("server: got connection from %s\n", s);

    int k = fork();

    if(k != 0) { // this is the child process
      /* close(sockfd); //child doesn't need the listener */

      int numbytes;

      if((numbytes = read(new_fd, filename, MAXCHARSIZE - 1)) == -1) {
        perror("recv");
        exit(1);
      }

      filename[numbytes] = '\0';
      fprintf(stderr, "sender: received filename '%s'\n", filename);

      FILE *fptr;

      fptr = fopen(filename, "r");

      if(fptr == NULL){
        fprintf(stderr, "sender: file %s does not exist\n", filename);
        write(new_fd, "0", 1);
        close(new_fd);
      }

      int c = fgetc(fptr);
      if(c == EOF){
        fprintf(stderr, "sender: file %s is empty\n", filename);
        write(new_fd, "1", 1);
        close(new_fd);
      }

      write(new_fd, "2", 1);

    }

  }

  close(sockfd);

  return 0;
}
