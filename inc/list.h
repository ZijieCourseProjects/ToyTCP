//
// Created by Eric Zhao on 27/9/2022.
//

#ifndef TJU_TCP_SRC_LIST_H_
#define TJU_TCP_SRC_LIST_H_

#include "global.h"

struct node {
  uint32_t seq;
  void *pkt;
  struct node *next;
};

struct list {
  struct node *head;
  struct node *tail;
  int size;
  int max_size;
};
uint16_t get_list_remain_size(struct list *list);

void *list_pop(struct list *list, uint32_t seq);
void list_push(struct list *list, uint32_t seq, void *pkt);
struct list *list_init();

#endif //TJU_TCP_SRC_LIST_H_
