#ifndef RUN_DB_H
#define RUN_DB_H

#include <stdbool.h>
#include <sqlite3.h>

/*adds run metadata to db*/
bool hplc_run_add(struct hplc_run_metadata*data,char*pk);
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
 * returns run pk if there is*/
bool hplc_run_meta_check(struct hplc_run_metadata*data,char**pk);
/*checks if column is present in db*/
bool hplc_run_col_check(char*col);
#endif
