#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "hplc.h"
#include "query.h"
#include "run-db.h"
#include "lib/metadata.h"
#include "cli.h"

/* Global database handle assumed from calling environment */
bool hplc_run_add(struct hplc_run_metadata *data, char *pk)
{
	int ret;
	sqlite3_stmt *pStmt;
	char temp_buf[32];
	struct hplc_column*clmn=&data->column;
	char*sql= "insert into hplc_run (grid,slvnt,ph,type,flow_rate,"
		"njctn_volume,njctn_type,sample_conc,clmn_id,temperature,wave)"
		"values(?,?,?,?,?,?,?,?,?,?,?) "
		"returning id";


	if (!data || !pk) {
		fprintf(stderr, "hplc_run_add: NULL argument passed\n");
		return false;
	}

	ret = sqlite3_prepare_v2(pDb, sql, -1, &pStmt, NULL);
	if (ret != SQLITE_OK) {
		fprintf(stderr, "hplc_run_add: failed to prepare statement: %s\n",
			sqlite3_errmsg(pDb));
		return false;
	}

	sqlite3_bind_text(pStmt, 1, pk, -1, SQLITE_STATIC);

	/* Columns 2-14: direct metadata mappings */
	sqlite3_bind_text(pStmt, 2, data->slvnt, -1, SQLITE_STATIC);
	sqlite3_bind_int64(pStmt, 3, data->pH);
	sqlite3_bind_int(pStmt, 4, (int)data->run.type);
//	sqlite3_bind_int64(pStmt, 5, data->slvnt_percent);
	sqlite3_bind_int64(pStmt, 5, data->flow_rate);
	sqlite3_bind_text(pStmt, 6, data->njctn_volume, -1, SQLITE_STATIC);
	sqlite3_bind_text(pStmt, 7, data->njctn_type, -1, SQLITE_STATIC);
	sqlite3_bind_int64(pStmt, 8, data->sample_conc);
	sqlite3_bind_int(pStmt,9,data->column.id);

	if(data->temperature==AMB)
		sqlite3_bind_text(pStmt, 10,"amb", -1, SQLITE_TRANSIENT);
	else{
		char*tmp_temp=hplc_sint_str(data->temperature);
		sqlite3_bind_text(pStmt, 10, tmp_temp, -1, SQLITE_TRANSIENT);
		free(tmp_temp);
	}

	sqlite3_bind_int(pStmt, 11, data->wavelength);

	ret = sqlite3_step(pStmt);
	if (ret != SQLITE_ROW) {
		fprintf(stderr, "hplc_run_add: execution failed: %s\n",
			sqlite3_errmsg(pDb));
		sqlite3_finalize(pStmt);
		return false;
	}
	
	int run_id;
	char*rsql;
	
	run_id=sqlite3_column_int(pStmt,0);
	sqlite3_finalize(pStmt);

	if(data->run.type==ISO){
		rsql="insert into hplc_run_iso values (?,?)";
		ret=sqlite3_prepare_v2(pDb,rsql,strlen(rsql)+1,&pStmt,NULL);
		if(ret!=SQLITE_OK){
			print_error("hplc_run_add:type:prepare failed\n");
			return false;
		}
		sqlite3_bind_int(pStmt,1,run_id);
		sqlite3_bind_int64(pStmt,2,data->run.iso.slvnt_percent);
	}
	else{
		rsql="insert into hplc_run_gra values (?,?,?,?)";
		ret=sqlite3_prepare_v2(pDb,rsql,strlen(rsql)+1,&pStmt,NULL);
		if(ret!=SQLITE_OK){
			print_error("hplc_run_add:type:prepare failed\n");
			return false;
		}
		sqlite3_bind_int(pStmt,1,run_id);
		sqlite3_bind_int64(pStmt,2,data->run.gra.slvnt_start);
		sqlite3_bind_int64(pStmt,3,data->run.gra.slvnt_end);
		sqlite3_bind_int64(pStmt,4,data->run.gra.tg);
	}
	ret=sqlite3_step(pStmt);
	if(ret!=SQLITE_DONE){
		print_error("hplc_run_add:type:step failed\n");
		return false;
	}
	sqlite3_finalize(pStmt);
	return true;
}
void hplc_run_delete(void)
{
	int ret;
	sqlite3_stmt *pStmt;
	char *sql = "DELETE FROM hplc_run";

	ret = sqlite3_prepare_v2(pDb, sql, strlen(sql)+1, &pStmt, NULL);
	if (ret != SQLITE_OK) {
		fprintf(stderr, "hplc_run_delete: failed to prepare statement: %s\n",
			sqlite3_errmsg(pDb));
		return;
	}

	ret = sqlite3_step(pStmt);
	if (ret != SQLITE_DONE) {
		fprintf(stderr, "hplc_run_delete: execution failed: %s\n",
			sqlite3_errmsg(pDb));
	}

	sqlite3_finalize(pStmt);
}
bool hplc_column_retrive(struct hplc_column *clmn, int id)
{
        int ret;
        sqlite3_stmt *pstmt = NULL;
        const char *sql = "select brand,length,diameter,ps,lig "
                          "from hplc_columns where id=?";

        // Prepare the SQL statement
        ret = sqlite3_prepare_v2(pDb, sql, -1, &pstmt, NULL);
        if (ret != SQLITE_OK) {
                return false;
        }

        // Bind id parameter
        if (sqlite3_bind_int(pstmt, 1, id) != SQLITE_OK) {
                sqlite3_finalize(pstmt);
                return false;
        }

        // Fetch row and populate struct fields
        ret = sqlite3_step(pstmt);
        if (ret == SQLITE_ROW) {
                clmn->brand = sqlite3_column_int(pstmt, 0);
                clmn->clmn_length = (hplc_sint)sqlite3_column_int64(pstmt, 1);
                clmn->clmn_id = (hplc_sint)sqlite3_column_int64(pstmt, 2);
                clmn->clmn_bead_size = (hplc_sint)sqlite3_column_int64(pstmt, 3);

                const unsigned char *text = sqlite3_column_text(pstmt, 4);
                if (text) {
                        clmn->clmn_phase = strdup((const char *)text);
                } else {
                        print_error("hplc_retrive_column:phase not found\n");
			sqlite3_finalize(pstmt);
			return false;
                }
        }

        sqlite3_finalize(pstmt);
        return true;
}
bool hplc_run_retrive(struct hplc_run_metadata *data, char *pk)
{
	int ret,run_id;
	sqlite3_stmt *pStmt;
	const unsigned char *txt;
	struct hplc_column*clmn=&data->column;
	char *sql = "select id,grid,slvnt,ph,type,slvnt_percent,flow_rate,njctn_volume,"
		    "njctn_type,sample_conc,clmn_id,"
		    "temperature,wave from hplc_run where grid=?";

	if (!data || !pk) {
		fprintf(stderr, "hplc_run_retrive: NULL argument passed\n");
		return false;
	}

	/* Zero-initialize pointers in data to ensure clean teardown on failure */
	data->slvnt = NULL;
	data->njctn_volume = NULL;
	data->njctn_type = NULL;

	ret = sqlite3_prepare_v2(pDb, sql, -1, &pStmt, NULL);
	if (ret != SQLITE_OK) {
		fprintf(stderr, "hplc_run_retrive: failed to prepare query: %s\n",
			sqlite3_errmsg(pDb));
		return false;
	}

	sqlite3_bind_text(pStmt, 1, pk, -1, SQLITE_STATIC);

	ret = sqlite3_step(pStmt);
	if (ret != SQLITE_ROW) {
		fprintf(stderr, "hplc_run_retrive: record non-existent or query error for primary key '%s': %s\n",
			pk, sqlite3_errmsg(pDb));
		sqlite3_finalize(pStmt);
		return false;
	}

	/* Retrieve slvnt */
	run_id=sqlite3_column_int(pStmt,0);
	txt = sqlite3_column_text(pStmt, 1);
	if (txt) {
		data->slvnt = strdup((const char *)txt);
		if (!data->slvnt) {
			fprintf(stderr, "hplc_run_retrive: memory allocation failed for slvnt\n");
			goto fail;
		}
	}

	data->pH = sqlite3_column_int(pStmt, 2);
	data->run.type = (enum hplc_run_type)sqlite3_column_int(pStmt, 3);
//	data->run.iso.slvnt_percent = sqlite3_column_int(pStmt, 4);
	data->flow_rate = sqlite3_column_int(pStmt, 4);

	/* Retrieve njctn_volume */
	txt = sqlite3_column_text(pStmt, 5);
	if (txt) {
		data->njctn_volume = strdup((const char *)txt);
		if (!data->njctn_volume) {
			fprintf(stderr, "hplc_run_retrive: memory allocation failed for njctn_volume\n");
			goto fail;
		}
	}

	/* Retrieve njctn_type */
	txt = sqlite3_column_text(pStmt, 6);
	if (txt) {
		data->njctn_type = strdup((const char *)txt);
		if (!data->njctn_type) {
			fprintf(stderr, "hplc_run_retrive: memory allocation failed for njctn_type\n");
			goto fail;
		}
	}

	data->sample_conc = sqlite3_column_int64(pStmt, 7);

	data->column.id = sqlite3_column_int(pStmt,8);

	/* Convert text back to double for temperature */
	if(!strcmp(sqlite3_column_text(pStmt,9),"amb")){
		data->temperature=AMB;
	}
	else
		data->temperature = sqlite3_column_int64(pStmt, 9);

	/* Retrieve wavelength (stored as wave in DB) */
	data->wavelength = sqlite3_column_int(pStmt, 10);

	sqlite3_finalize(pStmt);

	char*rsql;

	if(data->run.type==ISO){
		rsql="select slvnt_percent from hplc_run_iso where id=?";
		sqlite3_prepare_v2(pDb,rsql,strlen(rsql)+1,&pStmt,NULL);
		sqlite3_bind_int(pStmt,1,run_id);
		ret=sqlite3_step(pStmt);
		if(ret!=SQLITE_ROW){
			print_error("hplc_run_retrive:type:step failed\n");
			return false;
		}
		data->run.iso.slvnt_percent=sqlite3_column_int64(pStmt,0);
	}
	else{
		rsql="select slvnt_start,slvnt_end,tg "
		"from hplc_run_gra where id=?";
		sqlite3_prepare_v2(pDb,rsql,strlen(rsql)+1,&pStmt,NULL);
		sqlite3_bind_int(pStmt,1,run_id);
		ret=sqlite3_step(pStmt);
		if(ret!=SQLITE_ROW){
			print_error("hplc_run_retrive:type:step failed\n");
			return false;
		}
		data->run.gra.slvnt_start=sqlite3_column_int64(pStmt,0);
		data->run.gra.slvnt_end=sqlite3_column_int64(pStmt,1);
		data->run.gra.tg=sqlite3_column_int64(pStmt,2);
	}
	sqlite3_finalize(pStmt);

	if(hplc_column_retrive(&data->column,data->column.id));
		return true;
	return false;

fail:
	if (data->slvnt) {
		free(data->slvnt);
		data->slvnt = NULL;
	}
	if (data->njctn_volume) {
		free(data->njctn_volume);
		data->njctn_volume = NULL;
	}
	if (data->njctn_type) {
		free(data->njctn_type);
		data->njctn_type = NULL;
	}
	sqlite3_finalize(pStmt);
	return false;
}
sqlite3_stmt*hplc_run_table_prepare()
{
	int ret;
	sqlite3_stmt*pStmt;
	const char*tail;
	char *sql = "select hr.grid,hr.slvnt,hr.ph,hr.type,hr.flow_rate, "
		"hr.njctn_volume,hr.njctn_type,hr.sample_conc,hr.temperature,"
		"hr.wave,hc.brand,hc.clmn_phase,hc.clmn_length,hc.clmn_id,"
                    "hc.clmn_bead_size "
		    "from hplc_run hr "
		    "join hplc_columns hc on hr.clmn_id=hc.id ";

	ret=sqlite3_prepare_v3(pDb,sql,strlen(sql),0,&pStmt,&tail);
	if(ret!=SQLITE_OK){
		fprintf(stderr,"hplc_metadata_prepare:%s\ntail:%s",sqlite3_errstr(ret),tail);
		return NULL;
	}
	return pStmt;
}
void hplc_run_table_row_destroy(sqlite3_stmt*pStmt)
{
	sqlite3_finalize(pStmt);
}
bool hplc_run_table_row(sqlite3_stmt *pStmt, char ***pdata)
{
	int ret;
	int cols,ecols=12;
	int i, j,k;
	const char *txt;
	char **row;

	if (!pStmt || !pdata) {
		fprintf(stderr, "hplc_run_table_row: NULL argument passed\n");
		return false;
	}

	*pdata = NULL;

	ret = sqlite3_step(pStmt);
	if (ret == SQLITE_DONE) {
		return false;
	}
	if (ret != SQLITE_ROW) {
		fprintf(stderr, "hplc_run_table_row: sqlite3_step failed: %s\n",
			sqlite3_errstr(ret));
		return false;
	}

	cols = sqlite3_column_count(pStmt);
	row = (char **)malloc(ecols * sizeof(char *));
	if (!row) {
		fprintf(stderr, "hplc_run_table_row: memory allocation failed for row array\n");
		return false;
	}

	for (i = 0,k=0; i < cols; i++,k++) {
		char*tmp;
		int size;
		int itmp=0;
		
		if(i==3){
			itmp = sqlite3_column_int(pStmt, i);
			row[k]=strdup(hplc_run_type_str(itmp));
			continue;
		}
		if(i==10){
			if(i+1>=cols){
				fprintf(stderr,
				"hplc_run_table_row:%d >%d (cols)\n",i+1,cols);
				goto loop_error;
			}
			txt = (const char *)sqlite3_column_text(pStmt, i++);
			itmp = sqlite3_column_int(pStmt, i);
			tmp=(char*)hplc_clmn_brand_str(itmp);
			size=snprintf(NULL,0,"%s %s",txt,tmp);
			if(size<0){
				fprintf(stderr,"hplc_run_table_row:Loop error\n");
				goto loop_error;
			}
			row[k]=malloc(sizeof(char)*++size);
			snprintf(row[k],size,"%s %s",txt,tmp);
			row[k][size-1]='\0';
			free(tmp);
			continue;
		}
		if(i==12){
			if(i+2>=cols){
				fprintf(stderr,
                                "hplc_run_table_row:%d >%d (cols)\n",i+1,cols);
                                goto loop_error;
                        }
			char* tmp1,*tmp2,*tmp3;

			tmp3 = hplc_sint_str((hplc_sint)
				sqlite3_column_int64(pStmt, i++));
			tmp2 = hplc_sint_str((hplc_sint)
				sqlite3_column_int64(pStmt, i++));
			tmp1 = hplc_sint_str((hplc_sint)
				sqlite3_column_int64(pStmt, i));
			size=snprintf(NULL,0,"%sx%sx%s",tmp3,tmp2,tmp1);
			if(size<0){
				fprintf(stderr,"hplc_run_table_row:Loop error\n");
				goto loop_error;
			}
			row[k]=malloc(sizeof(char)*++size);
			snprintf(row[k],size,"%sx%sx%s",tmp3,tmp2,tmp1);
			row[k][size-1]='\0';
			continue;
		}


		txt = (const char *)sqlite3_column_text(pStmt, i);
		if (txt) {
			row[k] = strdup(txt);
		} else {
			row[k] = strdup("");
		}
		if (!row[k]) {
			fprintf(stderr, "hplc_run_table_row: memory allocation failed for column %d\n", i);
			loop_error:
			for (j = 0; j < k; j++) {
				free(row[j]);
			}
			free(row);
			return false;
		}
	}

	*pdata = row;
	return true;
}
bool hplc_run_check(char* pk)
{
        int ret;
        sqlite3_stmt* pStmt;
        const char *sql = "SELECT 1 FROM hplc_run WHERE grid = ? LIMIT 1;";
        bool exists = false;

        ret = sqlite3_prepare_v2(pDb, sql, -1, &pStmt, NULL);
        if (ret != SQLITE_OK) {
                fprintf(stderr, "Error [hplc_run_check]: Failed to prepare statement: %s\n", sqlite3_errmsg(pDb));
                return false;
        }

        // Bind the primary key (pk) to the prepared statement parameter
        ret = sqlite3_bind_text(pStmt, 1, pk, -1, SQLITE_STATIC);
        if (ret != SQLITE_OK) {
                fprintf(stderr, "Error [hplc_run_check]: Failed to bind primary key parameter: %s\n", sqlite3_errmsg(pDb));
                sqlite3_finalize(pStmt);
                return false;
        }

        ret = sqlite3_step(pStmt);

        if (ret == SQLITE_ROW) {
                exists = true;
        } else if (ret != SQLITE_DONE) {
                fprintf(stderr, "Error [hplc_run_check]: Step execution failed: %s\n", sqlite3_errmsg(pDb));
        }

        // Clean up statement handle
        sqlite3_finalize(pStmt);
        return exists;
}
bool hplc_run_meta_check(struct hplc_run_metadata* data, char** pk)
{
        int ret;
        sqlite3_stmt* pStmt;
	struct hplc_column*clmn=&data->column;

        if (!data) {
                fprintf(stderr, "Error [hplc_run_meta_check]: Input metadata struct is NULL.\n");
                return false;
        }

        char temp_str[64];
	if(data->temperature==AMB)
		strcpy(temp_str,"amb");
	else{
		char*tmp_temp=hplc_sint_str(data->temperature);
		if(sizeof(temp_str)<strlen(tmp_temp)+1){
			print_error(
			"hplc_run_meta_check:buffer overflow of temperature\n");
			free(tmp_temp);
			return false;
		}
		strcpy(temp_str,tmp_temp);
		free(tmp_temp);
	}

        char *sql;

	if(data->run.type==ISO){
		sql=
                "SELECT grid FROM hplc_run hr "
		"join hplc_run_iso hi on hr.id=hi.id WHERE "
                "hr.slvnt = ? AND "
                "hr.ph = ? AND "
                "hr.type = ? AND "
                "hr.flow_rate = ? AND "
                "hr.njctn_volume = ? AND "
                "hr.njctn_type = ? AND "
                "hr.sample_conc = ? AND "
                "hr.clmn_id = ? AND "
                "hr.temperature = ? AND "
                "hr.wave = ? AND "
                "hi.slvnt_percent = ? "
                "LIMIT 1;";
	}
	else{
		sql=
		"select grid from hplc_run hr "
		"join hplc_run_gra hg on hr.id=hg.id where "
                "hr.slvnt = ? AND "
                "hr.ph = ? AND "
                "hr.type = ? AND "
                "hr,flow_rate = ? AND "
                "hr.njctn_volume = ? AND "
                "hr.njctn_type = ? AND "
                "hr.sample_conc = ? AND "
                "hr.clmn_id = ? AND "
                "hr.temperature = ? AND "
                "hr.wave = ? AND "
		"hg.slvnt_start=? AND "
		"hg.slvnt_end=? AND "
		"hg.tg=? "
		"limit 1;";
	}

        ret = sqlite3_prepare_v2(pDb, sql, -1, &pStmt, NULL);
        if (ret != SQLITE_OK) {
                fprintf(stderr, "Error [hplc_run_meta_check]: Failed to prepare statement: %s\n", sqlite3_errmsg(pDb));
                return false;
        }

	sqlite3_bind_text(pStmt, 1, data->slvnt, -1, SQLITE_STATIC);
        sqlite3_bind_int64(pStmt, 2, data->pH);
        sqlite3_bind_int(pStmt, 3, (int)data->run.type);
        sqlite3_bind_int64(pStmt, 4, data->flow_rate);
	sqlite3_bind_text(pStmt, 5, data->njctn_volume, -1, SQLITE_STATIC);
	sqlite3_bind_text(pStmt, 6, data->njctn_type, -1, SQLITE_STATIC);
        sqlite3_bind_int64(pStmt, 7, data->sample_conc);
        sqlite3_bind_int64(pStmt, 8, clmn->id);
        sqlite3_bind_text(pStmt, 9, temp_str, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(pStmt, 10, data->wavelength);
	if(data->run.type==ISO)
	        sqlite3_bind_int64(pStmt, 11, data->run.iso.slvnt_percent);
	else{
		sqlite3_bind_int64(pStmt,11,data->run.gra.slvnt_start);
		sqlite3_bind_int64(pStmt,12,data->run.gra.slvnt_end);
		sqlite3_bind_int64(pStmt,13,data->run.gra.tg);
	}

        bool matched = false;
        ret = sqlite3_step(pStmt);

        if (ret == SQLITE_ROW) {
                matched = true;
                if (pk != NULL) {
                        const unsigned char *grid_val = sqlite3_column_text(pStmt, 0);
                        if (grid_val) {
                                *pk = strdup((const char*)grid_val);
                                if (!*pk) {
                                        fprintf(stderr, "Error [hplc_run_meta_check]: Memory allocation failed for pk.\n");
                                        sqlite3_finalize(pStmt);
                                        return false;
                                }
                        } else {
                                *pk = NULL;
                        }
                }
        } else if (ret != SQLITE_DONE) {
                fprintf(stderr, "Error [hplc_run_meta_check]: Step execution failed: %s\n", sqlite3_errmsg(pDb));
        }

        sqlite3_finalize(pStmt);
        return matched;
}
bool hplc_run_col_check(char *col)
{
    int ret;
    sqlite3_stmt *pStmt;
    bool found = false;

    const char *sql = "SELECT 1 FROM hplc_columns WHERE lig = ? LIMIT 1;";

    // Prepare SQL statement
    ret = sqlite3_prepare_v2(pDb, sql, -1, &pStmt, NULL);
    if (ret != SQLITE_OK) {
    	fprintf(stderr,"hplc_run_col_check:%s\n",sqlite3_errstr(ret));
        return false;
    }

    // Bind 'col' to the parameter '?'
    ret=sqlite3_bind_text(pStmt, 1, col, -1, SQLITE_STATIC);
    if(ret!=SQLITE_OK){
    	fprintf(stderr,"hplc_run_col_check:%s\n",sqlite3_errstr(ret));
	sqlite3_finalize(pStmt);
	return false;
   }

    // Execute step
    if (sqlite3_step(pStmt) == SQLITE_ROW) {
        found = true;
    }

    // Finalize statement to prevent resource leaks
    sqlite3_finalize(pStmt);

    return found;
}
void hplc_column_brand(struct hplc_column *clmn)
{
    int ret;
    sqlite3_stmt *pstmt = NULL;
    const char *sql = "select id,brand from hplc_columns where length=? and "
                      "diameter=? and ps=? and lig=?";

    if (!clmn || !clmn->clmn_phase || !pDb) {
        return;
    }

    ret = sqlite3_prepare_v2(pDb, sql, -1, &pstmt, NULL);
    if (ret != SQLITE_OK) {
        return;
    }

    // Bind parameters: 1 = length, 2 = diameter (clmn_id), 3 = ps (bead size), 4 = lig (clmn_phase)
    if (sqlite3_bind_int64(pstmt, 1, (sqlite3_int64)clmn->clmn_length) != SQLITE_OK ||
        sqlite3_bind_int64(pstmt, 2, (sqlite3_int64)clmn->clmn_id) != SQLITE_OK ||
        sqlite3_bind_int64(pstmt, 3, (sqlite3_int64)clmn->clmn_bead_size) != SQLITE_OK ||
        sqlite3_bind_text(pstmt, 4, clmn->clmn_phase, -1, SQLITE_STATIC) != SQLITE_OK) {
        sqlite3_finalize(pstmt);
        return;
    }

    ret = sqlite3_step(pstmt);
    if (ret == SQLITE_ROW) {
        // Update struct fields with retrieved SQLite results
        clmn->id = sqlite3_column_int(pstmt, 0);
        clmn->brand = sqlite3_column_int(pstmt, 1);
    }

    sqlite3_finalize(pstmt);
}
