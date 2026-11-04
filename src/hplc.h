#ifndef HPLC_H
#define HPLC_H

#include <time.h>
#include "hplc-types.h"
#include "../config.h"

struct hplc_csv_header{
        char*app_name;
        char*app_version;
        char*data_file_name;
        time_t time;/*output date, time*/
};
struct hplc_csv_original{
        char*data_file;
        char*method_file;
        char*batch_file;
        char*report_format_file;
};
struct hplc_csv_peak{
        hplc_sint rtime;
        hplc_sint itime;
        hplc_sint ftime;
        long area;
        long height;
        hplc_sint ah;
        hplc_sint conc;
        char*mark;
        char*id;
        char*name;
        hplc_sint k;
        long plate;
        hplc_sint plate_height;
        hplc_sint tailing;
        hplc_sint resolution;
        hplc_sint sep_factor;
        hplc_sint area_ratio;
        hplc_sint height_ratio;
        hplc_sint concp;
        hplc_sint norm_conc;
};
struct hplc_csv_chromatogram{
        int interval;/*msec*/
        int points;//number of values for both rtime and intensity
        hplc_sint start;/*min*/
        hplc_sint end;/*min*/
        char*units;
        hplc_sint multiplier;
        int wavelength;/*nm*/
        hplc_sint*rtime;
        int*intensity;
};
struct hplc_csv_pump{
	int itime;/*interval(msec)*/
	hplc_sint stime;/*start time*/
	hplc_sint etime;/*end time*/
	char*iunits;/*intensity Units*/
	hplc_sint imult;/*intensity multipier*/
	hplc_sint*rtime;
	hplc_sint*intensity;
	int points;/*number of values for both rtime and intensity*/
};

struct hplc_csv{
        struct hplc_csv_original original;
        struct hplc_csv_peak*peaks;
        int peak_count;
        struct hplc_csv_chromatogram chromatogram;
        struct hplc_csv_header header;
	struct hplc_csv_pump*pumps;
	int pump_count;
	char*hplc_file_data;//csv data struct points to this
};
struct hplc_column{
	char*clmn_phase;
	int brand;
	hplc_sint clmn_length;
	hplc_sint clmn_id;
	hplc_sint clmn_bead_size;
};
struct hplc_run_metadata{
	char*slvnt;
	hplc_sint pH;
	enum hplc_run_type type;
	hplc_sint slvnt_percent;
	hplc_sint flow_rate;
	char*njctn_volume;
	char*njctn_type;
	hplc_sint sample_conc;
	struct hplc_column column;
	hplc_sint temperature;//stored in db as text
	int wavelength; //stored in db as variable wave
};
	

	
struct hplc_data{
	struct hplc_csv csv;
	struct hplc_run_metadata data;
};

void hplc_destroy(struct hplc_data*data);
int hplc_column_phase(struct hplc_run_metadata*data);
#endif
