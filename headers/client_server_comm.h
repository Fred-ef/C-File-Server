/* DESCRIZIONE HEADER COMUNICAZIONI CLIENT-SERVER */

#ifndef client_server_comm_h
#define client_server_comm_h

typedef enum {OPEN_F=1, READ_F, READ_N_F, WRITE_F, WRITE_F_APP, LOCK_F, UNLOCK_F, RM_F, CLOSE_F, OPEN_C_F, OPEN_L_F} op_code;
typedef enum {O_CREATE=1, O_LOCK=2} flags;

#endif // client_server_comm_h