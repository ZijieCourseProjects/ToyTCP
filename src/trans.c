//
// Created by Eric Zhao on 8/9/2022.
//

#include "trans.h"
#include "global.h"

extern double srtt;

int auto_retransmit(int (*judge)()) {

}

void re_calculate_rtt(double rtt) {
  srtt = (RTT_ALPHA * srtt) + (1 - RTT_ALPHA) * rtt;

}