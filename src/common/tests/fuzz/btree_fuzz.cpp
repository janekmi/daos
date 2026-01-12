#include <iostream>
#include <fstream>
#include "generated/btree_in.h"
#include <kaitai/kaitaistream.h>

int
main()
{
	// Open binary file
	std::ifstream ifs("in.bin", std::ifstream::binary);
	if (!ifs) {
		std::cerr << "Cannot open file\n";
		return 1;
	}

	// Wrap file in Kaitai stream
	kaitai::kstream                 ks(&ifs);

	// Parse using generated class
	btree_in_t                      btree_in(&ks);
	btree_in_t::btree_parameters_t *params = btree_in.params();

	// Access fields
	std::cout << "tree_order = " << (int)params->tree_order() << "\n";
	std::cout << "seed = " << (int)params->seed() << "\n";
	std::cout << "key_num = " << (int)params->key_num() << "\n";
	std::cout << "value_num = " << (int)params->value_num() << "\n";

	return 0;
}
