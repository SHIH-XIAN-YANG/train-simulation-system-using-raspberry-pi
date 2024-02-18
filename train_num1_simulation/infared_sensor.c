#include <stdio.h>
#include <wiringPi.h>

// GPIO pin number
#define GPIO_PIN 4

// Interrupt service routine
void handleInterrupt() {
    printf("Interrupt occurred!\n");
    // Handle the interrupt event here
}

int main() {
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

    // Main program loop
    while (1) {
        // Your main program logic goes here
        // It will continue to run while the interrupt handler responds to interrupts
    }

    return 0;
}
