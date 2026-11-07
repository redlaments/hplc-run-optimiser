CFLAGS=-pipe -g -O0
LDFLAGS=-lm $$(pkg-config --libs --cflags gtk4) -lsqlite3
OBJ=csv.o file.o metadata.o logic.o hplc.o util.o hplc-types.o
GOBJ=query.o run-db.o
INCLUDE=-I $$(pwd)
all:$(OBJ) $(GOBJ) t1.o
	cc t1.o $(OBJ) $(GOBJ) $(CFLAGS) $(LDFLAGS)
cli:$(OBJ) main.o
	cc $(OBJ) main.o $(CFLAGS) -o hplc 
tool:csv.o
	cc -c $(INCLUDE) $(CFLAGS) tool.c
	cc csv.o tool.o -o tool
test-metadata:metadata.o util.o hplc-types.o query.o cli.o
	cc -c $(CFLAGS) $(INCLUDE) test-metadata.c
	cc $(CFLAGS) $(LDFLAGS) metadata.o util.o test-metadata.o hplc-types.o query.o cli.o
test-csv:csv.o
	cc -c $(INCLUDE) $(CFLAGS) test-csv.c
	cc $(CFLAGS) csv.o test-csv.o 
test-logic:logic.o hplc-types.o metadata.o cli.o query.o run-db.o util.o
	cc -c $(INCLUDE) $(CFLAGS) test-logic.c
	cc $(CFLAGS) $(LDFLAGS) logic.o hplc-types.o metadata.o cli.o query.o test-logic.o \
	util.o
%.o:%.c
	cc -c $(INCLUDE) $(CFLAGS) $< -o $@
t1.o:t1.c
	cc -c $(CFLAGS) $(INCLUDE) $$(pkg-config --cflags gtk4) t1.c
csv.o:lib/csv.c
	cc -c $(INCLUDE) $(CFLAGS) lib/csv.c
file.o:lib/file.c
	cc -c $(INCLUDE) $(CFLAGS) lib/file.c
metadata.o:lib/metadata.c
	cc -c $(INCLUDE) $(CFLAGS) lib/metadata.c

clean:
	rm *.o
	rm a.out
