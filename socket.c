#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>     /* inet_ntoa */
#include <netdb.h>         /* gethostname */
#include <netinet/in.h>    /* struct sockaddr_in */

#include "socket.h"

void setup_server_socket(struct listen_sock *s) {
    if(!(s->addr = malloc(sizeof(struct sockaddr_in)))) {
        perror("malloc");
        exit(1);
    }
    // Allow sockets across machines.
    s->addr->sin_family = AF_INET;
    // The port the process will listen on.
    s->addr->sin_port = htons(SERVER_PORT);
    // Clear this field; sin_zero is used for padding for the struct.
    memset(&(s->addr->sin_zero), 0, 8);
    // Listen on all network interfaces.
    s->addr->sin_addr.s_addr = INADDR_ANY;

    s->sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s->sock_fd < 0) {
        perror("server socket");
        exit(1);
    }

    // Make sure we can reuse the port immediately after the
    // server terminates. Avoids the "address in use" error
    int on = 1;
    int status = setsockopt(s->sock_fd, SOL_SOCKET, SO_REUSEADDR,
        (const char *) &on, sizeof(on));
    if (status < 0) {
        perror("setsockopt");
        exit(1);
    }

    // Bind the selected port to the socket.
    if (bind(s->sock_fd, (struct sockaddr *)s->addr, sizeof(*(s->addr))) < 0) {
        perror("server: bind");
        close(s->sock_fd);
        exit(1);
    }

    // Announce willingness to accept connections on this socket.
    if (listen(s->sock_fd, MAX_BACKLOG) < 0) {
        perror("server: listen");
        close(s->sock_fd);
        exit(1);
    }
}


int find_network_newline(const char *buf, int inbuf) {
    for(int i = 0; i < inbuf-1; i++){
        if(buf[i] == '\r' && buf[i + 1] == '\n'){
            return i + 2; 
        }
    }
    return -1;
}
int read_from_socket(int sock_fd, char *buf, int *inbuf) {
    
    char nbuf[BUF_SIZE] = {0};
    char attach[BUF_SIZE]; 
    int var_size = BUF_SIZE-(*inbuf); 
    int bytes = read(sock_fd, nbuf, var_size); 
    if(bytes == -1)
    {
        if (bytes > var_size){ 
            return -1;
        }  
    }
    if(bytes == 0)
    {
        return 1; 
    }
    int tot_size = (*inbuf) + bytes;
    memcpy(attach, buf, BUF_SIZE);
    memcpy(attach + (*inbuf), nbuf, bytes);

    if(find_network_newline(attach, tot_size) == -1)
    {
        if(tot_size < BUF_SIZE)
        {
            strncpy(buf + (*inbuf), nbuf, bytes); 
            *inbuf += bytes; 
            return 2;   
        }
        else
        {
            return -1; 
        }
    }
    memcpy(buf + *inbuf, nbuf, bytes); 
    *inbuf += bytes; 
    return 0; 
}

int get_message(char **dst, char *src, int *inbuf) {

    int index = find_network_newline(src, *inbuf);
    if (index != -1)
    {
        *dst = malloc(sizeof(char)*(index-1)); 

        src[index-2] = '\0'; 
        strncpy(*dst, src, index-1); 
        
        int new_buf = *inbuf; 
        *inbuf = *inbuf - index;
        memmove(src, src + index, *inbuf);
        memset(src+*inbuf, '\0', new_buf - *inbuf);  
        return 0; 
    }
    return 1;
}



int write_to_socket(int sock_fd, char *buf, int len) {
    int num_write;
    num_write = write(sock_fd, buf, len); 
    if(num_write == -1){
        perror("write"); 
        return 1;     
    }
    if(num_write == 0)
    {
        return 2;
    }
    return 0; 
    
}
 