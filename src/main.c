#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "lib/file.h"
#include "lib/csv.h"
#include "lib/metadata.h"
#include "logic.h"

int main(int argc,char*argv[])
{
	FILE*f;
	struct hplc_data hdata;
	struct benchmarks bench={
	7,1.4,15,4000};

	if(argc<2){
		printf("Requires PK argument\n");
		return 0;
	}
	f=file_open(argv[1],&hdata.data);
	if(!f){
		fprintf(stderr,"%s\n",strerror(errno));
		return 0;
	}
	if(!csv_parse_file(f,&hdata)){
		fprintf(stderr,"csv parsing failed\n");
		hplc_metadata_destroy(&hdata.data);
		fclose(f);
		return 0;
	}
	//csv_print(&hdata.csv);
	logic(&hdata,&bench);
	hplc_destroy(&hdata);
	fclose(f);
}

