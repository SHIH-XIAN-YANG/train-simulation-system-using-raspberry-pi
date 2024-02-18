#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <sys/time.h>

#define TRIGGER_PIN 4
#define ECHO_PIN 5

volatile struct timeval pulseStartTime;
volatile struct timeval pulseEndTime;

void handleEchoPulse() {
    if (digitalRead(ECHO_PIN) == HIGH) {
        gettimeofday(&pulseStartTime, NULL);
    } else {
        gettimeofday(&pulseEndTime, NULL);
    }
}

void setup() {
    wiringPiSetup();
    wiringPiSetupGpio();

    pinMode(TRIGGER_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    wiringPiISR(ECHO_PIN, INT_EDGE_BOTH, &handleEchoPulse);
}

float getDistance() {
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN, LOW);

    while (pulseStartTime.tv_sec == 0) {
        // Wait for the pulse start time to be set
    }

    while (pulseEndTime.tv_sec == 0) {
        // Wait for the pulse end time to be set
    }

    long pulseDuration = (pulseEndTime.tv_sec - pulseStartTime.tv_sec) * 1000000L +
                         pulseEndTime.tv_usec - pulseStartTime.tv_usec;

    float distance = pulseDuration * 0.034 / 2;

    pulseStartTime.tv_sec = 0;
    pulseStartTime.tv_usec = 0;
    pulseEndTime.tv_sec = 0;
    pulseEndTime.tv_usec = 0;

    return distance;
}

int main() {
    setup();

    while (1) {
        float distance = getDistance();
        printf("Distance: %.2f cm\n", distance);
        delay(1000);
    }

    return 0;
}
