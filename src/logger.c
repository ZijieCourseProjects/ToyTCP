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
  strcat(hostname, ".event.trace");
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
  if (flag & FIN_FLAG_MASK) {
    if (strlen(msg) > 0) strcat(msg, "|");
    strcat(msg, "FIN");
  }
  if (flag & ACK_FLAG_MASK) {
    if (strlen(msg) > 0) strcat(msg, "|");
    strcat(msg, "ACK");
  }

  fprintf(log_file, "[%ld] [SEND] [seq:%d ack:%d flag:%s]\n", getCurrentTime(), seq, ack, msg);
  fflush(log_file);
  pthread_mutex_unlock(&log_mutex);
}
void log_recv_event(uint32_t seq, uint32_t ack, uint32_t flag) {
  pthread_mutex_lock(&log_mutex);
  char msg[32];
  memset(msg, 0, sizeof(msg));
  if (flag & SYN_FLAG_MASK) strcat(msg, "SYN");
  if (flag & FIN_FLAG_MASK) {
    if (strlen(msg) > 0) strcat(msg, "|");
    strcat(msg, "FIN");
  }
  if (flag & ACK_FLAG_MASK) {
    if (strlen(msg) > 0) strcat(msg, "|");
    strcat(msg, "ACK");
  }
  fprintf(log_file, "[%ld] [RECV] [seq:%d ack:%d flag:%s]\n", getCurrentTime(), seq, ack, msg);
  fflush(log_file);
  pthread_mutex_unlock(&log_mutex);
}

void log_cwnd_event(int type, int size) {
  pthread_mutex_lock(&log_mutex);
  fprintf(log_file, "[%ld] [CWND] [type:%d size:%d]\n", getCurrentTime(), type, size);
  pthread_mutex_unlock(&log_mutex);
}
void log_rwnd_event(uint16_t size) {
  pthread_mutex_lock(&log_mutex);
  fprintf(log_file, "[%ld] [RWND] [size:%d]\n", getCurrentTime(), size);
  pthread_mutex_unlock(&log_mutex);
}

void log_rtt_event(double sample_rtt, double estimated_rtt, double dev_rtt, double timeout) {
  pthread_mutex_lock(&log_mutex);
  fprintf(log_file,
          "[%ld] [RTTS] SampleRTT:%f EstimatedRTT:%f DeviationRTT:%f TimeoutInterval:%f\n",
          getCurrentTime(),
          sample_rtt * 1000, estimated_rtt * 1000, dev_rtt * 1000, timeout * 1000);
  pthread_mutex_unlock(&log_mutex);
}

void log_swnd_event(uint32_t size) {
  pthread_mutex_lock(&log_mutex);
  fprintf(log_file, "[%ld] [SWND] [size:%u]\n", getCurrentTime(), size);
  pthread_mutex_unlock(&log_mutex);
}
void log_delv_event(uint32_t seq, uint32_t size) {
  pthread_mutex_lock(&log_mutex);
  fprintf(log_file, "[%ld] [DELV] [seq:%u size:%u]\n", getCurrentTime(), seq, size);
  fflush(log_file);
  pthread_mutex_unlock(&log_mutex);
}

void close_logger() {
  fclose(log_file);
}