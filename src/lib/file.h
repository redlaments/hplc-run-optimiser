#ifndef HPLC_FILE_H
#define HPLC_FILE_H

#include <stdio.h>
#include "hplc.h"

FILE*file_open(const char*pk,struct hplc_run_metadata*data);

#endif
