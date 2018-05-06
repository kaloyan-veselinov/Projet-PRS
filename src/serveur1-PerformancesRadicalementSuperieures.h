#ifndef _SERVEUR1_H
#define _SERVEUR1_H

#include "socket.h"

void reinit_sgmt_ack_buff(int sgmt_acknoledged_buff[BUFFER_SIZE],
                          int sequence_nb,
                          int window);

size_t load_sgmt(FILE *file,
                 char  buff[RCVSIZE],
                 int   sequence_nb);

int send_sgmts(FILE          *file,
               int            data_desc,
               char           segments[BUFFER_SIZE][RCVSIZE],
               size_t *bytes_read,
               struct timeval snd_time[BUFFER_SIZE],
               int           *last_loaded_sgmt,
               int            sequence_nb,
               int            window);

int rcv_ack(FILE          *file,
            int            data_desc,
            char           segments[BUFFER_SIZE][RCVSIZE],
            size_t *bytes_read,
            struct timeval snd_time[BUFFER_SIZE],
            int            sgmt_acknoledged_buff[BUFFER_SIZE],
            int           *last_loaded_sgmt,
            int            sequence_nb,
            int            nb_ack_to_rcv,
            long          *srtt,
            long          *rttvar);

#endif // ifndef _SERVEUR1_H
