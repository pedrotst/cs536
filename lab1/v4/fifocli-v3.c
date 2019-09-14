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

volatile sig_atomic_t alarm_flag = false;
char *server_fifo = "server_queue";
char *client_fifo = "client_queue";
int tries;
char buf[max_size];
int fd;

static jmp_buf env_alarm;
pid_t k;

void handle_alarm(int sig){
  static int sigcount;

  sigcount++;
  printf("[Client]: Timeout, resending request\n");
  fflush(stdout);

  siglongjmp(env_alarm, sigcount);
}

void create_client_fifo(){
  if(mkfifo(client_fifo, 0666) != 0){
    // printf("[Client]: FIFO failed to be created\n");
    // exit(1);
  }
}

char* read_command(){
  // print prompt
  fprintf(stdout,"[%d]$ ",getpid());

  // read command from stdin
  fgets(buf, 100, stdin);
  int len = strlen(buf);

  buf[len-1] = '\0';
  // printf("[Client]: Writing %s,", buf);
  // fflush(stdout);

}

int send_request(){
  int f;
  int len = strlen(buf);

  // printf("[Thread %d]: Sending '%s'\n", k, buf);
  // fflush(stdout);
  // Open Server FIFO
  f = open(server_fifo, O_WRONLY);

  // Write the request to Server FIFO
  write(f, buf, len + 1);

  close(f);
  // printf("[Thread %d]: Sent!\n", k);
  // fflush(stdout);
}

void open_read_file(){
  // Open client fifo to wait for server response
  // printf("Open Read File\n");
  // fflush(stdout);
  fd = open(client_fifo, O_RDONLY);
  // printf("File Read Opened\n");
  // fflush(stdout);
}

// Returns 1 if data was read and < 1 otherwise
int get_response(){
  int len;
  char c;
  int i;


  // FD_ZERO(&set);
  // FD_SET(fd, &set);

  i = 0;
  c = 's';
  // Receives the response from the server
  // Waits until the server sends an end-of-transmission character so we know the transmission is over
  // printf("Waiting for response\n");
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

/*
void request_loop(){
  if(tries > 2){

  tries++;
  alarm(2);
  send_request();

}
*/

int main(void)
{
  tries = 0;
  int rc;
  int status;
  sigset_t mask; //, omask;

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


  tries = 0;

  create_client_fifo();
  read_command();

  if (sigsetjmp(env_alarm, 1) > 2){ 
    printf("[Thread %d]: Tried to contact the server too much, dropping request\n", k);
    fflush(stdout);
    return 0;
  }

  alarm(2);
  send_request();
  open_read_file();
  get_response();

  // And we're done
  return 0;
}
