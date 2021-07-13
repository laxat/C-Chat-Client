#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <stdbool.h>

#include "socket.h"
#include "chat_helpers.h"

int sigint_received = 0;

void sigint_handler(int code) {
    sigint_received = 1;
}

/*
 * Wait for and accept a new connection.
 * Return -1 if the accept call failed.
 */
int accept_connection(int fd, struct client_sock **clients) {
    struct sockaddr_in peer;
    unsigned int peer_len = sizeof(peer);
    peer.sin_family = AF_INET;

    int num_clients = 0;
    struct client_sock *curr = *clients;
    while (curr != NULL && num_clients < MAX_CONNECTIONS && curr->next != NULL) {
        curr = curr->next;
        num_clients++;
    }

    int client_fd = accept(fd, (struct sockaddr *)&peer, &peer_len);
    
    if (client_fd < 0) {
        perror("server: accept");
        close(fd);
        exit(1);
    }

    if (num_clients == MAX_CONNECTIONS) {
        close(client_fd);
        return -1;
    }

    struct client_sock *newclient = malloc(sizeof(struct client_sock));
    newclient->sock_fd = client_fd;
    newclient->inbuf = newclient->state = 0;
    newclient->username = NULL;
    newclient->next = NULL;
    memset(newclient->buf, 0, BUF_SIZE);
    if (*clients == NULL) {
        *clients = newclient;
    }
    else {
        curr->next = newclient;
    }

    return client_fd;
}

/*
 * Close all sockets, free memory, and exit with specified exit status.
 */
void clean_exit(struct listen_sock s, struct client_sock *clients, int exit_status) {
    struct client_sock *tmp;
    while (clients) {
        tmp = clients;
        close(tmp->sock_fd);
        clients = clients->next;
        free(tmp->username);
        free(tmp);
    }
    close(s.sock_fd);
    free(s.addr);
    exit(exit_status);
}

