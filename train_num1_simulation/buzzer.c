#include <stdio.h>
#include <wiringPi.h>
#include <unistd.h>


#define BUZZER_PIN 18


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

int main(void){
    if(wiringPiSetupGpio() == -1){
        perror("Failed to initailize wiringPi.\n");
        return 1;
    }

    pinMode(BUZZER_PIN, OUTPUT);

        // Play different tones
    playTone(262, 500);   // C4
    playTone(294, 500);   // D4
    playTone(330, 500);   // E4
    playTone(349, 500);   // F4
    playTone(392, 500);   // G4
    playTone(440, 500);   // A4
    playTone(494, 500);   // B4
    playTone(523, 500);   // C5

    return 0;
}