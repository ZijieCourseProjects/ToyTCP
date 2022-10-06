//
// Created by Eric Zhao on 27/9/2022.
//

#include "../inc/list.h"

struct list *list_init() {
  struct list *list = malloc(sizeof(struct list));
  list->head = NULL;
  list->tail = NULL;
  list->size = 0;
  list->max_size = RECV_LIST_MAX_SIZE;
  return list;
}

uint16_t get_list_remain_size(struct list *list) {
  return list->max_size - list->size;
}

void list_push(struct list *list, uint32_t seq, void *pkt) {
  struct node *node = malloc(sizeof(struct node));
  node->seq = seq;
  node->pkt = pkt;
  node->next = NULL;
  if (list->head == NULL) {
    list->head = node;
    list->tail = node;
  } else {
    list->tail->next = node;
    list->tail = node;
  }
  list->size++;
}

void *list_pop(struct list *list, uint32_t seq) {
  struct node *tmp = list->head;
  struct node *pre = NULL;
  while (tmp != NULL) {
    if (tmp->seq == seq) {
      if (pre == NULL) {
        list->head = tmp->next;
      } else {
        pre->next = tmp->next;
      }
      if (tmp == list->tail) {
        list->tail = pre;
      }
      list->size--;
      void *pkt = tmp->pkt;
      free(tmp);
      return pkt;
    }
    pre = tmp;
    tmp = tmp->next;
  }
  return NULL;
}