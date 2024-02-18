#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <wiringPi.h>
#include <sys/time.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>

#define IGNORE_CHANGE_BELOW_USEC 10000

const int MSG_SIZE = 256;

int buz = 22;
int button[10] = {16, 20, 25, 26, 19, 3, 4, 27, 14, 15};
int led[10] = {12, 21, 6, 5, 13, 2, 17, 23, 24, 18};
int st[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

struct timeval last_change;

static int sockfd, fd;
static int prev_state[3] = {0, 0, 0};

void *buzzer() {
    digitalWrite(buz, HIGH);
    delay(100);
    //sleep(1);
    digitalWrite(buz, LOW);
}

static void buttonHandler(void *arg) {
    printf("button handler\n");
    char buf[MSG_SIZE];
    int sig = *(int *)arg;
    int pin = button[sig];
    int flag = 0;
    static int cnt = 0;
    //static int sockfd;
    printf("sig = %d\n", sig);

    uint8_t edge = 0x7F;
    uint8_t stream = 0;
    for (int i = 0; i < 16; i++) {
        stream <<= 1;
        stream |= digitalRead(pin);

        if (stream == edge) {
            flag = 1;
            break;
        }

        delay(3);
    }

    if (flag) {
        pthread_t thid;
        pthread_create(&thid, NULL, buzzer, NULL);
        pthread_detach(thid);

        if (sig < 5) {
            //sprintf(buf, "%d button\n", sig);
            memset(buf, 0, sizeof(buf));
            if (sig == 0)
                strcpy(buf, "1st-stop 2nd-stop 1\n");
            else if (sig == 1)
                strcpy(buf, "2nd-stop 3rd-stop 1\n");
            else if (sig == 2)
                strcpy(buf, "3rd-stop 4th-stop 1\n");
            else if (sig == 3)
                strcpy(buf, "4th-stop 5th-stop 1\n");
            // else if (sig == 4)   //can't het on at last stop
            //     strcpy(buf, "5th-stop 1st-stop 1\n");f
            send(sockfd, buf, sizeof(buf), 0);
            
            cnt ++;
            printf("cnt = %d\n", cnt);
        }
        else if (sig > 5) {
            //sprintf(buf, "%d button\n", sig);
            memset(buf, 0, sizeof(buf));
            // if (sig == 5)
            //     strcpy(buf, "1st-stop 2nd-stop 1\n");
            if (sig == 6)
                strcpy(buf, "2nd-stop 1st-stop 1\n");
            else if (sig == 7)
                strcpy(buf, "3rd-stop 2nd-stop 1\n");
            else if (sig == 8)
                strcpy(buf, "4th-stop 3rd-stop 1\n");
            else if (sig == 9)
                strcpy(buf, "5th-stop 4th-stop 1\n");
            // else if (sig == 4)   //can't het on at last stop
            //     strcpy(buf, "5th-stop 1st-stop 1\n");f
            send(sockfd, buf, sizeof(buf), 0);
            
            cnt ++;
            printf("cnt = %d\n", cnt);
        }
    }
}

void closeHandler() {
	close(fd);
	exit(0);
}

void *writeLED(void *n) {
	printf("writeLED\n");
	int st = *(int *)n;
	char c = st + '0';
    if (st == -1) c = 0;
	printf("write: %c\n", c);
	write(fd, &c, 1);
}

int main(int argc, char **argv) {
    //static int sockfd;
    signal(SIGINT, closeHandler);
    if (argc != 3) {
        printf("usage: ./station <ip> <port>\n");
        return 0;
    }
    
    fd = open("/dev/etx_device", O_RDWR);
    char buf[MSG_SIZE], delim[2] = " ";
    char *ptr, *token;
    printf("starting...\n");
    
    int station, id;

    //socket
    struct sockaddr_in info;
    bzero(&info,sizeof(info));
    info.sin_family = PF_INET;

    info.sin_addr.s_addr = inet_addr(argv[1]);
    info.sin_port = htons((u_short)atoi(argv[2]));
    
    sockfd = socket(AF_INET , SOCK_STREAM , 0);
    printf("connecting...\n");
    if(connect(sockfd,(struct sockaddr *)&info,sizeof(info)) < 0){
        perror("Connection error\n");
	exit(1);
    }
    printf("connected!\n");
    printf("sockfd = %d\n", sockfd);
	
    wiringPiSetupGpio();
    //set up interrupt
    if (wiringPiSetup() < 0) {
	    perror("wiringPiSetup");
	    exit(1);
    }
    printf("wiringPiSetup successful\n");

    pinMode(buz, OUTPUT);
    for (int i = 0; i < 10; i++) {
        pinMode(button[i], INPUT);
        pinMode(led[i], OUTPUT);
    }

    for (int i = 0; i < 10; i++) {
        wiringPiISR(button[i], INT_EDGE_RISING, &buttonHandler, &(st[i]));
    }
    printf("wiringPiISR successful\n");

    gettimeofday(&last_change, NULL);
    
    while(1) {
        if (recv(sockfd, buf, 256, 0) <= 0) {
            printf("server disconnected\n");
            kill(getpid(), SIGINT);
        }
        printf("received from server: %s\n", buf);
        if (buf[strlen(buf)-1] == '\n')
            buf[strlen(buf)-1] = '\0';
        token = strtok_r(buf, delim, &ptr);
        if (!strcmp(token, "arriving")) {
            token = strtok_r(ptr, delim, &ptr);
            id = atoi(token);
            token = strtok_r(ptr, delim, &ptr);
            station = atoi(token);
            if (id) station += 5;
            
            pthread_t thid;
            pthread_create(&thid, NULL, writeLED, &station);
            pthread_detach(thid);
        }
    }

    return 0;
}
