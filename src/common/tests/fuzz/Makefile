CXX = g++
# debug + hide all warnings (there are too many of them)
CXXFLAGS = -ggdb -O0 -w

# paths
CXXFLAGS += -I../../../include -L/opt/daos/lib64
CXXFLAGS += -Wl,-rpath,/opt/daos/lib64

# libs
CXXFLAGS += -lkaitai_struct_cpp_stl_runtime -lgurt -luuid

# required for debug.c to compile with g++
CXXFLAGS += -Wno-narrowing -Wno-literal-suffix -Wno-pointer-arith -Wno-write-strings

btree_fuzz: btree_fuzz.cpp misc_mock.c
	$(CXX) $(CXXFLAGS) btree_fuzz.cpp misc_mock.c generated/btree_in.cpp ../../debug.c ../../fail_loc.c -o $@

clean:
	rm -f btree_fuzz
