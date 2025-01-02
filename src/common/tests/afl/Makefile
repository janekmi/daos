CFLAGS = -I../../../include -L/opt/daos/lib64/
LDFLAGS = -lgurt -ldaos_common_pmem -lcmocka

btree:
	$(CC) $(CFLAGS) ../utest_common.c ../btree.c -o $@ -Wall $(LDFLAGS)
