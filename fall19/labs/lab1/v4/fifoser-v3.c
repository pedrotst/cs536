#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

int main(void)
{
  pid_t k;
  time_t raw_time;
  struct tm *timeinfo;

  char *server_fifo = "server_queue";
  char *client_fifo = "client_queue";

  int max_size = 100;
  char buf[max_size];
  char* argv[10];
  int arg_num;
  int status;
  int len;

  // Start by creating the FIFO
  // Make sure you don't have any files named server_queue before running the server
  if(mkfifo(server_fifo, 0666) != 0){
    printf("[Server]: FIFO failed to be created\n");
    exit(1);
  }

  // Use Current Time as seed for random number
  srand(time(0));

  while(1) {
    int fd = open(server_fifo, O_RDONLY);

    char c = 's'; // Start with a valid character
    int i = 0;
    int restoreout = dup(1);

    // Loop to read characters from the FIFO until the \0 is reached
    while(c != '\0' && i < max_size - 1){
      read(fd, &c, sizeof(char));
      buf[i] = c;
      i++;
    }
    buf[i] = '\0';

    time(&raw_time);
    timeinfo = localtime(&raw_time);
    printf("%s[Server]: Received request for '%s'\n", asctime(timeinfo), buf);

    fflush(stdout);

    close(fd);
    if(rand() % 2){
      printf("[Server]: You know what, I don't want to process the next request\n");
      printf("[Server]: ¯\\_(ツ)_/¯\n");
      fflush(stdout);
      continue;
    }


    // printf("Received: %s,\n", buf);

    fd = open(client_fifo, O_WRONLY);

    dup2(fd, 1);

    k = fork();
    if (k==0) {
      int i = 0;

      // We first split the buffer into tokens
      // printf("[SERVER]: Sending over %s", buf);
      argv[0] = strtok(buf, " ");
      while(argv[i] != NULL && i < 10){
        i++;
        argv[i] = strtok(NULL, " ");
      }

      // Now we can call execvp
      if(execvp(argv[0], argv) == -1){	// if execution failed, terminate child
        printf("[Server]: Error!\n");
        close(fd);
        exit(1);
      }
    }
    else {
      // Wait until execution of execvp is over and writes a end-of-transmission character so the client knows we are done
      waitpid(k, &status, 0);
      c = 3;
      write(fd, &c, sizeof(char)); 
      fflush(stdout);
      dup2(restoreout, 1);
      close(fd);
    }

  }
}
