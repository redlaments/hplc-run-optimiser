#include "hplc.h"
#include "lib/csv.h"
#include "lib/metadata.h"

void hplc_destroy(struct hplc_data*data)
{
	csv_destroy(&data->csv);
	hplc_metadata_destroy(&data->data);
}
