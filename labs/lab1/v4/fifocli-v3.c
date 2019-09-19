#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <setjmp.h>

#include <signal.h>
#include <stdbool.h>

#define max_size 999999

char *server_fifo = "server_queue";
char *client_fifo = "client_queue";
char buf[max_size];
int fd;

static jmp_buf env_alarm;

void handle_alarm(int sig){
  static int sigcount;

  sigcount++;
  printf("[Client]: Timeout, resending request\n");
  fflush(stdout);

  siglongjmp(env_alarm, sigcount);
}

void create_client_fifo(){
  if(mkfifo(client_fifo, 0666) != 0){
    printf("[Client]: FIFO failed to be created\n");
    exit(1);
  }
}

char* read_command(){
  // print prompt
  fprintf(stdout,"[%d]$ ",getpid());

  // read command from stdin
  fgets(buf, 100, stdin);
  int len = strlen(buf);

  buf[len-1] = '\0';

}

int send_request(){
  int f;
  int len = strlen(buf);

  // Open Server FIFO
  f = open(server_fifo, O_WRONLY);

  // Write the request to Server FIFO
  write(f, buf, len + 1);

  close(f);
}

void open_read_file(){
  // Open client fifo to wait for server response
  fd = open(client_fifo, O_RDONLY);
}

int get_response(){
  int len;
  char c;
  int i;


  i = 0;
  c = 's';
  // Receives the response from the server
  while(c != 3 && i < max_size - 2){
      // read( filedesc, buff, len ); /* there was data to read */
      read(fd, &c, 1);
      buf[i] = c;
      i++;
  }
  // We did it reddit, let's cancel the alarm now
  alarm(0);

  printf("\n");
  buf[i] = '\0';

  // Print the response
  fprintf(stdout, "%s", buf);
  close(fd);
}

int main(void)
{
  int status;
  sigset_t mask;

  // Setup non blocking signal handling
  sigemptyset(&mask);
  sigaddset(&mask, SIGALRM);

  struct sigaction sa;
  sigfillset(&sa.sa_mask);
  sa.sa_flags = 0; // DO interrupt blocking system calls
  sa.sa_handler = handle_alarm;

  if (sigaction(SIGALRM, &sa, 0)) {
    perror("sigaction");
    return 1;
  }


  // We're ready to setup the communication with the server
  create_client_fifo();
  read_command();

  if (sigsetjmp(env_alarm, 1) > 2){ 
    printf("Tried to contact the server too much, dropping request\n");
    fflush(stdout);
    return 0;
  }

  alarm(2);
  send_request();
  open_read_file(); // This function call hangs when no message was received
  get_response();

  // And we're done
  return 0;
}
