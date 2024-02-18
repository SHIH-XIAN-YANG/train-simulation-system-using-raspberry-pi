/****
 * Server.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

#define BUF_SIZE 1024
#define MAX_CLIENT 3

char *command_create(int, int);

void my_sleep(float seconds){
    unsigned int start = (unsigned int)(clock() * 1000 / CLOCKS_PER_SEC);
    unsigned int end = start + (unsigned int)(seconds * 1000);

    while ((unsigned int)(clock() * 1000 / CLOCKS_PER_SEC) < end) {
        // Do nothing, just wait
    }
}

void *handle_client(void *args){
    int client_socket = *(int *)args; 
    char *command;
    char buffer[BUF_SIZE];

    int duty_cycle;
    int direction;

    int read_size;
    int write_size;

    while((read_size = recv(client_socket, buffer, BUF_SIZE, 0)) > 0){
        printf("test\n");
        printf("Received command: %s\n", buffer);

        printf("Enter duty cycle and direction: ");
        scanf("%d %d",&duty_cycle,&direction);
        
        command = command_create(duty_cycle, direction);
        printf("command line:%s\n",command);
        

        // Send command to train
        write_size = write(client_socket, command, strlen(command));
        //send(client_socket, command, strlen(command), 0);
        printf("Response sent\n");
        free(command);

        memset(buffer, 0, sizeof(buffer));
        my_sleep(0.1);
    }

    /*while(1){
        read_size = recv(client_socket, buffer, BUF_SIZE, 0);
        //read(client_socket, buffer, BUF_SIZE);
    
        printf("Received command: %s\n", buffer);

        printf("Enter duty cycle and direction: ");
        scanf("%d %d",&duty_cycle,&direction);
        
        command = command_create(duty_cycle, direction);
        printf("command line:%s\n",command);
        

        // Send command to train
        write_size = write(client_socket, command, strlen(command));
        //send(client_socket, command, strlen(command), 0);
        printf("Response sent\n");
        free(command);

        memset(buffer, 0, sizeof(buffer));
        my_sleep(0.1);
    }*/
            
    close(client_socket);
}


char * command_create(int duty_cycle, int direction){
    char *command_line = (char *)malloc(BUF_SIZE*sizeof(char *));

    sprintf(command_line,"%d",duty_cycle);
    sprintf(command_line+strlen(command_line),"\n");
    sprintf(command_line+strlen(command_line), "%d",direction);
    sprintf(command_line+strlen(command_line),"\n");

    return command_line;
}

int main(int argc, char *argv[]){
    if(argc<3){
        perror("./server <IP> <PORT>\n");
        exit(EXIT_FAILURE);
    }

    int server_fd, client_socket;
    struct sockaddr_in address;
    pthread_t threads[MAX_CLIENT];

    int opt = 1;
    int addrlen = sizeof(address);

    // char *IP_addr = argv[1];
    int port = atoi(argv[2]);

    //create socket file descriptor
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }    

    //set socket options
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
                    &opt, sizeof(opt))) {
        perror("setsocketopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    // Bind the socket to  localhost: 8080
    if(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    //Listen for incomming connection from train
    if(listen(server_fd, 3) < 0){
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    int i=0;


    /*Listen to client */
    while(1){
        printf("Server is listening...\n");
        /*when we get signal from client we print menu*/

        if((client_socket = accept(server_fd, (struct sockaddr *) &address, (socklen_t *) &addrlen)) == -1) {
            perror("failed to accept connection from client");
            continue;
        }

        // create new thread to handle client
        int *client_socket_ptr = malloc(sizeof(int));
        *client_socket_ptr = client_socket;
        if(pthread_create(&threads[i], NULL, handle_client, (void *)client_socket_ptr) != 0) {
            perror("Failed to creat thread");
            free(client_socket_ptr);
            continue;
        }

        // detach thread
        if(pthread_detach(threads[i]) != 0){
            perror("Failed to detach thread");
            free(client_socket_ptr);
            continue;
        }

        i = (i+1) % MAX_CLIENT;
    }
    

    return 0;
}