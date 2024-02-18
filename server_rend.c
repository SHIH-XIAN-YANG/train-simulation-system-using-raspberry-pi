#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>
#include <fcntl.h>

#define SEM_KEY 12345678
#define RSEM_KEY0 1234444
#define RSEM_KEY1 1234555
#define RSEG_KEY0 1234666
#define RSEG_KEY1 1234777
#define SHM_KEY0 6789
#define SHM_KEY1 5678
#define MSG_SIZE 256

#define TRAIN_MAX 100

typedef struct {
    int fd;
    int passenger;
    int status; //0: running, 1: arriving, 2: stopped, 3: delayed
    int nextstop;
    int waitlist[2][10];
    int offset;
} train;

const char* status[10]={
    "5th-stop --> 1th-stop",
    "1st-stop",
    "1st-stop --> 2nd-stop",
    "2nd-stop",
    "2nd-stop --> 3rd-stop",
    "3rd-stop",
    "3rd-stop --> 4th-stop",
    "4th-stop",
    "4th-stop --> 5th-stop",
    "5th-stop"
};

const char* NumTable[11]={
    "1111110",
    "0110000",
    "1101101",
    "1111001",
    "0110011",
    "1011011",
    "1011111",
    "1110000",
    "1111111",
    "1111011",
    "0000000"
};

int sockfd, connfd, f_seg;
int segFlag[2];
int shmid0, shmid1, sem, rsem[2], rseg[2];
int childpid = -1, stationpid, trainpid[2];
int stationfd, detectfd[2];
train *trains;
const char* path = "/dev/seg_device";
// int (*waitlist)[3]; //0: boarding, 1: exit

void closeHandler();
void segHandler(int);
void clientHandler();
void trainHandler(int);
void detectHandler(int, int);
void stationHandler();

int P(int s) {
    // printf("P, %d\n", s);
    struct sembuf sop; /* the operation parameters */
    sop.sem_num = 0; /* access the 1st (and only) sem in the array */
    sop.sem_op = -1; /* wait..*/
    sop.sem_flg = 0; /* no special options needed */
    if (semop (s, &sop, 1)) {
        fprintf(stderr,"P(): semop failed: %s\n",strerror(errno));
        return -1;
    } else {
        return 0;
    }
}

