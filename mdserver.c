/* A webserver. Project 1: Distributed S16. 
 * (c) 2016. modsoussi
 *
 *  Comments:
 *		+ Program assumes existence of files called notfound.html, nohostheader.html, malformed.html, forbidden.html
 * 		  in working directory. These get sent in case of 404 Not Found, 400 Bad Request, and 403 Forbidden.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mdserver.h"
#include <sys/uio.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

static char DEFAULT_PATH[MAX_PATH_LENGTH]; /* working directory */

/* parsing HTTP requests */
int req_headers(char *buf, req_hdrs_t *hdrs){
	char *s = buf;
    size_t method_len, path_len;
    
    /* zeroing out hdrs */
    memset(hdrs, 0, sizeof(req_hdrs_t));
    
    /* getting request method */
    char *tmp = index(s,(int)' ');
    method_len = (size_t)((long)tmp-(long)s);
    strncpy(hdrs->method, s, method_len);
    s = tmp + 1;
    
    /* getting path of requested resource */
    char p[256];
    memset(p, 0, 256);
    strcpy(p, DEFAULT_PATH);
    tmp = index(s,(int)' ');
    if(((long)tmp - (long)s) == 1){/* if path received is / */
        strcat(p, "index.html");
        strcpy(hdrs->path, p);
    } else {
        path_len = (size_t)((long)tmp-(long)s);
        strncat(p, s, path_len);
        strcpy(hdrs->path, p);
    }
    /* if directory, ie path ends in /, append index.html */
    path_len = strlen(hdrs->path);
    if(hdrs->path[path_len-1] == '/'){
      strcat(hdrs->path,"index.html");
    }
    s = tmp + 1;
    
    /* getting http version */
    tmp = strstr(s,"HTTP/");
    if(tmp){
        strncpy(hdrs->http_version, s, (size_t)8);
    }
    
    /* getting host */
    tmp = strstr(s, "Host: ");
    if(tmp){
        s = tmp + strlen("Host: ");
        tmp = index(s, (int)'\r');
        long hostlen = (long)tmp - (long)s;
        strncpy(hdrs->host, s, hostlen);
        s = tmp+1;
    }
    
    
    /* getting connection status: if keep-alive 1, else 0 */
    tmp = strstr(s, "Connection:");
    if(tmp){
        if(strstr(hdrs->http_version, "1.1") && strstr(s, "keep-alive")){
            hdrs->keep_alive = 1;
        } else {
            hdrs->keep_alive = 0;
        }
    }
    
    return SUCCESS;
}

/* generates a response based on given request path */
void gen_response_header(char *resp, req_hdrs_t *req_hdrs){
	int fd;
	
    /* checking for bad requests: 1: HTTP/1.1 without host header */
    if((strstr(req_hdrs->http_version, "1.1")) && (strlen(req_hdrs->host) == 0)){
        strcpy(req_hdrs->path,"./nohostheader.html");
        sprintf(resp, "HTTP/1.1 400 Bad Request\r\nServer: %s\r\nContent-Type: text/html\r\nConnection: close\r\n", SERVER);
        return;
    }
	
	/* checking for other malformed requests */
    if((!(strstr(req_hdrs->http_version, "HTTP/1.0") || strstr(req_hdrs->http_version, "HTTP/1.1")))){
      strcpy(req_hdrs->path, "./malformed.html");
      sprintf(resp, "HTTP/1.1 400 Bad Request\r\nServer: %s\r\nContent-Type: text/html\r\nConnection: close\r\n", SERVER);
      return;
    }
    
    /* checking for forbidden access */
    if((strstr(req_hdrs->path, "../"))){
      strcpy(req_hdrs->path, "./forbidden.html");
      sprintf(resp, "HTTP/1.1 403 Forbidden\r\nServer: %s\r\nContent-Type: text/html\r\nConnection: close\r\n", SERVER);
      return;
    }
    
    printf("mdserver: requested resource: %s\n", req_hdrs->path);
    
    /* attempting to open file at path for read */
    if((fd = open(req_hdrs->path, O_RDONLY, 0)) < 0) {
        fprintf(stderr, "mdserver-error: Failed to open requested resource : %s : %s\n", req_hdrs->path, strerror(errno));
        strcpy(req_hdrs->path, "./notfound.html");
        sprintf(resp, "HTTP/1.1 404 NOT FOUND\r\nServer: %s\r\nContent-Type: text/html\r\nConnection: close\r\n", SERVER);
	return;
    }
    close(fd);

    
    /* content type */
    char type[15];
    if((strstr(req_hdrs->path, ".jpg"))){
      strcpy(type, "image/jpeg");
    } else if((strstr(req_hdrs->path, ".gif"))){
      strcpy(type, "image/gif");
    } else if((strstr(req_hdrs->path, ".png"))){
      strcpy(type, "image/png");
    } else if((strstr(req_hdrs->path, ".html"))){
      strcpy(type, "text/html");
    } else {
      strcpy(type, "text/plain");
    }
    
    /* date header */
    char date[32];
    time_t t;
    struct tm *tgmt = calloc(1, sizeof(struct tm));
    if((time(&t)) < 0){
      perror("mdserver-error: time.h");
      exit(FAILURE);
    }
    if(!(tgmt = gmtime(&t))){
      perror("mdserver-error: gmtime");
      exit(FAILURE);
    }
    strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", tgmt);
    
    /* connection status  */
    char c[16];
    if(req_hdrs->keep_alive){
      strcpy(c, "keep-alive");
    } else {
      strcpy(c, "close");
    }
    
    sprintf(resp, "HTTP/1.1 200 OK\r\nServer: %s\r\nContent-Type: %s\r\nDate: %s\r\nConnection: %s\r\n",
                    SERVER, type, date, c);
}

