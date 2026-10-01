#include <stdio.h>
#include <stdarg.h>

void print_data(const char*restrict format,...)
{
	va_list ap;

	va_start(ap,format);

	vprintf(format,ap);

	va_end(ap);
}
void print_error(char*data,...)
{
	va_list ap;

	va_start(ap,data);

	vfprintf(stderr,data,ap);

	va_end(ap);
}
