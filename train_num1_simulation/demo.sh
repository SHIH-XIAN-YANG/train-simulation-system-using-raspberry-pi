#!/bin/bash

SESSION="final"
PORT=8000
IP_addr="192.168.0.225"
trainID=1

make clean
make

g++ -o ./RPi-RFID/rfid_read ./RPi-RFID/MFRC522.cpp ./RPi-RFID/Read.cpp -std=c++11 -lbcm2835

insmod motor_rotation_module.ko
gpio export 4 out

tmux has-session -t $SESSION 2>/dev/null

if [[ $? -eq 0 ]]
then
	tmux kill-session -t $SESSION
fi

tmux new-session -d -s $SESSION

tmux split-window -h



tmux send-keys -t 0 "./train $IP_addr $PORT" C-m
sleep 2

tmux send-keys -t 1 "./RPi-RFID/rfid_read $IP_addr $PORT" C-m

tmux send-keys -t 1 "train 1 is ready"

tmux -2 attach-session -t $SESSION