#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <iostream>
#include <stdio.h>
#include <cstring>
#include <map>

// library for socket
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


void delay(int ms){
#ifdef WIN32
  Sleep(ms);
#else
  usleep(ms*1000);
#endif
}




#include "MFRC522.h"

#define SOCK_BUF_SIZE 256

//GPIO/ socket file descripter(fd)
int sock_fd;

int main(int argc, char *argv[]){
    if(argc<3){
      perror("./rfid_read <IP> <PORT>\n");
      exit(EXIT_FAILURE);
    }
    std::map<int,int> m;
    m[5] = 0;
    m[165] = 0;
    m[227] = 1;
  m[211] = 1;
    m[196] = 2;
    m[194] = 2;
    m[161] = 3;
    m[79] = 3;
    m[241] = 4;
    m[250] = 4;

    MFRC522 mfrc;

    mfrc.PCD_Init();

    int addr[4] = {0};


    // open socket
    // Construct Socket connet to server
    struct sockaddr_in serv_addr;

    char response[SOCK_BUF_SIZE] = {0};

    char motor_command[SOCK_BUF_SIZE] = {0};


    //Create socket file descriptor
    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }


    serv_addr.sin_family = PF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons((u_short)atoi(argv[2]));

    
    //Conver IP address from string to binary form
    if(inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        exit(EXIT_FAILURE);
    }

    //Connect to the server
    if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connection failed");
        exit(EXIT_FAILURE);
    }

    ssize_t write_size;
    ssize_t read_size;

    memset(response,0,SOCK_BUF_SIZE * sizeof(char));

    while(1){
        // Look for a card
        if(!mfrc.PICC_IsNewCardPresent())
          continue;

        if( !mfrc.PICC_ReadCardSerial())
          continue;

        // Print UID
        for(byte i = 0; i < mfrc.uid.size; ++i){
          if(mfrc.uid.uidByte[i] < 0x10){
            //printf(" 0");
            //printf("%X",mfrc.uid.uidByte[i]);
          }
          else{
            //printf(" ");
            //printf("%X", mfrc.uid.uidByte[i]);
          }
          addr[i] = static_cast<int>(mfrc.uid.uidByte[i]);
          //std::cout<<addr[i]<<" ";

          
        }


        //printf("%d\n", addr[0]);

        switch(m[addr[0]]){
          case 0:
            strcpy(response, "0\n");
            printf("station 0\n");
            break;
          case 1:
            strcpy(response,"1\n");
            printf("station 1\n");
            break;
          case 2:
            strcpy(response,"2\n");
            printf("station 2\n");
            break;
          case 3:
            strcpy(response,"3\n");
            printf("station 3\n");
            break;
          case 4:
            strcpy(response,"4\n");
            printf("station 4\n");
            break;
          default:
            printf("wrong ID\n");
            strcpy(response,"-1\n");
            break;
        }
        write_size = send(sock_fd,response, 256, 0);
        //printf("%s, %d\n",response, write_size);

        //write_size = write(sock_fd, response, 256);
        
        //printf("\n");
        delay(1500);
    }
    return 0;
}
