#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(void)
{
  pid_t k;
  char *fifo = "server_queue";

  int max_size = 100;
  char buf[max_size];
  char* argv[10];
  int arg_num;
  int status;
  int len;

  // Start by creating the FIFO
  // Make sure you don't have any files named server_queue before running the server
  if(mkfifo("server_queue", 0666) != 0){
    printf("Fifo failed to be created\n");
    exit(1);
  }

  while(1) {
    int fd = open(fifo, O_RDONLY);

    char c = 's'; // Start with a valid character
    int i = 0;

    // Loop to read characters from the FIFO until the \0 is reached
    while(c != '\0' && i < max_size - 1){
      read(fd, &c, sizeof(char));
      buf[i] = c;
      i++;
    }

    buf[i] = '\0';

    k = fork();
    if (k==0) {
      int i = 0;

      // We first split the buffer into tokens
      argv[0] = strtok(buf, " ");
      while(argv[i] != NULL && i < 10){
        i++;
        argv[i] = strtok(NULL, " ");
      }

      // Now we can call execvp
      if(execvp(argv[0], argv) == -1){	// if execution failed, terminate child
        printf("Error!\n");
        exit(1);
      }
    }
  }
}
