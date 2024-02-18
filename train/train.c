#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <time.h>


//library for PWM generator
#include "pca9685.h"
#include <wiringPi.h>

//library for socket
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// library for timer
#include <signal.h>
#include <sys/time.h>

#define WRITE_BUF_SIZE 1024

#define SOCK_BUF_SIZE 256
#define distence_sensor_PIN 4
#define BUZZER_PIN 18

//PWM parameter
#define PIN_BASE 300
#define MAX_PWM 4096
#define HERTZ 50
#define I2C 0x40

// buzzer tone
#define C4 262   // C4
#define D4 294   // D4
#define E4 330   // E4
#define F4 349   // F4
#define G4 392   // G4
#define A4 440   // A4
#define B4 494   // B4
#define C5 523   // C5


//GPIO/ socket file descripter(fd)
int I2C_fd, direction_GPIO_fd, sock_fd;


// Function to play a tone of a specific frequency
void playTone(int frequency, int duration){
    int halfPeriod = 500000 / frequency;
    int i;

    for(i=0;i< (duration * 1000) / (halfPeriod * 2); i++){
        digitalWrite(BUZZER_PIN, HIGH);
        delayMicroseconds(halfPeriod);
        digitalWrite(BUZZER_PIN, LOW);
        delayMicroseconds(halfPeriod);
    }
}

void slow_down_call(void){
    int tone[8] = {C4, D4, E4, F4, G4, A4,B4,C5};
    for(int i=0;i<8;i++){
        playTone(tone[i],300); //300 us duration
    }
}

void emergency_call(void){
    int tone[6] = {E4, C4, E4, C4, E4, C4};
    for(int i=0;i<6;i++){
        playTone(tone[i],500); //500 us duration
    }
}
/**
 * Calculate the number of ticks the signal should be high for the required amount of time
 */
int calcTicks(float impulseMs, int hertz)
{
	float cycleMs = 1000.0f / hertz;
	return (int)(MAX_PWM * impulseMs / cycleMs + 0.5f);
}

void motor_control(char *motor_command){

    char *copy = strdup(motor_command);
    
    uint8_t duty_cycle;
    uint8_t direction;

    char *token;
    char *saveptr;

    uint8_t command[2] = {0};
    token = strtok_r(copy, "\n", &saveptr);
    int i=0;
    while(token!=NULL){
        command[i++] = atoi(token);
        token = strtok_r(NULL, "\n", &saveptr);
    }

    duty_cycle = command[0];

    //printf("duty cycle = %d\n",duty_cycle);
    direction=command[1];
    // Write direction to gpio
    char write_buff;

    ssize_t num_written;
    write_buff = direction + '0';
    // if(direction==1){
    //     printf("forward\n");
    // }else if(direction==2){
    //     printf("backward\n");
    // }else if(direction==0){
    //     printf("stop\n");
    // }else{
    //     printf("brake\n");
    // }


    //Write direction to GPIO
    num_written = write(direction_GPIO_fd,&write_buff, sizeof(write_buff));
    if(num_written ==-1){
        perror("Failed to write to device file");
        exit(EXIT_FAILURE);
    }

    // motor PWM control
    float millis;
    int tick;
    
    // while(1){
    millis = 1000/HERTZ * duty_cycle/100;

    tick = calcTicks(millis, HERTZ);

    pwmWrite(PIN_BASE + 16, tick);
    // usleep(500);
    // }
    //close(I2C_fd);
    //close(direction_GPIO_fd);
}

void my_sleep(float seconds){
    unsigned int start = (unsigned int)(clock() * 1000 / CLOCKS_PER_SEC);
    unsigned int end = start + (unsigned int)(seconds * 1000);

    while ((unsigned int)(clock() * 1000 / CLOCKS_PER_SEC) < end) {
        // Do nothing, just wait
    }
}

