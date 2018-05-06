#ifndef _SERVEUR1_H
#define _SERVEUR1_H

#include "socket.h"

void reinit_sgmt_ack_buff(SEGMENT segments[BUFFER_SIZE],
                          unsigned int next_segment,
                          unsigned int cwnd);

size_t load_sgmt(FILE *file,
                 char buff[RCVSIZE],
                 int next_segment);

void handle_client(int data_desc, long srtt, long rttvar);

#endif // ifndef _SERVEUR1_H
