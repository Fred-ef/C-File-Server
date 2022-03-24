# makefile

gcc -ggdb3 -Wall -pedantic serv_manager.c serv_worker.c serv_logger.c conc_fifo.c conc_elem.c conc_hash.c linkedlist.c sc_cache.c part_rw_sol.c parser.c err_cleanup.c -I ../headers -pthread -o server.out

valgrind --leak-check=full ./server ./config/config1.txt

gcc -Wall -pedantic client.c client_API.c  part_rw_sol.c parser.c err_cleanup.c util.c linkedlist.c conc_elem.c -I ../headers -pthread -o client.out
