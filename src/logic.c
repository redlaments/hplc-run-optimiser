#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sqlite3.h>
#include "macro.h"
#include "lib/csv.h"
#include "lib/metadata.h"
#include "lib/error.h"
#include "../config.h"
#include "hplc-types.h"
#include "logic.h"
#include "run-db.h"
#include "query.h"

#ifdef CLI
#include "cli.h"
#endif

enum selectivity_issue{
PEAKS,
RUNTIME
};
enum selectivity_action efficiency(struct hplc_data*data,struct benchmarks*bench,void**adata);
/*does not factor in longer columns or internal diameter*/
hplc_sint hplc_resolve_rs_floor(struct hplc_column *column)
{
        int ret;
        sqlite3_stmt *pstmt = NULL;
        const char *sql = "select ps from hplc_columns "
                          "where length=? and diameter=? and lig=? "
                          "order by ps asc";
        hplc_sint ps = 0;

        if (!column || !column->clmn_phase || !pDb) {
                return 0;
        }

        ret = sqlite3_prepare_v2(pDb, sql, -1, &pstmt, NULL);
        if (ret != SQLITE_OK) {
                return 0;
        }

        if (sqlite3_bind_int64(pstmt, 1, (sqlite3_int64)column->clmn_length) != SQLITE_OK ||
            sqlite3_bind_int64(pstmt, 2, (sqlite3_int64)column->clmn_id) != SQLITE_OK ||
            sqlite3_bind_text(pstmt, 3, column->clmn_phase, -1, SQLITE_STATIC) != SQLITE_OK) {
                sqlite3_finalize(pstmt);
                return 0;
        }

        ret = sqlite3_step(pstmt);
        if (ret == SQLITE_ROW) {
                ps = (hplc_sint)sqlite3_column_int64(pstmt, 0);
        }

        sqlite3_finalize(pstmt);
	if(ps==0)
		return ps;
	//TODO
	return hplc_scale_int(1);
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
		*tmp=data->data.run.iso.slvnt_percent-hplc_scale_int(10);
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
			//TODO test
			double ratio;

			ratio=hplc_scale_int(bench->backpressure)/
				((double)max_backpressure(&data->csv));
			*action_data=malloc(sizeof(hplc_sint));
			if(!*action_data){
				print_error("Selectivity:0:malloc failed\n");
				return ERROR;
			}
			*(hplc_sint*)action_data=(hplc_sint)(ratio*data->data.flow_rate);
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
		*(hplc_sint*)*action_data=data->data.run.iso.slvnt_percent+
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
	hplc_sint rs,req_rs;
	hplc_sint hs_tmp;

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
			data->data.flow_rate=hplc_rt_iflow(bench,data);
			*(hplc_sint*)*adata=data->data.flow_rate;
			return CIFLOW;
		}
		req_rs=hplc_resolve_rs_floor(&data->data.column);
		if(req_rs==0){
			print_error("Rs Floor failed to resolve\n");
			return ERROR;
		}
		else if(rs>req_rs){
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
				aarg[0].arg=malloc(sizeof(hplc_sint));
				aarg[1].arg=malloc(sizeof(hplc_sint));
				if(!aarg[0].arg||!aarg[1].arg){
					print_error(
						"logic:1:Malloc failed:1\n");
					return ERROR;
				}
				data->data.column.clmn_bead_size=ps;
				hs_tmp=hplc_rt_iflow(bench,data)-data->data.flow_rate;
				data->data.flow_rate=hplc_rt_iflow(bench,data);
				aarg[0].action=CDPS;
				*(hplc_sint*)(aarg[0].arg)=
					data->data.column.clmn_bead_size;
				aarg[1].action=CIFLOW;
				*(hplc_sint*)(aarg[1].arg)=hs_tmp;
				aarg[2].action=END;
				return SMULT;
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
		if(res<hplc_resolve_rs_floor(&data->data.column)){
			if(clmn_len_change(&data->data,bench,&nl,bp,rt)){
				*adata=malloc(sizeof(hplc_sint));
				if(*adata==NULL){
					print_error(
					"efficiency:CCL[0]:malloc failed\n");
					return ERROR;
				}
				data->data.column.clmn_length=nl;
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
	{CIFLOW,"Increase Flow Rate"},
	{CMTG,"Multiply tg"},
	{CDVTG,"Divide tg"},
	{CMFLOW,"Multiply flow rate"},
	{CIBE,"Increase %B end"}
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
/* Calculates Gradient Steepness: GS = ΔΦ * Vm / (tG * F) */
double hplc_calc_gs(const struct hplc_run_metadata *m)
{
    double b_ini = (double)m->run.gra.slvnt_start / 100000.0;
    double b_fin = (double)m->run.gra.slvnt_end / 100000.0;
    double delta_phi = (b_fin - b_ini) / 100.0;
    double tg = (double)m->run.gra.tg / 100000.0;
    double flow = (double)m->flow_rate / 100000.0;

    // Estimate column dead volume Vm (mL) from column internal diameter & length (mm)
    double id_cm = ((double)m->column.clmn_id / 100000.0) / 10.0;
    double len_cm = ((double)m->column.clmn_length / 100000.0) / 10.0;
    double vm = 3.1415926535 * (id_cm / 2.0) * (id_cm / 2.0) * len_cm * 0.66;

    if (tg <= 0.0 || flow <= 0.0)
        return 0.0;

    return (delta_phi * vm) / (tg * flow);
}
/*assumes run type is gra, check before calling*/
bool increased_gs(struct hplc_run_metadata*data)
{
	/*TODO optional:change hplc_run_meta_check to allow NULL for pk*/
	char*tmp;
	hplc_sint ttg;
	bool ret;

	ttg=data->run.gra.tg;
	data->run.gra.tg<<=1;
	ret=hplc_run_meta_check(data,&tmp);
	free(tmp);
	data->run.gra.tg=ttg;
	if(ret)
		return true;
	return false;
	
}
bool decreased_gs(struct hplc_run_metadata*data)
{
	char*tmp;
	hplc_sint ttg;
	bool ret;

	ttg=data->run.gra.tg;
	data->run.gra.tg>>=1;
	ret=hplc_run_meta_check(data,&tmp);
	free(tmp);
	data->run.gra.tg=ttg;
	if(ret)
		return true;
	return false;
}
enum selectivity_action
gra_logic(struct hplc_data *data, struct benchmarks *bench, void **action_data)
{
    struct action_arg*aarg;

    if(data->data.run.type!=GRA){
    	print_error("gra_logic:wrong run type\n");
	return ERROR;
    }
    int peaks = resolve_peaks(&data->csv);
    hplc_sint rs = minimum_res(&data->csv);
    hplc_sint rt = resolve_runtime(&data->csv);

    if (peaks < bench->peaks) {
        // Case G1: Peaks < target
        if (!increased_gs(&data->data)){
            // Case G1 ↑: Increase GS -> same %B window, tG * 0.5
            *action_data=malloc(sizeof(hplc_sint));
            if (!*action_data) {
                print_error("gra_logic: malloc failed (G1_INC)\n");
                return ERROR;
            }
	    data->data.run.gra.tg>>=1;
	    *(hplc_sint*)*action_data=hplc_strtosi("0.5",NULL);

            return CMTG;
        } else if (!decreased_gs(&data->data)){
            // Case G1 ↓: Decrease GS -> back to scouting run, tG * 1.2
            *action_data=malloc(sizeof(hplc_sint));
            if (!*action_data) {
                print_error("gra_logic: malloc failed (G1_DEC)\n");
                return ERROR;
            }
	    data->data.run.gra.tg*=1.2;
            *(hplc_sint*)*action_data =hplc_strtosi("1.2",NULL);

            return CMTG;
        } else {
            // Case G1 ✕: Failed after G1 ↑ and G1 ↓ -> Change selectivity (temp, solvent, ligand)
            //return selectivity(data, bench, PEAKS, action_data);
	    //TODO
	    print_data("Theoritical gra selectivity function\n");
	    return ERROR;
        }
    } else if (peaks == bench->peaks) {
        // Peaks equal target (e.g. 7)
        if (rs < bench->resolution) {
            // Case G2: Rs < 1.4 -> Keep GS: F * 0.8 and tG / 0.8
	    struct hplc_gra_run*gra_run=&data->data.run.gra;

	    *action_data=malloc(sizeof(struct action_arg)*3);
            if (!action_data) {
                print_error("gra_logic: malloc failed (G2)\n");
                return ERROR;
            }
	    aarg=*action_data;
	    aarg[0].arg=malloc(sizeof(hplc_sint));
	    aarg[1].arg=malloc(sizeof(hplc_sint));
	    if(!aarg[0].arg||!aarg[1].arg){
	    	print_error("gra_logic:malloc failed (G2):1\n");
		return ERROR;
	    }
	    gra_run->tg/= 0.8;
	    data->data.flow_rate*=0.8;
	    aarg[0].action=CDVTG;
	    *(hplc_sint*)(aarg[0].arg)=hplc_strtosi("0.8",NULL);
	    aarg[1].action=CMFLOW;
	    *(hplc_sint*)(aarg[1].arg)=hplc_strtosi("0.8",NULL);
	    aarg[2].action=END;
            return SMULT;
        } else {
            // Rs >= spec
            hplc_sint last_peak_rt = data->csv.peaks[data->csv.peak_count - 1].rtime;
            
            // Check for unused/wasted %B window (Case G3)
            if (last_peak_rt < data->data.run.gra.tg && (data->data.run.gra.tg - last_peak_rt) > hplc_scale_int(2)) {
                // Case G3: Keep GS, shrink %B window and cut tG in proportion
                *action_data=malloc(sizeof(struct action_arg)*3);
                if (!*action_data) {
                    print_error("gra_logic: malloc failed (G3)\n");
                    return ERROR;
                }
		aarg=*action_data;
                double ratio = (double)last_peak_rt / data->data.run.gra.tg;
                hplc_sint delta_b = data->data.run.gra.slvnt_end -
			data->data.run.gra.slvnt_start;

		data->data.run.gra.tg*=ratio;
		data->data.run.gra.slvnt_end+=delta_b*ratio;
                return KGS;
            } else if (rt <= bench->runtime) {
                // Case G4: Rs >= spec and tR <= run-time spec -> Method met
                *action_data = NULL;
                return SUCCESS;
            } else {
                // Runtime spec exceeded -> fall back to flow efficiency logic
                //return efficiency(data, bench, action_data);
		//TODO
		print_data("Theoritical gra decrease rt function\n");
		return ERROR;
            }
        }
    } else {
        print_error("Too many peaks (%d). Is your column clean?\n", peaks);
        return ERROR;
    }
}