int V(int s) {
    // printf("V %d\n", s);
    struct sembuf sop; /* the operation parameters */
    sop.sem_num = 0; /* the 1st (and only) sem in the array */
    sop.sem_op = 1; /* signal */
    sop.sem_flg = 0; /* no special options needed */
    if (semop(s, &sop, 1)) {
        fprintf(stderr,"V(): semop failed: %s\n",strerror(errno));
        return -1;
    } else {
        return 0;
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("usage: ./server <ip> <port>\n");
        return 0;
    }

    signal(SIGINT, closeHandler);

    //semaphore
    sem = semget(SEM_KEY, 1, IPC_CREAT | IPC_EXCL | 0666);
    if (sem < 0) {
        perror("semget");
        exit(1);
    }
    if (semctl(sem, 0, SETVAL, 1) < 0) {
        perror("semctl: SETVAL");
        exit(1);
    }
    rsem[0] = semget(RSEM_KEY0, 1, IPC_CREAT | IPC_EXCL | 0666);
    if (rsem[0] < 0) {
        perror("semget");
        exit(1);
    }
    if (semctl(rsem[0], 0, SETVAL, 0) < 0) {
        perror("semctl: SETVAL");
        exit(1);
    }
    rsem[1] = semget(RSEM_KEY1, 1, IPC_CREAT | IPC_EXCL | 0666);
    if (rsem[1] < 0) {
        perror("semget");
        exit(1);
    }
    if (semctl(rsem[1], 0, SETVAL, 0) < 0) {
        perror("semctl: SETVAL");
        exit(1);
    }
    rseg[0] = semget(RSEG_KEY0, 1, IPC_CREAT | IPC_EXCL | 0666);
    if (rseg[0] < 0) {
        perror("semget");
        exit(1);
    }
    if (semctl(rseg[0], 0, SETVAL, 0) < 0) {
        perror("semctl: SETVAL");
        exit(1);
    }
    rseg[1] = semget(RSEG_KEY1, 1, IPC_CREAT | IPC_EXCL | 0666);
    if (rseg[1] < 0) {
        perror("semget");
        exit(1);
    }
    if (semctl(rseg[1], 0, SETVAL, 0) < 0) {
        perror("semctl: SETVAL");
        exit(1);
    }

    //shm
    if ((shmid0 = shmget(SHM_KEY0, sizeof(train[2]), IPC_CREAT | 0666)) < 0) {
        perror("shmget 0");
        exit(1);
    }
    if ((trains = shmat(shmid0, NULL, 0)) == (void*) - 1) {
        perror("shmat 0");
        exit(1);
    }
    memset(trains, 0, sizeof(train[2]));
    trains[0].nextstop = -1;
    trains[1].nextstop = -1;

    //socket
    struct sockaddr_in stSockAddr;
    sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sockfd == -1) {
		perror("can not create socket");
		exit(EXIT_FAILURE);
	}

	memset(&stSockAddr, 0, sizeof(struct sockaddr_in));

	stSockAddr.sin_family = AF_INET;
	stSockAddr.sin_port = htons((u_short)atoi(argv[2]));
	stSockAddr.sin_addr.s_addr = INADDR_ANY;

	int yes = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	if(bind(sockfd,(const struct sockaddr *)&stSockAddr, sizeof(struct sockaddr_in)) == -1) {
		perror("error bind failed");
		close(sockfd);
		exit(EXIT_FAILURE);
	}

	if(listen(sockfd, 100) == -1) {
		perror("error listen failed");
		close(sockfd);
		exit(EXIT_FAILURE);
	}

    //listen for trains & station
    trains[0].fd = accept(sockfd, NULL, NULL);
    printf("Train 1 connected, fd = %d\n", trains[0].fd);
    detectfd[0] = accept(sockfd, NULL, NULL);
    printf("Detector 1 connected, fd = %d\n", detectfd[0]);
    trains[1].fd = accept(sockfd, NULL, NULL);
    printf("Train 2 connected, fd = %d\n", trains[1].fd);
    detectfd[1] = accept(sockfd, NULL, NULL);
    printf("Detector 2 connected, fd = %d\n", detectfd[1]);
    if ((stationfd = accept(sockfd, NULL, NULL)) < 0)
	    perror("stationfd\n");
    printf("Station connected, stationfd = %d\n", stationfd);

    // 7 seg
    f_seg = open(path,O_RDWR);
    for (int i = 0; i < 2; i++) {
        // segFlag[i] = 0;
        childpid = fork();
        if (!childpid) {
            segHandler(i);
            return 0;
        }
    }

    write(f_seg, "00000000", 8);
    write(f_seg, "00000001", 8);
    printf("After 7 seg\n");
    
    //train
    int ntrain = 1;
    for (int i = 0; i < 2; i++) {
        childpid = fork();
        if (!childpid) {
            trainHandler(i);
            return 0;
        }
        else {
            trainpid[i] = childpid;
        }
    }
    for (int i = 0; i < 2; i++) {
        childpid = fork();
        if (!childpid) {
            detectHandler(i, detectfd[i]);
            return 0;
        }
        else {;
            /* do we need to save detectpid? */
        }
    }

    //station
    childpid = fork();
    if (!childpid) {
        stationHandler();
        return 0;
    }
    else {
        stationpid = childpid;
    }

    // listen for clients
    while (1) {
        connfd = accept(sockfd, NULL, NULL);
        childpid = fork();
        if (!childpid) {
            clientHandler();
            return 0;
        }
        else if (childpid < 0) {
            perror("fork error");
            exit(1);
        }
    }
}

void closeHandler() {
    if (childpid == 0)
        exit(0);
    
    if (semctl(sem, 0, IPC_RMID, 0) < 0) {
        perror("semctl: IPCRMID");
        exit(1);
    }
    if (semctl(rsem[0], 0, IPC_RMID, 0) < 0) {
        perror("semctl: IPCRMID");
        exit(1);
    }
    if (semctl(rsem[1], 0, IPC_RMID, 0) < 0) {
        perror("semctl: IPCRMID");
        exit(1);
    }
    if (semctl(rseg[0], 0, IPC_RMID, 0) < 0) {
        perror("semctl: IPCRMID");
        exit(1);
    }
    if (semctl(rseg[1], 0, IPC_RMID, 0) < 0) {
        perror("semctl: IPCRMID");
        exit(1);
    }
    if (shmctl(shmid0, IPC_RMID, NULL) < 0) {
        perror("shmctl: IPCRMID 0");
        exit(1);
    }

    close(f_seg);
    //char buf[MSG_SIZE];
    //sprintf(buf, "arriving %d %d\n", 0, -1);
    //send(stationfd, buf, MSG_SIZE, 0);

    exit(0);
}

