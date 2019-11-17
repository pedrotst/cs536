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
	if (sockfd == -1) {
		fprintf(stderr, "socket creation failed.\n");
		exit(1);
	}
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(atoi(argv[1]));
	if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr))) {
		fprintf(stderr, "socket binding failed.\n");
		exit(1);
	}

	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);
	if (getsockname(sockfd, (struct sockaddr *)& sin, &len) == -1) {
		fprintf(stderr, "getsockname() failed.\n");
		exit(1);
	}

	fprintf(stderr, "port number: %d\n", ntohs(sin.sin_port));

	listen(sockfd, 3);

	while (1) {
		struct sockaddr_in client;
		socklen_t client_len = sizeof(client);
		int client_sock = accept(sockfd, (struct sockaddr *) &client, &client_len);
		if (client_sock < 0) {
			fprintf(stderr, "accept() failed.\n");
			exit(1);
		}

		char filename[100];
		int bytes_read = read(client_sock, filename, 100); 
		filename[bytes_read] = '\0';

		printf("File name: %s\n", filename);	

		if (access(filename, F_OK) == -1) { //send 0
			char response = 0;
			write(client_sock, &response, 1);

		} else { //send 2+port+file_size
			FILE * fp = fopen(filename, "r");
			fseek(fp, 0L, SEEK_END);
			int file_size = ftell(fp);
			printf("File size: %d\n", file_size);
			rewind(fp);


			char response[7];
			response[0] = 2;

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
			printf("server_port: %hu\n", stream_port);
			memcpy(&response[1], &stream_port, 2);
			memcpy(&response[3], &file_size, 4);
			write(client_sock, &response, 7);

			short client_port = 0;
			read(client_sock, &client_port, 2);

			printf("client port: %hu\n", client_port);

		}




	}
}