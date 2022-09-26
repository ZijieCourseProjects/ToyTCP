//
// Created by Eric Zhao on 22/9/2022.
//

#include "stdio.h"
#include "../inc/logger.h"
#include "string.h"
#include "../inc/global.h"
#include "../inc/tju_packet.h"
#include <sys/time.h>

FILE *log_file;

void init_logger() {
  log_file = fopen("log.txt", "w");
}

long getCurrentTime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void log_send_event(int seq, int ack, int flag) {
  char msg[32];
  if (flag & SYN_FLAG_MASK) strcat(msg, "SYN");
  if (flag & FIN_FLAG_MASK) strcat(msg, "|FIN");
  if (flag & ACK_FLAG_MASK) strcat(msg, "|ACK");
  fprintf(log_file, "sqe:%d ack:%d flag:%s", seq, ack, msg);
}

void log_recv_event(int seq, int ack, int flag) {

}

void log_cwnd_event(int type, int size) {

}

void close_logger() {
  fclose(log_file);
}