void segHandler(int id){
    int idx, max;
    char people[3], num[10];
    printf("segHandler\n");

    while(1){
        // need to print passenger
        P(rseg[id]);

        printf("seg Critical\n");
        P(sem);
        //segFlag[id]=0;
        sprintf(people, "%d", trains[id].passenger);
        
        V(sem);
        printf("train %d, people %s\n", id, people);
        for(int i=0;i<strlen(people);i++){
            idx = (people[i]-'0');
            //P(sem);
            sprintf(num, "%s%d", NumTable[idx], id);
            printf("%s\n",num);
            write(f_seg, num, 8);
            //V(sem);
            sleep(1);
            
            // P(sem);
            // f_seg = open(path,O_RDWR);
            // sprintf(num, "%s%d", NumTable[10], id);
            // write(f_seg, num, 8);
            // close(f_seg);
            // V(sem);
            // usleep(100);
        }
    }
}

int findStop(char *token){
    for(int i=1;i<10;i+=2){
        if(!strcmp(status[i], token)){
            return (i/2);
        }
    }
    return -1;
}

int passengerBefore(int trainID, int board){
    int total=trains[trainID].passenger;
    int nextStop = trains[trainID].nextstop + trains[trainID].offset;
    while(nextStop != board){
        total+=trains[trainID].waitlist[0][nextStop];
        total-=trains[trainID].waitlist[1][nextStop];
        if (trainID == 0) {
            nextStop ++;
            if (nextStop > 9) nextStop = 0;
        }
        else {
            nextStop --;
            if (nextStop < 0) nextStop = 9;
        }
    }
    total+=trains[trainID].waitlist[0][board];
    total-=trains[trainID].waitlist[1][board];
    return total;
}

int addPassenger(int board, int exit, int num, int trainID){
    train *tptr = trains + trainID;
    int offset = tptr->offset;
    int passenger = tptr->passenger, nextstop = tptr->nextstop + offset;
    int success = 1;
    int shift = 5 - offset;

    P(sem);
    if (trainID == 0) { //clockwise (0-4)
        printf("b: %d, e: %d, ns: %d\n", board, exit, nextstop);
        if (board + offset < nextstop) {
            board += shift;  //shift if status == 1 (train is on the left-hand side)
            exit += shift;
        }
	else {
		board += offset;
		exit += offset;
	}
        tptr->waitlist[0][board] += num;
        tptr->waitlist[1][exit] += num;
        /*printf("(during) train %d at station %d:\n", trainID, nextstop);
        for (int i = 0; i < 2; i++) {
            printf("\t");
            for (int j = 0; j < 10; j++) {
                printf("%d ", tptr->waitlist[i][j]);
            }
            printf("\n");
        }
        printf("passenger: %d\n", tptr->passenger);
        printf("\n");*/

        for (int i = 0; i < 10; i++) {
            passenger += tptr->waitlist[0][nextstop];
            passenger -= tptr->waitlist[1][nextstop];
            if (passenger < 0 || passenger > TRAIN_MAX) {
                success = 0;
                break;
            }
            nextstop ++;
            if (nextstop > 9) nextstop = 0;
        }
    }
    else if (trainID == 1) { //counter-clockwise (4-0)
        if (nextstop < board + offset) {
            board += shift;  //shift if status == 1 (train is on the left-hand side)
            exit += shift;
        }
	else {
		board += offset;
		exit += offset;
	}
        tptr->waitlist[0][board] += num;
        tptr->waitlist[1][exit] += num;
        /*printf("(during) train %d at station %d:\n", trainID, nextstop);
        for (int i = 0; i < 2; i++) {
            printf("\t");
            for (int j = 0; j < 10; j++) {
                printf("%d ", tptr->waitlist[i][j]);
            }
            printf("\n");
        }
        printf("passenger: %d\n", tptr->passenger);
        printf("\n");*/

        for (int i = 0; i < 10; i++) {
            passenger += tptr->waitlist[0][nextstop];
            passenger -= tptr->waitlist[1][nextstop];
            if (passenger < 0 || passenger > TRAIN_MAX){
                success = 0;
                break;
            }
            nextstop --;
            if (nextstop < 0) nextstop = 9;
        }
    }

    if (!success) {
        tptr->waitlist[0][board] -= num;
        tptr->waitlist[1][exit] -= num;
    }
    V(sem);
    return success;
}

