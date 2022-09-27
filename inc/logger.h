//
// Created by Eric Zhao on 22/9/2022.
//

#ifndef TJU_TCP_SRC_LOGGER_H_
#define TJU_TCP_SRC_LOGGER_H_

#include <stdio.h>

void init_logger();
void close_logger();

void log_recv_event(int seq, int ack, int flag);
void log_send_event(int seq, int ack, int flag);

extern FILE *log_file;

#endif //TJU_TCP_SRC_LOGGER_H_
