#ifndef HPLC_LOGIC_H
#define HPLC_LOGIC_H

#include "hplc.h"
enum selectivity_action{
ERROR,
EFFICIENCY,
SUCCESS,
CITEMP,
CDTEMP,
CSOL,
CLIG,
CIB,
CDB,
CDFLOW,
CIFLOW
};
struct benchmarks{
        int peaks;
        double resolution;
        double runtime;
        int backpressure;
};

enum selectivity_action logic(struct hplc_data*data,struct benchmarks*bench,void**action_data);

#endif
