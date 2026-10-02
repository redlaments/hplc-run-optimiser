/*ensure all functions can handle excel csvs*/
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <float.h>
#include <stddef.h>
#include <time.h>
#include <ctype.h>
#include "hplc.h"
#include "macro.h"
#include "csv.h"

#define SKIP_ROW(I,D,S)\
{\
	while(I<S&&D[I]!='\n')\
		(I)++;\
	(I)++;\
}
#define SKIP_TAIL(I,D,S)\
{\
	while(I<S&&D[I]!='[')\
		(I)++;\
}

void(*deconst[3])(struct hplc_csv*csv);
int d_size=0;//for deconst
char*hplc_file_data;

/*handles return when data[i]='['*/
struct cmd{
	char*name;
	int(*handle)(char*args[],char*data,int*i,int size,struct hplc_csv*csv,void*varg);
	void(*destroy)(struct hplc_csv*csv);
};
struct parse_array{
	bool found;
	char**name;
	int name_size;
};
struct parse_array array[]={
{false,(char*[]){"Peak Table"},1},
{false,(char*[]){"LC Chromatogram","PDA Multi Chromatogram"},2},
{false,(char*[]){"LC Status Trace","Pump A Pressure","Pump B Pressure"},3}
};
int tokenize_str(char*str,int*i,int size,char*argv[],int argv_size)
{
	int argc=0;
	
	for(;;){
		if(argc==argv_size){
			fprintf(stderr,"tokenize_str:argv_size reached: %d\n",argv_size);
			while(*i<size&&str[*i]!='\n')
				(*i)++;
			(*i)++;
			break;
		}
		argv[argc++]=str+(*i);
		while(*i<size&&str[*i]!=','&&str[*i]!='\n'&&str[*i]!='\0'&&str[*i]!='\r')
			(*i)++;
		if(*i>=size)
			break;
		if(str[*i]==',')
			str[(*i)++]='\0';
		else if(str[*i]!='\n'){
			str[(*i)]='\0';
			while(*i<size&&str[*i]!='\n')
				(*i)++;
			(*i)++;
			break;
		}
		else{
			str[*i]='\0';
			(*i)++;
			break;
		}
	}
	return argc;
}
int table_rows(char*data,int i,int size)
{
	int rows=0;

	for(i;i<size;i++){
		if(data[i]=='\n')
			rows++;
		if(data[i]=='['){
			rows--;
			break;
		}
	}
	return rows;
}
void csv_print_file_data(void)
{
	printf("data:%s\n",hplc_file_data);
}
void csv_print_peaks(struct hplc_csv*csv)
{
	for(int i=0;i<csv->peak_count;i++)
		printf("peak[%d].name:%s\n",i,csv->peaks[i].name);
}
void csv_print_pumps(struct hplc_csv*csv)
{
	struct hplc_csv_pump*pump;

	for(int i=0;i<csv->pump_count;i++){
		pump=&csv->pumps[i];
		printf("pump[%d][%d]:%f,%f\n",i,pump->points,
		pump->rtime[pump->points-1],pump->intensity[pump->points-1]);
	}
}
void csv_print_chromatogram(struct hplc_csv*csv)
{
	struct hplc_csv_chromatogram*chr;

	chr=&csv->chromatogram;
	printf("chromatogram[%d]:%f,%d\n",chr->points,chr->rtime[chr->points-1],
	chr->intensity[chr->points-1]);
}
void csv_print(struct hplc_csv*csv)
{
	csv_print_file_data();
	csv_print_peaks(csv);
	csv_print_pumps(csv);
	csv_print_chromatogram(csv);
}
void csv_destroy_file_data(void)
{
	free(hplc_file_data);
}
void csv_destroy_peaks(struct hplc_csv*csv)
{
	free(csv->peaks);
}
void csv_destroy_pumps(struct hplc_csv*csv)
{
	struct hplc_csv_pump*pumps;

	pumps=csv->pumps;
	free(pumps->rtime);
	free(pumps->intensity);
	free(pumps);
}
void csv_destroy_chromatogram(struct hplc_csv*csv)
{
	struct hplc_csv_chromatogram*chromatogram;

	chromatogram=&csv->chromatogram;
	free(chromatogram->rtime);
	free(chromatogram->intensity);
}
void csv_destroy_placeholder(struct hplc_csv*csv)
{
}
void csv_destroy(struct hplc_csv*csv)
{
	csv_destroy_peaks(csv);
	csv_destroy_chromatogram(csv);
	csv_destroy_pumps(csv);
	csv_destroy_file_data();
}
int handle_null(char*args[],char*data,int*i,int size,struct hplc_csv*csv,void*varg)
{
	for(*i;*i<size&&data[*i]!='[';(*i)++);
	return 0;
}
int handle_chromatogram(char*args[],char*data,int*i,int size,struct hplc_csv*csv,void*varg)
{
	char*argv[2];
	int argc;
	int*var[]={
	&csv->chromatogram.interval,
	&csv->chromatogram.points,
	};
	double*var1[]={	
	&csv->chromatogram.start,
	&csv->chromatogram.end
	};
	char*errstr[]={
	"chromatogram interval has %d values\n",
	"chromatogram points has %d values\n",
	"chromatogram start time has %d values\n",
	"chromatogram ent time has %d values\n"
	};

	for(int j=0;j<SIZEOF(var);j++){
		argc=tokenize_str(data,i,size,argv,2);
		if(argc<2){
			*var[j]=0;
			fprintf(stderr,errstr[j],argc);
			if(argc==1)
				fprintf(stderr,"arg[0]:%s\n",argv[0]);
		}
		else
			*var[j]=strtol(argv[1],NULL,10);
	}
	for(int j=0;j<SIZEOF(var1);j++){
		argc=tokenize_str(data,i,size,argv,2);
		if(argc<2){
			*var1[j]=0;
			fprintf(stderr,errstr[j+2],argc);
		}
		else
			*var1[j]=strtod(argv[1],NULL);
	}
	argc=tokenize_str(data,i,size,argv,2);
	if(argc<2){
		csv->chromatogram.units=NULL;
		fprintf(stderr,"chromatogram unit has %d values!\n",argc);
	}
	else
		csv->chromatogram.units=argv[1];
	argc=tokenize_str(data,i,size,argv,2);
	if(argc<2){
		csv->chromatogram.multiplier=0;
		fprintf(stderr,"chromatogram multipler has %d values!\n",argc);
	}
	else
		csv->chromatogram.multiplier=strtod(argv[1],NULL);
	argc=tokenize_str(data,i,size,argv,2);
	if(argc<2){
		csv->chromatogram.wavelength=0;
		fprintf(stderr,"chromatogram wavelength has %d values!\n",argc);
		if(argc==1)
			fprintf(stderr,"argv[0]:%s\n",argv[0]);
	}
	else
		csv->chromatogram.wavelength=strtol(argv[1],NULL,10);
	while(*i<size&&data[*i]!='\n')
		(*i)++;
	(*i)++;
	
	if(csv->chromatogram.points==0)
		return -1;
	csv->chromatogram.rtime=malloc(sizeof(double)*csv->chromatogram.points);
	csv->chromatogram.intensity=malloc(sizeof(int)*csv->chromatogram.points);
	for(int j=0;j<csv->chromatogram.points;j++){
		argc=tokenize_str(data,i,size,argv,2);
		if(argc<2){
			fprintf(stderr,"%d values in row %d of chromatogram "
			"values\n",argc,j);
			csv->chromatogram.rtime[j]=0;
			csv->chromatogram.intensity[j]=0;
		}
		else{
			csv->chromatogram.rtime[j]=strtod(argv[0],NULL);
			csv->chromatogram.intensity[j]=strtol(argv[1],NULL,10);
		}
		if(*i>size){
			fprintf(stderr,"Expected %d points, EOF reached by %d"
			" points!\n", csv->chromatogram.points,j);
			free(csv->chromatogram.rtime);
			free(csv->chromatogram.intensity);
			return -1;
		}
	}
	SKIP_TAIL(*i,data,size);
	return 0;
}
	
