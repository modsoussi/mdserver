/* Initial attempt at implementing a simple http server. 
 * (c) 2016. modsoussi. 
 */

#ifndef MDSERVER_H
#define MDSERVER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

/* MACROS */

#define MAX_PENDING 10
#define MAX_LINE 8192
#define SERVER "mdserver/1.0"
#define SUCCESS 0
#define FAILURE -1
#define MAX_PATH_LENGTH 128
#define MAXCON 1000
#define TIMEOUT 10

/* STRUCTS & typedefs */

typedef struct {
    char method[8]; 
    char path[256]; /* will probably make this a 256 length limit */
    char http_version[12];
    char host[32];
    int keep_alive;
} req_hdrs_t;

/* METHODS */

extern int req_headers(char *buf, req_hdrs_t *hdrs);
extern void gen_response_header(char *resp, req_hdrs_t *req_hdrs);
extern void req_handle(int socket, req_hdrs_t *req_hdrs);
extern void usage();

#endif
