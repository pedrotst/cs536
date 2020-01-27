#ifndef __UTILS_H_
#define __UTILS_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#pragma pack(1) // No padding
typedef struct request_s {
  /* uint8_t sig; */
  char server_ip[16];
  uint32_t server_port;
} request_t;

#pragma pack(1) // No padding
typedef struct answer_s {
  uint8_t sig;
  /* char server_ip[16]; */
  uint32_t transit_port;
} answer_t;

typedef struct reg_tunnels_s{
  int sock_i, transit_port_1, transit_port_2, cli_port, server_port;
  char cli_ip[16], server_ip[16];
} reg_tunnels_t;


void print_hex(const char *string)
{
        unsigned char *p = (unsigned char *) string;

        for (int i=0; i < strlen(string); ++i) {
                if (! (i % 16) && i)
                        printf("\n");

                printf("0x%02x ", p[i]);
        }
        printf("\n\n");
}

#endif // __UTILS_H_
