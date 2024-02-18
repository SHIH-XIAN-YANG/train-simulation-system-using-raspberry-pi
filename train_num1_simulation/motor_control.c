#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

#include <string.h>
#include "pca9685.h"
#include <wiringPi.h>

#define WRITE_BUF_SIZE 2

#define GPIO_PIN 14 // for infared distance sensor

//PWM parameter
#define PIN_BASE 300
#define MAX_PWM 4096
#define HERTZ 50
#define I2C 0x40

// Interrupt service routine
void handleInterrupt() {
    printf("Interrupt occurred!\n");

    char dev_dir[32] = "/dev/etx_device";
    char write_buff;
    int fd = open(dev_dir, O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR); 
    if (fd == -1){
        perror("Failed to open file!\n");
        exit(EXIT_FAILURE);
    }

    ssize_t num_written;
    write_buff = '3';

    num_written = write(fd,&write_buff, sizeof(write_buff));
    if(num_written ==-1){
        perror("Failed to write to device file");
        exit(EXIT_FAILURE);
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



int main(int argc, char *argv[]){
    if(argc<3){
        printf("./motor_control <duty cycle(0~100)> <Direction(0/1/2/3)>\n");
        return -1;
    }

    // Initialize WiringPi library
    // wiringPiSetupGpio()
    if (wiringPiSetupGpio() == -1) {
        printf("WiringPi setup failed!\n");
        return 1;
    }

    // Set the GPIO pin mode to input
    pinMode(GPIO_PIN, INPUT);

    // Set up the interrupt handler
    if (wiringPiISR(GPIO_PIN, INT_EDGE_BOTH, &handleInterrupt) < 0) {
        printf("Unable to set up ISR!\n");
        return 1;
    }

    uint8_t duty_cicle = atoi(argv[1]);
    if(duty_cicle<0 || duty_cicle>100){
        printf("Duty cycle error\n");
        duty_cicle = 10;
    }
    uint8_t direction = atoi(argv[2]); //direction of motor
    if(direction <0 || direction >3){
        printf("direction error\n");
        direction = 0;
    }

    char dev_dir[32] = "/dev/etx_device";

    int fd = open(dev_dir, O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR); 
    if (fd == -1){
        perror("Failed to open file!\n");
        exit(EXIT_FAILURE);
    }
    printf("device open\n");

    // Setup PWM generater
    wiringPiSetup();

    // Setup with pinbase 300 and i2c location 0x40
	int I2C_fd = pca9685Setup(PIN_BASE, I2C, HERTZ);
	if (I2C_fd < 0)
	{
		printf("Error in setup\n");
		return I2C_fd;
	}

    // Reset all output
	pca9685PWMReset(I2C_fd);
    
    char write_buff;

    ssize_t num_written;
    write_buff = direction + '0';
    printf("direction = %d\n",direction);
    // sprintf(write_buff, direction+'0');

    num_written = write(fd,&write_buff, sizeof(write_buff));
    if(num_written ==-1){
        perror("Failed to write to device file");
        exit(EXIT_FAILURE);
    }

    float millis;
    int tick;
    
    // while(1){
    millis = 1000/HERTZ * duty_cicle/100;

    tick = calcTicks(millis, HERTZ);

    pwmWrite(PIN_BASE + 16, tick);
        // usleep(500);
    // }
    close(I2C_fd);
    close(fd);
    return 0;
}