void addTrainStatus(char* msg){
    int t_len=0;
    
    //1st train
    int n, idx=0;
    if(trains[0].status==2){
        idx=1;
    }
    idx += 2*(trains[0].nextstop);
    n = sprintf(msg+t_len, "1st Train | %d/%d | %s\n", trains[0].passenger, TRAIN_MAX, status[idx]);
    t_len += n;
    idx=0;
    if(trains[1].status==2){
        idx=1;
    }
    idx += 2*(trains[1].nextstop);
    n = sprintf(msg+t_len, "2nd Train | %d/%d | %s\n", trains[1].passenger, TRAIN_MAX, status[idx]);
    t_len += n;
}

void clientHandler() {
    char buf[MSG_SIZE];
    char *ptr, *token;
    char delim[2] = " ";
    char *msg = calloc(sizeof(char),sizeof(MSG_SIZE));
    int boarding_station, exit_station, num;
    int trainID;

    while (1) {
        memset(buf, 0, sizeof(buf));
        printf("receiving from %d...\n", connfd);
        if (recv(connfd, buf, MSG_SIZE, 0) < 0) {
            printf("client %d disconnected\n", connfd);
            continue;
        }
        printf("client %s\n", buf);

        // scanf("%[^\n]%*c", buf);
        //format: <boarding_station> <exit_station> <num_of_passenger>
        token = strtok_r(buf, delim, &ptr);
        boarding_station = findStop(token);
        if(boarding_station==-1){
            printf(msg, "Boarding station error\n");
            // send(connfd, msg, MSG_SIZE, 0);
            continue;
        }
        token = strtok_r(ptr, delim, &ptr);
        exit_station = findStop(token);
        if(exit_station==-1){
            printf(msg, "Exit station error\n");
            // send(connfd, msg, MSG_SIZE, 0);
            continue;
        }
        token = strtok_r(ptr, delim, &ptr);
        num = atoi(token);

        trainID = (boarding_station > exit_station);

        int succeed = addPassenger(boarding_station, exit_station, num, trainID);
        
        if(succeed){
            sprintf(msg, "Booking success!\n");
        }else{
            sprintf(msg, "Booking failed! There are no enough seats.\n");
        }

        addTrainStatus(msg+strlen(msg));
        //send success (or failed?) message to client
        printf("%s\n", msg);
        send(connfd, msg, MSG_SIZE, 0);
    }
}

void trainHandler(int id) {
    char buf[MSG_SIZE];
    train *tptr = trains + id;
    connfd = tptr->fd;
    
    sprintf(buf, "forward");
    send(trains[id].fd, buf, MSG_SIZE, 0);

    while (1) {
        memset(buf, 0, sizeof(buf));
        printf("receiving from train%d...\n", id);
        if (recv(connfd, buf, MSG_SIZE, 0) <= 0) {
            printf("train%d disconnected\n", id);
            return;
        }
        printf("train %s\n", buf);

        if (!strcmp(buf, "leaving")) {  //nextstop += 1
            P(sem);
            // tptr->status = 0; //status = running
            // tptr->nextstop = (tptr->nextstop + 1) % 3;
            V(sem);
        }
        else if (!strcmp(buf, "stopped")) { //update passenger count on train & waitlist at station
            P(sem);
            tptr->status = 2; //status = stopped
            V(sem);
        }
        else if (!strcmp(buf, "obstacle")) {
            P(sem);
            tptr->status = 3;
            printf("obstacle detected at train %d\n", id);
            V(sem);
            sleep(5);
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "forward");
            send(trains[id].fd, buf, MSG_SIZE, 0);
        }
    }
}

