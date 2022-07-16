#!/bin/bash

# retrieving the log data
log="./tmp/log.txt"

echo ""
echo "########## ANALYSIS SCRIPT ##########"
echo ""
echo "----------"


# printing the number and average size of read operations
reads=($(grep -oP "read_size\s+\K\d+" ${log}))
read_ops=0
read_total_sum=0
for i in ${reads[@]}; do
	let read_total_sum+=$i
	let read_ops+=1
done

readns=($(grep -oP "read_n\s+\K\d+" ${log}))
for i in ${readns[@]}; do
	let read_ops+=$i
done

read_avg_size=$((read_total_sum/read_ops))

echo "Number of read operations: ${read_ops}"
echo "Average size of read operations: ${read_avg_size}"
echo "----------"


# printing the number and average size of write operations
writes=($(grep -oP "write_size\s+\K\d+" ${log}))
write_ops=0
write_total_sum=0
for i in ${writes[@]}; do
	let write_total_sum+=$i
	let write_ops+=1
done

write_avg_size=$((write_total_sum/write_ops))

echo "Number of write operations: ${write_ops}"
echo "Average size of write operations: ${write_avg_size}"
echo "----------"


# printing the number of lock and open-lock operations
locks=($(grep -oP " locked\s+\K\w+" ${log}))
lock_ops=0
for i in ${locks[@]}; do
	let lock_ops+=1
done

open_locks=($(grep -oP "created\s+\K\w+" ${log}))
open_lock_ops=0
for i in ${open_locks[@]}; do
	let open_lock_ops+=1
done

final_lock_ops=$((lock_ops-open_lock_ops))

echo "Number of lock operations: ${final_lock_ops}"
echo "Number of open-lock operations: ${open_lock_ops}"


# printing the number of unlock operations
unlocks=($(grep -oP "unlocked\s+\K\w+" ${log}))
unlock_ops=0
for i in ${unlocks[@]}; do
	let unlock_ops+=1
done

echo "Number of (EXPLICIT) unlock operations: ${unlock_ops}"
echo "----------"


# printing the number of close operations
closes=($(grep -oP "closeFile:\s+\K\w+" ${log}))
close_ops=0
for i in ${closes[@]}; do
	let close_ops+=1
done

echo "Number of close operations: ${close_ops}"
echo "----------"


# printing the max cache's size (bytes)
max_byte_size=($(grep -oP "byte size:\s+\K\d+" ${log}))
let max_byte_size/=1000000
if [[ ${max_byte_size} -lt 1 ]]
then
	echo "Maximum cache's size (in MB): <1MB"
else
	echo "Maximum cache's size (in MB): ${max_byte_size}MB"
fi


# printing the max cache's size (bytes)
max_file_size=($(grep -oP "file size:\s+\K\d+" ${log}))
echo "Maximum cache's size (in files): ${max_file_size}"
echo "----------"


# printing the number of times the replacement algorithm expelled a file
expelled=($(grep -oP "expelled\s+\K\w+" ${log}))
files_expelled=0
for i in ${expelled[@]}; do
	let files_expelled+=1
done

echo "Number of times the cache has expelled a file: ${files_expelled}"
echo "----------"


# printing the number of requests served by each worker
served=($(grep -oP "WORKER\s+\K\d+" ${log}))
for i in ${served[@]}; do
	req_served=($(grep -oP "WORKER $i served\s+\K\d+" ${log}))
	echo "Worker $i served ${req_served} requests"
done
echo "----------"


# printing the maximum number of clients connected at the same time
conn=($(grep -oP "time:\s+\K\d+" ${log}))
echo "Maximum number of clients connected at the same time: ${conn}"
echo "----------"
echo ""

echo "########## END OF ANALYSIS SCRIPT ##########"
