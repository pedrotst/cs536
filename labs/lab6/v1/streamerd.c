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

#define DEBUG

void feedback_control(int sig) {

}

int main(int argc, char** argv) {
    if(argc != 6){
        printf("usage: streamerd tcp-port payload-size init-lambda mode logfile1\n");
        exit(1);
    }

    int tcp_port = atoi(argv[1]);
    int payload_size = atoi(argv[2]);
    unsigned long lambda = atoi(argv[3]);
    int seq_num = 0;

    if(payload_size > 1488){
        printf("Payload size must be smaller than 1488\n");
        exit(1);
    }

    signal(SIGIO, feedback_control);
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        fprintf(stderr, "socket creation failed.\n");
        exit(1);
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(tcp_port);
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

#ifdef DEBUG
    fprintf(stderr, "port number: %d\n", ntohs(sin.sin_port));
#endif

    listen(sockfd, 3);

    while (1) {
        struct sockaddr_in client_tcp;
        socklen_t client_len = sizeof(client_tcp);
        int client_sock = accept(sockfd, (struct sockaddr *) &client_tcp, &client_len);
        if (client_sock < 0) {
            fprintf(stderr, "accept() failed.\n");
            exit(1);
        }

        pid_t k = fork();

        if (k == 0) {   //child process
            char filename[100];
            int bytes_read = read(client_sock, filename, 100);
            filename[bytes_read] = '\0';

#ifdef DEBUG
            printf("File name: %s\n", filename);
#endif

            char res = 0;
            if (access(filename, F_OK) == -1) { //send 0
                write(client_sock, &res, 1);
                printf("File requested does not exists, dropping request\n");
                fflush(stdout);

            } else { //send 2+port+file_size
                FILE * fp = fopen(filename, "r");
                if(fp == NULL){
                    printf("Could not open file\n");
                    write(client_sock, &res, 1);
                    continue;
                }
                fseek(fp, 0L, SEEK_END);
                int file_size = ftell(fp);
#ifdef DEBUG
                printf("File size: %d\n", file_size);
#endif
                rewind(fp);


                char response[7];
                response[0] = 2;
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
                printf("server_port: %hu\n", stream_port);
#endif
                memcpy(&response[1], &stream_port, 2);
                memcpy(&response[3], &file_size, 4);
                write(client_sock, &response, 7);

                short client_port = 0;
                read(client_sock, &client_port, 2);
                close(client_sock);

#ifdef DEBUG
                printf("client port: %hu\n", client_port);
#endif
                struct sockaddr_in client_udp;
                client_udp.sin_family = AF_INET;
                client_udp.sin_addr.s_addr = client_tcp.sin_addr.s_addr;
                client_udp.sin_port = htons(client_port);
                int socklen = sizeof(client_udp);


                fcntl(child_sock, F_SETOWN, getpid());
                fcntl(child_sock, F_SETFL, O_ASYNC);

                char buf[payload_size + 4];
                int size_read = 0;
                int completed = 0;

                while(!completed){
                    memcpy(buf, &seq_num, 4);
                    size_read = fread(&buf[4], 1, payload_size, fp);
                    if(size_read < payload_size)
                        completed = 1;
                       
                    sendto(child_sock, buf, size_read + 4, 0, (struct sockaddr*) &client_udp, socklen);
                    seq_num++;
                    usleep((useconds_t)(((1.0/lambda) * 1000000)));
                }
                buf[0] = '5';

                sendto(child_sock, buf, 1, 0, (struct sockaddr*) &client_udp, socklen);
                close(child_sock);
            }
        }
        // parent process
        close(client_sock);


    }

    return 0;
}
