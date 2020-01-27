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
  int max_size = 100;
  char buf[max_size];
  char *fifo = "server_queue";

  // print prompt
  fprintf(stdout,"[%d]$ ",getpid());

  // read command from stdin
  fgets(buf, 100, stdin);
  int len = strlen(buf);

  buf[len-1] = '\0';

  // Open FIFO
  fd = open(fifo, O_WRONLY);

  // Write to the FIFO
  write(fd, buf, len);

  // And we're done
  return 0;

}
