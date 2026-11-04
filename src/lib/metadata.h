#ifndef HPLC_METADATA_H
#define HPLC_METADATA_H

#include <stdbool.h>
#include "hplc.h"
#include "hplc-types.h"

void hplc_metadata_destroy(struct hplc_run_metadata*data);
bool hplc_brand_id(char s,int*id);
bool hplc_metadata(char*file_name,struct hplc_run_metadata*data);
void hplc_metadata_print(struct hplc_run_metadata*data);
const char*hplc_run_type_str(enum hplc_run_type type);
char*hplc_clmn_brand_str(int brand);

/*returns the length of the next longest column with current specifications
 * otherwise returns 0*/
hplc_sint hplc_clmn_long(struct hplc_column*clmn);
/*returns next smallest ps size or 0 if unavailable*/
hplc_sint hplc_clmn_ps_small(struct hplc_column*clmn);
#endif
