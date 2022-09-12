//
// Created by Eric Zhao on 8/9/2022.
//

#include "trans.h"
#include "util.h"
#include "timer_list.h"
#include "global.h"
#include "tju_tcp.h"

extern double srtt;
extern double rto;

uint32_t ack_id_hash[1 << 16];

pthread_cond_t packet_available = PTHREAD_COND_INITIALIZER;
pthread_mutex_t cond_mutex = PTHREAD_MUTEX_INITIALIZER;

time_list *timer_list = NULL;

void init_retransmit_timer() {
  timer_list = time_list_init();
  memset(ack_id_hash, 0, sizeof(ack_id_hash));
  start_work_thread(timer_list);
  DEBUG_PRINT("retransmit thread initialized\n");
}

void re_calculate_rtt(double rtt) {
  srtt = (RTT_ALPHA * srtt) + (1 - RTT_ALPHA) * rtt;
  rto = min(RTT_UBOUND, max(RTT_LBOUND, RTT_BETA * srtt));
}

void *retransmit(tju_packet_t *pkt) {
  uint32_t id = set_timer_without_mutex(timer_list, 0, SEC2NANO(rto), (void *(*)(void *)) retransmit, pkt);
  uint16_t dlen = pkt->header.plen - pkt->header.hlen;
  ack_id_hash[pkt->header.seq_num + dlen + 1] = id;
  DEBUG_PRINT("retransmit packet\n");
  send_packet(pkt);
  return NULL;
}

uint32_t auto_retransmit(tju_packet_t *pkt, int requiring_ack) {
  uint32_t id = 0;
  if (requiring_ack) {
    id = set_timer(timer_list, 0, SEC2NANO(rto), (void *(*)(void *)) retransmit, pkt);
    uint16_t dlen = pkt->header.plen - pkt->header.hlen;
    ack_id_hash[pkt->header.seq_num + dlen + 1] = id;
    DEBUG_PRINT("set timer %d expecting ack: %d, timeout at %f\n", id, pkt->header.seq_num + dlen + 1, rto);
  }
  send_packet(pkt);
  DEBUG_PRINT("Sending Packet: ack:%d, seq:%d\n", pkt->header.ack_num, pkt->header.seq_num);
  return id;
}

void on_ack_received(uint32_t ack) {

  // TODO: an ack should remove all the packets with seq_num+length < ack
  DEBUG_PRINT("ack received: %d\n", ack);
  if (ack_id_hash[ack] != 0) {
    cancel_timer(timer_list, ack_id_hash[ack]);
    ack_id_hash[ack] = 0;
  }
}

void *transit_work_thread(time_list *list) {
  DEBUG_PRINT("transit work thread start\n");
  struct timespec spec;
  spec = TO_TIMESPEC(get_recent_timeout(list));
  for (;;) {
    pthread_mutex_lock(&cond_mutex);
    pthread_cond_timedwait(&packet_available, &cond_mutex, &spec);
    pthread_mutex_unlock(&cond_mutex);
    check_timer(list);
    spec = TO_TIMESPEC(get_recent_timeout(list));
  }
}

pthread_t start_work_thread(time_list *list) {
  pthread_t work_thread;
  pthread_create(&work_thread, NULL, (void *(*)(void *)) transit_work_thread, list);
  return work_thread;
}

