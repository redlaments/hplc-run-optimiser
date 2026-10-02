#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "hplc.h"
#include "query.h"
#include "run-db.h"

/* Global database handle assumed from calling environment */
bool hplc_run_add(struct hplc_run_metadata *data, char *pk)
{
	int ret;
	sqlite3_stmt *pStmt;
	char temp_buf[32];
	char *sql = "insert into hplc_run values(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";

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

	/* Column 1: grid (primary key) */
	sqlite3_bind_text(pStmt, 1, pk, -1, SQLITE_STATIC);

	/* Columns 2-14: direct metadata mappings */
	sqlite3_bind_text(pStmt, 2, data->slvnt, -1, SQLITE_STATIC);
	sqlite3_bind_double(pStmt, 3, data->pH);
	sqlite3_bind_int(pStmt, 4, (int)data->type);
	sqlite3_bind_double(pStmt, 5, data->slvnt_percent);
	sqlite3_bind_double(pStmt, 6, data->flow_rate);
	sqlite3_bind_text(pStmt, 7, data->njctn_volume, -1, SQLITE_STATIC);
	sqlite3_bind_text(pStmt, 8, data->njctn_type, -1, SQLITE_STATIC);
	sqlite3_bind_double(pStmt, 9, data->sample_conc);
	sqlite3_bind_text(pStmt, 10, data->clmn_phase, -1, SQLITE_STATIC);
	sqlite3_bind_int(pStmt, 11, (int)data->brand);
	sqlite3_bind_double(pStmt, 12, data->clmn_length);
	sqlite3_bind_double(pStmt, 13, data->clmn_id);
	sqlite3_bind_double(pStmt, 14, data->clmn_bead_size);

	/* Column 15: temperature (stored as text in DB) */
	if(isnan(data->temperature))
		sqlite3_bind_text(pStmt, 15,"amb", -1, SQLITE_TRANSIENT);
	else{
		snprintf(temp_buf, sizeof(temp_buf), "%f", data->temperature);
		sqlite3_bind_text(pStmt, 15, temp_buf, -1, SQLITE_TRANSIENT);
	}

	/* Column 16: wave (stored as wave integer in DB) */
	sqlite3_bind_int(pStmt, 16, data->wavelength);

	ret = sqlite3_step(pStmt);
	if (ret != SQLITE_DONE) {
		fprintf(stderr, "hplc_run_add: execution failed: %s\n",
			sqlite3_errmsg(pDb));
		sqlite3_finalize(pStmt);
		return false;
	}

	sqlite3_finalize(pStmt);
	return true;
}

