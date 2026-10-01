#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lib/csv.h"
#include "macro.h"

/*testing tokenize_str*/
int main(int argc,char*argv[])
{
	char*str="Uracil Arg\nGMP,Transporter",*tmp;
	char*results[]={"Uracil Arg","GMP","Transporter"};
	char*args[3];
	int i=0,arg;

	tmp=strdup(str);
	arg=tokenize_str(tmp,&i,strlen(str),args,1);
	if(arg!=1){
		printf("Argc:%d, expected 1\n",arg);
		free(tmp);
		return 0;
	}
	arg=tokenize_str(tmp,&i,strlen(str),args+1,2);
	if(arg!=2){
		printf("Argc:%d, expected 2\n",arg);
		for(int j=0;j<argc;j++)
			printf("argv[%d]:%s\n",j,args[j]);
		free(tmp);
		return 0;
	}
	
	for(int j=0;j<SIZEOF(results);j++)
		if(strcmp(results[j],args[j]))
			printf("result[%d] was unexpected: expected:%s, "
				"got:%s\n",j,results[j],args[j]);
	free(tmp);
}
