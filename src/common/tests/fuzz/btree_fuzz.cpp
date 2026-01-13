/**
 * (C) Copyright 2026 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#define D_LOGFAC DD_FAC(tests)

#include <iostream>
#include <fstream>
#include <getopt.h>
#include <kaitai/kaitaistream.h>

#include "generated/btree_in.h"

#include <daos/debug.h>

static struct option btr_ops[] = {
    {"batch", required_argument, NULL, 'b'},
    {NULL, 0, NULL, 0},
};

#define BTR_SHORTOPTS "+b:"

/**
 * XXX copy from misc.c
 */
char *
daos_str_trimwhite(char *str)
{
	char *end = str + strlen(str);

	while (isspace(*str))
		str++;

	if (str == end)
		return NULL;

	while (isspace(end[-1]))
		end--;

	*end = 0;
	return str;
}

int
main(int argc, char **argv)
{
	int   opt;
	char *file_name = NULL;

	while ((opt = getopt_long(argc, argv, BTR_SHORTOPTS, btr_ops, NULL)) != -1) {
		if (opt == 'b') {
			file_name = optarg;
		} else if (opt == '?') {
			break;
		}
	}
	if (opt == '?') {
		/* invalid option - error message printed on stderr already */
		return -1;
	} else if (argc != optind) {
		D_ERROR("Cannot interpret parameter: \"%s\" at optind: %d.\n", argv[optind],
			optind);
	}

	// Open binary file
	std::ifstream ifs(file_name, std::ifstream::binary);
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
