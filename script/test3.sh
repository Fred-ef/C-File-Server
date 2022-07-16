#!/bin/bash

# commands used for the stress test
stress_test="-f ./tmp/fileserver.sk -d ./saved/ -D ./expelled/ \
-w ./files/ \
-r ./files/file1.txt,./files/file2.txt,./files/file3.txt,./files/file4.txt,./files/file5.txt,./files/file6.txt \
-R n=0"

# specifying an end time for the generation loop
endtime=$((SECONDS+30))

# test3, executed with config3.txt
./server ./config/config3.txt &
SRV_PID=$!
export SRV_PID

bash -c 'sleep 30 && kill -s SIGINT ${SRV_PID}' &

# creating PIDs array to keep track of the client processes
PIDS=()

# executing first client to upload files onto the server and unlock them
./client -f ./tmp/fileserver.sk -d ./saved/ -D ./expelled/ -w ./files/

# executing client processes
while [ $SECONDS -lt $endtime ]; do
	for i in {0..10}; do
		./client ${stress_test} &
		sleep 0.09
	done
done

# joining and exiting
wait ${SRV_PID}
exit 0
