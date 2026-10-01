#ifndef HPLC_METADATA_H
#define HPLC_METADATA_H

#include <stdbool.h>
#include "hplc.h"

void hplc_metadata_destroy(struct hplc_run_metadata*data);
bool hplc_metadata(char*file_name,struct hplc_run_metadata*data);
void hplc_metadata_print(struct hplc_run_metadata*data);
const char*hplc_run_type_str(enum hplc_run_type type);
const char*hplc_clmn_brand_str(enum hplc_clmn_brand brand);

#endif
