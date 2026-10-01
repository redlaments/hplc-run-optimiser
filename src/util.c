#include "util.h"

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

