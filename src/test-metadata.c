#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lib/metadata.h"
#include "macro.h"

int main(int argc,char*argv[])
{
	char*tests[]={
	"GR19",
	"GR19-10-1-ACN-3-ISO-70-1.5-20u-CP-C18P-150x4.6x5-amb-254.CSV",
	"GR19-22-3-MIX-3-ISO-60-0.56-20u-CP-0.1-C18P-150x4.6x5-40-254.CSV"
	};
	char*tmp;
	struct hplc_run_metadata data;

	for(int i=0;i<SIZEOF(tests);i++){
		tmp=strdup(tests[i]);
		if(!hplc_metadata(tmp,&data)){
			printf("Failed to acquire metadata\n");
		}
		else
			hplc_metadata_print(&data);
		printf("%s\n",tests[i]);
		free(tmp);
	}
}
