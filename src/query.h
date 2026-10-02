#ifndef HPLC_QUERY_H
#define HPLC_QUERY_H

#include <stdbool.h>
#include <sqlite3.h>

extern sqlite3*pDb;

bool hplc_query_init();
void hplc_query_destroy();
bool hplc_query_part_name(char*part,int*sizes,char***data);
/*currently only returns additional info to compliment brief and name from
 * hplc-query_part_name*/
bool hplc_query_part_name_verbose(char*part,char***data,int*count);
/*returns whether call was successful
 * on successful call data contains part names
 * count is the number of names in part names
 * data must be freed by application when call is successful*/
bool hplc_query_part_names(char***data,int*count);
/*returns whether call was successful
 * on successful call, data contains modifiable parts
 * count contiains the number of parts
 * data and underlying pointers must be freed by application on successful
 * call*/
bool hplc_query_parts_modify(char***data,int*count);
/*returns whether the query was successful
 * data contains the modifications possible for part when successful
 * data and underlying pointers must be freed by application when call is
 * successful
 * count is the number of items in data*/
bool hplc_query_part_modify(char*part,char***data,int*count);
/* returns the id of the part or -1 on error*/
long hplc_query_part_id(char*part);
#endif
