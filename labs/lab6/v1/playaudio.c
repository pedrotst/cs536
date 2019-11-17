#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

#define DEBUG 1

char *recbuf;
int payload_size;
int buf_size;
int bytes_flushed = 0;
int bytes_occupied = 0;
FILE * audio_device;

void flush_buffer(int sig) {
    char * buf_cpy = malloc(buf_size);
   

    if (bytes_occupied >=  payload_size) {
        fwrite(recbuf, 1, payload_size, audio_device);
        bytes_occupied -= payload_size;
        memcpy(buf_cpy, &recbuf[payload_size], bytes_occupied);
        bytes_flushed += payload_size;
        
    } else {
        fwrite(recbuf, 1, bytes_occupied, audio_device);
        bytes_flushed += bytes_occupied;
        bytes_occupied = 0;
        
    }
    free(recbuf);
    recbuf = buf_cpy;
}

int main(int argc, char** argv) {

    struct sigaction sa;
    sa.sa_handler = flush_buffer;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGALRM, &sa, NULL);

    //signal(SIGALRM, flush_buffer);
    if(argc != 9){
        printf("usage: playaudio tcp-ip tcp-port audiofile payload-size gamma buf-size target-buf logfile2\n");
        exit(1);
    }

    audio_device = fopen("./test.txt", "w");
    if (audio_device == NULL) {
        printf("/dev/audio\n");
        return 1;
    }
    payload_size = atoi(argv[4]);
    int gamma = atoi(argv[5]);
    buf_size = atoi(argv[6]);
    recbuf = malloc(buf_size);
    int target_buf = atoi(argv[7]);

    int tcp_port = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr(argv[1]);
    dest.sin_port = htons(atoi(argv[2]));

    if (connect(tcp_port, (struct sockaddr*) &dest, sizeof(dest))) {
        fprintf(stderr, "socket connection failed.\n");
        exit(1);
    }

    char * buf = strdup(argv[3]);
#ifdef DEBUG
    printf("Filename: %s\n", buf);
#endif
    write(tcp_port, buf, strlen(buf));

    char response[7];
    int bytes_read = read(tcp_port, response, 7);

    short server_udp_port = 0;
    int file_size = 0;

    if (response[0] != '2') {
        fprintf(stderr, "request rejected.\n");
        return 1;
    }

    memcpy(&server_udp_port, &response[1], 2);
    memcpy(&file_size, &response[3], 4);

#ifdef DEBUG
    printf("Server port: %hu, File size: %d\n",server_udp_port,file_size );
#endif
    int child_sock = socket(AF_INET, SOCK_DGRAM,0);

    struct sockaddr_in stream_addr;
    stream_addr.sin_family = AF_INET;
    stream_addr.sin_addr.s_addr = INADDR_ANY;
    stream_addr.sin_port = 0;

    if (bind(child_sock, (struct sockaddr *)&stream_addr, sizeof(stream_addr))) {
        fprintf(stderr, "bind\n" );
        return 1;
    }

    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    getsockname(child_sock, (struct sockaddr *)&sin, &len);
    short stream_port = (short) ntohs(sin.sin_port);

#ifdef DEBUG
    printf("Client port: %hu\n", stream_port);
#endif
    write(tcp_port, &stream_port, 2);
    close(tcp_port);

    struct sockaddr_in server_udp;
    server_udp.sin_family = AF_INET;
    server_udp.sin_addr.s_addr = inet_addr(argv[1]);
    server_udp.sin_port = htons(server_udp_port);

    fd_set active_fd_set;
    FD_ZERO(&active_fd_set);
    FD_SET(child_sock, &active_fd_set);

	struct sockaddr_in src;
	socklen_t src_len = sizeof(src);
	
	char recv_content[payload_size+4];
	int seq_num = 0;
	int completed = 0;
	//FILE *fp = fopen("received_file.txt", "w");
    ualarm( (useconds_t) ((1.0 / gamma) * 1000000),  (useconds_t) ((1.0 / gamma) * 1000000));
	while (1) {
#ifdef DEBUG
		printf("Waiting to receive\n");
#endif
		// if (select(FD_SETSIZE, &active_fd_set, NULL, NULL, NULL) < 0) {
  //           continue;
  //       }
		bytes_read = recvfrom(child_sock, recv_content, payload_size+4, 0, (struct sockaddr *) &src, &src_len);
        int actual_data_size = bytes_read - 4;

#ifdef DEBUG
		printf("Received %d bytes\n", bytes_read);
#endif

		if(bytes_read == 1 && recv_content[0] == '5'){
			break;
		}

		memcpy(&seq_num, recv_content, 4);
        memcpy(&recbuf[seq_num*actual_data_size- bytes_flushed], &recv_content[4], actual_data_size);
        bytes_occupied += actual_data_size;


		//memcpy(recv_content, &recbuf[4], bytes_read - 4);
		//fwrite(recv_content, bytes_read - 4, 1, fp);
    }
	fclose(audio_device);
    free(recbuf);
	return 1;
}
