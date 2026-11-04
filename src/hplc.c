#include <string.h>
#include "hplc.h"
#include "lib/csv.h"
#include "lib/metadata.h"
#include "macro.h"

void hplc_destroy(struct hplc_data*data)
{
	csv_destroy(&data->csv);
	hplc_metadata_destroy(&data->data);
}
int hplc_column_phase(struct hplc_run_metadata*data)
{
	//TODO change to use db
	for(int i=0;i<SIZEOF(column_types);i++){
		if(!strcasecmp(data->column.clmn_phase,column_types[i]))
			return i;
	}
	return -1;
}
