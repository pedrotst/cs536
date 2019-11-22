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
#include <semaphore.h> 
#include <alsa/asoundlib.h>
#include <pthread.h>

#define mulawwrite(x) snd_pcm_writei(mulawdev, x, mulawfrms)


static snd_pcm_t *mulawdev;
static snd_pcm_uframes_t mulawfrms;

char *recbuf;
int payload_size;
int buf_size;
int max_buf_packet_num;
int bytes_flushed = 0;
int file_size = 0;
int buf_writeptr = 0;
int buf_readptr = 0;
int buf_occupied = 0;
FILE * audio_device;
FILE * logfile_fp;
int completed;
sem_t mutex; 

int total_bytes_written = 0;
int total_bytes_read = 0;

void *writelogfile(void* argvp){
    static int counter = 0;

    while(1){
        sleep(1);
        fprintf(logfile_fp, "%d %d\n", counter, buf_occupied);
        fflush(logfile_fp);
        counter++;
    }

    return argvp;
}

void mulawopen(size_t *bufsiz) {
    snd_pcm_hw_params_t *p;
    unsigned int rate = 8000;

    snd_pcm_open(&mulawdev, "default", SND_PCM_STREAM_PLAYBACK, 0);
    snd_pcm_hw_params_alloca(&p);
    snd_pcm_hw_params_any(mulawdev, p);
    snd_pcm_hw_params_set_access(mulawdev, p, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(mulawdev, p, SND_PCM_FORMAT_MU_LAW);
    snd_pcm_hw_params_set_channels(mulawdev, p, 1);
    snd_pcm_hw_params_set_rate_near(mulawdev, p, &rate, 0);
    snd_pcm_hw_params(mulawdev, p);
    snd_pcm_hw_params_get_period_size(p, &mulawfrms, 0);
    *bufsiz = (size_t)mulawfrms;
    return;
}

void mulawclose(void) {
    snd_pcm_drain(mulawdev);
    snd_pcm_close(mulawdev);
}

void flush_buffer(int sig) {

    // int buf_used = buf_writeptr - buf_readptr;
    // Don't flush if there is nothing to flush
    if (buf_occupied < 4096) {
        if (completed) {
            exit(0);
        }
        printf("less than 4096\n");
        return;
    }

    int sem_val;
    sem_getvalue(&mutex, &sem_val);
    if (sem_val != 1) {
        return;
    }

    // Don't write more than we have on buffer
    //int writesize = (buf_occupied > payload_size) ? payload_size : buf_occupied;
    //fwrite(&recbuf[buf_readptr * payload_size], sizeof(char), writesize, audio_device);

    // if (audio_device_buffer_ptr + writesize > 4096) {
    //     writesize = 4096 - audio_device_buffer_ptr;
    // }

    // if (buf_readptr + writesize > buf_size) {
    //     memcpy(&audio_device_buffer[audio_device_buffer_ptr], &recbuf[buf_readptr],buf_size - buf_readptr);
    //     audio_device_buffer_ptr += buf_size - buf_readptr;
    //     memcpy(&audio_device_buffer[audio_device_buffer_ptr], recbuf,writesize-(buf_size - buf_readptr));
    //     audio_device_buffer_ptr += writesize-(buf_size - buf_readptr);
    //     buf_readptr = writesize-(buf_size - buf_readptr);
    // } else {
    //     memcpy(&audio_device_buffer[audio_device_buffer_ptr], &recbuf[buf_readptr],writesize);
    //     buf_readptr+=writesize;
    //     if (buf_readptr >= buf_size) {
    //         buf_readptr = 0;
    //     }
    //     audio_device_buffer_ptr += writesize;
    // }


    char to_be_sent[4096];
    if (buf_readptr + 4096 > buf_size) {
        memcpy(to_be_sent, &recbuf[buf_readptr], buf_size - buf_readptr);
        buf_readptr = 0;
        memcpy(&to_be_sent[buf_size - buf_readptr], &recbuf[buf_readptr], 4096-(buf_size - buf_readptr));
        buf_readptr += 4096-(buf_size - buf_readptr);
    } else {
        memcpy(to_be_sent, &recbuf[buf_readptr], 4096);
        buf_readptr+=4096;
        if (buf_readptr >= buf_size) {
            buf_readptr = 0;
        }
    }

    
    buf_occupied-=4096; 

    mulawwrite(to_be_sent);

    total_bytes_written+=4096;
    printf("Writing: %d\n", total_bytes_written);

    

    // if (audio_device_buffer_ptr >= 4096) {
    //     mulawwrite(audio_device_buffer);
    //     //fwrite(audio_device_buffer, 1, 4096, audio_device);
    //     audio_device_buffer_ptr = 0;

    //     total_bytes_written += 4096;
    //     printf("Writing: %d\n", total_bytes_written);
    // }

    // For efficiency, let's memcpy only when
    // I can make enough space for another packet

    /* 
    if(buf_readptr < payload_size)
        return;

    char * buf_cpy = calloc(buf_size, sizeof(char));
    memcpy(buf_cpy, &recbuf[buf_readptr], buf_writeptr);

    bytes_flushed += writesize;
    buf_writeptr -= writesize;
    buf_readptr = 0;

    free(recbuf);
    recbuf = buf_cpy; */
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
    size_t bufsiz;
    mulawopen(&bufsiz);


    sem_init(&mutex, 0, 1); 
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

    max_buf_packet_num = buf_size / payload_size;

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

    logfile_fp = fopen(argv[8], "w");
    if(logfile_fp == NULL){
        fprintf(stderr, "Could not open %s, exiting\n", argv[8]);
        exit(1);
    }

    pthread_t ptid;
    pthread_create(&ptid, NULL, writelogfile, (void*)&ptid);

    double inverse_gamma = 1.0 / gamma;
    ualarm((useconds_t) (inverse_gamma * 1000000), (useconds_t) (inverse_gamma * 1000000));

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
            completed = 1;
            printf("Finished to download the data\n");
            break;
        }

        memcpy(&seq_num, recv_content, 4);
        if(seq_num > buf_size){
            fprintf(stderr, "Error with seq_num, dropping package\n");
            continue;
        }

        // Populate buffer if it's not full
        if(buf_occupied + actual_data_size < buf_size){
            /* memcpy(&recbuf[seq_num*payload_size- bytes_flushed], &recv_content[4], actual_data_size); */
            sem_wait(&mutex);
            if (buf_writeptr + actual_data_size > buf_size) {
                memcpy(&recbuf[buf_writeptr], &recv_content[4], buf_size - buf_writeptr);
                memcpy(recbuf, &recv_content[4+buf_size - buf_writeptr], actual_data_size-(buf_size - buf_writeptr));
                buf_writeptr = actual_data_size-(buf_size - buf_writeptr);
            }  else {
                memcpy(&recbuf[buf_writeptr], &recv_content[4], actual_data_size);
                buf_writeptr += actual_data_size;
                if (buf_writeptr >= buf_size) {
                    buf_writeptr = 0;
                }
            }
            
            sem_post(&mutex); 

            char feedback[12];
            memcpy(feedback, &buf_occupied, 4);
            memcpy(&feedback[4], &target_buf, 4);
            memcpy(&feedback[8], &gamma,4);
            sendto(child_sock, feedback,12,0, (struct sockaddr*) &server_udp, sizeof(server_udp));

            buf_occupied+= actual_data_size;

            total_bytes_read += actual_data_size;
            printf("Reading: %d\n", total_bytes_read);

        }
        else
            fprintf(stderr, "Buffer is full, the last package was dropped\n");
    }

    printf("Waiting to end streaming\n");
    fflush(stdout);
    // Communication ended but there may still be data waiting
    // to be streamed in the buffer.
    // Let's wait until the transfer is completed
    // and the buffer is zero
    while(1);
    mulawclose();
    // if (audio_device_buffer_ptr != 0) {
    //     fwrite(audio_device_buffer, 1, audio_device_buffer_ptr, audio_device);
    // }
    printf("We are done streaming, thanks for being a valuable costumer\n");
    pthread_cancel(ptid);
    fflush(stdout);
    fclose(audio_device);
    free(recbuf);
    return 1;
}
