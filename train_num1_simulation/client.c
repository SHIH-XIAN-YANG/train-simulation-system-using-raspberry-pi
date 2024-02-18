#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include<arpa/inet.h>


#define BUF_SIZE 1024

int main(int argc, char *argv[]){



    int sock = 0, valread;

    struct sockaddr_in serv_addr;

    char command[1024] = {0};

    char buffer[1024] = {0};

    //char *IP_addr = argv[1];
    int port = atoi(argv[1]);


    // Create socket file descriptor
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    //serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(port);
    
    //Conver IP address from string to binary form
    /*if(inet_pton(AF_INET, IP_addr, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        exit(EXIT_FAILURE);
    }*/

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connection failed");
        exit(EXIT_FAILURE);
    }

    ssize_t write_size;
    int read_size;
    printf("test\n");

    while(1){
        strcpy(command,"12");
        printf("command:%s, strlen(command):%d\n",command, strlen(command));
        printf("ready to send\n");
        // Send the command to the server
        //send(sock, command, strlen(command)+1, 0);
        write_size = write(sock, command, strlen(command));
        // printf("Command sent\n");

        // Receice and printf the response from the server
        //valread = read(sock, buffer, BUF_SIZE);
        read_size = recv(sock, buffer, BUF_SIZE, 0);
        printf("response: %s\n", buffer);
        char *token;
        char *saveptr;

        uint8_t command[2] = {0};
        token = strtok_r(buffer, "\n", &saveptr);
        int i=0;
        while(token!=NULL){
            command[i++] = atoi(token);
            token = strtok_r(NULL, "\n", &saveptr);
        }

        uint8_t duty_cycle = command[0];
        uint8_t direction=command[1];
        printf("duty cycle = %d\n",duty_cycle);
        printf("direction = %d\n",direction);
        
    }

    return 0;
}