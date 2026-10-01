#ifndef HPLC_CSV_H
#define HPLC_CSV_H

#include <stdio.h>
#include <stdbool.h>
#include "hplc.h"

/*included for testing reasons*/
int tokenize_str(char*str,int*i,int size,char*argv[],int argv_size);

void csv_print_file_data(void);
void csv_print_peaks(struct hplc_csv*csv);
void csv_print_pumps(struct hplc_csv*csv);
void csv_print_chromatogram(struct hplc_csv*csv);
void csv_print(struct hplc_csv*csv);
/*public*/
void csv_destroy(struct hplc_csv*csv);
bool csv_parse_name(char*file,struct hplc_data*data);
bool csv_parse_file(FILE*f,struct hplc_data*hdata);
void csv_parse_names(char*file_name);

double resolve_runtime(const struct hplc_csv*csv);
double minimum_res(const struct hplc_csv*csv);
double max_backpressure(const struct hplc_csv*csv);

#endif
