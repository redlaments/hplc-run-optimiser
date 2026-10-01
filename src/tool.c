#include <stdio.h>
#include <string.h>
#include "lib/csv.h"
int main(int argc,char*argv[])
{
	char*data;

	if(argc==1){
		printf("Requires arguments\n");
		return 0;
	}
	csv_parse_names(argv[1]);
}
