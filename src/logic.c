#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "macro.h"
#include "lib/csv.h"
#include "lib/metadata.h"
#include "lib/error.h"
#include "../config.h"
#include "hplc-types.h"
#include "logic.h"
#include "run-db.h"

#ifdef CLI
#include "cli.h"
#endif

enum selectivity_issue{
PEAKS,
RUNTIME
};
enum selectivity_action efficiency(struct hplc_data*data,struct benchmarks*bench,void**adata);

hplc_sint hplc_resolve_rs_floor()
{
	return hplc_scale_int(1);/*TODO*/
}

hplc_sint hplc_clmn_bp_ceiling(hplc_sint bp,struct hplc_run_metadata*data,hplc_sint new_length)
{
	struct hplc_column*clmn;
	double ratio;

	clmn=&data->column;
	ratio=(double)new_length/clmn->clmn_length;
#ifdef DEBUG
	char*tmp=hplc_sint_str(bp*ratio);
	print_data("bp ratio,ceiling:%f,%s\n",ratio,tmp);
	free(tmp);
#endif

	return (bp*ratio);
}
/*ceiling for column length*/
hplc_sint hplc_clmn_rt_ceiling(hplc_sint rt,struct hplc_column*clmn,hplc_sint new_length)
{
	double ratio;

	ratio=clmn->clmn_length/new_length;
#ifdef DEBUG	
	char*tmp=hplc_sint_str(rt*ratio);
	print_data("rt ceiling:%s\n",tmp);
	free(tmp);
#endif

	return rt*ratio;
}
hplc_sint hplc_clmn_ps_bp_ceiling(hplc_sint bp,struct hplc_run_metadata*data,hplc_sint new_ps)
{
	struct hplc_column*clmn;
	double ratio;

	clmn=&data->column;
	ratio=1;/*TODO*/
	return bp*ratio;
}
/*returns optimal increased flow to be just in spec*/
hplc_sint hplc_rt_iflow(struct benchmarks*bench,struct hplc_data*data)
{
	double ratio;
	/*TODO*/
	return hplc_scale_int(1);
}
/*checks if decreasing by the lowest configurable amount is possible
 * currently set to 0.1*/
hplc_sint hplc_clmn_flow_rt_ceiling(hplc_sint rt,struct hplc_data*data)
{
	
	double ratio,tmp;
	hplc_sint fl;

	fl=data->data.flow_rate;
	tmp=(fl-hplc_strtosi("0.1",NULL));
	ratio=tmp/fl;
	return rt*ratio;
}
/*returns optimal decreased flow to be just in spec*/
hplc_sint hplc_optimal_rt_flow(struct benchmarks*bench,struct hplc_data*data)
{
	double ratio;
	
	ratio=(double)resolve_runtime(&data->csv)/hplc_scale_int(15);
	return data->data.flow_rate*ratio;
}