void detectHandler(int id, int fd){
    char buf[MSG_SIZE];
    train *tptr = trains + id;
    sprintf(buf, "detect %d\n", id);
    send(fd, buf, MSG_SIZE, 0);

    while (1) {
        memset(buf, 0, sizeof(buf));
        //printf("receiving from detector %d...\n", id);
        if (recv(fd, buf, MSG_SIZE, 0) <= 0) {
            printf("detector disconnected\n");
            return;
        }

        int station = atoi(buf);
        if (tptr->nextstop == -1) tptr->nextstop = station;
        printf("%d detector %s, station %d\n", id, buf, station + tptr->offset);

        if (station != tptr->nextstop) {
            printf("%d station != nextstop!!\n", id);
            printf("%d nextstop = %d\n\n", id, tptr->nextstop);
        }
        sprintf(buf, "arriving %d %d\n", id, station);
        send(stationfd, buf, MSG_SIZE, 0);
        // printf("detector %s\n", buf);

        station += tptr->offset;
        memset(buf, 0, sizeof(buf));
        if (!tptr->waitlist[0][station] && !tptr->waitlist[1][station]) {
            printf("%d No one, passenger %d\n", id, tptr->passenger);
            printf("station = %d\n", station);
            if (station == 0 || station == 5) {
                sprintf(buf, "slowdown\n");
                send(tptr->fd, buf, MSG_SIZE, 0);
		        sleep(4);

                V(rsem[id]);
                P(rsem[1-id]);
                usleep(100);
                V(rsem[1-id]);
                P(rsem[id]);
                memset(buf, 0, sizeof(buf));
                sprintf(buf, "forward");
                send(tptr->fd, buf, MSG_SIZE, 0);
            }
        }
        else {

    	    P(sem);	
            sprintf(buf, "slowdown\n");
            send(tptr->fd, buf, MSG_SIZE, 0);
            printf("detector %s\n", buf);
            tptr->status = 2;

            tptr->passenger -= tptr->waitlist[1][station];
            tptr->waitlist[1][station] = 0;
            tptr->passenger += tptr->waitlist[0][station];
            // if(tptr->passenger > TRAIN_MAX){
                // waitlist[0][tptr->nextstop] = (tptr->passenger - TRAIN_MAX);
                // tptr->passenger = TRAIN_MAX;
            // }else
                // waitlist[0][tptr->nextstop] = 0;
            tptr->waitlist[0][station] = 0;
	        V(sem);
            V(rseg[id]);
            sleep(5);
            
            tptr->status = 0; //status = running

            if (station == 0 || station == 5) {
                V(rsem[id]);
                P(rsem[1-id]);
            usleep(100);
            V(rsem[1-id]);
            P(rsem[id]);
            }
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "forward");
            send(tptr->fd, buf, MSG_SIZE, 0);
        }
        if (id == 0) {
	    P(sem);
            tptr->nextstop = tptr->nextstop + 1;
            if (tptr->nextstop == 5) {
                tptr->nextstop = 0;
                tptr->offset = tptr->offset ^ 5;    //0^5 = 5, 5^5 = 0
            }
	    V(sem);
            printf("%d (after) nextstop = %d (%d)\n", id, tptr->nextstop, tptr->nextstop + tptr->offset);
        }
        else {
	        P(sem);
            tptr->nextstop = tptr->nextstop - 1;
            if (tptr->nextstop == -1) {
                tptr->nextstop = 4;
                tptr->offset = tptr->offset ^ 5;
            }
	        V(sem);
            printf("%d (after) nextstop = %d (%d)\n", id, tptr->nextstop, tptr->nextstop + tptr->offset);
        }
        printf("(after) train %d at station %d:\n", id, station);
        for (int i = 0; i < 2; i++) {
            printf("\t");
            for (int j = 0; j < 10; j++) {
                printf("%d ", tptr->waitlist[i][j]);
            }
            printf("\n");
        }
        printf("passenger: %d\n", tptr->passenger);
        printf("\n");
    }
}

void stationHandler() {
    int boarding_station, exit_station, num;
    char buf[MSG_SIZE];
    char *ptr, *token;
    char delim[2] = " ";
    int station, id;
    train* tptr;

    while (1) {
        memset(buf, 0, sizeof(buf));
        printf("receiving from station...\n");
        if (recv(stationfd, buf, MSG_SIZE, 0) < 0) {
            printf("station disconnected\n");
            continue;
        }
        printf("station %s\n", buf);
        
        //format: <boarding_station> <exit_station> 1
        token = strtok_r(buf, delim, &ptr);
        boarding_station = findStop(token);
        if(boarding_station==-1) {
            printf(buf, "Boarding station error\n");
            // send(connfd, msg, MSG_SIZE, 0);
            continue;
        }
        token = strtok_r(ptr, delim, &ptr);
        exit_station = findStop(token);
        if(exit_station==-1) {
            printf(buf, "Exit station error\n");
            // send(connfd, msg, MSG_SIZE, 0);
            continue;
        }
        token = strtok_r(ptr, delim, &ptr);
        num = atoi(token);

        int trainID = (boarding_station > exit_station);

        int succeed = addPassenger(boarding_station, exit_station, num, trainID);
        
        if (succeed) {
            printf("Booking success!\n");
        }
        else {
            printf("You are in the line. There are %d people in front of you, you may not get on the next train.\n", succeed);
        }
    }
}
