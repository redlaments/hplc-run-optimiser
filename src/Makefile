CFLAGS=-pipe -g -O0
LDFLAGS=-lm $$(pkg-config --libs --cflags gtk4) -lsqlite3
OBJ=csv.o file.o metadata.o logic.o hplc.o cli.o util.o
GOBJ=query.o run-db.o
INCLUDE=-I $$(pwd)
all:$(OBJ) $(GOBJ) t1.o
	cc $(OBJ) $(GOBJ) $(CFLAGS) $(LDFLAGS) t1.o
cli:$(OBJ) main.o
	cc $(OBJ) main.o $(CFLAGS) -o hplc 
tool:csv.o
	cc -c $(INCLUDE) $(CFLAGS) tool.c
	cc csv.o tool.o -o tool
test-gen:$(OBJ)
	cc -c $(INCLUDE) $(CFLAGS) test-gen.c
	cc $(CFLAGS) $(LDFLAGS) $(OBJ) test-gen.o -o test
test-metadata:metadata.o util.o
	cc -c $(CFLAGS) test-metadata.c
	cc $(CFLAGS) $(LDFLAGS) metadata.o util.o test-metadata.o
test-csv:csv.o
	cc -c $(INCLUDE) $(CFLAGS) test-csv.c
	cc $(CFLAGS) csv.o test-csv.o 
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
