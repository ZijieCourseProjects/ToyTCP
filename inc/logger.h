//
// Created by Eric Zhao on 22/9/2022.
//

#ifndef TJU_TCP_SRC_LOGGER_H_
#define TJU_TCP_SRC_LOGGER_H_

#include <stdio.h>

void init_logger();
void close_logger();
extern FILE *log_file;

#define LOG_PRINT(...) \
    do {                 \
        if(DEBUG){       \
        fprintf(log_file, "DEBUG MESSAGE: %s:%d:%s(): ", __FILE__, \
                __LINE__, __func__); \
        fprintf(log_file, __VA_ARGS__);}             \
        } while (0)

#endif //TJU_TCP_SRC_LOGGER_H_