// Interrupt service routine
void handleInterrupt(void) {
    printf("Interrupt occurred!\n");
    char stop_command[10] = "0\n3\n";

    motor_control(stop_command); //<duty cycle=0> <direction=3> brake
    //printf("Emergency stop!!\n");

    char response[SOCK_BUF_SIZE] = {0};
    strcpy(response,"obstacle");
    send(sock_fd, response, SOCK_BUF_SIZE, 0);

    // Temporarily mask the interrupt by setting the edge mode to INT_EDGE_FALLING
    //pinMode(distence_sensor_PIN, INT_EDGE_FALLING);
    //printf("temperary mask the interrupt\n");
    //printf("sleep for 5 second\n");
    //my_sleep(5);
    emergency_call();
    
    // Restore the interrupt by setting the edge mode back to INT_EDGE_SETUP
    //pinMode(distence_sensor_PIN, INT_EDGE_SETUP);
}

int main(int argc, char *argv[]){
    if(argc<3){
        printf("./motor_control <server IP> <server PORT>\n");
        return -1;
    }


    //open the direction control gpio module 
    char dev_dir[16] = "/dev/etx_device";

    direction_GPIO_fd = open(dev_dir, O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR); 
    if (direction_GPIO_fd == -1){
        perror("Failed to open file!\n");
        exit(EXIT_FAILURE);
    }
    // Setup PWM generater
    wiringPiSetup();

    // Initialize WiringPi library
    if (wiringPiSetupGpio() == -1) {
        printf("WiringPi setup failed!\n");
        return 1;
    }

    // Setup with pinbase 300 and i2c location 0x40
	I2C_fd = pca9685Setup(PIN_BASE, I2C, HERTZ);
	if (I2C_fd < 0)
	{
		printf("Error in setup\n");
		return I2C_fd;
	}

    // Reset all output
	pca9685PWMReset(I2C_fd);

    // Construct Socket connet to server
    struct sockaddr_in serv_addr;

    char response[SOCK_BUF_SIZE] = {0};


    char motor_command[SOCK_BUF_SIZE] = {0};

    char *IP_addr = argv[1];
    int port = atoi(argv[2]);


    // Create socket file descriptor
    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(port);
    
    //Conver IP address from string to binary form
    if(inet_pton(AF_INET, IP_addr, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connection failed");
        exit(EXIT_FAILURE);
    }

    // Set the GPIO pin mode to input
    pinMode(distence_sensor_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    // Set up the interrupt handler
    // if (wiringPiISR(distence_sensor_PIN, INT_EDGE_RISING, &handleInterrupt) < 0) {
    //     printf("Unable to set up ISR!\n");
    //     return 1;
    // }
    


    ssize_t write_size;
    ssize_t read_size;

    while(1){

        // Send the command to the server
        //send(sock_fd, response, strlen(response)+1, 0);

        // Receice and printf the response from the server
        read_size = recv(sock_fd, motor_command, SOCK_BUF_SIZE, 0);
        //printf("%s\n",motor_command);
        char command[10] = {0};

        if(motor_command[strlen(motor_command)-1]=='\n'){
            motor_command[strlen(motor_command)-1] = '\0';
        }

        if(!(strcmp(motor_command,"forward"))){
            printf("forward\n");
            char forward_command[10] = "50\n1\n";
            strcpy(command,"50\n1\n");
            motor_control(forward_command);
            memset(command,0,10*sizeof(char));

        }else if(!(strcmp(motor_command,"stop"))){
            printf("brake\n");
            strcpy(command,"0\n3\n");
            motor_control(command);
            memset(command,0,10*sizeof(char));
        }else if(!(strcmp(motor_command,"backward"))){
            printf("backward\n");
            strcpy(command,"50\n2\n");
            motor_control(command);
            memset(command,0,10*sizeof(char));
        }else if(!(strcmp(motor_command,"slowdown"))){
            printf("slowdown\n");
            for(int i=2;i>=0;i--){
                sprintf(command,"%d",i*16);
                strcat(command,"\n1\n");
                
                motor_control(command);
                my_sleep(0.3);
                memset(command,0,10*sizeof(char));
            }
            slow_down_call();
            strcpy(command,"0\n0\n");
            motor_control(command);
            memset(command,0,10*sizeof(char));
        }else{
            printf("stop\n");
            strcpy(command,"0\n0\n");
            motor_control(command);
            memset(command,0,10*sizeof(char));
        }

    }
        
    return 0;
}
