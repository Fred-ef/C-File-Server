SHELL		= /bin/bash
CC		= gcc
INCLUDES	= -I ./headers
THREADS	= -pthread
FLAGS		= -g -Wall -pedantic 
tmp		= ./expelled ./saved ./tmp

client_dep	= ./src/client_API.c ./src/part_rw_sol.c ./src/parser.c \
			./src/err_cleanup.c ./src/util.c ./src/linkedlist.c \
			./src/conc_elem.c

serv_dep	= ./src/serv_worker.c ./src/serv_logger.c \
			./src/conc_fifo.c ./src/conc_elem.c ./src/conc_hash.c \
			./src/linkedlist.c ./src/sc_cache.c ./src/part_rw_sol.c \
			./src/parser.c ./src/err_cleanup.c

.PHONY		: all clean cleanall test1 test2 test3


all		:
		make clean
		-mkdir -p $(tmp)
		make client;
		make server;
		
client		: ./src/client.c
		$(CC) $(FLAGS) $(client_dep) $< $(INCLUDES) $(THREADS) -o $@

server		: ./src/serv_manager.c
		$(CC) $(FLAGS) $(serv_dep) $< $(INCLUDES) $(THREADS) -o $@


test1		:
		make all;
		chmod +x script/test1.sh
		chmod +x script/analysis.sh
		script/test1.sh
		script/analysis.sh

test2		:
		make all;
		chmod +x script/test2.sh
		chmod +x script/analysis.sh
		script/test2.sh
		script/analysis.sh

test3		:
		make all;
		chmod +x script/test3.sh
		chmod +x script/analysis.sh
		script/test3.sh
		script/analysis.sh


clean		:
		-rm -r -d $(tmp) ./client ./server

cleanall	:
		-rm -r -d $(tmp) ./client ./server
