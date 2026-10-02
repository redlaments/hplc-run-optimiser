#include <sqlite3.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "../config.h"
#include "query.h"

sqlite3*pDb;

bool hplc_query_init()
{
	int ret;

	ret=sqlite3_open_v2(HPLC_DB_PATH,&pDb,SQLITE_OPEN_READWRITE,NULL);
	
	if(ret!=SQLITE_OK){
		fprintf(stderr,"query_init:%s\n",sqlite3_errstr(ret));
		return false;
	}
	return true;
}
void hplc_query_destroy()
{
	int ret;

	ret=sqlite3_close(pDb);
	if(ret!=SQLITE_OK){
		fprintf(stderr,"query_destroy:%s\n",sqlite3_errstr(ret));
	}
}
long hplc_query_part_id(char*part)
{
	sqlite3_stmt*pStmt;
	char*sql="select id from hplc_part hp "
	"where name=?";
	int ret;
	long id;

	ret=sqlite3_prepare_v3(pDb,sql,strlen(sql)+1,0,&pStmt,NULL);
	if(ret!=SQLITE_OK){
		fprintf(stderr,"hplc_query_part_id:prepare:%s\n",sqlite3_errstr(ret));
		return -1;
	}
	ret=sqlite3_bind_text(pStmt,1,part,strlen(part),SQLITE_STATIC);
	if(ret!=SQLITE_OK)
		goto error;
	ret=sqlite3_step(pStmt);
	if(ret!=SQLITE_ROW)
		goto error;
	id=sqlite3_column_int64(pStmt,0);
	sqlite3_finalize(pStmt);
	return id;
	error:
		sqlite3_finalize(pStmt);
		fprintf(stderr,"hplc_query_part_id:%s\n",sqlite3_errstr(ret));
}
bool hplc_query_part_modify(char*part,char***data,int*count)
{
	sqlite3_stmt*pStmt;
	char*csql="select count() from hplc_var hv "
	"join hplc_part_var hpv on hpv.var_id=hv.id "
	"where hpv.part_id=?";
	int ret;
	long id;

	id=hplc_query_part_id(part);
	if(id==-1)
		return false;
	ret=sqlite3_prepare_v3(pDb,csql,strlen(csql)+1,0,&pStmt,NULL);
	if(ret!=SQLITE_OK){
		fprintf(stderr,"hplc_query_part_modify:cprepare%s\n",sqlite3_errstr(ret));
		return false;
	}
	ret=sqlite3_bind_int64(pStmt,1,id);
	if(ret!=SQLITE_OK)
		goto error;
	ret=sqlite3_step(pStmt);
	if(ret!=SQLITE_ROW)
		goto error;
	*count=sqlite3_column_int(pStmt,0);
	sqlite3_finalize(pStmt);

	char*sql="select hv.desc from hplc_var hv "
	"join hplc_part_var hpv on hpv.var_id=hv.id "
	"where hpv.part_id=?";

	ret=sqlite3_prepare_v3(pDb,sql,strlen(sql)+1,0,&pStmt,NULL);
	if(ret!=SQLITE_OK){
		fprintf(stderr,"hplc_query_part_modify:prepare:%s\n",sqlite3_errstr(ret));
		return false;
	}
	ret=sqlite3_bind_int64(pStmt,1,id);
	if(ret!=SQLITE_OK)
		goto error;
	*data=malloc(sizeof(char**)*(*count));
	if(!*data){
		fprintf(stderr,"hplc_query_part_modify:malloc failed\n");
		sqlite3_finalize(pStmt);
		return false;
	}
	for(int i=0;i<*count;i++){
		ret=sqlite3_step(pStmt);
		if(ret==SQLITE_DONE){
			for(int j=i;j>-1;j--)
				free((*data)[j]);
			fprintf(stderr,"hplc_query_part_modify:SQLITE_DONE "
			"occured at %d, count:%d",i,*count);
			return false;
		}
		(*data)[i]=strdup(sqlite3_column_text(pStmt,0));
	}
	sqlite3_finalize(pStmt);
	return true;
	error:
		sqlite3_finalize(pStmt);
		fprintf(stderr,"hplc_query_part_modify:%s\n",sqlite3_errstr(ret));
		return false;
}

