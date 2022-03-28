#!/bin/bash

# test1, execute with valgring and config1.txt
valgrind -s --leak-check=full --show-leak-kinds=all ./server ./config/config1.txt &
SRV_PID=$!
export SRV_PID

bash -c 'sleep 10 && kill -s SIGHUP ${SRV_PID}' &

# first command
./client -f ./tmp/fileserver.sk -W ./files/file1.txt,./files/file2.txt -p -u ./files/file1.txt,./files/file2.txt -l ./files/file1.txt,./files/file2.txt -t 200

# second command
./client -f ./tmp/fileserver.sk -r ./files/file1.txt,./files/file2.txt -p -c ./files/file1.txt,./files/file2.txt -d ./saved/ -t 200

#third command
./client -f ./tmp/fileserver.sk -W ./files/file1.txt,./files/file2.txt -p -R n=0 -l ./files/file1.txt,./files/file2.txt -d ./saved/ -t 200

#fourth command
./client -f ./tmp/fileserver.sk -w ./files/ -p -R n=0 -l ./files/file1.txt,./files/file2.txt -t 200

#fifth command
./client -f ./tmp/fileserver.sk -l ./files/file1.txt,./files/file2.txt -p -R n=0 -u ./files/file1.txt,./files/file2.txt -d ./saved/ -t 200

exit 0
