#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "hplc.h"
#include "util.h"
#include "macro.h"
#include "metadata.h"
#include "../config.h"

#ifdef CLI
#include "cli.h"
#endif

#define HPLC_STRTOD(X) strtod(X,&tail);if(tail!=NULL&&*tail!='\0')
void hplc_metadata_destroy(struct hplc_run_metadata*data)
{
	free(data->njctn_volume);
	free(data->njctn_type);
	free(data->clmn_phase);
	free(data->slvnt);
}
bool hplc_metadata(char*file_name,struct hplc_run_metadata*data)
{
	char*argv[NAME_MAX_ARGS],*tail=NULL;
	int argc;

	argc=tokenize_str1(file_name,argv,NAME_MAX_ARGS,'-');
	
	if(argc<16){
		fprintf(stderr,"Args:%d\n",argc);
		return false;//too few args
	}
	data->slvnt=strdup(argv[3]);
	data->pH=HPLC_STRTOD(argv[4]){
		fprintf(stderr,"pH tail:%s\n",tail);
		tail=NULL;
	}
	for(int i=0;i<SIZEOF(run);i++)
		if(!strcmp(argv[5],run[i].name)){
			data->type=run[i].type;
			break;
		}
	data->slvnt_percent=HPLC_STRTOD(argv[6]){
		fprintf(stderr,"Solvent tail:%s\n",tail);
		tail=NULL;
	}
	data->flow_rate=HPLC_STRTOD(argv[7]){
		fprintf(stderr,"Flow Rate tail:%s\n",tail);
		tail=NULL;
	}
	data->njctn_volume=strdup(argv[8]);
	data->njctn_type=strdup(argv[9]);
	data->sample_conc=HPLC_STRTOD(argv[10]){
		fprintf(stderr,"Sample Concentration tail:%s\n",tail);
		tail=NULL;
	}
	data->clmn_phase=strdup(argv[11]);
	for(int i=0;i<SIZEOF(brands);i++)
		if(!strcmp(argv[12],brands[i].name)){
			data->brand=brands[i].type;
			break;
		}
	
	char*argv1[3];
	int argc1;

	argc=tokenize_str1(argv[13],argv1,3,'x');
	if(argc!=3)
		return false;
		
	data->clmn_length=HPLC_STRTOD(argv1[0]){
		fprintf(stderr,"Column Length tail:%s\n",tail);
		tail=NULL;
	}
	data->clmn_id=HPLC_STRTOD(argv1[1]){
		fprintf(stderr,"Column Internal Diameter tail:%s\n",tail);
		tail=NULL;
	}
	data->clmn_bead_size=HPLC_STRTOD(argv1[2]){
		fprintf(stderr,"Column bead size tail:%s\n",tail);
		tail=NULL;
	}
	if(!strcmp(argv[14],"amb"))
		data->temperature=NAN;
	else{
		data->temperature=HPLC_STRTOD(argv[14]){
			fprintf(stderr,"Temperature tail:%s\n",tail);
			tail=NULL;
		}
	}
	data->wavelength=(int)strtol(argv[15],&tail,10);
	if(tail!=NULL&&*tail!='\0'){
		if(!strcmp(tail,".csv"))
			return true;
	}
			
	print_data("Warning:File does not have csv suffix\n");
	return true;
}
const char*hplc_run_type_str(enum hplc_run_type type)
{
	return run[type].name;
}
const char*hplc_clmn_brand_str(enum hplc_clmn_brand brand)
{
	return brands[brand].name;
}
char*hplc_temperature_str(double temperature)
{
	int size;
	char*tmp;

	if(fpclassify(temperature)==FP_NAN)
		return strdup("amb");
	else{
		size=snprintf(NULL,0,"%f",temperature);
		tmp=malloc(sizeof(char)*++size);
		snprintf(tmp,size,"%f",temperature);
		return tmp;
	}
}
void hplc_metadata_print(struct hplc_run_metadata*data)
{
	char*tmp;

	tmp=hplc_temperature_str(data->temperature);
	printf("\033[35m%s\033[0m\n",tmp);

	printf(
	"Solvent:%s\n"
	"pH:%f\n"
	"Run Type:%s\n"
	"Solvent Percent:%f\n"
	"Flow Rate:%f\n"
	"Injection Volume:%s\n"
	"Injection Type:%s\n"
	"Column Phase:%s\n"
	"Column Brand:%s\n"
	"Column Length:%f\n"
	"Column Internal Diameter:%f\n"
	"Column Bead Size:%f\n"
	"Temperature:%s\n"
	"Wavelength:%d\n",
	data->slvnt,data->pH,hplc_run_type_str(data->type),data->slvnt_percent,
	data->flow_rate,data->njctn_volume,data->njctn_type,data->clmn_phase,
	hplc_clmn_brand_str(data->brand),data->clmn_length,data->clmn_id,
	data->clmn_bead_size,tmp,data->wavelength);

	free(tmp);
}

