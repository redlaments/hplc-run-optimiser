#ifndef HPLC_LOGIC_H
#define HPLC_LOGIC_H

#include <stdbool.h>
#include "hplc.h"

enum selectivity_action{
END,//used to term arrays, use optional
ERROR,
MULTI,//multiple options, select one
SMULT,//multiple actions to be done
SUCCESS,
SPP,//coreshell
CITEMP,
CDTEMP,
CSOL,
CLIG,
CCL,//change column length
CDPS,//particle size
CIB,
CDB,
CDFLOW,
CIFLOW
};
struct benchmarks{
        int peaks;
        hplc_sint resolution;
        hplc_sint runtime;
        int backpressure;
};
/*arg is value associated with action*/
struct action_arg{
	enum selectivity_action action;
	void*arg;
};
struct pred_chr{
	int peaks;
	hplc_sint rs;
	hplc_sint rt;
	hplc_sint bp;
};
enum selectivity_action logic(struct hplc_data*data,struct benchmarks*bench,void**action_data);
/*Resolves selectivity_action into a char*/
const char*resolve_action_name(enum selectivity_action action);
/*fill chr with current run data*/
bool predicted_chr(struct pred_chr*chr,enum selectivity_action action,void*old,void*new);
#endif
