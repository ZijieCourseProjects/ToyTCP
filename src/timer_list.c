//
// Created by Eric Zhao on 9/9/2022.
//
#include <tgmath.h>
#include "time.h"
#include "../inc/timer_list.h"
#include "stdlib.h"
#include "../inc/tju_packet.h"
#include "../inc/util.h"

#define TO_TIMESPEC(nano) (struct timespec){.tv_sec = (time_t)(nano / 1000000000), .tv_nsec = (long)(nano % 1000000000)}
void free_node(struct time_node *tmp);

struct time_list *time_list_init() {
  struct time_list *list = malloc(sizeof(struct time_list));
  list->head = NULL;
  list->tail = NULL;
  list->size = 0;
  list->id_pool = 1;
  pthread_mutex_init(&list->lock, NULL);
  return list;
}

/*
 * set a timer with nano_sec timeout
 * callback is the function to be called when timeout
 * args is the arguments to be passed to the callback function
 * return timer id
 */
uint32_t set_timer(struct time_list *list, uint32_t sec, uint64_t nano_sec, void *(*callback)(void *), void *args) {

  pthread_mutex_lock(&list->lock);

  struct timespec *timeout = malloc(sizeof(struct timespec));
  struct timespec *now = malloc(sizeof(struct timespec));
  clock_gettime(CLOCK_REALTIME, timeout);
  clock_gettime(CLOCK_REALTIME, now);
  struct time_node *node = malloc(sizeof(struct time_node));
  memset(node, 0, sizeof(struct time_node));
  timeout->tv_sec += sec;
  timeout->tv_nsec += nano_sec;
  node->event.create_time = now;
  node->event.timeout = timeout;
  node->event.callback = callback;
  node->event.args = args;
  node->id = (list->id_pool++) % 100000 + 1;
  node->next = NULL;
  if (list->head == NULL) {
    list->head = node;
    list->tail = node;
  } else {
    list->tail->next = node;
    list->tail = node;
  }
  list->size++;
//  DEBUG_PRINT("set timer %d, timeout at %u\n", node->id, node->event.timeout);
  pthread_mutex_unlock(&list->lock);

  return node->id;
}
uint32_t set_timer_without_mutex(struct time_list *list,
                                 uint32_t sec,
                                 uint64_t nano_sec,
                                 void *(*callback)(void *),
                                 void *args) {

  struct timespec *timeout = malloc(sizeof(struct timespec));
  struct timespec *now = malloc(sizeof(struct timespec));
  clock_gettime(CLOCK_REALTIME, timeout);
  clock_gettime(CLOCK_REALTIME, now);
  struct time_node *node = malloc(sizeof(struct time_node));
  memset(node, 0, sizeof(struct time_node));
  timeout->tv_sec += sec;
  timeout->tv_nsec += nano_sec;
  node->event.timeout = timeout;
  node->event.create_time = now;
  node->event.callback = callback;
  node->event.args = args;
  node->id = (list->id_pool++) % 100000 + 1;
  node->next = NULL;
  if (list->head == NULL) {
    list->head = node;
    list->tail = node;
  } else {
    list->tail->next = node;
    list->tail = node;
  }
  list->size++;
//  DEBUG_PRINT("set timer %d, timeout at %u\n", node->id, node->event.timeout);
  return node->id;
}
extern uint64_t id_recv_time[100000];

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

void free_retrans_arg(void *arg) {
  time_node *node = (time_node *) arg;
  timer_event *ptr = &node->event;
  uint64_t current_time = id_recv_time[node->id];
  uint64_t create_time = TO_NANO((*ptr->create_time));
  re_calculate_rtt((current_time - create_time) / 1000000000.0, ((retransmit_arg_t *) ptr->args)->sock);
  free(((retransmit_arg_t *) ptr->args)->pkt);
}
extern bitmap *ackmap;
/*
 * check if any timer is timeout
 * invoke the callback function if timeout
 * return the callback function's return value
 * NULL if no timer is timeout
 */
