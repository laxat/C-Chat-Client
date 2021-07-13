#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "socket.h"
#include "chat_helpers.h"

int write_buf_to_client(struct client_sock *c, char *buf, int len) {
    // To be completed. 
    if(buf[len] != '\0')
    {
        return 1;
    } 
    buf[len] = '\r';
    buf[len + 1] = '\n'; 

    return write_to_socket(c->sock_fd, buf, len+2);
}

int remove_client(struct client_sock **curr, struct client_sock **clients) {
    struct client_sock *temp = *clients; 
    struct client_sock *prev = NULL; 
    
    if (temp != NULL && temp == *curr){
        temp = *clients; 
        *clients = temp->next; 
        *curr = *clients; 
        free(temp->username); 
        free(temp);
        return 0; 
    }
    while(temp != NULL && temp != *curr){
        prev = temp; 
        temp = temp->next; 
    }
    if(temp  == NULL){
        free(temp); 
        return 1; // Couldn't find the client in the list, or empty list
    }
    prev->next = temp->next;
    *curr = prev->next; 
    free(temp->username); 
    free(temp); 
    return 0; 
}

int read_from_client(struct client_sock *curr) {
    return read_from_socket(curr->sock_fd, curr->buf, &(curr->inbuf));
}

int set_username(struct client_sock *curr) {
    char *msg; 
    curr->username = malloc(MAX_NAME * sizeof(char)); 
    
    if(get_message(&msg, curr->buf, &(curr->inbuf)) != 1){
        strncpy(curr->username, msg + 1, MAX_NAME); 
        free(msg); 
        return 0; 
    }
    free(msg); 
    return 1; 
}
 