int lower_b(struct hplc_data*data,struct benchmarks*bench,void**action_data)
{
	hplc_sint runtime,*tmp;

	runtime=resolve_runtime(&data->csv);
	if(runtime<bench->runtime){
		*action_data=malloc(sizeof(hplc_sint));
		tmp=*action_data;
		*tmp=data->data.slvnt_percent-hplc_scale_int(10);
		return 1; 
	}
	return HPLC_MAXRUNTIME;
}
int change_temperature(struct hplc_data*data,struct benchmarks*bench,void**action_data)
{
	hplc_sint*tmp;

	if(data->data.temperature==AMB){
		*action_data=malloc(sizeof(hplc_sint));
		tmp=*action_data;
		*tmp=hplc_scale_int(TEMP1);
		return 1;
	}
	if(data->data.temperature<hplc_scale_int(MAX_TEMP)){
		*action_data=malloc(sizeof(hplc_sint));
		tmp=*action_data;
		*tmp=hplc_scale_int(MAX_TEMP);
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
int change_column(struct hplc_data*data,struct benchmarks*bench,void**action_data)
{
	for(int i=0;i<SIZEOF(column_types);i++)
	/*only checks for past use for processing speed reasons*/
		if(!hplc_run_col_check(column_types[i])){
			*action_data=strdup(column_types[i]);
			return CLIG;
		}
	return HPLC_LIGAND;
}
static bool
clmn_len_change(struct hplc_run_metadata*data,struct benchmarks*bench,
		hplc_sint*nl,hplc_sint bp,hplc_sint rt)
{
	*nl=hplc_clmn_long(&data->column);
	if(nl==0)
		return false;
        if(hplc_scale_int(bench->backpressure)>=hplc_clmn_bp_ceiling(bp,data,*nl)&&
	bench->runtime>=hplc_clmn_rt_ceiling(rt,&data->column,*nl))
		return true;
	return false;
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
		{change_solvent,CSOL},
		{change_column,CLIG}
		};
		if(hplc_scale_int(bench->backpressure)<max_backpressure(&data->csv)){
			//TODO put something in action_data
			print_data("TODO:CDFLOW\n");
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
		*action_data=malloc(sizeof(hplc_sint));
		*(hplc_sint*)*action_data=data->data.slvnt_percent+
			hplc_scale_int(IB_AMOUNT);
		return CIB;
	default:
		print_data("Option unrecognised\n");
		return ERROR;
	}
}
enum selectivity_action logic(struct hplc_data*data,struct benchmarks*bench,void**adata)
{
	enum selectivity_action action;
	hplc_sint rs;

//TODO add check for bp coformation
	if(resolve_runtime(&data->csv)>bench->runtime){
		rs=minimum_res(&data->csv);
		if(bench->resolution<rs){
			*adata=malloc(sizeof(hplc_sint));
			if(!*adata){
				print_error("logic:0:Malloc failed\n");
				return ERROR;
			}
			//TODO ensure increasing by a discrete amount is 
			//possible
			*(hplc_sint*)*adata=hplc_rt_iflow(bench,data);
			action=CIFLOW;
		}
		else if(rs>hplc_resolve_rs_floor()){
			struct action_arg*aarg;
			hplc_sint ps;

			ps=hplc_clmn_ps_small(&data->data.column);
			if(ps==0)
				action=selectivity(data,bench,RUNTIME,adata);
			else{
				*adata=malloc(sizeof(struct action_arg)*3);
				if(!*adata){
					print_error("logic:1:Malloc failed\n");
					return ERROR;
				}
				aarg=*adata;
				aarg[0].action=CDPS;
				aarg[0].arg=malloc(sizeof(hplc_sint));
				if(!aarg[0].arg){
					print_error("logic:2:Malloc failed\n");
					free(*adata);
					return ERROR;
				}
				*(hplc_sint*)aarg[0].arg=ps;
				aarg[1].action=CIFLOW;
				aarg[1].arg=malloc(sizeof(hplc_sint));
				if(!aarg[1].arg){
					print_error("logic:3:Malloc failed\n");
					free(aarg[0].arg);
					free(*adata);
					return ERROR;
				}
				*(hplc_sint*)aarg[1].arg=
					hplc_rt_iflow(bench,data);
				aarg[2].action=END;
				action=SMULT;
			}
		}
		else
			action=selectivity(data,bench,RUNTIME,adata);
	}
	else if(resolve_peaks(&data->csv)<bench->peaks){
		action=selectivity(data,bench,PEAKS,adata);
	}
	else if(resolve_peaks(&data->csv)>bench->peaks){
		print_data("Error occured\n");
		return ERROR;
	}
	//TODO keep decreasing %b until runtime issue
	else{
		action=efficiency(data,bench,adata);
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
enum selectivity_action efficiency(struct hplc_data*data,struct benchmarks*bench,void**adata)
{
	struct action_arg*aarg;
	enum selectivity_action action;
	hplc_sint res,bp,rt;
	int l_index=0;
	hplc_sint nl,ps;

	res=minimum_res(&data->csv);
	if(res<bench->resolution){
		bp=max_backpressure(&data->csv);
		rt=resolve_runtime(&data->csv);
		if(res<hplc_resolve_rs_floor()){
			if(clmn_len_change(&data->data,bench,&nl,bp,rt)){
				*adata=malloc(sizeof(hplc_sint));
				if(*adata==NULL){
					print_error(
					"efficiency:CCL[0]:malloc failed\n");
					return ERROR;
				}
				*(hplc_sint*)(*adata)=nl;
				return CCL;
			}
			else{
				action=selectivity(data,bench,PEAKS,
					adata);
				return action;
			}
		}
		*adata=malloc(sizeof(struct action_arg)*5);
		if(!*adata){
			print_error("Efficiency:malloc failed\n");
			return ERROR;
		}
		aarg=(struct action_arg*)*adata;

		if(clmn_len_change(&data->data,bench,&nl,bp,rt)){
			if(bench->runtime>=hplc_clmn_rt_ceiling(rt,&data->data.column,nl))
				aarg[l_index].action=CCL;
				aarg[l_index].arg=malloc(sizeof(hplc_sint));
				if(aarg[l_index].arg==NULL){
					print_error(
					"efficiency:CCL[1]:malloc failed\n");
					l_index++;
				}
				else
					*(hplc_sint*)(aarg[l_index++].arg)=nl;
		}
		if((ps=hplc_clmn_ps_small(&data->data.column))!=0&&
		hplc_scale_int(bench->backpressure)>=
		hplc_clmn_ps_bp_ceiling(bp,&data->data,ps)){
			aarg[l_index].action=CDPS;
			aarg[l_index].arg=malloc(sizeof(hplc_sint));
			if(aarg[l_index].arg==NULL){
				print_error(
				"efficiency:CDPS:malloc failed\n");
				l_index++;
			}
			else
				*(hplc_sint*)(aarg[l_index++].arg)=ps;
		}
		if(bench->runtime>hplc_clmn_flow_rt_ceiling(rt,data)){
			aarg[l_index].action=CDFLOW;
			aarg[l_index].arg=malloc(sizeof(hplc_sint));
			if(aarg[l_index].arg==NULL){
				print_error(
					"efficiency:CDFLOW:malloc failed\n");
				l_index++;
			}
			else
				*(hplc_sint*)(aarg[l_index++].arg)=hplc_optimal_rt_flow(bench,data);
		}
		//TODO check if column already has SPP
		aarg[l_index].action=SPP;
		aarg[l_index++].arg=NULL;
		aarg[l_index].action=END;
		return MULTI;
	}
	else{
		if(res==bench->resolution)
			return SUCCESS;
		//print_data("All requirements met\n");
		*adata=malloc(sizeof(struct action_arg)*3);
		if(!*adata){
			print_error("Successfully failed to configure MULTI\n");
			return ERROR;
		}
		aarg=(struct action_arg*)*adata;
		aarg[l_index].action=SUCCESS;
		aarg[l_index++].arg=NULL;
		aarg[l_index].action=CIFLOW;
		aarg[l_index].arg=malloc(sizeof(hplc_sint));
		if(aarg[l_index].arg==NULL){
			print_error("Error occured");
			l_index++;
		}
		else{
			*(hplc_sint*)(aarg[l_index++].arg)=hplc_rt_iflow(bench,data);
		}
		aarg[l_index].action=END;
		return MULTI;
	}
		
}
const char*resolve_action_name(enum selectivity_action action)
{
	struct action_name{
		enum selectivity_action action;
		const char*name;
	};
	const struct action_name anames[]={
	{ERROR,"Error"},
	{SUCCESS,"Success!"},
	{MULTI,"Multiple Choice"},
	{SMULT,"Multiple Selction"},
	{SPP,"Coreshell"},
	{CITEMP,"Increase Temperature"},
	{CDTEMP,"Decrease Temperature"},
	{CSOL,"Change Solvent"},
	{CLIG,"Change Ligand"},
	{CCL,"Change Column Length"},
	{CDPS,"Decrease Particle Size"},
	{CIB,"Increase %B"},
	{CDB,"Decrease %B"},
	{CDFLOW,"Decrease Flow Rate"},
	{CIFLOW,"Increase Flow Rate"}
	};
	for(int i=0;i<SIZEOF(anames);i++)
		if(anames[i].action==action)
			return anames[i].name;
	return "Resolve Error";
}
bool predicted_chr(struct pred_chr*chr,enum selectivity_action action,void*old,void*new)
{
	double ratio;

	switch(action){
	case SPP:
	//TODO
	break;
	case CITEMP:
	case CDTEMP:
	//TODO
	break;
	case CSOL:
	/*TODO*/
	case CLIG:
	/*TODO*/
	case CCL:
		//ratio=(double(*(hplc_sint*)new))/(*(hplc_sint*)old);
		chr->rs*=sqrt(ratio);
		chr->rt*=ratio;
		chr->bp*=ratio;
		return true;
	case CDPS:
	/*TODO*/
	case CIB:
	case CDB:
	/*TODO*/
	case CDFLOW:
	case CIFLOW:
		//ratio=(double(*(hplc_sint*)new))/(*(hplc_sint*)old);
		/*TODO van deemter equation effects*/
		//chr->rs=???
		chr->rt*=(1/ratio);
		chr->bp*=ratio;
		return true;
	default:
	}
}