/* handling request. */
void req_handle(int sd, req_hdrs_t *req_hdrs){
	char resp[MAX_LINE];
	memset(resp, 0, MAX_LINE); /* zero out response */
	gen_response_header(resp, req_hdrs);
	FILE *f;
	if(!(f = fopen(req_hdrs->path, "r"))){
		perror("mdserver-error: couldn't open requested resource");
		fclose(f);
		exit(FAILURE);
	}
  
	/* getting file size */
	long fsize;
	if((fseek(f, 0, SEEK_END)) < 0){
		perror("mdserver-error: fseek");
		exit(FAILURE);
	} else {
		fsize = ftell(f);
	}
  
	/* adding file size to headers, and sending headers to client */
	char fsizeh[32];
	sprintf(fsizeh, "Content-Length: %ld\r\n\n", fsize);
	strcat(resp, fsizeh);
	if((send(sd, resp, (size_t)strlen(resp), 0)) < 0){
		perror("mdserver-error: send");
		close(sd);
		exit(FAILURE);
	}
	printf("mdserver: Response Sent: \n%s\n", resp);
  
	if(strcmp(req_hdrs->method, "GET") == 0){ /* only send file if GET */
		fputs("mdserver: Sending file ... \n", stdout);
    
		/* sending file requested */
		if((fseek(f, 0, SEEK_SET)) < 0){
			perror("mdserver-error: fseek");
			fclose(f);
			exit(FAILURE);
		}
		bzero(resp, (size_t)MAX_LINE);
		ssize_t bytes_sent = 0;
		while(!feof(f)){
			fread(resp, 1, (size_t)MAX_LINE, f);
			if((bytes_sent += write(sd, resp, (bytes_sent + MAX_LINE) > fsize ? fsize % MAX_LINE : MAX_LINE)) < 0){
				perror("mdserver-error: writing to socket");
				close(sd);
				exit(FAILURE);
			}
			memset(resp, 0 , (size_t)MAX_LINE);
		}
		fclose(f);
	}
}

/* Prints usage to terminal */
void usage(){
	fputs("mdserver-usage: ./mdserver [-port] port_number [-document_root] path_to_document_root.\n", stdout);
}

