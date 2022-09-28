//
// Created by Eric Zhao on 22/9/2022.
//

#include "stdio.h"
#include "../inc/logger.h"
#include "string.h"
#include "../inc/global.h"
#include "../inc/tju_packet.h"
#include <sys/time.h>
#include "pthread.h"

FILE *log_file;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void init_logger() {
  //hostname
  char hostname[256];
  gethostname(hostname, 256);
  strcat(hostname, ".log");
  remove(hostname);
  log_file = fopen(hostname, "w");
}

long getCurrentTime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void log_send_event(uint32_t seq, uint32_t ack, uint32_t flag) {
  pthread_mutex_lock(&log_mutex);
  char msg[32];
  memset(msg, 0, sizeof(msg));
  if (flag & SYN_FLAG_MASK) strcat(msg, "SYN");
  if (flag & FIN_FLAG_MASK) strcat(msg, "|FIN");
  if (flag & ACK_FLAG_MASK) strcat(msg, "|ACK");
  fprintf(log_file, "[SEND] [%ld] [sqe:%d ack:%d flag:%s]\n", getCurrentTime(), seq, ack, msg);
  pthread_mutex_unlock(&log_mutex);
}
void log_recv_event(uint32_t seq, uint32_t ack, uint32_t flag) {
  pthread_mutex_lock(&log_mutex);
  char msg[32];
  memset(msg, 0, sizeof(msg));
  if (flag & SYN_FLAG_MASK) strcat(msg, "SYN");
  if (flag & FIN_FLAG_MASK) strcat(msg, "|FIN");
  if (flag & ACK_FLAG_MASK) strcat(msg, "|ACK");
  fprintf(log_file, "[RECV] [%ld] [sqe:%d ack:%d flag:%s]\n", getCurrentTime(), seq, ack, msg);
  pthread_mutex_unlock(&log_mutex);
}

void log_cwnd_event(int type, int size) {
  pthread_mutex_lock(&log_mutex);
  fprintf(log_file, "[CWND] [%ld] [type:%d size:%d]\n", getCurrentTime(), type, size);
  pthread_mutex_unlock(&log_mutex);
}
void log_rwnd_event(int size) {
  pthread_mutex_lock(&log_mutex);
  fprintf(log_file, "[RWND] [%ld] [size:%d]\n", getCurrentTime(), size);
  pthread_mutex_unlock(&log_mutex);
}
void log_swnd_event(int size) {
  pthread_mutex_lock(&log_mutex);
  fprintf(log_file, "[SWND] [%ld] [size:%d]\n", getCurrentTime(), size);
  pthread_mutex_unlock(&log_mutex);
}

void close_logger() {
  fclose(log_file);
}