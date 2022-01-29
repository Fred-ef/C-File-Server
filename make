# makefile

gcc -Wall -pedantic serv_manager.c serv_worker.c conc_fifo.c conc_elem.c conc_hash.c linkedlist.c sc_cache.c part_rw_sol.c parser.c err_cleanup.c -I ../headers -pthread -o server.out

./server.out ../config.txt

gcc -Wall -pedantic client.c client_API.c  part_rw_sol.c parser.c err_cleanup.c util.c conc_fifo.c conc_elem.c -I ../headers -pthread -o client.out