/* main method */
int main(int argc, char **argv){
	int sin, sout; /* sockets */
	u_short port_num; /* port number */
	int len; /* length of message received */
	char buf[MAX_LINE]; /* buffer */
	struct sockaddr_in sin_addr; /* s_in is server's address, s_out is client's address */
	int opts; /* getopt_long helper */
	int *numcon;

	/* shared memory declarations */
	const char *shmname = "NUMCON";
	int shmfd; /* shared memory file descriptor */
	void *shmptr; /* pointer to shared memory object */

	/* shared memory creation */
	if((shmfd = shm_open(shmname, O_RDWR | O_CREAT, 0666)) < 0){
		perror("mdserver-error: could not open shared memory");
		exit(FAILURE);
	}
	ftruncate(shmfd, sizeof(int));
	shmptr = mmap(0, sizeof(int), PROT_WRITE | PROT_WRITE, MAP_SHARED, shmfd, 0);
	numcon = shmptr;
	*numcon = 0; /* initializing number of connections to 0 */
  
	memset(DEFAULT_PATH, 0, MAX_PATH_LENGTH); /* zeroing out DEFAULT_PATH buffer */
  
	/* checking the program was called with the right options */
	if(argc < 5){
		usage();
		exit(FAILURE);
	}
  
	/* options descriptor */
	static struct option longopts[] = {
		{"port", required_argument, NULL, 'p'},
		{"document_root", required_argument, NULL, 'd'},
		{0              , 0                , 0   ,  0 }
	};
  
	/* parsing argv options */
	while((opts = getopt_long_only(argc, argv, "", longopts, NULL)) != -1){
		switch(opts){
		case 'p':
			port_num = (u_short)atoi(optarg);
			break;
		case 'd':
			strcpy(DEFAULT_PATH, optarg);
			strcat(DEFAULT_PATH, "/");
			break;
		default:
			usage();
			exit(FAILURE); 
		}
	}
  
	/* initializing address */
	sin_addr.sin_family = AF_INET;
	sin_addr.sin_port = htons(port_num);
	sin_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  
	/* Creating socket */
	if((sin = socket(PF_INET, SOCK_STREAM, 0)) < 0){
		perror("mdserver-error: Failed to create socket");
		exit(FAILURE);
	}
  
	/* Socket was successfully created. Bind it. */
	if((bind(sin, (struct sockaddr *)&sin_addr, sizeof(struct sockaddr_in))) < 0){
		perror("mdserver-error: Failed to bind socket to address");
		exit(FAILURE);
	}
  
	if((listen(sin, MAX_PENDING)) < 0){ 
		perror("mdserver-error: socket unable to listen:");
		exit(FAILURE);
	}
	printf("mdserver: Listening on port %hu\n", port_num); /* socket now listening for connections */
  
	/* main loop */
	while(1){
		socklen_t sout_len;
		struct in_addr in;
		if((sout = accept(sin, (struct sockaddr *)&sin_addr, &sout_len)) < 0){
			perror("mdserver-error: Failed to accept");
			exit(FAILURE);
		}
		if(*numcon < MAXCON){ /* making sure server is not flooded */
			(*numcon)++;
			in = sin_addr.sin_addr;
			printf("mdserver: Accepted connection from %s\n", inet_ntoa(in));
			printf("mdserver: current number of connections is %d\n", *numcon); 
		} else { /* if maxconn is reached, send 503 and close connection */
			memset(buf, 0, sizeof(buf));
			sprintf(buf, "HTTP/1.1 503 Service Unavailable\r\nServer: %s\r\n\n", SERVER);
			write(sout, buf, strlen(buf)); 
			close(sout);
			(*numcon)--;
			continue;
		}    
    
		/* setting timeout on socket */
		struct timeval timeout;
		timeout.tv_sec = TIMEOUT/(*numcon);
		timeout.tv_usec = 0;
		if(setsockopt(sout, SOL_SOCKET, SO_RCVTIMEO,(char *)&timeout, sizeof(timeout)) < 0){
			perror("mdserver-error: setsockopt failed");
			exit(FAILURE);
		}
    
		/* Spawning child process to handle request */
		pid_t pid;
    
		if((pid = fork()) < 0){
			perror("mdserver-error: couldn't spawn process");
			exit(FAILURE);
		}
    
		if(pid == 0){ /* child process work */   
      
			req_hdrs_t hdrs;
      
			memset(buf, 0, sizeof(buf));
			/* receiving messages */
			while((len = recv(sout, buf, sizeof(buf), 0)) > 0){
				printf("\nRequest received:\n%s\n", buf);
				req_headers(buf, &hdrs);
				printf("request method: %s\n", hdrs.method);
        
				if(strstr(hdrs.method, "GET") || strstr(hdrs.method, "HEAD")){
					req_handle(sout, &hdrs);
				} else { /* print error */
					fputs("mdserver-error: Request method neither GET nor HEAD. Closing connection.\n", stdout);
					exit(FAILURE);
				}
				if(hdrs.keep_alive == 0){
					break; /* if connection header says close, break out of loop, close connection and exit child process */
				}
			}
      
			/* if out of loop, close connection and exit child process */
			close(sout);
			(*numcon)--;
			printf("mdserver: closed connection from %s\n", inet_ntoa(in));
			printf("mdserver: number of connections now %d\n", *numcon);
			exit(SUCCESS);
		} else { /* main process work */
			close(sout); /* closing main process's copy of sout */
			continue;
		}
	}
  
	shm_unlink(shmname);
	return 0;
}
