#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib/metadata.h"
#include "hplc-types.h"
#include "macro.h"
#include "logic.h"
#include "query.h"
#include "test-logic.h"

#define RUN_SIZE 9
int count=0;
struct test_run*runs;
hplc_sint resolve_runtime(struct hplc_csv*csv)
{
	return runs[count].rt;
}
hplc_sint minimum_res(struct hplc_csv*csv)
{
	return runs[count].rs;
}
hplc_sint max_backpressure(struct hplc_csv*csv)
{
	return runs[count].bp;
}
int resolve_peaks(struct hplc_csv*csv)
{
	return 7;
}
void test_run_init(struct test_run*run,int index,char*name,hplc_sint rs,hplc_sint rt,hplc_sint bp)
{
	run[index].name=strdup(name);
	run[index].rs=rs;
	run[index].rt=rt;
	run[index].bp=bp;
}
void test_run_destroy(struct test_run*run,int size)
{
	for(int i=0;i<size;i++)
		free(run[i].name);
	free(run);
}
int main(int argc,char*argv[])
{
	struct benchmarks bench={7,hplc_strtosi("1.4",NULL),hplc_scale_int(15),4000};
	struct hplc_data data={0};
	enum selectivity_action action;
	void*vdata=NULL;
	char*tmp;

	if(!hplc_query_init()){
		fprintf(stderr,"query_init failed\n");
		return 0;
	}
	runs=malloc(sizeof(struct test_run)*RUN_SIZE);
	test_run_init(runs,0,
	"GR09-15-4-ACN-3-ISO-35-1.5-20u-CP-0.1-C18P-150x4.6x5-T40-254.csv",
	hplc_strtosi("1.1",NULL),hplc_scale_int(8),hplc_scale_int(2000));
	test_run_init(runs,1,
	"GR09-16-4-ACN-3-ISO-35-1.5-20u-CP-0.1-C18P-150x4.6x5-T40-254.csv",
	hplc_strtosi("0.5",NULL),hplc_scale_int(8),hplc_scale_int(2000));
	test_run_init(runs,2,
	"GR09-17-4-ACN-3-ISO-35-1.5-20u-CP-0.1-C18P-150x4.6x5-T40-254.csv",
	hplc_strtosi("1.5",NULL),hplc_scale_int(14),hplc_scale_int(2500));
	test_run_init(runs,3,
	"GR09-18-4-ACN-3-ISO-35-1.5-20u-CP-0.1-C18P-150x4.6x5-T40-254.csv",
	hplc_strtosi("0.8",NULL),hplc_scale_int(14),hplc_scale_int(3200));
	test_run_init(runs,4,
	"GR09-19-4-ACN-3-ISO-35-1.5-20u-CP-0.1-C18P-150x4.6x5-T40-254.csv",
	hplc_strtosi("1.4",NULL),hplc_scale_int(15),hplc_scale_int(4000));
	test_run_init(runs,5,
	"GR09-20-4-ACN-3-ISO-35-1.5-20u-CP-0.1-C18P-150x4.6x5-T40-254.csv",
	hplc_strtosi("1.3",NULL),hplc_scale_int(14),hplc_scale_int(4000));
	test_run_init(runs,6,
	"GR09-21-4-ACN-3-ISO-35-1.5-20u-CP-0.1-C18P-150x4.6x5-T40-254.csv",
	hplc_strtosi("0.8",NULL),hplc_scale_int(19),hplc_scale_int(2000));
	test_run_init(runs,7,
	"GR09-22-4-ACN-3-ISO-35-1.5-20u-CP-0.1-C18P-150x4.6x5-T40-254.csv",
	hplc_strtosi("1.3",NULL),hplc_scale_int(19),hplc_scale_int(2000));
	test_run_init(runs,8,
	"GR09-23-4-ACN-3-ISO-35-1.5-20u-CP-0.1-C18P-150x4.6x5-T40-254.csv",
	hplc_strtosi("1.7",NULL),hplc_scale_int(19),hplc_scale_int(2000));

	for(count=0;count<RUN_SIZE;count++){
		vdata=NULL;
		if(!hplc_metadata(runs[count].name,&data.data)){
			fprintf(stderr,"Metadata failed\n");
			continue;
		}
		action=logic(&data,&bench,&vdata);
		if(action==MULTI||action==SMULT){
			printf("[%d]:%s\n",count,resolve_action_name(action));
			for(int i=0;
			(action=((struct action_arg*)vdata)[i].action)!=END;
				i++){
				if(action==SPP||action==SUCCESS)
					printf("%s\n",
						resolve_action_name(action));
				else{
					hplc_sint*hs_tmp;

					hs_tmp=(hplc_sint*)
					((struct action_arg*)vdata)[i].arg;
					if(hs_tmp==NULL){
						printf("%s:NULL\n",
						resolve_action_name(action));
					}
					else{
					tmp=hplc_sint_str(*hs_tmp);
					printf("%s:%s\n",
					resolve_action_name(action),tmp);
					free(tmp);
					}
				}
			}
		}
		else if(action==SUCCESS){
			printf("[%d]:%s\n",count,resolve_action_name(action));
		}
		else{
			if(vdata==NULL){
				printf("[%d] vdata is NULL\n",count);
				continue;
			}
			tmp=hplc_sint_str(*(hplc_sint*)vdata);
			printf("[%d]:%s:%s\n",count,resolve_action_name(action),tmp);
			free(tmp);
		}
		free(vdata);
	}
	test_run_destroy(runs,RUN_SIZE);
	hplc_query_destroy();
}
