CXX = g++

# paths
CXXFLAGS = -I../../../include -L/opt/daos/lib64
CXXFLAGS += -Wl,-rpath,/opt/daos/lib64

# libs
CXXFLAGS += -lkaitai_struct_cpp_stl_runtime -lgurt -luuid

# required for debug.c to compile with g++
CXXFLAGS += -Wno-narrowing -Wno-literal-suffix -Wno-pointer-arith -Wno-write-strings

btree_fuzz: btree_fuzz.cpp
	$(CXX) $(CXXFLAGS) btree_fuzz.cpp generated/btree_in.cpp ../../debug.c ../../fail_loc.c -o $@
