CFLAGS = -I../../../include -L/opt/daos/lib64/
LDFLAGS = -lgurt -ldaos_common_pmem -lcmocka

btree: ../btree.c
	$(CC) $(CFLAGS) ../utest_common.c ../btree.c -ggdb -O0 -o $@ -Wall $(LDFLAGS)
