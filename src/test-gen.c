#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "lib/file.h"
#include "lib/csv.h"
#include "logic.h"

int main(int argc,char*argv[])
{
	FILE*f;
	struct hplc_data hdata;
	struct benchmarks bench={
	7,1.4,15,4000};
	char*file_name="GR11-11-test.csv";

	f=fopen(file_name,"r");
	if(!f){
		fprintf(stderr,"%s\n",strerror(errno));
		return 0;
	}
	if(!csv_parse_file(f,&hdata)){
		fprintf(stderr,"csv parsing failed\n");
		fclose(f);
		return 0;
	}
	logic(&hdata,&bench);
	csv_destroy(&hdata.csv);
	fclose(f);
}

