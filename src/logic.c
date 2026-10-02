#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "macro.h"
#include "lib/csv.h"
#include "lib/error.h"
#include "../config.h"
#include "logic.h"

#ifdef CLI
#include "cli.h"
#endif

enum selectivity_issue{
PEAKS,
RUNTIME
};
int lower_b(struct hplc_data*data,struct benchmarks*bench,void**action_data)
{
	double runtime,*tmp;

	runtime=resolve_runtime(&data->csv);
	if(runtime<bench->runtime){
		*action_data=malloc(sizeof(double));
		tmp=*action_data;
		*tmp=data->data.slvnt_percent-10;
		return 1; 
	}
	return HPLC_MAXRUNTIME;
}
int change_temperature(struct hplc_data*data,struct benchmarks*bench,void**action_data)
{
	double*tmp;

	if(isnan(data->data.temperature)){
		*action_data=malloc(sizeof(double));
		tmp=*action_data;
		*tmp=TEMP1;
		return 1;
	}
	if(data->data.temperature<(double)MAX_TEMP){
		*action_data=malloc(sizeof(double));
		tmp=*action_data;
		*tmp=MAX_TEMP;
		return 1;
	}
	return HPLC_MAXTEMP;
}
int change_solvent(struct hplc_data*data,struct benchmarks*bench,void**action_data)
{
	for(int i=0;i<SIZEOF(solvents);i++)
		if(!strcmp(solvents[i],data->data.slvnt)){
			if(i+1<SIZEOF(solvents)){
				*action_data=strdup(solvents[i]);
				return 1;
			}
			return HPLC_SOLVENT;
		}
	return HPLC_UNKNOWN;
}
/*Determines what should be done
 * see action() on how that is implemented*/
enum selectivity_action selectivity(struct hplc_data*data,
struct benchmarks*bench,enum selectivity_issue issue,void**action_data)
{
	switch(issue){
	case PEAKS:
		struct sel_func{
			int(*handle)(struct hplc_data*data,struct benchmarks*bench,void**action_data);
			enum selectivity_action action;
		};
		struct sel_func funcs[]={
		{lower_b,CDB},
		{change_temperature,CITEMP},
		{change_solvent,CSOL}
		/*||change_column(data,bench) TODO*/
		};
		if(bench->backpressure<max_backpressure(&data->csv)){
			return CDFLOW;
		}
		for(int i=0;i<SIZEOF(funcs);i++){
			if(funcs[i].handle(data,bench,action_data)>-1)
				return funcs[i].action;
			else
				print_data("Impossible to resolve peaks\n");
			return ERROR;
		}
	case RUNTIME:
		
		if(change_temperature(data,bench,action_data)>-1)
			return CITEMP;
		*action_data=malloc(sizeof(double));
		double*tmp=*action_data;
		*tmp=data->data.slvnt_percent+IB_AMOUNT;
		return CIB;
	default:
		print_data("Option unrecognised\n");
		return ERROR;
	}
}
void hplc_action(enum selectivity_action action,void*action_data)
{
	switch(action){
	case CITEMP:
		//double b;
		//show optimal %b, changing variable, required input
		//b=input_ctemp();
		//show result
		print_data("CITEMP:%f\n",*(double*)action_data);
		free(action_data);
		break;
	case CSOL:
		//show options, visually tag previously tried
		//get input
		print_data("New Sol.:%s\n",(char*)action_data);
		free(action_data);
		break;
	case CDFLOW:
		print_data("Decrease Flow\n");
		//TODO put in number?
		break;
	case CDB:
		print_data("Decrease %B by %f\n",*(double*)action_data);
		free(action_data);
		break;
	case ERROR:
		break;
	}
}
enum selectivity_action logic(struct hplc_data*data,struct benchmarks*bench,void**adata)
{
	enum selectivity_action action;

	if(resolve_runtime(&data->csv)>bench->runtime){
		action=selectivity(data,bench,RUNTIME,adata);
	}
	else if(data->csv.peak_count<bench->peaks){
		action=selectivity(data,bench,PEAKS,adata);
	}
	else if(data->csv.peak_count>bench->peaks){
		print_data("Error occured\n");
		return ERROR;
	}
	//TODO keep decreasing %b until runtime issue
	else{
		double res;

		res=minimum_res(&data->csv);
		if(res!=1.4){
			print_data("Efficency\n");
			return EFFICIENCY;
		}
		else{
			print_data("All requirements met\n");
			return SUCCESS;
		}
			
	/*efficiency	
		if(bench->resolution<res)
			//maybe attempt to approach minimum desired (econ)
		else if(bench->resolution>res){
			efficiency(csv);
			return;
		}TODO*/
	}
	//determine how action should be taken
//	hplc_action(action,*(void**)action_data);
	/*perform_optimal_b,TODO*/
	//change temp. req. user %b spec.
	if(action<0){
		fprintf(stderr,"logic:error occured:%d\n",action);
		return ERROR;
	}
	return action;
}
