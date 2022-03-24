#!/bin/bash

# test1, execute with valgring and config1.txt
./server ./config/config2.txt &
SRV_PID=$!
export SRV_PID

bash -c 'sleep 5 && kill -s SIGHUP ${SRV_PID}' &

# first command
./client -f ./tmp/fileserver.sk -W ./files/file1.txt,./files/file2.txt -p -W ./files/file1.txt,./files/file2.txt -W ./files/file1.txt,./files/file2.txt -W ./files/file1.txt,./files/file2.txt

# second command
./client -f ./tmp/fileserver.sk -r ./files/file1.txt,./files/file2.txt -w ./files/ -p -c ./files/file3.txt -d ./saved/ -W ./files/file1.txt,./files/file2.txt

#third command
./client -f ./tmp/fileserver.sk -W ./files/file1.txt,./files/file2.txt -p -R n=0 -l ./files/file1.txt,./files/file2.txt -d ./saved/

#fourth command
./client -f ./tmp/fileserver.sk -w ./files/ -p -R n=0 -l ./files/file1.txt,./files/file2.txt -W ./files/file1.txt,./files/file2.txt -W ./files/file1.txt,./files/file2.txt

#fifth command
./client -f ./tmp/fileserver.sk -l ./files/file1.txt,./files/file2.txt -u ./files/file1.txt,./files/file2.txt -p -W ./files/file3.txt,./files/file2.txt -W ./files/file3.txt -W ./files/file3.txt -R n=0

exit 0
