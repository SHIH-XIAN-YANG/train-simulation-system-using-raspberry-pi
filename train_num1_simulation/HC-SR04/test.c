#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <wiringPi.h>
#define TRIG 4
#define ECHO 5

int main(){
    if(wiringPiSetup() == -1){
        exit(EXIT_FAILURE);
    }

    wiringPiSetupGpio();



    pinMode(TRIG, OUTPUT);
    pinMode(ECHO, INPUT);

    digitalWrite(TRIG, LOW);
    delay(1);

    digitalWrite(TRIG, HIGH);
    delayMicroseconds(50);
    printf("test\n");
    // digitalWrite(TRIG, LOW);

    return 0;
}