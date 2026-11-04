#ifndef HPLC_TYPES_H
#define HPLC_TYPES_H
/*scaled by 10^5*/
typedef unsigned int hplc_sint;
#define HPLC_SCALE 5
#define AMB 4000000000

hplc_sint hplc_scale_int(int num);
hplc_sint hplc_strtosi(const char*nptr,char**endptr);
/*returned string must be freed by calling application*/
char*hplc_sint_str(hplc_sint data);

#endif
