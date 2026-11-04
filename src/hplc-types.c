#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "hplc-types.h"
hplc_sint hplc_scale_int(int num)
{
	unsigned int ret;
	int scale=1;

	ret=(unsigned int)num;
	for(int i=0;i<HPLC_SCALE;i++)
		scale*=10;
	return ret*scale;
}
hplc_sint hplc_strtosi(const char*nptr,char**endptr)
{	
	/*int sign = 1;
	if (*nptr == '-') { sign = -1; nptr++; }
	else if (*nptr == '+') { nptr++; }*/

	hplc_sint integer_part = 0;
	hplc_sint frac_part = 0;
	int frac_digits = 0;
	bool in_fraction = false;

	while (*nptr) {
		if (isdigit((unsigned char)*nptr)) {
			int digit = *nptr - '0';
			if (!in_fraction) {
				integer_part = integer_part * 10 + digit;
			} else if (frac_digits < HPLC_SCALE) {
				frac_part = frac_part * 10 + digit;
				frac_digits++;
			}
			// Optional: read one extra digit past scale_places to perform rounding
		} else if (*nptr == '.' && !in_fraction) {
			in_fraction = true;
		} else {
			*endptr=nptr;
			return 0; // Invalid character
		}
		nptr++;
	}

	// Pad remaining scale places with zeros if fewer fraction digits were provided
	while (frac_digits < HPLC_SCALE) {
		frac_part *= 10;
		frac_digits++;
	}

	// Compute multiplier (10^scale_places)
	int64_t multiplier = 1;
	for (int i = 0; i < HPLC_SCALE; i++) multiplier *= 10;

	if(endptr!=NULL)
		*endptr=NULL;
	return /*sign */ (integer_part * multiplier + frac_part);
}/*
char*hplc_sint_str(hplc_sint data)
{
	char buf[20];
	int size;

	size=snprintf(buf,sizeof(buf),"%d",data);
	for(int i=size-1;i>-1;i--){
		buf[5+i]=buf[i];
		buf[i]=0;
	}
	size-=5;
	if(size<1)
		buf[1]='.';
	else
		buf[size]='.';
	return strdup(buf);
}*/
char* hplc_sint_str(hplc_sint data)
{
    char buf[32];

    // Split into integer part and 5-digit fractional part
    unsigned int int_part = data / 100000;
    unsigned int frac_part = data % 100000;

    snprintf(buf, sizeof(buf), "%u.%05u", int_part, frac_part);

    return strdup(buf);
}
