//
// Created by Eric Zhao on 8/9/2022.
//

#include <math.h>
#include "trans.h"
#include "util.h"
#include "timer_list.h"
#include "logger.h"
#include "global.h"
#include "tju_tcp.h"

uint32_t ack_id_hash[100000000];
uint64_t id_recv_time[100000];

pthread_cond_t packet_available = PTHREAD_COND_INITIALIZER;
pthread_mutex_t cond_mutex = PTHREAD_MUTEX_INITIALIZER;

time_list *timer_list = NULL;
extern bitmap *ackmap;

void set_ack_id_hash(uint32_t ack, uint32_t id) {
  ack_id_hash[ack % 100000000] = id;
}

uint32_t get_ack_id_hash(uint32_t ack) {
  return ack_id_hash[ack % 100000000];
}

void init_retransmit_timer() {
  timer_list = time_list_init();
  start_work_thread(timer_list);
  memset(ack_id_hash, 0, sizeof(ack_id_hash));
  memset(id_recv_time, 0, sizeof(id_recv_time));
  DEBUG_PRINT("retransmit thread initialized\n");
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
  set_ack_id_hash(pkt->header.seq_num + dlen + 1, id);
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
    set_ack_id_hash(pkt->header.seq_num + dlen + 1, id);
    DEBUG_PRINT("set timer %d expecting ack: %d, timeout at %f\n",
                id,
                pkt->header.seq_num + dlen + 1,
                sock->window.wnd_send->rto);
  }
  send_packet(pkt);
  DEBUG_PRINT("Sending Packet: ack:%d, seq:%d\n", pkt->header.ack_num, pkt->header.seq_num);
  return id;
}

void on_ack_received(uint32_t ack, tju_tcp_t *sock, uint16_t rwnd) {

  switch (sock->window.wnd_send->cwnd_state) {
    case SLOW_START:
      if (sock->window.wnd_send->cwnd < sock->window.wnd_send->ssthresh) {
        sock->window.wnd_send->cwnd += 1;
        log_cwnd_event(SLOW_START, sock->window.wnd_send->cwnd);
      } else {
        sock->window.wnd_send->cwnd_state = CONGESTION_AVOIDANCE;
      }
      break;
    case CONGESTION_AVOIDANCE:
      if (ack == sock->window.wnd_send->dup_ack) {
        if (sock->window.wnd_send->dup_ack_count == -1)
          break;
        sock->window.wnd_send->dup_ack_count++;
        if (sock->window.wnd_send->dup_ack_count == 3) {
          sock->window.wnd_send->ssthresh = sock->window.wnd_send->cwnd / 2;
          sock->window.wnd_send->cwnd = sock->window.wnd_send->ssthresh + 3;
          log_cwnd_event(FAST_RECOVERY, sock->window.wnd_send->cwnd);
          sock->window.wnd_send->dup_ack_count = -1;
          immidiate_invoke_callback(timer_list, get_ack_id_hash(ack));
          break;
        }
      } else {
        if (ack != sock->window.wnd_send->base) {
          sock->window.wnd_send->dup_ack = ack;
          sock->window.wnd_send->dup_ack_count = 1;
        } else {
          sock->window.wnd_send->cwnd_congestion_count += 1;
          if (sock->window.wnd_send->cwnd_congestion_count >= sock->window.wnd_send->cwnd) {
            sock->window.wnd_send->cwnd += 1;
            log_cwnd_event(CONGESTION_AVOIDANCE, sock->window.wnd_send->cwnd);
            sock->window.wnd_send->cwnd_congestion_count = 0;
          }
        }
      }
      break;
  }

  // TODO: an ack should remove all the packets with seq_num+length < ack
  DEBUG_PRINT("ack received: %d\n", ack);
  sock->window.wnd_send->rwnd = rwnd;
  log_rwnd_event(rwnd);
  sock->window.wnd_send->window_size = umin(rwnd, sock->window.wnd_send->cwnd);
  log_swnd_event(sock->window.wnd_send->window_size);
  if (get_ack_id_hash(ack) != 0) {
    uint32_t tmp = sock->window.wnd_send->base;
    while (get_ack_id_hash(++tmp) == 0);
    if (tmp == ack) {
      sock->window.wnd_send->base = ack;
      DEBUG_PRINT("base updated to %d\n", ack);
    }
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    uint64_t current_time = TO_NANO(now);
    id_recv_time[get_ack_id_hash(ack)] = current_time;
    bitmap_set(ackmap, get_ack_id_hash(ack));
    //cancel_timer(timer_list, get_ack_id_hash(ack), 1, free_retrans_arg);
    set_ack_id_hash(ack, 0);
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

