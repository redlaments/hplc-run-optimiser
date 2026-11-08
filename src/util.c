#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "util.h"
#include "hplc-types.h"
#include "cli.h"

int tokenize_str1(char*str,char*argv[],int argv_size,char delimiter)
{
	int argc=0;
	char*tmp=str;

	while(*tmp==delimiter)
		tmp++;
	if(*tmp=='\0')
		return argc;
	for(;;){
		argv[argc++]=tmp;
		if(argc==argv_size)
			break;
		while(*tmp!=delimiter&&*tmp!='\0')
			tmp++;
		if(*tmp==delimiter){
			*tmp='\0';
			tmp++;
		}
		else
			break;
	}
	return argc;
}
char*action_str(enum selectivity_action aaction,void*adata)
{
	char*action,*tmp,*buf;

	action=(char*)resolve_action_name(aaction);
	if(aaction==MULTI||aaction==SMULT){
		print_error("Multi choice selected\n");
		return strdup(action);
	}
	else if(aaction==SUCCESS||aaction==ERROR||aaction==SPP||aaction==KGS
	||aaction==KGS1||aaction==EWC)
		return strdup(action);
	else{
		int size;

		tmp=hplc_sint_str(*(hplc_sint*)adata);
		size=snprintf(NULL,0,"%s:%s",action,tmp);
		buf=malloc(sizeof(char)*++size);
		snprintf(buf,size,"%s:%s",action,tmp);
		return buf;
	}
}
char*action_arg_str(struct action_arg arg)
{
	return action_str(arg.action,arg.arg);
}