int handle_peaks(char*args[],char*data,int*i,int size,struct hplc_csv*csv,void*varg)
{
	int argc,index;
	char*argv[21];
	struct hplc_csv_peak empty=
	{0,0,0,0,0,0,0,NULL,NULL,NULL,0,0,0,0,0,0,0,0,0,0};
	
	argc=tokenize_str(data,i,size,argv,2);
	if(argc!=2){
		fprintf(stderr,"handle_peaks:expected 2 args got %d\n",argc);
		return -1;
	}
	csv->peak_count=(int)strtol(argv[1],NULL,10);
#ifdef DEBUG
	printf("Peak Count:%d\n",csv->peak_count);
#endif
	csv->peaks=malloc(sizeof(struct hplc_csv_peak)*(csv->peak_count));
	if(!csv->peaks){
		fprintf(stderr,"handle_peaks:malloc failed\n");
		return -1;
	}
	SKIP_ROW(*i,data,size);
	for(int j=0;j<csv->peak_count;j++){
		argc=tokenize_str(data,i,size,argv,21);
		if(argc!=21){
			fprintf(stderr,"peak %d had %d values!\n",j,argc);
			memcpy(&csv->peaks[j],&empty,sizeof(struct hplc_csv_peak));
			for(int k=0;k<argc;k++)
				fprintf(stderr,"argv[%d]:%s\n",k,argv[k]);
			continue;
		}
		if((index=(int)strtol(argv[0],NULL,10)!=j+1))
			fprintf(stderr,"peak %d has index %d\n",j+1,index);
		csv->peaks[j].rtime=strtod(argv[1],NULL);
		csv->peaks[j].itime=strtod(argv[2],NULL);
		csv->peaks[j].ftime=strtod(argv[3],NULL);
		csv->peaks[j].area=strtol(argv[4],NULL,10);
		csv->peaks[j].height=strtol(argv[5],NULL,10);
		csv->peaks[j].ah=strtod(argv[6],NULL);
		csv->peaks[j].conc=strtod(argv[7],NULL);
		csv->peaks[j].mark=argv[8];
		csv->peaks[j].id=argv[9];
		csv->peaks[j].name=argv[10];
		csv->peaks[j].k=strtod(argv[11],NULL);
		csv->peaks[j].plate=strtol(argv[12],NULL,10);
		csv->peaks[j].plate_height=strtod(argv[13],NULL);
		csv->peaks[j].tailing=strtod(argv[14],NULL);
		csv->peaks[j].resolution=strtod(argv[15],NULL);
		csv->peaks[j].sep_factor=strtod(argv[16],NULL);
		csv->peaks[j].area_ratio=strtod(argv[17],NULL);
		csv->peaks[j].height_ratio=strtod(argv[18],NULL);
		csv->peaks[j].concp=strtod(argv[19],NULL);
		csv->peaks[j].norm_conc=strtod(argv[20],NULL);
	}
	while(*i<size&&data[*i]!='[')
		(*i)++;
	return 0;
}

