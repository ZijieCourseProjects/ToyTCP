//
// Created by Eric Zhao on 8/9/2022.
//

#ifndef TJU_TCP_INC_DEBUG_H_
#define TJU_TCP_INC_DEBUG_H_

#include "stdio.h"
#include "pthread.h"
extern pthread_mutex_t print_lock;

//debug print macro with function name
#define DEBUG_PRINT(...) \
    do {                 \
        if(1){           \
        pthread_mutex_lock(&print_lock);                \
        fprintf(stderr, "DEBUG MESSAGE: %s:%d:%s(): ", __FILE__, \
                __LINE__, __func__); \
        fprintf(stderr, __VA_ARGS__);}             \
        pthread_mutex_unlock(&print_lock);                \
        } while (0)

#endif //TJU_TCP_INC_DEBUG_H_
