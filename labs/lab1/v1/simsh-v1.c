// simple shell example using fork() and execlp()

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>

int main(void)
{
  pid_t k;
  char buf[100];
  char* argv[10];
  int arg_num;
  int status;
  int len;

  while(1) {

    // print prompt
    fprintf(stdout,"[%d]$ ",getpid());

    // read command from stdin
    fgets(buf, 100, stdin);
    len = strlen(buf);
    if(len == 1) 				// only return key pressed
      continue;
    buf[len-1] = '\0';

    k = fork();
    if (k==0) {
      // child code
      int i = 0;

      // We first split the buffer into tokens
      argv[0] = strtok(buf, " ");
      while(argv[i] != NULL && i < 10){
        i++;
        argv[i] = strtok(NULL, " ");
      }

      printf("[SERVER]: Received request to execute %s\n", argv[0]);
      // Now we can call execv
      if(execvp(argv[0], argv) == -1){	// if execution failed, terminate child
        printf("error!!\n");
        exit(1);
      }
    }
    else {
      // parent code 
      waitpid(k, &status, 0);
    }
  }
}
