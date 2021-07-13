#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "socket.h"

struct server_sock
{
    int sock_fd;
    char buf[BUF_SIZE];
    int inbuf;
};

int main(void)
{
    struct server_sock s;
    s.inbuf = 0;
    int exit_status = 0;

    // Create the socket FD.
    s.sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s.sock_fd < 0)
    {
        perror("client: socket");
        exit(1);
    }

    // Set the IP and port of the server to connect to.
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server.sin_addr) < 1)
    {
        perror("client: inet_pton");
        close(s.sock_fd);
        exit(1);
    }

    // Connect to the server.
    if (connect(s.sock_fd, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
        perror("client: connect");
        close(s.sock_fd);
        exit(1);
    }

    char *buf = NULL; // Buffer to read name from stdin
    int name_valid = 0;
    while (!name_valid)
    {
        printf("Please enter a username: ");
        fflush(stdout);
        size_t buf_len = 0;
        size_t name_len = getline(&buf, &buf_len, stdin);
        if (name_len < 0)
        {
            perror("getline");
            fprintf(stderr, "Error reading username.\n");
            free(buf);
            exit(1);
        }

        if (name_len - 1 > MAX_NAME)
        { // name_len includes '\n'
            printf("Username can be at most %d characters.\n", MAX_NAME);
        }
        else
        {

            char write_buf[MAX_NAME + 3];
            write_buf[0] = '\0';
            strncat(write_buf, "1", 1);
            strncat(write_buf, buf, MAX_NAME);
            write_buf[name_len] = '\r';
            write_buf[name_len + 1] = '\n';
            if (write_to_socket(s.sock_fd, write_buf, name_len + 2))
            {
                fprintf(stderr, "Error sending username.\n");
                free(buf);
                exit(1);
            }
            name_valid = 1;
            free(buf);
        }
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(s.sock_fd, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    int num_fd = s.sock_fd + 1;


    int mon = select(num_fd, &read_fds, NULL, NULL, NULL);
    if (mon == -1)
    {
        perror("select");
        exit(1);
    }
    
    while (1)
    {
        

        if(FD_ISSET(STDIN_FILENO, &read_fds))
        {
            char buf[MAX_USER_MSG + 2]; 
            
            while(fgets(buf, MAX_USER_MSG + 1, stdin) != NULL){
                bool is_done = false; 
                int index = strlen(buf) - 1;

                if(buf[0] == '.' && buf[1] == 'k' && buf[2] == ' ')
                {

                    if(buf[index] == '\n'){
                        is_done = true; 
                    }

                    if(buf[0] == '\n' && index == 0){
                        break; 
                    }

                    char write_buf[MAX_NAME + 3];
                    write_buf[0] = '\0';
                    strncat(write_buf, "0", 1);
                    strncat(write_buf, buf + 3, MAX_NAME);
                    write_buf[strlen(write_buf) + 1] = '\r';
                    write_buf[strlen(write_buf) + 2] = '\n';


                   if (write_to_socket(s.sock_fd, write_buf, (strlen(write_buf) + 3)) == 1)
                        {
                            fprintf(stderr, "Error writing to socket");
                            close(s.sock_fd);
                            exit(0);
                        }
                    
                    if (is_done == true)
                    { 
                        break;
                    }
                    
                }
                
                else if(buf[0] == '.' && buf[1] == 'e' && buf[2] == ' ')
                {
                    bool is_not_file = false; 
                    char file_name[MAX_IMG_LEN  + 3];
                    file_name[0] = '\0';
                    strncat(file_name, "emotes/", 7); 
                    strncat(file_name, buf+3, strlen(buf));
                    strtok(file_name, "\n"); 
                    strncat(file_name, ".jpg", 4); 

                    int fd[2], r; 
                        
                    char name[75536];

                    if ((pipe(fd)) == -1)
                        {
                            perror("pipe");
                            exit(1);
                        }
                        if ((r = fork()) == 0)
                        {  
                            int f_desc = open(file_name, O_RDONLY); 
                            if(f_desc != -1){
                            

                            if ((dup2(f_desc, fileno(stdin))) == -1)
                            {
                                perror("dup2");
                                exit(1);
                            }
                                
                            if ((dup2(fd[1], fileno(stdout))) == -1)
                            {
                                perror("dup2");
                                exit(1);
                            }

                            if ((close(fd[1])) == -1) {
                                perror("close");
                                exit(1); 
                            }

                            if ((close(fd[0])) == -1) {
                                perror("close");
                                exit(1); 
                            }

                            if((close(f_desc)) == -1){
                                perror("close"); 
                                exit(1); 
                            }
                        
                            execlp("base64", "base64", "-w 0", (char *)0); 
                            fprintf(stderr, "ERROR: exec should not return \n");
                        
                            }else{
                                is_not_file = true;
                                printf("Invalid Name\n"); 
                                break; 
                            }
                        }
                        else if(r > 0) 
                        {
                             
                            FILE *file;
                            file = fdopen(fd[0], "r"); 
                            

                            wait(NULL); 

                            if ((close(fd[1])) == -1) {
                                perror("close");
                            }
                            
                            fscanf(file, "%s", name); 

                            if(strlen(name) > MAX_IMG_LEN){
                                printf("Size of Image is too High\n"); 
                                is_not_file = true; 
                                fflush(stdout); 
                                break; 
                            }

                            if ((close(fd[0])) == -1) {
                                perror("close");
                            }

                        }
                        else
                        {
                            perror("fork");
                            exit(1);
                        }

                        if(!is_not_file){
                            char write_buf[MAX_IMG_LEN  + 2];
                            write_buf[0] = '\0';
                            strncat(write_buf, "2", 1);
                            strncat(write_buf, name, strlen(name));
                            write_buf[strlen(write_buf) + 1] = '\r';
                            write_buf[strlen(write_buf) + 2] = '\n';

                            fflush(stdout); 

                            if (write_to_socket(s.sock_fd, write_buf, (strlen(write_buf) + 3)) == 1)
                                {
                                    fprintf(stderr, "Error writing to socket"); 
                                    close(s.sock_fd);
                                    exit(0);
                        
                                }  
                                break;      
                        }
                }           

                else
                {

                    if(buf[index] == '\n'){
                        is_done = true; 
                    }

                    if(buf[0] == '\n' && index != 0){
                        break; 
                    }
                    
                    char write_buf[MAX_USER_MSG + 5] = {0};

                    strncpy(write_buf, "1", 1); 
                    strncat(write_buf, buf, (strlen(buf)-1));
                     
                    int num_read = strlen(write_buf); 

                    write_buf[num_read] = '\r';
                    write_buf[num_read + 1] = '\n'; 

                    
                    
                    if (write_to_socket(s.sock_fd, write_buf, num_read + 2) == 1)
                        { 
                            fprintf(stderr, "Error writing to socket");
                            close(s.sock_fd); 
                            exit(0);
                        }
                     if (is_done){
                         break; 
                     }  
                }
            }
        }
        
        FD_ZERO(&read_fds);
        FD_SET(s.sock_fd, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        num_fd = s.sock_fd + 1;
        int monk = select(num_fd, &read_fds, NULL, NULL, NULL);
        if (monk == -1)
        {
            perror("select");
            exit(1);
        }
        
        if (FD_ISSET(s.sock_fd, &read_fds))
        {
            int client_closed = read_from_socket(s.sock_fd, s.buf, &(s.inbuf));

            if (client_closed == -1)
            {
                fprintf(stderr, "Error reading from socket");
                exit(1);
            }
            else if (client_closed == 1)
            {
                printf("Closed Client\n");
                close(s.sock_fd);
                break; 
            }
            else if (client_closed == 0)
            {
                char *msg;
                while (!get_message(&msg, s.buf, &(s.inbuf)))
                {
                
                    char *token = strtok(msg, " ");
                    if (token[0] == '1' || token[0] == '0') 
                    {
                        char mess[MAX_NAME + 1];
                        strncpy(mess, token + 1, MAX_NAME);
                        char *next = strtok(NULL, "");
                        strtok(next, "\n"); 
                        printf("%s: %s\n", mess, next);
                        free(msg);
                    }

                    else if(token[0] == '2')
                    {  
                        char mess[MAX_NAME];
                        strncpy(mess, token + 1, MAX_NAME);
                        char *next = strtok(NULL, "");
                        strtok(next, "\n"); 
                        int fd[2], r;
                        if ((pipe(fd)) == -1)
                            {
                                perror("pipe");
                                exit(1);
                            }
                            if((r = fork()) == 0)
                            {
                            
                                int file_des;
                                mkfifo("emotepipe.jpg", 0666);
                                file_des = open("emotepipe.jpg", O_WRONLY); 

                                if ((dup2(fd[0], fileno(stdin))) == -1) {
                                    perror("dup2");
                                    exit(1);
                                }
                                if ((dup2(file_des, fileno(stdout))) == -1) {
                                    perror("dup2");
                                    exit(1);
                                }

                                if ((close(fd[1])) == -1) {
                                    perror("close");
                                }

                                 if ((close(fd[0])) == -1) {
                                    perror("close");
                                }

                                if ((close(file_des)) == -1) {
                                    perror("close");
                                }

                                execlp("base64", "base64", "--decode", (char *)0);
                                fprintf(stderr, "ERROR: exec should not return \n");
                            }
                            else if (r > 0)
                            { 
                                write(fd[1], next, strlen(next));  

                                if ((close(fd[1])) == -1) {
                                    perror("close");
                                }

                                 if ((close(fd[0])) == -1) {
                                    perror("close");
                                } 

                                printf("%s sent an emote:\n", mess);
                                free(msg);
                                fflush(stdout); 

                                system("catimg -w80 emotepipe.jpg");
                                
                                unlink("emotepipe.jpg"); 
                                break;  

                            }
                            
                            else{
                                perror("fork"); 
                                exit(1); 
                            }

                    }
                }
            }
        }
    }
    close(s.sock_fd);
    exit(exit_status);
}