bool hplc_query_parts_modify(char***data,int*count)
{
	sqlite3_stmt*pStmt;
	char*csql="select count() from hplc_part hp where hp.variable=1";
	int ret;

	ret=sqlite3_prepare_v3(pDb,csql,strlen(csql)+1,0,&pStmt,NULL);
	if(ret!=SQLITE_OK){
		fprintf(stderr,"hplc_query_parts_modify:c prepare:%s\n",sqlite3_errstr(ret));
		return false;
	}
	ret=sqlite3_step(pStmt);
	if(ret!=SQLITE_ROW)
		goto error;
	*count=sqlite3_column_int64(pStmt,0);
	sqlite3_finalize(pStmt);

	char*sql="select hp.name from hplc_part hp where hp.variable=1";
	int i=0;

	ret=sqlite3_prepare_v3(pDb,sql,strlen(sql)+1,0,&pStmt,NULL);
	if(ret!=SQLITE_OK){
		fprintf(stderr,"hplc_query_parts_modify:prepare:%s\n",sqlite3_errstr(ret));
		return false;
	}
	
	*data=malloc(sizeof(char*)*(*count));
	if(!*data){
		fprintf(stderr,"hplc_query_parts_modify:malloc failed\n");
		return false;
	}
	for(i;i<*count;i++){
		int len;

		ret=sqlite3_step(pStmt);
		if(ret==SQLITE_DONE){
			fprintf(stderr,"hplc_query_parts_modify:SQLITE_DONE occured at %d but size is %d\n",i,*count);
			goto error_loop;
		}
		len=sqlite3_column_bytes(pStmt,0);
		if(len==0){
			fprintf(stderr,"hplc_query_parts_modify:len was 0 at iteration %d\n",i);
			goto error_loop;
		}
		(*data)[i]=strdup(sqlite3_column_text(pStmt,0));
	}
	sqlite3_finalize(pStmt);
	return true;
	error_loop:
		for(i;i>-1;i--)
			free((*data)[i]);
		free(*data);
		return false;
	error:
		sqlite3_finalize(pStmt);
		fprintf(stderr,"hplc_query_parts_modify:%s\n",sqlite3_errstr(ret));
		return false;
}

	
bool hplc_query_part_names(char***data,int*count)
{
	sqlite3_stmt*pStmt;
	char*csql="select count() from hplc_part hp";
	int ret;

	ret=sqlite3_prepare_v3(pDb,csql,strlen(csql)+1,0,&pStmt,NULL);
	if(ret!=SQLITE_OK){
		fprintf(stderr,"hplc_query_part_names:c prepare:%s\n",sqlite3_errstr(ret));
		return false;
	}
	ret=sqlite3_step(pStmt);
	if(ret!=SQLITE_ROW)
		goto error;

	*count=sqlite3_column_int64(pStmt,0);
	sqlite3_finalize(pStmt);

	char*zsql="select hp.name from hplc_part hp";
	int i=0;

	ret=sqlite3_prepare_v3(pDb,zsql,strlen(zsql)+1,0,&pStmt,NULL);
	if(ret!=SQLITE_OK){
		fprintf(stderr,"hplc_query_part_names:prepare:%s\n",sqlite3_errstr(ret));
		return false;
	}
	
	*data=malloc(sizeof(char*)*(*count));
	while((ret=sqlite3_step(pStmt))!=SQLITE_DONE){
		int size;
		
		if(i>=*count){
			fprintf(stderr,"hplc_query_part_names:i is larger than count\n");
			goto error_loop;
		}
		size=sqlite3_column_bytes(pStmt,0);
		if(size==0){
			fprintf(stderr,"hplc_query_part_names:null size on %d node\n",i);
			goto error_loop;
		}
		(*data)[i]=malloc(sizeof(char)*size);
		(*data)[i++]=strdup(sqlite3_column_text(pStmt,0));
	}
	sqlite3_finalize(pStmt);
	return true;

	error:
		fprintf(stderr,"hplc_query_part_names:%s\n",sqlite3_errstr(ret));
		sqlite3_finalize(pStmt);
		return false;
	error_loop:
		for(int j=i;j>-1;j--)
			free(data[j]);
		free(data);
		sqlite3_finalize(pStmt);
		return false;
}

