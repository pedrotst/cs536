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

int main(int argc, char** argv) {
	if(argc != 8){
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
	//printf("Filename: %s\n", buf);
	write(tcp_port, buf, strlen(buf));

	char response[7];
	int bytes_read = read(tcp_port, response, 7);

	short server_udp_port = 0;
	int file_size = 0;

	if (response[0] != 2) {
		fprintf(stderr, "request rejected.\n");
		return 1;
	} 

	memcpy(&server_udp_port, &response[1], 2);
	memcpy(&file_size, &response[3], 4);

	//printf("Server port: %hu, File size: %d\n",server_udp_port,file_size );
	int stream_sock = socket(AF_INET, SOCK_DGRAM,0);

	struct sockaddr_in stream_addr;
	stream_addr.sin_family = AF_INET;
	stream_addr.sin_addr.s_addr = INADDR_ANY;
	stream_addr.sin_port = 0;

	if (bind(stream_sock, (struct sockaddr *)&stream_addr, sizeof(stream_addr))) {
		fprintf(stderr, "bind\n" );
		return 1;
	}

	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);
	getsockname(stream_sock, (struct sockaddr *)&sin, &len);
	short stream_port = (short) ntohs(sin.sin_port);

	//printf("Client port: %hu\n", stream_port);
	write(tcp_port, &stream_port, 2);
	close(tcp_port);

	struct sockaddr_in server_udp;
	server_udp.sin_family = AF_INET;
	server_udp.sin_addr.s_addr = inet_addr(argv[1]);
	server_udp.sin_port = htons(server_udp_port);

	fd_set active_fd_set;
	FD_ZERO(&active_fd_set);
	FD_SET(stream_sock, &active_fd_set);

	while (1) {
		if (select(FD_SETSIZE, &active_fd_set, NULL, NULL, NULL) < 0) {
			fprintf(stderr, "select\n" );
			return 1;
		}
	}
}
