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

char *recbuf;
int payload_size;
int buf_size;
int bytes_flushed = 0;
int bytes_occupied = 0;
int file_size = 0;
int writeptr = 0;
FILE * audio_device;
int completed;

void flush_buffer(int sig) {
    char * buf_cpy = calloc(buf_size, sizeof(char));

    /* printf("\n======= flush  =========\n"); */
    /* fflush(stdout); */
    if (bytes_occupied == 0) {
        return;
    }

    fwrite(recbuf, sizeof(char), 1, audio_device);
    // FIXME: Easy performance gain: Set a threshold to read from before flushing
    bytes_occupied--;
    memcpy(buf_cpy, &recbuf[1], bytes_occupied);
    bytes_flushed++;
    writeptr--;

    if (bytes_flushed == file_size) {
        completed = 1;
    }
    free(recbuf);
    recbuf = buf_cpy;
}

/* A good set of parameters to see the buffer being filled: */
/* ./playaudio 128.10.112.201 4445 /tmp/pdacost/pp.au 1100 500000 10000  400 log2.txt */
/* ./streamerd 4445 1100 100 1 log.txt */

int main(int argc, char** argv) {
    signal(SIGALRM, flush_buffer);

    if(argc != 9){
        printf("usage: playaudio tcp-ip tcp-port audiofile payload-size gamma buf-size target-buf logfile2\n");
        exit(1);
    }

    audio_device = fopen("./test.txt", "w");
    if (audio_device == NULL) {
        fprintf(stderr, "Audio device failed to open\n");
        exit(1);
    }
    payload_size = atoi(argv[4]);
    int gamma = atoi(argv[5]);
    buf_size = atoi(argv[6]);
    recbuf = malloc(buf_size * sizeof(char));
    int target_buf = atoi(argv[7]);

#ifdef DEBUG
    printf("gamma:\t %0.3f us (%d pps)\n", (1.0/gamma) * 1000000, gamma);
    printf("bufsize:\t %0.2f kb \n", buf_size / 1000.0);
    printf("targetbuf:\t %d\n", target_buf);
#endif

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
    completed = 0;
    ualarm((1.0 / gamma) * 1000000, (1.0 / gamma) * 1000000);

    while (1) {
#ifdef DEBUG
        printf("Waiting to receive\n");
#endif
        bytes_read = recvfrom(child_sock, recv_content, payload_size+4, 0, (struct sockaddr *) &src, &src_len);
        int actual_data_size = bytes_read - 4;

#ifdef DEBUG
        printf("Received %d bytes\n", bytes_read);
#endif

        if(bytes_read == 1 && recv_content[0] == '5'){
            break;
        }

        memcpy(&seq_num, recv_content, 4);
        if(seq_num > buf_size){
            fprintf(stderr, "Error with seq_num, dropping package\n");
            continue;
        }

        // Populate buffer if it's not full
        if(bytes_occupied + actual_data_size <= buf_size){
            /* memcpy(&recbuf[seq_num*payload_size- bytes_flushed], &recv_content[4], actual_data_size); */
            memcpy(&recbuf[writeptr], &recv_content[4], actual_data_size);
            writeptr += actual_data_size;
            bytes_occupied += actual_data_size;
        }
        else
            fprintf(stderr, "Buffer is full, the last package was dropped\n");
    }

    while (!completed) {
        sleep(1);
    }

    fclose(audio_device);
    free(recbuf);
    return 1;
}
