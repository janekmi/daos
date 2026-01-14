CXX = g++
# debug + hide all warnings (there are too many of them)
CXXFLAGS = -ggdb -O0 -w

# CXXFLAGS += -fsanitize=address

# paths
CXXFLAGS += -I../../../include -L/opt/daos/lib64
CXXFLAGS += -Wl,-rpath,/opt/daos/lib64

# libs
CXXFLAGS += -lkaitai_struct_cpp_stl_runtime -lgurt -luuid

# required for debug.c to compile with g++
CXXFLAGS += -Wno-narrowing -Wno-literal-suffix -Wno-pointer-arith -Wno-write-strings -fpermissive

btree_fuzz: btree_fuzz.cpp btree_ik.c misc_mock.c generated/btree_in.cpp ../../btree.c ../../mem.c  ../../debug.c ../../fail_loc.c
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -f btree_fuzz