bool hplc_query_part_name(char*part,int*sizes,char***data)
{
	sqlite3_stmt*pStmt;
	char*zsql="select hp.name,hp.brief from hplc_part hp where name=?";
	int ret;
	
	ret=sqlite3_prepare_v3(pDb,zsql,strlen(zsql)+1,0,&pStmt,NULL);
	if(ret!=SQLITE_OK){
		fprintf(stderr,"hplc_query_part:%s\n",sqlite3_errstr(ret));
		return false;
	}
	
	ret=sqlite3_bind_text(pStmt,1,part,strlen(part),SQLITE_STATIC);
	if(ret!=SQLITE_OK)
		goto error;
	ret=sqlite3_step(pStmt);
	if(ret!=SQLITE_ROW){
		printf("!SQLITE_ROW\n");
		goto error;
	}
	
	*sizes=2;
	*data=malloc(sizeof(char**)*(*sizes));
	if(!*data){
		fprintf(stderr,"hplc_query_part:malloc failed\n");
		sqlite3_finalize(pStmt);
		return false;
	}
	(*data)[0]=strdup(sqlite3_column_text(pStmt,0));
	(*data)[1]=strdup(sqlite3_column_text(pStmt,1));
	sqlite3_finalize(pStmt);
	return true;

	error:
		fprintf(stderr,"hplc_query_part:%s\n",sqlite3_errstr(ret));
		sqlite3_finalize(pStmt);
		return false;
}
bool hplc_query_part_name_verbose(char*part,char***data,int*count)
{
	sqlite3_stmt*pStmt;
	int ret;
	char*csql="select count() from hplc_desc hd "
	"where id=?";
	long id;

	id=hplc_query_part_id(part);
	if(id==-1)
		return false;
	
	ret=sqlite3_prepare_v3(pDb,csql,strlen(csql)+1,0,&pStmt,NULL);
	if(ret!=SQLITE_OK){
		fprintf(stderr,"hplc_query_part_name_verbose:cprepare:%s\n",
			sqlite3_errstr(ret));
		return false;
	}
	ret=sqlite3_bind_int64(pStmt,1,id);
	if(ret!=SQLITE_OK)
		goto error;
	ret=sqlite3_step(pStmt);
	if(ret!=SQLITE_ROW)
		goto error;
	*count=sqlite3_column_int(pStmt,0);
	sqlite3_finalize(pStmt);

	char*sql="select desc from hplc_desc hd where id=?";

	ret=sqlite3_prepare_v3(pDb,sql,strlen(sql)+1,0,&pStmt,NULL);
	if(ret!=SQLITE_OK){
		fprintf(stderr,"hplc_query_part_name_verbose:prepare:%s\n",
			sqlite3_errstr(ret));
		return false;
	}
	ret=sqlite3_bind_int64(pStmt,1,id);
	if(ret!=SQLITE_OK)
		goto error;
	*data=malloc(sizeof(char**)*(*count));
	if(!*data){
		fprintf(stderr,"hplc_query_part_name_verbose:malloc failed\n");
		sqlite3_finalize(pStmt);
		return false;
	}
	for(int i=0;i<*count;i++){
		ret=sqlite3_step(pStmt);
		if(ret==SQLITE_DONE){
			fprintf(stderr,"hplc_query_part_verbose:SQLITE_DONE "
				"received at iteration %d but count was %d\n",
				i,*count);
			for(int j=i;j>-1;j--)
				free((*data)[j]);
			free(*data);
			sqlite3_finalize(pStmt);
			return false;
		}
		(*data)[i]=strdup(sqlite3_column_text(pStmt,0));
	}
	sqlite3_finalize(pStmt);
	return true;
	error:
		sqlite3_finalize(pStmt);
		fprintf(stderr,"hplc_query_part_verbose:%s\n",sqlite3_errstr(ret));
		return false;
}
