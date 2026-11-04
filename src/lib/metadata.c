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
bool hplc_metadata_column(char*argv[],struct hplc_column*data)
{
	char c=argv[11][strlen(argv[11])-1];
	argv[11][strlen(argv[11])-1]='\0';
	
	if(!hplc_brand_id(c,&data->brand)){
		print_error("Brand short %c not valid\n",c);
		return false;
	}
	
	data->clmn_phase=strdup(argv[11]);
	
	char*argv1[3],*tail=NULL;
	int argc;

	argc=tokenize_str1(argv[12],argv1,3,'x');
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
	return true;
}
bool hplc_metadata(char*file_name,struct hplc_run_metadata*data)
{
	char*argv[NAME_MAX_ARGS],*tail=NULL;
	int argc;

	argc=tokenize_str1(file_name,argv,NAME_MAX_ARGS,'-');
	
	if(argc<15){
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
	
	if(!hplc_metadata_column(argv,&data->column)){
		return false;
	}
	if(!strcmp(argv[13],"amb"))
		data->temperature=AMB;
	else{
		if(argv[13][0]=='t'||argv[13][0]=='T')
			argv[13]++;
		data->temperature=HPLC_STRTOD(argv[13]){
			fprintf(stderr,"Temperature tail:%s\n",tail);
			tail=NULL;
		}
	}
	data->wavelength=(int)strtol(argv[14],&tail,10);
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
	*tmp_brand;

	clmn=&data->column;
	tmp=hplc_temperature_str(data->temperature);
	//printf("\033[35m%s\033[0m\n",tmp);
	tmp_ph=hplc_sint_str(data->pH);
	tmp_sv_p=hplc_sint_str(data->slvnt_percent);
	tmp_fl=hplc_sint_str(data->flow_rate);
	tmp_cl_len=hplc_sint_str(clmn->clmn_length);
	tmp_cl_id=hplc_sint_str(clmn->clmn_id);
	tmp_cl_ps=hplc_sint_str(clmn->clmn_bead_size);
	tmp_brand=hplc_clmn_brand_str(clmn->brand);

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
	data->slvnt,tmp_ph,hplc_run_type_str(data->type),tmp_sv_p,
	tmp_fl,data->njctn_volume,data->njctn_type,clmn->clmn_phase,
	tmp_brand,tmp_cl_len,tmp_cl_id,tmp_cl_ps,tmp,data->wavelength);

	free(tmp);
	free(tmp_ph);
	free(tmp_sv_p);
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
    const char *sql = "select ps from hplc_columns "
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