bool hplc_run_retrive(struct hplc_run_metadata *data, char *pk)
{
	int ret;
	sqlite3_stmt *pStmt;
	const unsigned char *txt;
	char *sql = "select grid,slvnt,ph,type,slvnt_percent,flow_rate,njctn_volume,"
		    "njctn_type,sample_conc,clmn_phase,brand,clmn_length,clmn_id,"
		    "clmn_bead_size,temperature,wave from hplc_run where grid=?";

	if (!data || !pk) {
		fprintf(stderr, "hplc_run_retrive: NULL argument passed\n");
		return false;
	}

	/* Zero-initialize pointers in data to ensure clean teardown on failure */
	data->slvnt = NULL;
	data->njctn_volume = NULL;
	data->njctn_type = NULL;
	data->clmn_phase = NULL;

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
	txt = sqlite3_column_text(pStmt, 1);
	if (txt) {
		data->slvnt = strdup((const char *)txt);
		if (!data->slvnt) {
			fprintf(stderr, "hplc_run_retrive: memory allocation failed for slvnt\n");
			goto fail;
		}
	}

	data->pH = sqlite3_column_double(pStmt, 2);
	data->type = (enum hplc_run_type)sqlite3_column_int(pStmt, 3);
	data->slvnt_percent = sqlite3_column_double(pStmt, 4);
	data->flow_rate = sqlite3_column_double(pStmt, 5);

	/* Retrieve njctn_volume */
	txt = sqlite3_column_text(pStmt, 6);
	if (txt) {
		data->njctn_volume = strdup((const char *)txt);
		if (!data->njctn_volume) {
			fprintf(stderr, "hplc_run_retrive: memory allocation failed for njctn_volume\n");
			goto fail;
		}
	}

	/* Retrieve njctn_type */
	txt = sqlite3_column_text(pStmt, 7);
	if (txt) {
		data->njctn_type = strdup((const char *)txt);
		if (!data->njctn_type) {
			fprintf(stderr, "hplc_run_retrive: memory allocation failed for njctn_type\n");
			goto fail;
		}
	}

	data->sample_conc = sqlite3_column_double(pStmt, 8);

	/* Retrieve clmn_phase */
	txt = sqlite3_column_text(pStmt, 9);
	if (txt) {
		data->clmn_phase = strdup((const char *)txt);
		if (!data->clmn_phase) {
			fprintf(stderr, "hplc_run_retrive: memory allocation failed for clmn_phase\n");
			goto fail;
		}
	}

	data->brand = (enum hplc_clmn_brand)sqlite3_column_int(pStmt, 10);
	data->clmn_length = sqlite3_column_double(pStmt, 11);
	data->clmn_id = sqlite3_column_double(pStmt, 12);
	data->clmn_bead_size = sqlite3_column_double(pStmt, 13);

	/* Convert text back to double for temperature */
	txt = sqlite3_column_text(pStmt, 14);
	data->temperature = txt ? strtod((const char *)txt, NULL) : NAN;

	/* Retrieve wavelength (stored as wave in DB) */
	data->wavelength = sqlite3_column_int(pStmt, 15);

	sqlite3_finalize(pStmt);
	return true;

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
	if (data->clmn_phase) {
		free(data->clmn_phase);
		data->clmn_phase = NULL;
	}
	sqlite3_finalize(pStmt);
	return false;
}
sqlite3_stmt*hplc_run_table_prepare()
{
	int ret;
	sqlite3_stmt*pStmt;
	const char*tail;
	char *sql = "select grid,slvnt,ph,type,slvnt_percent,flow_rate,njctn_volume, "
                    "njctn_type,sample_conc,clmn_phase,brand,clmn_length,clmn_id, "
                    "clmn_bead_size,temperature,wave from hplc_run";

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
	int cols,ecols=13;
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

		if(i==9){
			if(i+1>=cols){
				fprintf(stderr,
				"hplc_run_table_row:%d >%d (cols)\n",i+1,cols);
				goto loop_error;
			}
			txt = (const char *)sqlite3_column_text(pStmt, i++);
			tmp = (const char *)sqlite3_column_text(pStmt, i);
			size=snprintf(NULL,0,"%s %s",txt,tmp);
			if(size<0){
				fprintf(stderr,"hplc_run_table_row:Loop error\n");
				goto loop_error;
			}
			row[k]=malloc(sizeof(char)*++size);
			snprintf(row[k],size,"%s %s",txt,tmp);
			row[k][size-1]='\0';
			continue;
		}
		if(i==11){
			if(i+2>=cols){
				fprintf(stderr,
                                "hplc_run_table_row:%d >%d (cols)\n",i+1,cols);
                                goto loop_error;
                        }
			double tmp1,tmp2,tmp3;

			tmp3 = sqlite3_column_double(pStmt, i++);
			tmp2 = sqlite3_column_double(pStmt, i++);
			tmp1 = sqlite3_column_double(pStmt, i);
			size=snprintf(NULL,0,"%.3fx%.2fx%.2f",tmp3,tmp2,tmp1);
			if(size<0){
				fprintf(stderr,"hplc_run_table_row:Loop error\n");
				goto loop_error;
			}
			row[k]=malloc(sizeof(char)*++size);
			snprintf(row[k],size,"%.3fx%.2fx%.2f",tmp3,tmp2,tmp1);
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

        if (!data) {
                fprintf(stderr, "Error [hplc_run_meta_check]: Input metadata struct is NULL.\n");
                return false;
        }

        // Format double temperature to string as stored in DB
        char temp_str[64];
	if(isnan(data->temperature))
		strcpy(temp_str,"amb");
	else
	        snprintf(temp_str, sizeof(temp_str), "%.2f", data->temperature);

        const char *sql =
                "SELECT grid FROM hplc_run WHERE "
                "slvnt = ?1 AND "
                "ph = ?2 AND "
                "type = ?3 AND "
                "slvnt_percent = ?4 AND "
                "flow_rate = ?5 AND "
                "njctn_volume = ?6 AND "
                "njctn_type = ?7 AND "
                "sample_conc = ?8 AND "
                "clmn_phase = ?9 AND "
                "brand = ?10 AND "
                "clmn_length = ?11 AND "
                "clmn_id = ?12 AND "
                "clmn_bead_size = ?13 AND "
                "(temperature = ?14 OR CAST(temperature AS REAL) = CAST(?14 AS REAL)) AND "
                "wave = ?15 "
                "LIMIT 1;";

        ret = sqlite3_prepare_v2(pDb, sql, -1, &pStmt, NULL);
        if (ret != SQLITE_OK) {
                fprintf(stderr, "Error [hplc_run_meta_check]: Failed to prepare statement: %s\n", sqlite3_errmsg(pDb));
                return false;
        }

        // Bind all 15 metadata fields
        if (data->slvnt) {
                sqlite3_bind_text(pStmt, 1, data->slvnt, -1, SQLITE_STATIC);
        } else {
                sqlite3_bind_null(pStmt, 1);
        }

        sqlite3_bind_double(pStmt, 2, data->pH);
        sqlite3_bind_int(pStmt, 3, (int)data->type);
        sqlite3_bind_double(pStmt, 4, data->slvnt_percent);
        sqlite3_bind_double(pStmt, 5, data->flow_rate);

        if (data->njctn_volume) {
                sqlite3_bind_text(pStmt, 6, data->njctn_volume, -1, SQLITE_STATIC);
        } else {
                sqlite3_bind_null(pStmt, 6);
        }

        if (data->njctn_type) {
                sqlite3_bind_text(pStmt, 7, data->njctn_type, -1, SQLITE_STATIC);
        } else {
                sqlite3_bind_null(pStmt, 7);
        }

        sqlite3_bind_double(pStmt, 8, data->sample_conc);

        if (data->clmn_phase) {
                sqlite3_bind_text(pStmt, 9, data->clmn_phase, -1, SQLITE_STATIC);
        } else {
                sqlite3_bind_null(pStmt, 9);
        }

        sqlite3_bind_int(pStmt, 10, (int)data->brand);
        sqlite3_bind_double(pStmt, 11, data->clmn_length);
        sqlite3_bind_double(pStmt, 12, data->clmn_id);
        sqlite3_bind_double(pStmt, 13, data->clmn_bead_size);
        sqlite3_bind_text(pStmt, 14, temp_str, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(pStmt, 15, data->wavelength);

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
