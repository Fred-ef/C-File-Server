#!/bin/bash

# test1, execute with valgring and config1.txt
./server ./config/config2.txt &
SRV_PID=$!
export SRV_PID

bash -c 'sleep 5 && kill -s SIGHUP ${SRV_PID}' &

# first command
./client -f ./tmp/fileserver.sk -W ./files/file1.txt,./files/file2.txt -u ./files/file1.txt,./files/file2.txt -p -W ./files/file1.txt,./files/file2.txt -W ./files/file1.txt,./files/file2.txt -W ./files/file1.txt,./files/file2.txt -D ./expelled/

# second command
./client -f ./tmp/fileserver.sk -r ./files/file2.txt -W ./files/file3.txt -u ./files/file3.txt -p -c ./files/file3.txt -d ./saved/ -W ./files/file1.txt -D ./expelled/

#third command
./client -f ./tmp/fileserver.sk -W ./files/file1.txt,./files/file2.txt -p -R n=0 -l ./files/file2.txt -d ./saved/ -D ./expelled/

#fourth command
./client -f ./tmp/fileserver.sk -d ./saved/ -w ./files/ -p -R n=0 -l ./files/file1.txt,./files/file2.txt -W ./files/file1.txt,./files/file3.txt -W ./files/file1.txt,./files/file3.txt -D ./expelled/

#fifth command
./client -f ./tmp/fileserver.sk -l ./files/file1.txt,./files/file2.txt -u ./files/file1.txt,./files/file2.txt -p -W ./files/file3.txt,./files/file2.txt -W ./files/file3.txt -W ./files/file3.txt -R n=0 -D ./expelled/ -d ./saved/

exit 0