int handle_original(char*args[],char*data,int*i,int size,struct hplc_csv*csv,void*varg)
{
	int argc;
	char**var[]={
	&csv->original.data_file,
	&csv->original.method_file,
	&csv->original.batch_file,
	&csv->original.report_format_file
	};
	char*argv[2];

	for(int j=0;j<SIZEOF(var);j++){
		argc=tokenize_str(data,i,size,argv,2);
		if(argc<2)
			*var[j]=NULL;
		else
			*var[j]=argv[1];
	}
	while(*i<size&&data[*i]!='[')
		(*i)++;
	return 0;
}
int handle_header(char*args[],char*data,int*i,int size,struct hplc_csv*csv,void*varg)
{
	int argc;
	char*argv[2];
	char*var[]={
	csv->header.app_name,
	csv->header.app_version,
	csv->header.data_file_name
	};
	char*errstr[]={
	"Expected 2 values for app name, got %d\n",
	"Expected 2 values for app version, got %d\n",
	"Expected 2 values for data file name, got %d\n"
	};
	for(int j=0;j<SIZEOF(var);j++){
		argc=tokenize_str(data,i,size,argv,2);
		if(argc<2){
			fprintf(stderr,errstr[j],argc);
			for(int k=0;k<argc;k++)
				fprintf(stderr,"argv[%d]:%s\n",k,argv[k]);
			var[j]=NULL;
		}
		else
			var[j]=argv[1];
	}

	struct tm hplc_time={0,0,0,1,0,0,0,0,0,0,"UTC"};

	argc=tokenize_str(data,i,size,argv,2);
	if(argc<2){
		fprintf(stderr,"Expected 2 values for output date, have %d\n"
		,argc);
		for(int j=0;j<argc;j++)
			fprintf(stderr,"argv[%d]:%s\n",j,argv[j]);
		hplc_time.tm_mday=0;
		hplc_time.tm_mon=0;
		hplc_time.tm_year=0;
	}
	else{
		if(strptime(argv[1],"%d/%m/%Y",&hplc_time))
			fprintf(stderr,"Output date failed to parse:%s\n",argv[1]);
		else{
			hplc_time.tm_mday=0;
			hplc_time.tm_mon=0;
			hplc_time.tm_year=0;
		}
	}
	argc=tokenize_str(data,i,size,argv,2);
	if(argc<2){
		fprintf(stderr,"Expected 2 values for time, have %d\n",argc);
		hplc_time.tm_sec=0;
		hplc_time.tm_min=0;
		hplc_time.tm_hour=0;
	}
	else{
		if(strptime(argv[1],"%I:%M:%S %r",&hplc_time))
			fprintf(stderr,"Output time failed to parse:%s\n",argv[1]);
		else{
			hplc_time.tm_sec=0;
			hplc_time.tm_min=0;
			hplc_time.tm_hour=0;
		}

	}
	csv->header.time=mktime(&hplc_time);

	while(*i<size&&data[*i]!='[')
		(*i)++;
	return 0;
}
int handle_pump(char*args[],char*data,int*i,int size,struct hplc_csv*csv,void*varg)
{
	int argc;
	char*argv[2];
	struct hplc_csv_pump*pump;

	if(csv->pumps==NULL){
		csv->pump_count=1;

		csv->pumps=malloc(sizeof(struct hplc_csv_pump));
		if(!csv->pumps){
			fprintf(stderr,"handle_pump:malloc failed\n");
			return -1;
		}
		pump=csv->pumps;
	}
	else{
		if(csv->pump_count!=1){
			fprintf(stderr,
			"handle_pump:pump count is %d not reallocing\n",
			csv->pump_count);
			return -1;
		}
		csv->pumps=realloc(csv->pumps,2*sizeof(struct hplc_csv_pump));
		if(!csv->pumps){
			fprintf(stderr,"handle_pump:realloc failed\n");
			return -1;
		}
		pump=&csv->pumps[1];
		csv->pump_count++;
	}
	
	argc=tokenize_str(data,i,size,argv,2);
	if(argc<2)
		csv->pumps->itime=0;
	else
		csv->pumps->itime=(int)strtol(argv[1],NULL,10);
	
	argc=tokenize_str(data,i,size,argv,2);
	if(argc<2){
		fprintf(stderr,"handle_pump:insufficient point data\n");
		free(csv->pumps);
		return -1;
	}
	else
		pump->points=strtol(argv[1],NULL,10);

	argc=tokenize_str(data,i,size,argv,2);
	if(argc<2)
		pump->stime=0;
	else
		pump->stime=strtod(argv[1],NULL);
	
	argc=tokenize_str(data,i,size,argv,2);
	if(argc<2)
		pump->etime=0;
	else
		pump->etime=strtod(argv[1],NULL);

	argc=tokenize_str(data,i,size,argv,2);
	if(argc<2)
		pump->iunits=NULL;
	else
		pump->iunits=argv[1];
	
	argc=tokenize_str(data,i,size,argv,2);
	if(argc<2)
		pump->imult=0;
	else
		pump->imult=strtod(argv[1],NULL);
	
	while(data[*i]!='\n'&&data[*i]!='\0')
		(*i)++;
	if(data[*i]=='\0')
		return -1;
	(*i)++;
	pump->rtime=malloc(sizeof(double)*pump->points);
	pump->intensity=malloc(sizeof(double)*pump->points);
	if(!pump->rtime||!pump->intensity){
		if(pump->rtime)
			free(pump->rtime);
		else
			fprintf(stderr,"handle_pump:malloc failed for pump->rtime\n");
		if(pump->intensity)
			free(pump->intensity);
		else
			fprintf(stderr,"handle_pump:malloc failed for pump->intensity\n");
		free(pump);
		return -1;
	}

	for(int j=0;j<pump->points;j++){
		argc=tokenize_str(data,i,size,argv,2);

		if(argc<2){
			pump->rtime[j]=0;
			pump->intensity[j]=0;
		}
		else{
			pump->rtime[j]=strtod(argv[0],NULL);
			pump->intensity[j]=strtod(argv[1],NULL);
		}
	}
	while(data[*i]!='['&&data[*i]!='\0')
		(*i)++;
	return 0;
}
int handle_status_trace(char*args[],char*data,int*i,int size,struct hplc_csv*csv,void*varg)
{
	struct cmd handle[]={
	{"Pump A Pressure",handle_pump,csv_destroy_pumps},
	{"Pump A Pressure",handle_pump,csv_destroy_pumps}
	};
	void(*destroy)(struct hplc_csv*);
	static bool pump=false;

	destroy=(void(*)(struct hplc_csv*))(varg);
	for(int j=0;j<SIZEOF(handle);j++){
		if(!strcmp(args[1],handle[j].name)){
			if(handle[j].handle(args,data,i,size,csv,NULL)==-1)
				return -1;
			 else if(!pump){
                	        if(d_size==3){
                        	        destroy(csv);
                                	fprintf(stderr,"deconst is full\n");
	                                return -1;
        	                }
                	        deconst[d_size++]=destroy;
				pump=true;
#ifdef DEBUG
                        	printf("Added (Unknown) deconst\n");
#endif
				return 2;
			}
			else
				return 2;
                }
        }
	handle_null(args,data,i,size,csv,NULL);
	return 2;
}
int handle_pda_chromatogram(char*args[],char*data,int*i,int size,struct hplc_csv*csv,void*varg)
{
	int argc,max_arg=2;
	char*argv[max_arg];
	struct hplc_csv_chromatogram*chr;

	chr=&csv->chromatogram;
	argc=tokenize_str(data,i,size,argv,max_arg);

	if(argc!=2){
		fprintf(stderr,"%d args for points, expected %d\n",argc,max_arg);
		return -1;
	}
	else{
		chr->points=(int)strtol(argv[1],NULL,10);
	}

	argc=tokenize_str(data,i,size,argv,max_arg);

	if(argc!=2){
		fprintf(stderr,"%d args for interval, expected %d\n",argc,
			max_arg);
		chr->interval=0;
	}
	else
		chr->interval=(int)strtol(argv[1],NULL,10);
	
	argc=tokenize_str(data,i,size,argv,max_arg);
	if(argc!=2){
		fprintf(stderr,"%d args for start time,expected %d\n",argc,
			max_arg);
		chr->start=0;
	}
	else
		chr->start=strtod(argv[1],NULL);
	
	argc=tokenize_str(data,i,size,argv,max_arg);
	if(argc!=2){
		fprintf(stderr,"%d args for end time, expected %d\n",argc,
			max_arg);
		chr->end=0;
	}
	else
		chr->end=strtod(argv[1],NULL);
	
	argc=tokenize_str(data,i,size,argv,max_arg);
	if (argc != 2) {
	    fprintf(stderr, "%d args for intensity units, expected %d\n", argc, max_arg);
	    chr->units = NULL;
	} else {
	    chr->units = argv[1];
	}

	argc=tokenize_str(data,i,size,argv,max_arg);
	if (argc != 2) {
	    fprintf(stderr, "%d args for intensity multiplier, expected %d\n", argc, max_arg);
	    chr->multiplier = 1.0;
	} else {
	    chr->multiplier = strtod(argv[1], NULL);
	}
	SKIP_ROW(*i,data,size);
	argc=tokenize_str(data,i,size,argv,max_arg);
	if (argc != 2) {
	    fprintf(stderr, "%d args for wavelength, expected %d\n", argc, max_arg);
	    chr->wavelength = 0;
	} else {
	    chr->wavelength = (int)strtol(argv[1], NULL, 10);
	}
	SKIP_ROW(*i,data,size);
	SKIP_ROW(*i,data,size);
	chr->rtime=malloc(sizeof(double)*chr->points);
	chr->intensity=malloc(sizeof(int)*chr->points);
	if(!chr->rtime||!chr->intensity){
		fprintf(stderr,"rtime or intensity failed to malloc\n");
		return -1;
	}
	for(int j=0;j<chr->points;j++){
		argc=tokenize_str(data,i,size,argv,2);
                if(argc<2){
                        fprintf(stderr,"%d values in row %d of chromatogram "
                        "values\n",argc,j);
                        chr->rtime[j]=0;
                        chr->intensity[j]=0;
                }
                else{
                        chr->rtime[j]=strtod(argv[0],NULL);
                        chr->intensity[j]=strtol(argv[1],NULL,10);
                }
                if(*i>size){
                        fprintf(stderr,"Expected %d points, EOF reached by %d"
                        " points!\n", chr->points,j);
                        free(chr->rtime);
                        free(chr->intensity);
                        return -1;
                }
	}
	SKIP_TAIL(*i,data,size);
	return 0;
}
int read_file(FILE*f,char**data)
{
	long size,read;

	if(fseek(f,0,SEEK_END)==-1){
		fprintf(stderr,"readfile:%s\n",strerror(errno));
		return -1;
	}
	size=ftell(f);
	rewind(f);
	*data=malloc(sizeof(char)*++size);
	if(!*data){
		fprintf(stderr,"read_file:malloc failed\n");
		return -1;
	}
	read=fread(*data,sizeof(char),size,f);
	if(read!=size-1){
		fprintf(stderr,"Expected %ld items got %ld\n",size,read);
		return -1;
	}
	(*data)[size-1]='\0';
	return size;
}
/* argv size is always 2*/
int csv_name(char*data,int*i,int size,char*argv[])
{
	int argc=0;
	bool close=false;

	argv[argc++]=data+((*i)+1);
	for(*i;*i<size;(*i)++){
		if(data[*i]==']'||data[*i]=='('||data[*i]==')'){
			if(data[*i]=='('){
				if(argc==2){
					fprintf(stderr,
					"csv_name:argc already reached 2\n");
					break;
				}
				data[*i]='\0';
				close=true;
				(*i)++;
				argv[argc++]=data+(*i);
			}
			else if(data[*i]==')'){
				if(close){
					data[*i]='\0';
					close=false;
				}
			}
			else{
				data[*i]='\0';
				break;
			}
		}
	}
	return argc;
}
static inline
int parse_handle(char*args[],char*data,int*i,int size,struct hplc_csv*hplc,struct cmd*name)
{
	int ret;

	if((ret=name->handle(args,data,i,size,hplc,name->destroy))==-1)
		return -1;
	else if(ret!=2){
		if(name->destroy!=csv_destroy_placeholder){
			if(d_size==3){
				name->destroy(hplc);
				fprintf(stderr,"deconst is full\n");
				return -1;
			}
			deconst[d_size++]=name->destroy;
#ifdef DEBUG
			printf("Added %s deconst\n",name->name);
#endif
		}
	}
	return 0;
}
int array_check(char*name[],int*crit_count)
{
	int k=0;

	for(k;k<SIZEOF(array);k++){
		if(array[k].found)
			continue;
		for(int l=0;l<array[k].name_size;l++){
			if(k==2&&!strcmp(name[0],array[k].name[0])){
				for(int m=0;m<array[k].name_size-1;m++)
					if(!strcmp(name[1],array[k].name[1+m])){
						goto found;
					}
						
			}
			if(!strcmp(name[0],array[k].name[l])){
				goto found;
			}
		}
	}
	return -1;
	found:
		if(!array[k].found){
			array[k].found=true;
			(*crit_count)++;
	#ifdef DEBUG
			printf("Found %s in array\n",name);
	#endif
			return 0;
		}
		else{
			fprintf(stderr,
			"array_check:%s has already been found!",
			array[k].name[0]);
			return -1;
		}
}
int _csv_parse(char*data,int*i,int size,struct hplc_data*hdata)
{
	struct cmd names[]={
	{"Header",handle_header,csv_destroy_placeholder},
	{"File Information",handle_null,csv_destroy_placeholder},
	{"Sample Information",handle_null,csv_destroy_placeholder},
	{"Original Files",handle_original,csv_destroy_placeholder},
	{"File Description",handle_null,csv_destroy_placeholder},
	{"Configuration",handle_null,csv_destroy_placeholder},
	{"Peak Table",handle_peaks,csv_destroy_peaks},
	{"Compound Results",handle_null,csv_destroy_placeholder},
	{"Group Results",handle_null,csv_destroy_placeholder},
	{"Slice Data",handle_null,csv_destroy_placeholder},
	{"LC Chromatogram",handle_chromatogram,
		csv_destroy_chromatogram},
	{"LC Status Trace",handle_status_trace,csv_destroy_placeholder},
	{"PDA Multi Chromatogram",handle_pda_chromatogram,
		csv_destroy_placeholder},
	{"Fraction Collection Report",handle_null,csv_destroy_placeholder}
	};
	char*name[2];
	struct hplc_csv*hplc=&(hdata->csv);
	int j=0,crit_count=0,argc;

	d_size=0;
	for (int k = 0; k < SIZEOF(array); k++) {
        	array[k].found = false;
	}

	hdata->csv.pumps=NULL;
	hdata->csv.pump_count=0;

	for(;;){
		argc=csv_name(data,i,size,name);
		if(name[0]==NULL)
			break;
		for(*i;*i<size&&!isalnum((unsigned char)data[*i]);(*i)++);
		(*i)++;
#ifdef DEBUG
		printf("<name>%s</name>\n",name[0]);
#endif
		j=0;
		for(j;j<SIZEOF(names);j++){
			if(!strcmp(names[j].name,name[0])){
				if(parse_handle(name,data,i,size,hplc,
				&names[j])==-1)
					goto error;
				array_check(name,&crit_count);
				if(crit_count==SIZEOF(array))
					return 0;
			}
		}
#ifdef DEBUG
		if(*i<size&&data[*i]!='['){
			fprintf(stderr,"%s handle misbehaved!\n",name[0]);
			while(*i<size&&data[*i]!='[')
				(*i)++;
		}
#endif
	}
	return 0;
	error:
		for(int k=0;k<d_size;k++)
			deconst[k](hplc);
		return -1;
}
void csv_parse_names(char*file_name)
{
	char*name[2];
	int i=0,size,argc;
	FILE*f;

	f=fopen(file_name,"r");
	if(!f){
		fprintf(stderr,"%s failed to open\n",file_name);
		return;
	}
	size=read_file(f,&hplc_file_data);
        for(;;){
                argc=csv_name(hplc_file_data,&i,size,name);
                if(name==NULL)
                        return;
                for(i;i<size&&hplc_file_data[i]!='[';(i)++);
		printf("%s\n",name[0],name[1]);
        }
	csv_destroy_file_data();
}
bool csv_parse(char*data,int size,struct hplc_data*hdata)
{
	for(int i=0;i<size;i++){
                if(data[i]=='['){
                        if(_csv_parse(data,&i,size,hdata)==-1)
				return false;
                        break;
                }
        }
	return true;
}
bool csv_parse_file(FILE*f,struct hplc_data*hdata)
{
	int size;

	size=read_file(f,&hplc_file_data);
	if(size<0)
		return false;
	if(!csv_parse(hplc_file_data,size,hdata)){
		csv_destroy_file_data();
		return false;
	}
	return true;
}
bool csv_parse_name(char*file,struct hplc_data*hdata)
{
	int size;
	FILE*f;

	f=fopen(file,"r");
	size=read_file(f,&hplc_file_data);
	if(size<0)
		return false;
	if(!csv_parse(hplc_file_data,size,hdata)){
		csv_destroy_file_data();
		fclose(f);
		return false;
	}
	fclose(f);
	return true;
}
/*gemini code 26/9/26*/

