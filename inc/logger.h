//
// Created by Eric Zhao on 22/9/2022.
//

#ifndef TJU_TCP_SRC_LOGGER_H_
#define TJU_TCP_SRC_LOGGER_H_

#include <stdio.h>
#include "global.h"

void init_logger();
void close_logger();

void log_recv_event(uint32_t seq, uint32_t ack, uint32_t flag);
void log_send_event(uint32_t seq, uint32_t ack, uint32_t flag);
void log_rwnd_event(uint16_t size);
void log_rtt_event(double sample_rtt, double estimated_rtt, double dev_rtt, double timeout);
void log_cwnd_event(int type, int size);
void log_swnd_event(uint32_t size);
void log_delv_event(uint32_t seq, uint32_t size);

extern FILE *log_file;

#endif //TJU_TCP_SRC_LOGGER_H_
