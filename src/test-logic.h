#ifndef TEST_LOGIC_H
#define TEST_LOGIC_H
struct test_run{
        char*name;
        hplc_sint rs;
        hplc_sint rt;
        hplc_sint bp;
	int peaks;
};
extern struct test_run*runs;
extern int count;
#endif