/**
 * Resolves the total runtime of the HPLC run in minutes.
 *
 * @param data Pointer to the target hplc_data struct.
 * @return Total runtime in minutes (end - start), or -1.0 on invalid input.
 */
double resolve_runtime(const struct hplc_csv *csv)
{
    if (csv == NULL) {
        return -1.0;
    }
    
    // Calculates total run duration using the start and end timestamps from chromatogram metadata
    return csv->chromatogram.end - csv->chromatogram.start;
}

/**
 * Resolves the minimum peak resolution present across all identified chromatogram peaks.
 *
 * @param data Pointer to the target hplc_data struct.
 * @return Smallest peak resolution value found, or -1.0 if no peaks exist or pointer is NULL.
 */
double minimum_res(const struct hplc_csv*csv)
{
    if (csv == NULL || csv->peaks == NULL || csv->peak_count <= 0) {
        return -1.0;
    }

    double min_resolution = DBL_MAX;

    for (int i = 0; i < csv->peak_count; i++) {
        if (csv->peaks[i].resolution < min_resolution) { //
            min_resolution = csv->peaks[i].resolution; //
        }
    }

    return min_resolution;
}

/**
 * Determines the maximum backpressure recorded across all pump trace channels during the run.
 *
 * @param data Pointer to the target hplc_data struct[cite: 1].
 * @return Highest pressure value encountered, or -1.0 if no pump data exists or pointer is NULL.
 */
double max_backpressure(const struct hplc_csv*csv)
{
    if (csv== NULL || csv->pumps == NULL || csv->pump_count <= 0) {
        return -1.0;
    }

    double max_pressure = -DBL_MAX;
    bool data_found = false;

    for (int p = 0; p < csv->pump_count; p++) {
        const struct hplc_csv_pump *pump = &csv->pumps[p]; //[cite: 1]
        
        if (pump->intensity == NULL || pump->points <= 0) {
            continue;
        }

        // Apply intensity multiplier if non-zero[cite: 1]
        double mult = (pump->imult != 0.0) ? pump->imult : 1.0; //[cite: 1]

        for (int i = 0; i < pump->points; i++) {
            double current_pressure = pump->intensity[i] * mult; //[cite: 1]
            if (current_pressure > max_pressure) {
                max_pressure = current_pressure;
            }
            data_found = true;
        }
    }

    return data_found ? max_pressure : -1.0;
}
