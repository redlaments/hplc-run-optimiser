#ifndef HPLC_CONFIG_H
#define HPLC_CONFIG_H

#define NAME_MAX_ARGS 30
#define MAX_TEMP 60 /*Maximum column temperature in degrees celsius*/
#define TEMP1 40 /*temp to move to when amb does not work*/
#define IB_AMOUNT 3/*%b to increase by*/

#define CLI
#define HPLC_DB_PATH "hplc.db"

/*Solvents are listed in order of change*/
static char*solvents[]={
"ACN",
"MeOH",
"MIX"
};

struct hplc_type_name{
        int type;
        char*name;
};

/*value must corrospond to index in brands*/
enum hplc_clmn_brand{
PINNACLE=0
};

static struct hplc_type_name brands[]={
{PINNACLE,"P"}
};

/*value must corrospond to index in run*/
enum hplc_run_type{
ISO=0,
GRA=1
};

static struct hplc_type_name run[]={
{ISO,"ISO"},
{GRA,"GRA"}
};
#endif
