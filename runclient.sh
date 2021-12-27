#!/bin/sh
PORT=5000
IP="127.0.0.1"

make client
./client $IP $PORT