void *check_timer(struct time_list *list) {
  pthread_mutex_lock(&list->lock);

  if (list->head == NULL) {
    pthread_mutex_unlock(&list->lock);
    return NULL;
  }

  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  uint64_t current_time = TO_NANO(now);
  uint64_t timeout = TO_NANO((*(list->head->event.timeout)));
  //DEBUG_PRINT("Currenttime: %lu, timeout: %lu", current_time, timeout);
  if (timeout <= current_time) {
    if (bitmap_read(ackmap, list->head->id)) {
      DEBUG_PRINT("timer received for %d, free pkt with seq: %d\n",
                  list->head->id,
                  ((tju_packet_t *) ((retransmit_arg_t *) list->head->event.args)->pkt)->header.seq_num);
      bitmap_clear(ackmap, list->head->id);
      free_retrans_arg(list->head);
      free_node(list->head);
    } else {
      DEBUG_PRINT("Currenttime: %lu, timeout: %lu\n", current_time, timeout);
      struct time_node *tmp = list->head;
      DEBUG_PRINT("timer %d timeout\n", tmp->id);
      ((retransmit_arg_t *) tmp->event.args)->sock->window.wnd_send->cwnd = 1;
      ((retransmit_arg_t *) tmp->event.args)->sock->window.wnd_send->cwnd_state = SLOW_START;
      log_cwnd_event(4, 1);
      //invoke the callback function
      void *result = tmp->event.callback(tmp->event.args);
      //TODO:Potential memory leak
      free_node(tmp);

    }
    list->head = list->head->next;
    if (list->head == NULL) {
      list->tail = NULL;
    }
    list->size--;

    pthread_mutex_unlock(&list->lock);

    return 0;
  }
  pthread_mutex_unlock(&list->lock);

  return NULL;
}

void free_node(struct time_node *tmp) {
  free(tmp->event.timeout);
  free(tmp->event.create_time);
  free(tmp);
}

/*
 * get the timeout of the first timer
 */
uint32_t get_recent_timeout(struct time_list *list) {
  pthread_mutex_lock(&list->lock);
  if (list->head == NULL) {
    pthread_mutex_unlock(&list->lock);
    return 0;
  }

  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  uint32_t current_time = TO_NANO(now);
  uint32_t timeout = TO_NANO((*(list->head->event.timeout))) - current_time;

  pthread_mutex_unlock(&list->lock);
  return timeout;
}

/*
 * cancel a timer
 * return 0 if success
 * return -1 if timer not found
 */
int cancel_timer(struct time_list *list, uint32_t id, int destroy, void (*des)(void *)) {
  DEBUG_PRINT("trying to cancel timer %d\n", id);
  pthread_mutex_lock(&list->lock);
  struct time_node *tmp = list->head;
  struct time_node *prev = NULL;
  while (tmp != NULL) {
    if (tmp->id == id) {
      if (prev == NULL) {
        list->head = tmp->next;
      } else {
        prev->next = tmp->next;
      }
      if (tmp == list->tail) {
        list->tail = prev;
      }
      if (destroy) {
        des(tmp);
      }
      free_node(tmp);
      list->size--;
      DEBUG_PRINT("timer %d canceled\n", id);
      pthread_mutex_unlock(&list->lock);
      return 0;
    }
    prev = tmp;
    tmp = tmp->next;
  }
  pthread_mutex_unlock(&list->lock);

  return -1;
}

int cancel_timer_until(struct time_list *list, int id) {
  pthread_mutex_lock(&list->lock);
  struct time_node *tmp = list->head;
  struct time_node *prev = NULL;
  while (tmp != NULL) {
    if (tmp->id <= id) {
      if (prev == NULL) {
        list->head = tmp->next;
      } else {
        prev->next = tmp->next;
      }
      free_node(tmp);
      list->size--;
      return 0;
    }
    prev = tmp;
    tmp = tmp->next;
  }
  pthread_mutex_unlock(&list->lock);
  return -1;
}

void immidiate_invoke_callback(struct time_list *list, uint32_t id) {
  pthread_mutex_lock(&list->lock);
  struct time_node *tmp = list->head;
  struct time_node *prev = NULL;
  while (tmp != NULL) {
    if (tmp->id == id) {
      if (prev == NULL) {
        list->head = tmp->next;
      } else {
        prev->next = tmp->next;
      }
      if (tmp == list->tail) {
        list->tail = prev;
      }
      tmp->event.callback(tmp->event.args);
      free_node(tmp);
      list->size--;
      DEBUG_PRINT("timer %d canceled\n", id);
      pthread_mutex_unlock(&list->lock);
      return;
    }
    prev = tmp;
    tmp = tmp->next;
  }
  pthread_mutex_unlock(&list->lock);
}