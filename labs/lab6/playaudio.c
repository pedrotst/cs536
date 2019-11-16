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
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in dest;
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = inet_addr(argv[1]);
	dest.sin_port = htons(atoi(argv[2]));

	if (connect(sockfd, (struct sockaddr*) &dest, sizeof(dest))) {
		fprintf(stderr, "socket connection failed.\n");
		exit(1);
	}

	char * buf = strdup(argv[3]);
	printf("Filename: %s\n", buf);
	write(sockfd, buf, strlen(buf));

	char response[7];
	int bytes_read = read(sockfd, response, 7);

	short server_udp_port = 0;
	int file_size = 0;

	if (response[0] != 2) {
		fprintf(stderr, "request rejected.\n");
		return 1;
	} 

	memcpy(&server_udp_port, &response[1], 2);
	memcpy(&file_size, &response[3], 4);

	printf("Server port: %hu, File size: %d\n",server_udp_port,file_size );
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

	printf("Client port: %hu\n", stream_port);
	write(sockfd, &stream_port, 2);




}