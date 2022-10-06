//
// Created by Eric Zhao on 8/9/2022.
//

#include <math.h>
#include "trans.h"
#include "util.h"
#include "timer_list.h"
#include "global.h"
#include "tju_tcp.h"

uint32_t ack_id_hash[100000000];
uint64_t id_recv_time[100000];

pthread_cond_t packet_available = PTHREAD_COND_INITIALIZER;
pthread_mutex_t cond_mutex = PTHREAD_MUTEX_INITIALIZER;

time_list *timer_list = NULL;

void init_retransmit_timer() {
  timer_list = time_list_init();
  memset(ack_id_hash, 0, sizeof(ack_id_hash));
  memset(id_recv_time, 0, sizeof(id_recv_time));
  start_work_thread(timer_list);
  DEBUG_PRINT("retransmit thread initialized\n");
}

void re_calculate_rtt(double rtt, tju_tcp_t *tju_tcp) {
  double srtt = tju_tcp->window.wnd_send->estmated_rtt;
  tju_tcp->window.wnd_send->estmated_rtt = (RTT_ALPHA * rtt) + (1 - RTT_ALPHA) * srtt;
  tju_tcp->window.wnd_send->dev_rtt = (1 - RTT_BETA) * tju_tcp->window.wnd_send->dev_rtt + RTT_BETA *
      fabs(tju_tcp->window.wnd_send->estmated_rtt - rtt);
  tju_tcp->window.wnd_send->rto =
      min(RTT_UBOUND, tju_tcp->window.wnd_send->estmated_rtt + 4 * tju_tcp->window.wnd_send->dev_rtt);
  log_rtt_event(rtt,
                tju_tcp->window.wnd_send->estmated_rtt,
                tju_tcp->window.wnd_send->dev_rtt,
                tju_tcp->window.wnd_send->rto);
}

void *retransmit(retransmit_arg_t *args) {
  tju_tcp_t *tju_tcp = (tju_tcp_t *) args->sock;
  tju_packet_t *pkt = args->pkt;
  uint32_t id = set_timer_without_mutex(timer_list,
                                        0,
                                        SEC2NANO(tju_tcp->window.wnd_send->rto),
                                        (void *(*)(void *)) retransmit,
                                        args);
  uint16_t dlen = pkt->header.plen - pkt->header.hlen;
  ack_id_hash[pkt->header.seq_num + dlen + 1] = id;
  DEBUG_PRINT("transmit : set timer %d expecting ack: %d, timeout at %f\n",
              id,
              pkt->header.seq_num + dlen + 1,
              tju_tcp->window.wnd_send->rto);
  send_packet(pkt);
  return NULL;
}

uint32_t auto_retransmit(tju_tcp_t *sock, tju_packet_t *pkt, int requiring_ack) {
  uint32_t id = 0;
  retransmit_arg_t *args = malloc(sizeof(retransmit_arg_t));
  args->pkt = pkt;
  args->sock = sock;
  if (requiring_ack) {
    id = set_timer(timer_list, 0, SEC2NANO(sock->window.wnd_send->rto), (void *(*)(void *)) retransmit, args);
    uint16_t dlen = pkt->header.plen - pkt->header.hlen;
    ack_id_hash[pkt->header.seq_num + dlen + 1] = id;
    DEBUG_PRINT("set timer %d expecting ack: %d, timeout at %f\n",
                id,
                pkt->header.seq_num + dlen + 1,
                sock->window.wnd_send->rto);
  }
  send_packet(pkt);
  DEBUG_PRINT("Sending Packet: ack:%d, seq:%d\n", pkt->header.ack_num, pkt->header.seq_num);
  return id;
}

void free_retrans_arg(void *arg) {
  time_node *node = (time_node *) arg;
  timer_event *ptr = &node->event;
  uint64_t current_time = id_recv_time[node->id];
  uint64_t create_time = TO_NANO((*ptr->create_time));
  re_calculate_rtt((current_time - create_time) / 1000000000.0, ((retransmit_arg_t *) ptr->args)->sock);
  free(((retransmit_arg_t *) ptr->args)->pkt);
}

void on_ack_received(uint32_t ack, tju_tcp_t *sock, uint16_t rwnd) {

  // TODO: an ack should remove all the packets with seq_num+length < ack
  DEBUG_PRINT("ack received: %d\n", ack);
  sock->window.wnd_send->rwnd = rwnd;
  log_rwnd_event(rwnd);
  sock->window.wnd_send->window_size = umin(rwnd, INIT_SEND_WINDOW);
  log_swnd_event(sock->window.wnd_send->window_size);
  if (ack_id_hash[ack] != 0) {
    uint32_t tmp = sock->window.wnd_send->base;
    while (ack_id_hash[++tmp] == 0);
    if (tmp == ack) {
      sock->window.wnd_send->base = ack;
    }
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    uint64_t current_time = TO_NANO(now);
    id_recv_time[ack_id_hash[ack]] = current_time;
    cancel_timer(timer_list, ack_id_hash[ack], 1, free_retrans_arg);
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