int main(void) {
    setbuf(stdout, NULL);
    
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        exit(1);
    }

    // Linked list of clients
    struct client_sock *clients = NULL;
    
    struct listen_sock s;
    setup_server_socket(&s);
    
    // Set up SIGINT handler
    struct sigaction sa_sigint;
    memset (&sa_sigint, 0, sizeof (sa_sigint));
    sa_sigint.sa_handler = sigint_handler;
    sa_sigint.sa_flags = 0;
    sigemptyset(&sa_sigint.sa_mask);
    sigaction(SIGINT, &sa_sigint, NULL);
    
    int exit_status = 0;
    
    int max_fd = s.sock_fd;

    fd_set all_fds, listen_fds;
    
    FD_ZERO(&all_fds);
    FD_SET(s.sock_fd, &all_fds);

    do {
        listen_fds = all_fds;
        int nready = select(max_fd + 1, &listen_fds, NULL, NULL, NULL);
        if (sigint_received) break;
        if (nready == -1) {
            if (errno == EINTR) continue;
            perror("server: select");
            exit_status = 1;
            break;
        }

        /* 
         * If a new client is connecting, create new
         * client_sock struct and add to clients linked list.
         */
        if (FD_ISSET(s.sock_fd, &listen_fds)) {
            int client_fd = accept_connection(s.sock_fd, &clients);
            if (client_fd < 0) {
                printf("Failed to accept incoming connection.\n");
                continue;
            }
            if (client_fd > max_fd) {
                max_fd = client_fd;
            }
            FD_SET(client_fd, &all_fds);
            printf("Accepted connection\n");
        }

        if (sigint_received) break;

        /*
         * Accept incoming messages from clients,
         * and send to all other connected clients.
         */
        struct client_sock *curr = clients;
        while (curr) {
            if (!FD_ISSET(curr->sock_fd, &listen_fds)) {
                curr = curr->next;
                continue;
            } 
            int client_closed = read_from_client(curr);
            
            // If error encountered when receiving data
            if (client_closed == -1) {
                perror("read");
                client_closed = 1; // Disconnect the client
            }
            
            // If received at least one complete message
            // and client is newly connected: Get username
            if (client_closed == 0 && curr->username == NULL) {
                if (set_username(curr)) { 
                    printf("Error processing user name from client %d.\n", curr->sock_fd);
                    client_closed = 1; // Disconnect the client
                }
                else {
                    if(strchr(curr->username, ' ') != NULL){
                        client_closed = 1;  
                        char buf[BUF_SIZE] = {0}; 
                        strncpy(buf, "1SERVER Username invalid or already taken.", 42); 
                        int data_len = 42; 
                        write_buf_to_client(curr, buf, data_len); 
                    }
                    if(strcmp(curr->username, "SERVER") == 0){
                        client_closed = 1;  
                        char buf[BUF_SIZE] = {0};
                        strncpy(buf, "1SERVER Username invalid or already taken.", 42); 
                        int data_len = 42; 
                        write_buf_to_client(curr, buf, data_len); 
                   }
                    if(strlen(curr->username) > MAX_NAME){
                        client_closed = 1;  
                        char buf[BUF_SIZE] = {0};
                        strncpy(buf, "1SERVER Username invalid or already taken.", 42); 
                        int data_len = 42; 
                        write_buf_to_client(curr, buf, data_len); 
                    }
                    struct client_sock *temp = clients; 
                    while(temp != NULL){
                        
                    if(temp != curr && temp->username != NULL && strcmp(temp->username, curr->username) == 0){
                        client_closed = 1;  
                        char buf[BUF_SIZE] = {0};
                        strncpy(buf, "1SERVER Username invalid or already taken.", 42); 
                        int data_len = 42; 
                        write_buf_to_client(curr, buf, data_len); 
                    }
                    temp = temp->next; 

                    }
                    printf("Client %d user name is %s.\n", curr->sock_fd, curr->username);
                }
            }
                
            char *msg; 
            
            // Loop through buffer to get complete message(s)
            while (client_closed == 0 && !get_message(&msg, curr->buf, &(curr->inbuf))) {
                printf("Echoing message from %s.\n", curr->username);
                struct client_sock *admin; 
                bool user_dead = false; 
                bool check_del = false; 
                char write_buf[BUF_SIZE];
                char name[MAX_NAME] = {0};  
                write_buf[0] = '\0';
                char ic = msg[0];

                if(ic == '0'){ 
                    check_del = true; 
                    if(curr == clients){
                        admin = curr;
                        strncat(name, msg + 1, MAX_NAME);
                        strtok(name, "\n");
                    }
                } 

                strncat(write_buf, msg, 1); 
                strncat(write_buf, curr->username, MAX_NAME);
                strncat(write_buf, " ", MAX_NAME);
                if(ic == '0' || ic == '1'){
                    if((strlen(msg) - 1) <= MAX_USER_MSG){
                        strncat(write_buf, msg + 1, MAX_USER_MSG);
                    }
                    else{
                        printf("Message is too big\n");
                        client_closed = 1; 
                        break; 
                    }
                }
                else if(ic == '2'){
                    if((strlen(msg) -1) <= MAX_IMG_LEN){
                        strncat(write_buf, msg + 1, MAX_IMG_LEN);
                    }else{
                        printf("Image is too big\n");
                        client_closed = 1; 
                        break; 
                    }
                }
                free(msg);
                int data_len = strlen(write_buf);                
                struct client_sock *dest_c = clients;
                while (dest_c) {
                     if(check_del == true && curr == admin){
                        printf("Looking for: %s\n", name); 
                        struct client_sock *temp = clients; 
                        while(temp != NULL && strcmp(temp->username, name) != 0){
                            temp = temp->next;
                        } 
                        if(temp != NULL){
                                curr = temp; 
                                client_closed = 1; 
                                printf("User %s (%d) disconnected.\n", temp->username, temp->sock_fd);
                                printf("Admin %s kicked User %s from the server\n", admin->username, temp->username); 
                                user_dead = true;
                            }
                                
                        if(temp == NULL){
                            printf("Invalid Name\n"); 
                        }  
                    } 
                    else{
                        if (dest_c != curr && user_dead == false && check_del == false) {
                                int ret = write_buf_to_client(dest_c, write_buf, data_len);
                                if (ret == 0) {
                                    printf("Sent message from %s (%d) to %s (%d).\n",
                                        curr->username, curr->sock_fd,
                                        dest_c->username, dest_c->sock_fd);
                                }
                                else {
                                    printf("Failed to send message to user %s (%d).\n", dest_c->username, dest_c->sock_fd);
                                    if (ret == 2) {
                                        printf("User %s (%d) disconnected.\n", dest_c->username, dest_c->sock_fd);
                                        assert(remove_client(&dest_c, &clients) == 0); // If this fails we have a bug
                                        continue;
                                }
                            }
                        }
                    }

                if(check_del && dest_c != curr && curr != admin && user_dead == false){
                    printf("User %s does not have the rights to kick\n", curr->username); 
                }
                dest_c = dest_c->next;
            }
        }

            if (client_closed == 1) { // Client disconnected
                // Note: Never reduces max_fd when client disconnects
                FD_CLR(curr->sock_fd, &all_fds);
                close(curr->sock_fd);
                printf("Client %d disconnected\n", curr->sock_fd);
                assert(remove_client(&curr, &clients) == 0); // If this fails we have a bug
            }
            else {
                curr = curr->next;
                }
        }
    } while(!sigint_received);
    
    clean_exit(s, clients, exit_status);
}
