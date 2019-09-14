#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


int main(void)
{
  int fd;
  int max_size = 99999;
  char buf[max_size];
  char c;
  int i;

  char *server_fifo = "server_queue";
  char *client_fifo = "client_queue";

  if(mkfifo(client_fifo, 0666) != 0){
    printf("[Client]: FIFO failed to be created\n");
    exit(1);
  }

  // print prompt
  fprintf(stdout,"[%d]$ ",getpid());

  // read command from stdin
  fgets(buf, 100, stdin);
  int len = strlen(buf);

  buf[len-1] = '\0';
  // printf("[Client]: Writing %s,", buf);
  fflush(stdout);

  // Open Server FIFO
  fd = open(server_fifo, O_WRONLY);

  // Write the request to Server FIFO
  write(fd, buf, len);

  close(fd);

  // Open client fifo to wait for server response
  fd = open(client_fifo, O_RDONLY);

  i = 0;
  c = 's';
  // Receives the response from the server
  // Waits until the server sends an end-of-transmission character so we know the transmission is over
  while(c != 3 && i < max_size - 2){
    read(fd, &c, sizeof(char));
    buf[i] = c;
    i++;
  }
  printf("\n");
  buf[i] = '\0';

  // Print the response
  fprintf(stdout, "%s", buf);

  close(fd);

  // And we're done
  return 0;
}
