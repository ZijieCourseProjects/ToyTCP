//
// Created by Eric Zhao on 8/9/2022.
//

#ifndef TJU_TCP_INC_DEBUG_H_
#define TJU_TCP_INC_DEBUG_H_

#define DEBUG 1

//debug print macro with function name
#define DEBUG_PRINT(...) \
    do {                 \
        if(DEBUG){                 \
        fprintf(stderr, "DEBUG MESSAGE: %s:%d:%s(): ", __FILE__, \
                __LINE__, __func__); \
        fprintf(stderr, __VA_ARGS__);}             \
        } while (0)

#endif //TJU_TCP_INC_DEBUG_H_
