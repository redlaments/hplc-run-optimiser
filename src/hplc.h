#ifndef HPLC_H
#define HPLC_H

#include <time.h>
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
        double rtime;
        double itime;
        double ftime;
        long area;
        long height;
        double ah;
        double conc;
        char*mark;
        char*id;
        char*name;
        double k;
        long plate;
        double plate_height;
        double tailing;
        double resolution;
        double sep_factor;
        double area_ratio;
        double height_ratio;
        double concp;
        double norm_conc;
};
struct hplc_csv_chromatogram{
        int interval;/*msec*/
        int points;//number of values for both rtime and intensity
        double start;/*min*/
        double end;/*min*/
        char*units;
        double multiplier;
        int wavelength;/*nm*/
        double*rtime;
        int*intensity;
};
struct hplc_csv_pump{
	int itime;/*interval(msec)*/
	double stime;/*start time*/
	double etime;/*end time*/
	char*iunits;/*intensity Units*/
	double imult;/*intensity multipier*/
	double*rtime;
	double*intensity;
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
};
struct hplc_run_metadata{
	char*slvnt;
	double pH;
	enum hplc_run_type type;
	double slvnt_percent;
	double flow_rate;
	char*njctn_volume;
	char*njctn_type;
	double sample_conc;
	char*clmn_phase;
	enum hplc_clmn_brand brand;
	double clmn_length;
	double clmn_id;
	double clmn_bead_size;
	double temperature;
	int wavelength;
};
	

	
struct hplc_data{
	struct hplc_csv csv;
	struct hplc_run_metadata data;
};

void hplc_destroy(struct hplc_data*data);
#endif
