//
// Created by Eric Zhao on 8/9/2022.
//

#ifndef TJU_TCP_SRC_TRANS_H_
#define TJU_TCP_SRC_TRANS_H_
#include "global.h"
#include "tju_packet.h"

uint32_t auto_retransmit(tju_tcp_t *, tju_packet_t *pkt, int requiring_ack);
void on_ack_received(uint32_t ack, tju_tcp_t *sock, uint16_t rwnd);
void *transit_work_thread(time_list *list);
pthread_t start_work_thread(time_list *list);
void init_retransmit_timer();

#endif //TJU_TCP_SRC_TRANS_H_
