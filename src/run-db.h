#ifndef RUN_DB_H
#define RUN_DB_H

#include <stdbool.h>
#include <sqlite3.h>

/*adds run metadata to db*/
bool hplc_run_add(struct hplc_run_metadata*data,char*pk);
/*deletes all runs from db*/
void hplc_run_delete();
/*returns the column data associated with id*/
bool hplc_column_retrieve(struct hplc_column*clmn,int id);
/*returns the run metadata associated with pk*/
bool hplc_run_retrive(struct hplc_run_metadata*data,char*pk);
/*prepares a sqlite3_stmt for use with hplc_run_table_row*/
sqlite3_stmt*hplc_run_table_prepare();
/*destroys a sqlite3_stmt from hplc_run_table_prepare*/
void hplc_run_table_row_destroy(sqlite3_stmt*pStmt);
/* Returns a row affilated with a sqlite3_stmt created by hplc_run_table
 * prepare in pdata. pdata memory is malloced and freed by calling app*/
bool hplc_run_table_row(sqlite3_stmt*pStmt,char***pdata);
/*checks if there is a run associated with pk in db*/
bool hplc_run_check(char*pk);
/*checks if there is a run associated with metadata
 * returns run pk if there is, pk is malloced by function*/
bool hplc_run_meta_check(struct hplc_run_metadata*data,char**pk);
/*checks if column is present in db*/
bool hplc_run_col_check(char*col);
/*changes the brand and id of the column if needed*/
void hplc_column_brand(struct hplc_column*clmn);

#endif
