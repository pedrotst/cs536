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

#define DEBUG

int main(int argc, char** argv) {
    if(argc != 9){
        printf("usage: playaudio tcp-ip tcp-port audiofile payload-size gamma buf-size target-buf logfile2\n");
        exit(1);
    }

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
	char recbuf[1500];
	char recv_content[1488];
	int seq_num = 0;
	int completed = 0;
	FILE *fp = fopen("received_file.txt", "w");

	while (1) {
#ifdef DEBUG
		printf("Waiting to receive\n");
#endif
		if (select(FD_SETSIZE, &active_fd_set, NULL, NULL, NULL) < 0) {
            fprintf(stderr, "select\n" );
            return 1;
        }
		bytes_read = recvfrom(child_sock, recbuf, 1492, 0, (struct sockaddr *) &src, &src_len);

#ifdef DEBUG
		printf("Received %d bytes\n", bytes_read);
#endif

		if(bytes_read == 1 && recbuf[0] == '5'){
			break;
		}

		memcpy(&seq_num, recbuf, 4);
		memcpy(recv_content, &recbuf[4], bytes_read - 4);
		fwrite(recv_content, bytes_read - 4, 1, fp);
    }
	fclose(fp);

	return 1;
}
