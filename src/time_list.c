//
// Created by Eric Zhao on 9/9/2022.
//
#include "time.h"
#include "timer_list.h"
#include "stdlib.h"

#define TO_SEC(timespec) (timespec.tv_sec + timespec.tv_nsec / 1000000000.0)
#define TO_NANO(timespec) (timespec.tv_sec * 1000000000 + timespec.tv_nsec)
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

  struct timespec *now = malloc(sizeof(struct timespec));
  memset(now, 0, sizeof(struct timespec));
  clock_gettime(CLOCK_REALTIME, now);
  struct time_node *node = malloc(sizeof(struct time_node));
  now->tv_sec += sec;
  now->tv_nsec += nano_sec;
  node->event.timeout = now;
  node->event.callback = callback;
  node->event.args = args;
  node->id = list->id_pool++;
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

  struct timespec *now = malloc(sizeof(struct timespec));
  memset(now, 0, sizeof(struct timespec));
  clock_gettime(CLOCK_REALTIME, now);
  struct time_node *node = malloc(sizeof(struct time_node));
  now->tv_sec += sec;
  now->tv_nsec += nano_sec;
  node->event.timeout = now;
  node->event.callback = callback;
  node->event.args = args;
  node->id = list->id_pool++;
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
/*
 * check if any timer is timeout
 * invoke the callback function if timeout
 * return the callback function's return value
 * NULL if no timer is timeout
 */
void *check_timer(struct time_list *list) {
  if (list->head == NULL) {
    return NULL;
  }

  pthread_mutex_lock(&list->lock);

  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  uint64_t current_time = TO_NANO(now);
  uint64_t timeout = TO_NANO((*(list->head->event.timeout)));
  //DEBUG_PRINT("Currenttime: %lu, timeout: %lu", current_time, timeout);
  if (timeout <= current_time) {
    DEBUG_PRINT("Currenttime: %lu, timeout: %lu\n", current_time, timeout);
    struct time_node *tmp = list->head;
    list->head = list->head->next;
    DEBUG_PRINT("timer %d timeout\n", tmp->id);

    //invoke the callback function
    void *result = tmp->event.callback(tmp->event.args);

    free_node(tmp);
    list->size--;
    pthread_mutex_unlock(&list->lock);

    return result;
  }
  pthread_mutex_unlock(&list->lock);

  return NULL;
}

void free_node(struct time_node *tmp) {
  free(tmp->event.timeout);
  free(tmp);
}

/*
 * get the timeout of the first timer
 */
uint32_t get_recent_timeout(struct time_list *list) {
  if (list->head == NULL) {
    return 0;
  }

  pthread_mutex_lock(&list->lock);

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
      if (destroy) {
        des(tmp->event.args);
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