#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sqlite3.h>
#include "hplc.h"
#include "util.h"
#include "macro.h"
#include "metadata.h"
#include "hplc-types.h"
#include "query.h"
#include "../config.h"

#ifdef CLI
#include "cli.h"
#endif

#define HPLC_STRTOD(X) hplc_strtosi(X,&tail);if(tail!=NULL&&*tail!='\0')
void hplc_metadata_column_destroy(struct hplc_column*column)
{
	free(column->clmn_phase);
}
void hplc_metadata_destroy(struct hplc_run_metadata*data)
{
	hplc_metadata_column_destroy(&data->column);
	free(data->njctn_volume);
	free(data->njctn_type);
	free(data->slvnt);
}

bool hplc_brand_id(char s, int *id)
{
    sqlite3_stmt *pstmt = NULL;
    int ret;
    const char *sql = "select id from hplc_brand where short=?";
    char str[2] = { s, '\0' };
    bool found = false;

    // Prepare statement
    ret = sqlite3_prepare_v2(pDb, sql, -1, &pstmt, NULL);
    if (ret != SQLITE_OK) {
        return false;
    }

    // Bind char 's' as a single-character string parameter
    if (sqlite3_bind_text(pstmt, 1, str, 1, SQLITE_STATIC) != SQLITE_OK) {
        sqlite3_finalize(pstmt);
        return false;
    }

    // Execute query and retrieve id if row exists
    ret = sqlite3_step(pstmt);
    if (ret == SQLITE_ROW) {
        *id = sqlite3_column_int(pstmt, 0);
        found = true;
    }

    sqlite3_finalize(pstmt);
    return found;
}
bool hplc_metadata_column(char*argv[],int*index,struct hplc_column*data)
{
	char c=argv[*index][strlen(argv[*index])-1];
	argv[*index][strlen(argv[*index])-1]='\0';
	
	if(!hplc_brand_id(c,&data->brand)){
		print_error("Brand short %c not valid\n",c);
		return false;
	}
	
	data->clmn_phase=strdup(argv[(*index)++]);
	
	char*argv1[3],*tail=NULL;
	int argc;

	argc=tokenize_str1(argv[(*index)++],argv1,3,'x');
	if(argc!=3){
		fprintf(stderr,"Column:have %d args,expected 3\n",argc);
		return false;
	}
		
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
	if(!hplc_column_id(data))
		return false;
	return true;
}
bool hplc_column_id(struct hplc_column *clmn)
{
        int ret;
        sqlite3_stmt *pstmt = NULL;
        const char *sql = "select id from hplc_columns where brand=? and length=? and "
                          "diameter=? and ps=? and lig=?";


        // Prepare SQL statement
        ret = sqlite3_prepare_v2(pDb, sql, -1, &pstmt, NULL);
        if (ret != SQLITE_OK) {
                return false;
        }

        // Bind input criteria:
        // ?1 = brand    -> clmn->brand
        // ?2 = length   -> clmn->clmn_length
        // ?3 = diameter -> clmn->clmn_id
        // ?4 = ps       -> clmn->clmn_bead_size
        // ?5 = lig      -> clmn->clmn_phase
        if (sqlite3_bind_int(pstmt, 1, (int)clmn->brand) != SQLITE_OK ||
            sqlite3_bind_int64(pstmt, 2, (sqlite3_int64)clmn->clmn_length) != SQLITE_OK ||
            sqlite3_bind_int64(pstmt, 3, (sqlite3_int64)clmn->clmn_id) != SQLITE_OK ||
            sqlite3_bind_int64(pstmt, 4, (sqlite3_int64)clmn->clmn_bead_size) != SQLITE_OK ||
            sqlite3_bind_text(pstmt, 5, clmn->clmn_phase, -1, SQLITE_STATIC) != SQLITE_OK) {
                sqlite3_finalize(pstmt);
                return false;
        }

        ret = sqlite3_step(pstmt);
        if (ret == SQLITE_ROW) {
                clmn->id = sqlite3_column_int(pstmt, 0);
        }

        sqlite3_finalize(pstmt);
        return true;
}
bool hplc_metadata(char*file_name,struct hplc_run_metadata*data)
{
	char*argv[NAME_MAX_ARGS],*tail=NULL;
	int argc,index=3,cmp;
	enum hplc_run_type rrun;

	argc=tokenize_str1(file_name,argv,NAME_MAX_ARGS,'-');
	
	if(argc==15){
		rrun=ISO;
	}
	else if(argc==17){
		rrun=GRA;
	}
	else{
		fprintf(stderr,"Args:%d, ISO has 15, GRA has 17\n",argc);
		return false;//too few args
	}
	data->slvnt=strdup(argv[index++]);
	data->pH=HPLC_STRTOD(argv[index++]){
		fprintf(stderr,"pH tail:%s\n",tail);
		tail=NULL;
	}
	cmp=index;
	for(int i=0;i<SIZEOF(run);i++)
		if(!strcmp(argv[index],run[i].name)){
			data->run.type=run[i].type;
			index++;
			break;
		}
	if(cmp==index){
		print_error("Run type %s is not valid\n",argv[index]);
		return false;
	}
	if(rrun!=data->run.type){
		if(data->run.type==GRA)
			print_error(
			"Argument number %d not does corrolate to GRA run type"
			", GRA takes 17 arguments!",argc);
		else
			print_error(
			"Argument number %d not does corrolate to ISO run type"
			", ISO takes 15 arguments!",argc);
		return false;
	}
	if(data->run.type==ISO){
		data->run.iso.slvnt_percent=HPLC_STRTOD(argv[index++]){
			fprintf(stderr,"Solvent tail:%s\n",tail);
			tail=NULL;
		}
	}
	else{
		data->run.gra.slvnt_start=HPLC_STRTOD(argv[index++]){
			fprintf(stderr,"Solvent start tail:%s\n",tail);
			tail=NULL;
		}
		data->run.gra.slvnt_end=HPLC_STRTOD(argv[index++]){
			fprintf(stderr,"Solvent end tail:%s\n",tail);
			tail=NULL;
		}
		if(data->run.gra.slvnt_end<=data->run.gra.slvnt_start){
			print_error(
			"Gradient end %% being larger than gradient start is not supported\n");
			return false;
		}
		data->run.gra.tg=HPLC_STRTOD(argv[index++]){
			fprintf(stderr,"tg tail:%s\n",tail);
			tail=NULL;
		}
	}
	data->flow_rate=HPLC_STRTOD(argv[index++]){
		fprintf(stderr,"Flow Rate tail:%s\n",tail);
		tail=NULL;
	}
	data->njctn_volume=strdup(argv[index++]);
	data->njctn_type=strdup(argv[index++]);
	data->sample_conc=HPLC_STRTOD(argv[index++]){
		fprintf(stderr,"Sample Concentration tail:%s\n",tail);
		tail=NULL;
	}
	
	if(!hplc_metadata_column(argv,&index,&data->column)){
		return false;
	}
	if(!strcmp(argv[index],"amb")){
		data->temperature=AMB;
		index++;
	}
	else{
		if(argv[index][0]=='t'||argv[index][0]=='T')
			argv[index]++;
		data->temperature=HPLC_STRTOD(argv[index++]){
			fprintf(stderr,"Temperature tail:%s\n",tail);
			tail=NULL;
		}
	}
	data->wavelength=(int)strtol(argv[index],&tail,10);
	//printf("Index:%d\n",index);
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
char* hplc_clmn_brand_str(int brand)
{
    int ret;
    sqlite3_stmt *pstmt = NULL;
    const char *sql = "select name from hplc_brand where id=?";
    char *brand_name = NULL;

    // Prepare statement
    ret = sqlite3_prepare_v2(pDb, sql, -1, &pstmt, NULL);
    if (ret != SQLITE_OK) {
        return NULL;
    }

    // Bind parameter ?1 to brand integer
    if (sqlite3_bind_int(pstmt, 1, brand) != SQLITE_OK) {
        sqlite3_finalize(pstmt);
        return NULL;
    }

    // Execute query and capture string result
    ret = sqlite3_step(pstmt);
    if (ret == SQLITE_ROW) {
        const unsigned char *text = sqlite3_column_text(pstmt, 0);
        if (text) {
            brand_name = strdup((const char *)text);
        }
    }

    sqlite3_finalize(pstmt);
    return brand_name;
}
char*hplc_temperature_str(hplc_sint temperature)
{
	if(temperature==AMB)
		return strdup("amb");
	else{
		return hplc_sint_str(temperature);
	}
}
void hplc_metadata_print(struct hplc_run_metadata*data)
{
	struct hplc_column*clmn;
	char*tmp,*tmp_ph,*tmp_sv_p,*tmp_fl,*tmp_cl_len,*tmp_cl_id,*tmp_cl_ps,
	*tmp_brand,*tmp_sv_pe,*tmp_tg;

		clmn=&data->column;
		tmp=hplc_temperature_str(data->temperature);
		//printf("\033[35m%s\033[0m\n",tmp);
		tmp_ph=hplc_sint_str(data->pH);
		tmp_fl=hplc_sint_str(data->flow_rate);
		tmp_cl_len=hplc_sint_str(clmn->clmn_length);
		tmp_cl_id=hplc_sint_str(clmn->clmn_id);
		tmp_cl_ps=hplc_sint_str(clmn->clmn_bead_size);
		tmp_brand=hplc_clmn_brand_str(clmn->brand);

	if(data->run.type==ISO){
		tmp_sv_p=hplc_sint_str(data->run.iso.slvnt_percent);

		printf(
		"Solvent:%s\n"
		"pH:%s\n"
		"Run Type:%s\n"
		"Solvent Percent:%s\n"
		"Flow Rate:%s\n"
		"Injection Volume:%s\n"
		"Injection Type:%s\n"
		"Column Phase:%s\n"
		"Column Brand:%s\n"
		"Column Length:%s\n"
		"Column Internal Diameter:%s\n"
		"Column Bead Size:%s\n"
		"Temperature:%s\n"
		"Wavelength:%d\n",
		data->slvnt,tmp_ph,hplc_run_type_str(data->run.type),tmp_sv_p,
		tmp_fl,data->njctn_volume,data->njctn_type,clmn->clmn_phase,
		tmp_brand,tmp_cl_len,tmp_cl_id,tmp_cl_ps,tmp,data->wavelength);
		
		free(tmp_sv_p);
	}
	else{
		tmp_sv_p=hplc_sint_str(data->run.gra.slvnt_start);
		tmp_sv_pe=hplc_sint_str(data->run.gra.slvnt_end);
		tmp_tg=hplc_sint_str(data->run.gra.tg);

		printf(
		"Solvent:%s\n"
		"pH:%s\n"
		"Run Type:%s\n"
		"Solvent %% start:%s\n"
		"Solvent %% end:%s\n"
		"tg:%s\n"
		"Flow Rate:%s\n"
		"Injection Volume:%s\n"
		"Injection Type:%s\n"
		"Column Phase:%s\n"
		"Column Brand:%s\n"
		"Column Length:%s\n"
		"Column Internal Diameter:%s\n"
		"Column Bead Size:%s\n"
		"Temperature:%s\n"
		"Wavelength:%d\n",
		data->slvnt,tmp_ph,hplc_run_type_str(data->run.type),tmp_sv_p,
		tmp_sv_pe,tmp_tg,
		tmp_fl,data->njctn_volume,data->njctn_type,clmn->clmn_phase,
		tmp_brand,tmp_cl_len,tmp_cl_id,tmp_cl_ps,tmp,data->wavelength);

		free(tmp_sv_p);
		free(tmp_sv_pe);
		free(tmp_tg);
	}

	free(tmp);
	free(tmp_ph);
	free(tmp_fl);
	free(tmp_cl_len);
	free(tmp_cl_id);
	free(tmp_cl_ps);
}
hplc_sint hplc_clmn_long(struct hplc_column *clmn)
{
    sqlite3_stmt *pstmt = NULL;
    int ret;
    const char *sql = "select length from hplc_columns "
    "where diameter=? and ps=? and lig=? and length>?"
    "order by length asc";
    hplc_sint length = 0;

    // Prepare statement
    ret = sqlite3_prepare_v2(pDb, sql, -1, &pstmt, NULL);
    if (ret != SQLITE_OK) {
        return 0;
    }

    // Bind values based on mapping:
    // ?1 = diameter (clmn_id)
    // ?2 = ps (clmn_bead_size)
    // ?3 = lig (clmn_phase)
    if (sqlite3_bind_int64(pstmt, 1, (long)clmn->clmn_id)!= SQLITE_OK ||
        sqlite3_bind_int64(pstmt, 2, (long)clmn->clmn_bead_size)!= SQLITE_OK ||
        sqlite3_bind_text(pstmt,3,clmn->clmn_phase,-1,SQLITE_STATIC)!=SQLITE_OK
	||sqlite3_bind_int64(pstmt,4,(long)clmn->clmn_length)){
        sqlite3_finalize(pstmt);
        return 0;
    }

    // Execute query and extract result
    ret = sqlite3_step(pstmt);
    if (ret == SQLITE_ROW) {
        length = (hplc_sint)sqlite3_column_int(pstmt, 0);
    } else {
        length = 0;
    }

    sqlite3_finalize(pstmt);
    return length;
}

hplc_sint hplc_clmn_ps_small(struct hplc_column *clmn)
{
    sqlite3_stmt *pstmt = NULL;
    int ret;
    const char *sql = "select ps,brand from hplc_columns "
                      "where diameter=? and ps<? and lig=? and length=? "
                      "order by ps desc";
    hplc_sint ps = 0;


    // Prepare SQL query
    ret = sqlite3_prepare_v2(pDb, sql, -1, &pstmt, NULL);
    if (ret != SQLITE_OK) {
        return 0;
    }

    // Bind parameters in sequential order:
    // ?1 = diameter -> clmn_id
    // ?2 = ps       -> clmn_bead_size
    // ?3 = lig      -> clmn_phase
    // ?4 = length   -> clmn_length
    if (sqlite3_bind_int(pstmt, 1, (int)clmn->clmn_id) != SQLITE_OK ||
        sqlite3_bind_int(pstmt, 2, (int)clmn->clmn_bead_size) != SQLITE_OK ||
        sqlite3_bind_text(pstmt, 3, clmn->clmn_phase, -1, SQLITE_STATIC) != SQLITE_OK ||
        sqlite3_bind_int(pstmt, 4, (int)clmn->clmn_length) != SQLITE_OK) {
        sqlite3_finalize(pstmt);
        return 0;
    }

    ret = sqlite3_step(pstmt);
    if (ret == SQLITE_ROW) {
        ps = (hplc_sint)sqlite3_column_int(pstmt, 0);
    }

    sqlite3_finalize(pstmt);
    return ps;
}
