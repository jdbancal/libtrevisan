/* This file is part of libtrevisan, a modular implementation of
   Trevisan's randomness extraction construction.

   Copyright (C) 2011-2012, Wolfgang Mauerer <wm@linux-kernel.net>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with libtrevisan. If not, see <http://www.gnu.org/licenses/>. */
/* -------------------------------------------------------------------------- */
//
// Modified by Alan Mink and Andrea Rommal for NIST application, 2013-2016
//
//  The identification of any commercial product or trade name does not imply
//  endorsement or recommendation by the National Institute of Standards
//  and Technology.
//  Any software code/algorithm is expressly provided "AS IS." The National
//  Institute of Standards and Technology makes no warranty of any kind,
//  express, implied, in fact or arising by operation of law, including,
//  without limitation, the implied warranty of merchantability,
//  fitness for a particular purpose, non-infringement and data accuracy.
//
/* -------------------------------------------------------------------------- */

// Trevisan extractor main dispatcher
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>               // memset
#include <string>
#include <cmath>
#include <sys/time.h>
#include <tclap/CmdLine.h>
#include <openssl/opensslconf.h> // we need to check the ossl configuration later
#include "ossl_locking.h"
#include "utils.hpp"
#include "debug_levels.h"
#include "timing.h"
#include "extractor.h"
#include "prng.hpp"
#include <tbb/tbb.h>
#include <tbb/task_scheduler_init.h>
#include <RInside.h>
#include "stream_ops.h"          // for streaming option

using namespace TCLAP;
using namespace std;

int debug_level = 0;
int socket_fd_data, socket_fd_seed, socket_fd_out; // for streaming option

// TODO: Use C++ memory management once the interferences between new and GMP
// are sorted out. Proper thing to do would be to only use the new operator,
// and write a failure handler (or use the provided exception)
void *do_alloc(size_t size, const string &type) {
	void *mem = new (nothrow) char[size];

	if (mem == NULL) {
		cerr << "Internal error: "
				 << "Cannot allocate space for " << type << endl;
		exit(-1);
	}

	return mem;
}

// allocate & clr full ints, rounded-up from m=the input bit length
void *alloc_and_zero(size_t m) {
	int *mem =
			(int *)do_alloc((m / BITS_PER_TYPE(int) + 1) * sizeof(int), "zero mem");

	memset(mem, 0, (m / BITS_PER_TYPE(int) + 1) * sizeof(int)); // clr full words

	return mem;
}

// fct to run a histogram on the seed usage distribution
void Si_test(uint64_t rows, weakdes *wd, params &params) {
	int *histo = (int *)alloc_and_zero(
			(32 * params.pp.d)); // histogram of seed usage over "d"
	int distro_limit = 8 * 1024;
	int *distr_histo = (int *)alloc_and_zero(
			(32 * distro_limit)); // overlap histogram distribition
	vector<uint64_t> indices;
	indices.reserve(params.pp.t_req);
	for (uint64_t i = 0; i < rows; i++) {
		wd->compute_Si(i, indices); // get indices for ith row
		for (int j = 0; j < params.pp.t_req; j++) {
//			printf("%ld ", indices[j] );
			if (indices[j] < params.pp.d) {
				histo[indices[j]]++; // build histo of seed usage
			} else {
				printf("***Error in compute_Si for %ld row and %d index, "
							 "indices[j]=%ld \n",
							 i, j, indices[j]);
			}
		}
//			printf("\n\n" );
	}
	for (int j = 0; j < params.pp.d; j++) {
		if (histo[j] < distro_limit) {
			distr_histo[histo[j]]++;
//			if ( histo[j] < 1 )
//				printf(" histo[%d]=%d \n", j, histo[j]
//			);
		} else {
			printf("***Error Histo[%d]= %d is too large\n", j, histo[j]);
		}
	}
	int sum = 0;
	printf(" seed_usage_freq		 num_of_bits\n");
	printf("(i.e., expect to see 2 or 3 entries, [0], [n] & maybe [n+1], "
				 "otherwise not uniformly accessed)\n");
	for (int j = 0; j < distro_limit; j++) {
		if (distr_histo[j] > 0) {
			printf("distr_histo[%d] = %d\n", j, distr_histo[j]);
			sum += distr_histo[j];
		}
	}
	printf("Total seed usage = %d for %ld rows\n", sum, rows);
}

// fct to compute block sizes from Mauerer's Block Design
void gen_blk_design(params &params) {
	int a_pt = 0;
	double ni; // [params.pp.a+1];
	double sum_ni;
	double pwr_fact;
	vector<uint32_t> *ptr_base_mi =
			static_cast<vector<uint32_t> *>(params.pp.base_mi_ptr);
	vector<uint32_t> *ptr_mi = static_cast<vector<uint32_t> *>(params.pp.mi_ptr);
	uint32_t t_threshold; // --

	cout << "ptr_base_mi->size()=" << ptr_base_mi->size() << endl;
	pwr_fact = (1.0 - 1.0 / params.pp.r); // init pwr factor
	ni = ((1.0 * params.pp.m_req) / params.pp.r - 1.0); // use total output size "pp.m_req"
	sum_ni = ni;
	ptr_mi->at(0) = ceil(ni);
	ptr_base_mi->at(0) = 0;
	if (ptr_mi->at(0) < params.pp.t_pri)
		t_threshold = ptr_mi->at(0);
	else
		t_threshold = params.pp.t_pri;
	cout << "t_threshold=" << t_threshold << endl;
	for (int i = 1; i < params.pp.a; i++) {
		ni = pwr_fact * ni;
		sum_ni += ni;
		ptr_base_mi->at(i) = ptr_mi->at(i - 1) + ptr_base_mi->at(i - 1);
		ptr_mi->at(i) = ceil(sum_ni) - ptr_base_mi->at(i);
		cout << i << ". " << ptr_mi->at(i) << "	" << ptr_base_mi->at(i) << endl;
//		if ( ptr_mi->at(i)<=params.pp.t_pri && a_pt==0 )
		if (ptr_mi->at(i) <= t_threshold && a_pt == 0)
			a_pt = i; // save index to table where Blk size 1st drops to t_prime or less
	}
	ptr_base_mi->at(params.pp.a) =
			ptr_mi->at(params.pp.a - 1) + ptr_base_mi->at(params.pp.a - 1);
	ptr_mi->at(params.pp.a) =
			params.pp.m_req -
			ptr_base_mi->at(params.pp.a); // use total output size "pp.m_req"
	int sum_a = params.pp.m_req - ptr_base_mi->at(a_pt);
	int new_a = sum_a / t_threshold + a_pt;
	if (new_a > sum_a) { // not able to reduce number of block, don't change allocations
		return;
	}
	cout << params.pp.a << ". " << ptr_mi->at(params.pp.a) << "	"
			 << ptr_base_mi->at(params.pp.a) << endl;
	cout << "a_pt=" << a_pt << ", sum_a=" << sum_a << ", new_a=" << new_a << endl;
	for (int i = a_pt; i < new_a; i++) { // set last few Blk sizes to t_prime in an attempt to reduce # of Blks & relate seed
		ptr_base_mi->at(i) = ptr_mi->at(i - 1) + ptr_base_mi->at(i - 1);
		ptr_mi->at(i) = t_threshold;
		sum_a -= t_threshold;
	}
	if (sum_a > 0) { // process last Blk
		ptr_base_mi->at(new_a) = ptr_mi->at(new_a - 1) + ptr_base_mi->at(new_a - 1);
		ptr_mi->at(new_a) = sum_a; // set last Blk size entry to remain bits
	} else {
		new_a--; // no remaining bits, reduce Blk cnt by 1
	}
	params.pp.a = new_a;
}

// common fct to read ASCII "1" & "0" data from files & ignore any other chars
// replace code with a better fct
uint64_t rd_ascii_bit_file(ifstream &file_handle, uint64_t bits_needed,
													vector<uint64_t> *data_vector) {
	char c;
	uint64_t cnt = 0;
	uint64_t num = 0;
	while (file_handle >> c && cnt < bits_needed) {
		if (c == '1') {
			int wrd = cnt / 64;
			int loc = cnt % 64;
			data_vector->at(wrd) |= (1LLU << (63 - loc));
			cnt++;
		} else if (c == '0') {
			cnt++;
		} else {
			num++;
		}
	}
	if (num > 0)
		cout << "*** " << cnt << " 1's and 0's read along with " << num
				 << " num of other chars" << endl;
	return (cnt);
}

// build mask for upper part of partial word
uint64_t part_mask(uint64_t size) {
	int loc = size % 64;
	uint64_t mask = 0LLU; // init to zero, so if 64-bit aligned then mask all 1's
	if (loc != 0) {
		mask = (1LLU << (64 - loc));
		mask -= 1; // mask for lower bits
	}
	mask = ~mask; // invert bits to get mask to clr out unwanted lower bits, if any
	return (mask);
}

// This function contains only generic calls to class bitext
// and weakdes. This makes the trevisan extractor algorithm
// independent of the specific 1-bit-extractor and weak design
// algorithms
inline void
trevisan_extract(uint64_t i, vector<uint64_t> &indices,
								bitfield<unsigned int, uint64_t> &init_rand_bf,
								unsigned int *y_S_i, unsigned int *y_S_ib, params &params,
								bitext *bext, weakdes *wd, // added a 2nd sub-seed vector
								uint64_t t, int *extracted_rand,
								uint64_t num_rand_bits) { // replaced "ofstream & out_data"
	if (debug_level >= PROGRESS) {
		if (i % 10000 == 0) {
			cout << "Computing bit " << i << endl;
		}
	}

	// Compute the index set S_{i}
	if (i % 10000 == 0 && debug_level >= INFO)
		cout << "Computing weak design for i=" << i << " (t=" << t << ")" << endl;

	wd->compute_Si(i, indices);

	vector<uint32_t> *ptr_base_mi = static_cast<vector<uint32_t> *>(params.pp.base_mi_ptr);
	vector<uint32_t> *ptr_mi      = static_cast<vector<uint32_t> *>(params.pp.mi_ptr);
	int cur_seed_base = 0; //	Mink 9-10-2014, offset to seed sets
	for (int a = 0; a <= params.pp.a; a++) { // repeat for each Blk use same seed loc but on a diff set of seed
		if (i >= ptr_mi->at(a)) {
			break; // if rows exceed "i" we have already completed this Blk
		}

		// Select the bits from y indexed by S_{i}, that
		// is, compute y_{S_{i}}. y_S_i thus contains t bits	// now contains "l=t/2" bits
		// of randomness.
		memset(y_S_i, 0, params.pp.t_req); // Mink 9-10-2014 corrected to "t_req" size
		for (uint64_t count = 0; count < params.pp.l; count++) { // chg for 1st half from:	count < t; count++) {
			// Set the bit indexed count in y_S_i to the bit
			// indexed by indices[count] in init_rand.
#ifdef EXPENSIVE_SANITY_CHECKS
			if (indices[count] > num_rand_bits) {
				cerr << "Internal error: Bit index exceeds amount of available "
								"randomness (selected:"
						 << indices[count] << ", available: " << num_rand_bits
						 << ", index: " << count << ")!" << endl;
				exit(-1);
			}
#endif
			if (init_rand_bf.get_bit(indices[count] + cur_seed_base)) {
				y_S_i[count / BITS_PER_TYPE(unsigned int)] |=
						((unsigned int)1 << (count % BITS_PER_TYPE(unsigned int)));
			}
		}
		// fill in the 2nd vector with the 2nd half of the sub_seed
		memset(y_S_ib, 0, params.pp.t_req); // init Byte cnt to "0"
		for (uint64_t count = 0; count < params.pp.l; count++) {
			if (init_rand_bf.get_bit(indices[count + params.pp.l] + cur_seed_base)) {
				y_S_ib[count / (8 * sizeof(unsigned int))] |=
						((unsigned int)1 << (count % (8 * sizeof(unsigned int))));
			}
		}

		// Feed the subset of the initial randomness to the
		// 1-bit extractor, and insert the resulting bit into
		// position i of the extracted randomness (most significant bit, MSB, is first).
		if (!params.skip_bitext) {
			if (bext->extract(y_S_i, y_S_ib)) { // added a 2nd vector & replaced "out_data"
				#pragma omp atomic update
				extracted_rand[(i + ptr_base_mi->at(a)) / BITS_PER_TYPE(int)] |= // added "base_mi" offset to output
						(1 << (31 - ((i + ptr_base_mi->at(a)) % (8 * sizeof(int)))));	// reverse order of bit insertion to MSB 1st
			}
		}
		cur_seed_base += params.pp.t_req;
	}
}

void trevisan_dispatcher(class weakdes *wd, class bitext *bext, params &params) {
	// To determine the amount of required randomness, first determine
	// how many bits the single bit extractor requires per extracted bit.
	// Then, pass this value to the weak design, which infers
	// the total amount d of requires random input bits.
	// NOTE: For the 1-bit-extractor run, the number of required
	// random bits is given by num_rand_bits() -- it
	// can be slightly larger than the theoretically required
	// amount because of alignment or efficiency reasons.
	uint64_t t = bext->num_random_bits();
	params.pp.t_req = t; // set values in params struct
	params.pp.l = t / 2;
	cout << "1-bit extractor requires " << t << " bits per "
			 << "extracted bit" << endl;
	wd->set_params(t, params.pp.m);
	params.pp.t_pri = wd->get_t(); // set value in params

	uint64_t num_rand_bits = wd->compute_d();
	params.pp.d = num_rand_bits; // Mink 9-10-2014, set value in params
	params.pp.d_req = params.pp.t_pri * params.pp.t_pri; // set value in params
	params.pp.d_tot = params.pp.d_req; // init to single Blk value
	long double r = wd->get_r();
	if (params.verbose) {
		cout.precision(3);
		cout << "Total amount of initial randomness (d=t^2): " << num_rand_bits
				 << " (" << fixed << (((float)num_rand_bits) / (1024 * 1024))
				 << " MiBit)" << endl;
		cout << "Ratio of extracted bits to initial randomness: "
				 << ((long double)params.pp.m) / num_rand_bits << endl;
		// added printout of mu, as referenced in Mauerer et al paper
		cout << "Ratio of extracted bits to input bits, mu: ";
		cout << static_cast<long double>(params.pp.m) /
								(params.pp.alpha * params.pp.n)
				 << fixed << endl;
		cout << "-------------------------------------------------------------"
				 << endl
				 << endl;
	}

	if (params.dryrun)
		return;

	// deleted section about saving the weak design, currently unused.

	vector<uint32_t> mi_array; // def Vectors for len & base_len for Blk_Design
	vector<uint32_t> base_mi_array;
	params.pp.mi_ptr = &mi_array; // store ptr to "mi_array" in "param.pp"
	params.pp.base_mi_ptr = &base_mi_array; // store ptr to "base_mi_array" in "param.pp"
	if (params.blk_des) { // prepare for Blk_design
												// use "m_req" & init Blk cnt
		params.pp.a = ceil((log(1.0 * params.pp.m_req - params.pp.r) -
												log(1.0 * params.pp.t_pri - params.pp.r)) /
											 (log(params.pp.r) - log(params.pp.r - 1.0)));
		mi_array.resize(params.pp.a + 1); // resize Blk_Design Vectors for max len
		base_mi_array.resize(params.pp.a + 1);
		gen_blk_design(params); // compute Blk size & fill-in Vectors
		params.pp.m_req = mi_array[params.pp.a] + base_mi_array[params.pp.a];
		params.pp.d_tot = (params.pp.a + 1) * params.pp.d_req;
		cout << " M[i]	BaseMi[i] " << endl;
		for (int i = 0; i <= params.pp.a; i++) {
			cout << i << ". " << mi_array[i] << "	" << base_mi_array[i] << endl;
		}
	} else {
		mi_array.resize(params.pp.a + 1); // resize Blk_Design Vectors for single len
		base_mi_array.resize(params.pp.a + 1);
		mi_array.at(0) = params.pp.m_req; // set to amt requested "m_req"
		base_mi_array.at(0) = 0;
	}

	printf("*****t=%ld\n", t);
	printf("params.pp.l=%ld\n", params.pp.l);
	printf("params.pp.d=%ld\n", params.pp.d);
	printf("params.pp.d_req=%ld\n", params.pp.d_req);
	printf("params.pp.d_tot=%ld\n", params.pp.d_tot);
	printf("params.pp.a=%ld\n", params.pp.a);
	printf("params.pp.t_req=%ld\n", params.pp.t_req);
	printf("params.pp.t_pri=%ld\n", params.pp.t_pri);
	printf("params.pp.eps=%g\n", params.pp.eps); // %g use %e or %f which ever is shortest
	printf("params.pp.r=%f\n", params.pp.r);
	printf("params.pp.max_entropy=%ld\n", params.pp.max_entropy);
	printf("params.pp.new_a=%ld\n", params.pp.a);
	printf("params.pp.m=%ld\n", params.pp.m);
	printf("params.pp.m_req=%ld\n", params.pp.m_req);
	printf("params.pp.n=%ld\n", params.pp.n);
	fflush(stdout);

	// Allocate space for the extracted randomness
	int *extracted_rand = (int *)alloc_and_zero(params.pp.m_req); // pass size in bits, but allocates full ints

	// Create the initial randomness y (and make the result accessible
	// as void pointer)
	vector<uint64_t> initial_rand_vector((params.pp.d_tot / 64 + 1), 0); // added init size & value to alloc

	// TODO: Using uint64_t instead of the previously used, but incorrect
	// unsigned int as index data type induces a 20% performance penalty.
	// We should thus adaptively use shorter data types for all bit index related
	// operations when smaller amounts of data are processed.
	bitfield<unsigned int, uint64_t> init_rand_bf;

	if (params.streaming) { // for streaming option, get seed data
		initial_rand_vector.resize(params.pp.d_tot / (8 * sizeof(uint64_t)) + 1);
		uint64_t *v_data = initial_rand_vector.data();
		stream_get_seed_bits(params.pp.d_tot, socket_fd_seed, v_data); // get streaming weak data
		initial_rand_vector.at(params.pp.d_tot / 64) &=
				part_mask(params.pp.d_tot); // clr out unneeded lower bits when len is not aligned at 64
		init_rand_bf.set_raw_data(initial_rand_vector.data(), params.pp.d_tot); // chg for Blk_Des
		cout << "QRNG Streaming Seed Done" << endl;
	} else if (params.pp.q == "NA") { // if no user-given source bits file, get
																		// bits with create_randomness(...)
		prng initrandom;
		initrandom.create_randomness(params.pp.d_tot, &initial_rand_vector);	// chg for Blk_Des
		initial_rand_vector.at(params.pp.d_tot / 64) &= 
				part_mask(params.pp.d_tot); // clr out unneeded lower bits when len is not aligned at 64
		init_rand_bf.set_raw_data(initial_rand_vector.data(), params.pp.d_tot); // chg for Blk_Des
		cout << "QRNG Pseudo-Random Seed Ready" << endl;
	} else {
		cout << "Loading Seed File..." << flush;
		ifstream data_file(params.pp.q); // replace code with new rd fct & more checking
		if (data_file.is_open()) {
			uint64_t len = rd_ascii_bit_file(data_file, params.pp.d_tot, &initial_rand_vector);
			if (len < params.pp.d_tot) {
				cout << "Insufficient input data in Seed file, only " << len
						 << " instead of specified " << params.pp.d_tot << endl;
				exit(-1);
			}
			data_file.close();
		} else {
			cout << "****Can't open specified Seed file " << params.pp.q
					 << ", Terminating Program" << endl;
			exit(-1);
		}
		init_rand_bf.set_raw_data(initial_rand_vector.data(), params.pp.d_tot); // chg for Blk_Des
		cout << "done" << endl << flush;
	}
	if (params.si_tst == -1) // test seed usage for all "m"	rows
		params.si_tst = params.pp.m;
	if (params.si_tst > 0)
		Si_test(params.si_tst, wd,params); // test seed usage for specified "params.si_tst" rows

	meas_t start, end, delta;
	measure(&start);

	// 3.) Compute all weak design index sets S_{i}, select the
	// appropriate bits from the initial randomness, and pass the
	// selected randomness to the 1-bit extractor. Concatenate
	// the 1-bit results.

	// TODO: Create an apply class and move all constant
	// parameters to private data elements that are initialised
	// in the constructor. Essentially, the only parameter
	// that should remain in the function call is i

	// Also, eventually restore TBB parallel_for construct...

	//	allocation within a 64-bit word:
	//	 3, 2, 1, 0 (Byte#) for word #0 (where Byte 3 is MSB)
	//	 7, 6, 5, 4 (Byte#) for word #1
	#pragma omp parallel for
	for (uint64_t i = 0; i < params.pp.m; i++) { // loop thru each (row) extracted bit, for Blk_des each Blk is proc by at the same time
		vector<uint64_t> indices;
		vector<unsigned int> y_S_i; // use only for 1st half of sub-seed
		vector<unsigned int> y_S_ib; // need a 2nd list for the 2nd half of sub_seed
		// oversizing, sub-seed is "t", each half only needs t/2
		y_S_i.reserve(t);
		y_S_ib.reserve(t);
		indices.reserve(t);

		trevisan_extract(i, indices, init_rand_bf, &y_S_i[0],
										 &y_S_ib[0], // added 2nd sub-seed list
										 params, bext, wd, t, extracted_rand, num_rand_bits);
	}

	measure(&end);

	if (params.streaming) { // for streaming option, send output bits & wait for ACK
		cout << " Streaming Output Data " << endl;
		stream_send_output_bits(params.pp.m_req, socket_fd_out, extracted_rand); // send output data
		cout << " Streaming Output Data " << endl;
	} else if (params.pp.of != "NA") { // wrt to optional user spec output data file
		ofstream out_file_data(params.pp.of);
		for (int i = 0; i < (params.pp.m_req / (8 * sizeof(int))); i++) {
			std::bitset<32> y(extracted_rand[i]); // map int value to 32 separate bits
			out_file_data << y;								// output those 32-bits as chars
			if ((i & 0x1) == 1)
				out_file_data << endl;
		}
		int rem = (params.pp.m_req % (8 * sizeof(int)));
		if (rem > 0) { // handle the last partially filled int
			int last = params.pp.m_req / (8 * sizeof(int));
			for (int i = 0; i < rem; i++) {
				out_file_data << ((extracted_rand[last] >> (31 - i)) & 0x1);
			}
		}
	}

	timestamp_subtract(&delta, &end, &start);

	cout.precision(3);
	cout << endl << "-------------------- Summary --------------------" << endl;
	cout << "Required " << dec << delta_to_ms(delta) << " ms for "
			 << params.pp.m_req << " bits extracted from " << params.pp.n
			 << " bits, using " << params.pp.d_tot << " Seed bits. " << endl;

	cout << "Performance: " << params.pp.m / (delta_to_s(delta) * 1000)
			 << " kbits/s" << endl;

	cout << "Ratio of extracted bits to initial randomness: ";
	cout << ((long double)params.pp.m) / num_rand_bits << fixed << endl;
	
	cout << "Ratio of extracted bits to input bits, mu: "; // added printout of mu, as referenced in Mauerer et al paper
	cout << static_cast<long double>(params.pp.m) / (params.pp.alpha * params.pp.n)
			 << fixed << endl;
}

void parse_cmdline(struct params &params, int argc, char **argv) {
	try {
		CmdLine cmd("Trevisan extractor framework", ' ', "Sep 2012");

		ValueArg<string> loadArg(
				"i", "input",
				"Enter your file containing bit input", // added this to allow user-input source bits
				false, "NA", "string");
		ValueArg<string> seedArg(
				"q", "seed",
				"Enter your file containing bit seed", // added this to allow user-input seed bits
				false, "NA", "string");
		ValueArg<string> outfArg(
				"o", "output_file",
				"Enter your output extracted_data file name", // added this to allow user-output data file for extracted data
				false, "NA", "string");
		SwitchArg blockArg(
				"b", "Blk_Design", "Enable Block Design",
				false); // added this to allow user to select Block Design`
		SwitchArg streamArg("", "stream", "Enable QRNG streaming",
												false); // added to allow QRNG data streaming`
		ValueArg<long> Si_tstArg(
				"s", "si_test",
				"Enable Si_tst with opt length (0 implies m)", // added this to allow user to select Si_test w/ opt len
				 false, 0, "Integer");

		ValueArg<unsigned long> nArg(
				"n", "inputsize", "Length of input data (bits)", false, 0, "Integer");
		ValueArg<unsigned long> mArg(
				"m", "outputsize", // changed "true" to "false" in mArg's parameters,
				"Length of extracted data (bits)", // this makes it NOT required to give an m to run the program
				false, 0, "Integer");
		ValueArg<string> weakdesArg(
				"w", "weakdes", // in the 3rd parameter deleted options block and aot, not used in this version
				"Weak design construction " // in the 5th parameter changed "gf2x" to "gfp" to make gfp the default
				"(gfp, gf2x)",
				false, "gfp", "string");
		ValueArg<string> bitextArg(
				"x", "bitext",
				"Bit extractor construction (lu, xor, rsh)", // in the 3rd parameter deleted options xor and lu, not used in this version
				false, "rsh", "string"); // in the 5th parameter changed "xor" to "rsh" to make rsh the default
		ValueArg<double> alphaArg("a", "alpha",
				"Source entropy factor alpha (0 < alpha < 1)",
				false, 0.9, "Real");
		ValueArg<double> epsArg("e", "eps",
				"1 bit error probability (0 < eps < 1)",
				false, 1e-7, "Real");
		ValueArg<double> lu_nuArg("", "lu:NU",
				"nu parameter for the Lu extractor",
				false, 0.45, "Integer");
		ValueArg<int> numTasksArg("", "numtasks",
				"Number of tasks (0 means unlimited)", false, -1,
				"Integer");
		SwitchArg ignoreEntropyArg("", "ignore-entropy-violation",
															 "Allow minimum entropy requirements", false);
		SwitchArg bytesArg("", "bytes",
											 "Use bytes instead of bits "
											 "to compute input/output sizes",
											 false);
		SwitchArg kiloArg("", "kilo", "Multiply size units by 1024", false);
		SwitchArg megaArg("", "mega", "Multiply size units by 1024*1024", false);
		SwitchArg verboseArg("v", "verbose", "Enable verbose output", false);
		SwitchArg dryrunArg("", "dry-run", "Only compute parameters, don't extract",
												false);

		cmd.add(loadArg); // added cmd for user-input source bits
		cmd.add(seedArg); // added cmd for user-input seed bits
		cmd.add(outfArg); // added cmd for user-output data file
		cmd.add(nArg);
		cmd.add(mArg);
		cmd.add(Si_tstArg); // added Si_test
		cmd.add(weakdesArg);
		cmd.add(alphaArg);
		cmd.add(epsArg);
		cmd.add(lu_nuArg);
		cmd.add(numTasksArg);
		cmd.add(bitextArg);
		cmd.add(ignoreEntropyArg);
		cmd.add(bytesArg);
		cmd.add(kiloArg);
		cmd.add(megaArg);
		cmd.add(verboseArg);
		cmd.add(blockArg);	// added Blk_Design
		cmd.add(streamArg); // added QRNG data streaming
		cmd.add(dryrunArg);

		SwitchArg skipBitextArg("", "skip-bitext", "Skip the bit extraction step",
														false);
		cmd.add(skipBitextArg);

#ifndef NO_DEBUG
		MultiSwitchArg debugArg("d", "debug",
														"Emit diagnostic output "
														"(use multiple times for more details)");
		cmd.add(debugArg);
#endif

		cmd.parse(argc, argv);

		params.pp.i = loadArg.getValue();
		params.pp.n = nArg.getValue(); // always init "n" to requested value
		if (params.pp.i != "NA") { // if there is a user-defined source bits file, check its size vs "n"
			ifstream n_file(params.pp.i);
			if (n_file.is_open()) {
				n_file.seekg(0, ifstream::end);
				uint64_t size = n_file.tellg();
				cout << endl << endl
						 << "The number of bits in your file was: " << size
						 << " you requested " << params.pp.n << endl;
				uint64_t size_n = (size / 64) * 64;
				if (params.pp.n > size_n) { // size_n = # of items in that file, divisible by 64.
					params.pp.n = size_n;
					cout << "Insufficient bits in file, revising n=" << params.pp.n << endl;
				} else {
					cout << "Input Data file exceeds specified input size of "
							 << params.pp.n << ", will only use " << params.pp.n
							 << " from file" << endl;
				}
				n_file.close();
			}
		}

		params.pp.q = seedArg.getValue(); // added variable to store seed file name

		params.si_tst = Si_tstArg.getValue();		// added opt Si_test
		params.blk_des = blockArg.getValue();		// added opt Block design
		params.streaming = streamArg.getValue(); // added opt for QRNG data straeming

		params.pp.of = outfArg.getValue(); // added variable to store output data file name
		if (params.pp.of != "NA") {
			cout << "optional output extracted data file name=" << params.pp.of << endl;
		}

		params.wdt = get_weakdes(weakdesArg.getValue());
		params.bxt = get_bitext(bitextArg.getValue());

		unsigned int mult_factor = 1;
		if (kiloArg.getValue() && megaArg.getValue()) {
			cout << "Please specify either kilo or mega as multiplier, "
					 << "not both!" << endl;
			exit(-1);
		}

		if (bytesArg.getValue()) {
			mult_factor *= 8;
		}
		if (kiloArg.getValue()) {
			mult_factor *= 1024;
		}
		if (megaArg.getValue()) {
			mult_factor *= 1024 * 1024;
		}

		params.pp.n *= mult_factor;
		params.pp.r = M_E; // Mink 9/10/2014, init r=e

		params.verbose = verboseArg.getValue();

		if (params.streaming)
			cout << "QRNG Streaming enabled" << endl;
		
		// Dry run does not make any sense without verbose parameter messages
		params.dryrun = dryrunArg.getValue();
		if (params.dryrun)
			params.verbose = true;

		params.ignore_entropy = ignoreEntropyArg.getValue();

		params.pp.alpha = alphaArg.getValue();
		if (params.pp.alpha > 1.0) { // assume entropy input is actual number of bits
			cout << "min-entropy, alpha=" << params.pp.alpha << endl;
			params.pp.alpha = (params.pp.alpha + 1.0) / params.pp.n;
		}
		if (params.streaming == false &&
				(params.pp.alpha <= 0 || params.pp.alpha >= 1)) {
			cerr << "Source entropy factor alpha must be in the range (0,1)" << endl;
			exit(-1);
		}

		params.pp.eps = epsArg.getValue();
		if (params.pp.eps <= 0 || params.pp.eps > 1) {
			cerr << "1 bit error eps must be in the range (0,1)" << endl;
			exit(-1);
		}

		params.pp.lu_nu = lu_nuArg.getValue();
		if (params.pp.lu_nu <= 0 || params.pp.lu_nu >= 0.5) {
			cerr << "nu parameter for the Lu extractor must satisfy "
					 << "0 < nu < 0.5" << endl;
			exit(-1);
		}

		params.num_tasks = numTasksArg.getValue();
#ifndef NO_DEBUG
		debug_level = debugArg.getValue();
#endif

		params.skip_bitext = skipBitextArg.getValue();

		params.pp.m = mArg.getValue();
		params.pp.m *= mult_factor; //	moved before "if" clause so any use defined values
																// would be augmented by the "multi_factor", but any derived 
																// values from "params.pp.n" already incorp "multi_factor"
	} catch (ArgException &e) {
		cerr << "Error encountered during command line parsing: " << e.error()
				 << " for argument " << e.argId() << endl;
		exit(-1);
	}
}

// Some weak designs need special intialisation sequences
// This can be handled by providing an appropriate template
// specialisation for the initialisation function that does nothing
// in the general case
template <class W>
void init_primitives(W *wd, class bitext *bext, struct params &params) {
//	bext->set_r(wd_overlap_trait<W>::r);
	return;
}

template <>
void init_primitives(class weakdes_gf2x *wd, class bitext *bext, struct params &params) {
	// Initialise the weak design GF(2^t)
	// There are 2^t field elements, which means we have values
	// from [0,2^t-1]. We therefore need to represent values
	// of at most t-1 bits.
//	bext->set_r(wd_overlap_trait<weakdes_gf2x>::r);
	uint64_t t = bext->num_random_bits();
	int log_t = numbits<uint64_t>(t - 1);
	wd->init_wd(log_t);
}

// When there's a sub-design, the main design can only be a block design
template <class S>
void init_primitives(class weakdes_block *wd, S *sub_wd, class bitext *bext, struct params &params) {
	blockdes_params bd_params(params.R);
	block_t blocks;

// bext->set_r(wd_overlap_trait<weakdes_block>::r);
	uint64_t t = bext->num_random_bits();
	blocks = bd_params.compute_blocks(2 * M_E, params.pp.m, t);
	wd->init_wd(blocks, sub_wd);

	return;
}

void alloc_init_weakdes(wd_type type, weakdes **wd, bitext *bext, params &params) {
	switch (type) {
	case wd_type::GF2X: {
		*wd = new weakdes_gf2x;
		init_primitives(dynamic_cast<weakdes_gf2x *>(*wd), bext, params);
		break;
	}
	case wd_type::GFP: {
		*wd = new weakdes_gfp;
		init_primitives(dynamic_cast<weakdes_gfp *>(*wd), bext, params);
		break;
	}
	case wd_type::AOT: {
		*wd = new weakdes_aot;
		init_primitives(dynamic_cast<weakdes_aot *>(*wd), bext, params);
		break;
	}
	default:
		cerr << "Internal error: Unknown weak design type requested" << endl;
		exit(-1);
	}
}

void show_params(struct params &params, bitext *bext, weakdes *wd) {
	cout << "-------------------------------------------------------------"
			 << endl;
	cout << "Number of input bits: " << params.pp.n << endl;
	cout << "Number of extracted bits: " << params.pp.m << endl;
	cout << "Bit extractor: " << bitext_to_string(params.bxt) << endl;
	cout << "Weak design: " << weakdes_to_string(params.wdt);
	if (params.wdt == wd_type::BLOCK) {
		cout << " (basic construction: " << weakdes_to_string(params.basic_wdt)
				 << ", number of blocks: "
				 << dynamic_cast<weakdes_block *>(wd)->get_num_blocks() << ")" << endl;
	}
	cout << endl;

	cout << "Source entropy factor alpha: " << params.pp.alpha << endl;
	cout << "1 bit error eps: " << params.pp.eps << endl;
	cout << "Required source entropy: " << bext->compute_k() << " (available: "
			 << static_cast<uint64_t>(params.pp.alpha * params.pp.n) << ")" << endl;

	if (params.bxt == bext_type::XOR) {
		cout << "XOR-extractor parameter l: "
				 << dynamic_cast<bitext_xor *>(bext)->get_l() << endl;
	}

	if (params.bxt == bext_type::LU) {
		cout << "Lu-extractor parameters: nu=" << params.pp.lu_nu
				 << ", c=" << dynamic_cast<bitext_expander *>(bext)->get_c()
				 << ", l=" << dynamic_cast<bitext_expander *>(bext)->get_l()
				 << ", w=" << dynamic_cast<bitext_expander *>(bext)->get_w() << endl;
	}

	if (params.save_weakdes && params.verbose)
		cout << "Saving weak design to " << params.wd_filename << endl;

	cout << "Using " << params.num_tasks << " parallel computation unit";
	if (params.num_tasks != 1)
		cout << "s";
	cout << endl;
}

/////////////////////////////////// Dispatcher /////////////////////////////
int dispatch(struct params &params) {
	int num_tasks;
	num_tasks = tbb::task_scheduler_init::default_num_threads();
	if (params.num_tasks > 0)
		num_tasks = params.num_tasks;
	else
		params.num_tasks = num_tasks;
	tbb::task_scheduler_init init(num_tasks);

	// We rely on openssl being compiled with thread support
#ifndef OPENSSL_THREADS
#error Please use a thread capable openssl
#endif

	init_timekeeping();
	init_openssl_locking(); // Not currently needed
	// NOTE: Must be set up before any Rcpp data types are instantiated:
	R_interp *r_interp = new R_interp;

	// Suitable quick test parameters for XOR and GF2X: eps=1e-7, n=10e9, m=10e5
	// Parameters of Ma et al.: RSH and GF2X, n=2**14, m=2**13
	class bitext *bext;
	class weakdes *wd;

	// We rely on double having at least 64 bits in the modular
	// multiplication code
	if (sizeof(double) < 8) {
		cerr << "Internal error: double must at least encompass 64 bits!" << endl;
		exit(-1);
	}

	void *global_rand; // beginning of new lines replacing the above, (initializes global_rand pointer)
	vector<uint64_t> global_rand_vector; // chg Mink 7/25/2014, pass ptr not a copy of "global_rand_vector"

	if (params.streaming) { // for streaming option, start server, wait for connections & init fd's
		start_server(&socket_fd_data, &socket_fd_seed, &socket_fd_out);
	}

NEXT_STREAM:

	if (params.streaming) { // for streaming option, get cmd_line info & then weak data
		stream_get_cmd_line_bits(socket_fd_data, &params.pp.n, &params.pp.m,
														 &params.pp.eps, &params.pp.alpha, &params.blk_des);
		global_rand_vector.resize(params.pp.n / (8 * sizeof(uint64_t)) + 1);
		uint64_t *v_data = global_rand_vector.data();
		stream_get_data_bits(params.pp.n, socket_fd_data, v_data); // get streaming weak data
		global_rand_vector.at(params.pp.n / 64) &=
				part_mask(params.pp.n); // clr out unneeded lower bits when bit len is not aligned at 64
		global_rand = &global_rand_vector;
		cout << "QRNG Streaming CMD and Weak Data Done" << endl;
	} else if (params.pp.i == "NA") { // if no user-given source bits file, get
																		// bits with create_randomness(...)
		prng random; // create new prng object in order to use prng.hpp's PRNG
		global_rand = random.create_randomness(params.pp.n, &global_rand_vector);
		global_rand_vector.at(params.pp.n / 64) &= part_mask(params.pp.n); // clr out unneeded lower bits when len is not aligned at 64
//		printf("&global_rand_vector=%p\n", &global_rand_vector);
//		printf("sizeof(global_rand_vector)=%lu\n", sizeof(global_rand_vector) );
//		printf("global_rand_vector.size)=%ld\n", global_rand_vector.size() );
//		printf("global_rand_vector.at(0)=%ld\n", global_rand_vector.at(0) );
//		printf("global_rand=%p\n", global_rand);
	} else { // if there is a user-given source bits file, get the size of it and
					 // extract the bits in chunks of 64, storing them
					 // in 64-bit unsigned long long ints in an array
		cout << "reading data file..." << flush;
		const int size = params.pp.n / 64 + 1; 
		global_rand_vector.resize(size);
		ifstream data_file(params.pp.i);
		if (data_file.is_open()) {
			uint64_t len = rd_ascii_bit_file(data_file, params.pp.n, &global_rand_vector);
			if (len < params.pp.n) {
				cout << "Insufficient input data in file, only " << len
						 << " instead of specified " << params.pp.n << endl;
				exit(-1);
			}
			data_file.close();
		} else {
			cout << "****Can't open specified input data file " << params.pp.i
					 << ", Terminating Program" << endl;
			exit(-1);
		}
		void *global_vector = &global_rand_vector; // puts the input file's bits, currently in an array, into global_rand *
		global_rand = global_vector;
		cout << "done" << endl << endl;
	}

	switch (params.bxt) {
	case bext_type::LU: {
		cerr << "LU extractors are not currently supported. Its code needs to be "
						"updated to support a seedsplit into 2 sub-seed vectors."
				 << endl;
		exit(-1);
	}
	case bext_type::XOR: {
		cerr << "XOR extractors are not currently supported. Its code needs to be "
						"updated to support a seedsplit into 2 sub-seed vectors."
				 << endl;
		exit(-1);
	}
	case bext_type::RSH: {
		uint64_t k = params.pp.alpha * params.pp.n - 1; // Mink 8/15/2014, added params.pp.max_entropy to params
		double k_frac = params.pp.alpha * params.pp.n - 1; // Mink 8/15/2014, added params.pp.max_entropy to params
		params.pp.max_entropy = (k_frac - (4.0 * log2(1.0 / params.pp.eps)) - 6);
//			params.pp.max_entropy = (k-(4.0*log2(1.0/params.pp.eps)) - 6);
//			double max_entropy_frac = (k_frac-(4.0*log2(1.0/params.pp.eps)) - 6);
//			printf("Max extractable Entropy =%f, k=a*n-1=%d (%f)\n",
//					(k_frac-(4*log2(1.0/params.pp.eps)) - 6), k , k_frac);
		printf("Max extractable Entropy=%f, k=a*n-1=%ld (%f)\n",
					 (k_frac - (4 * log2(1.0 / params.pp.eps)) - 6), k, k_frac);
		params.pp.m_req = params.pp.m; // set to requested output amt, "m"
		if (params.pp.m == 0) {				// move here from "parse_cmdline"
			params.pp.m = (uint64_t)(params.pp.max_entropy / (M_E)); // (2*M_E));	// Max Weak Design
			if (params.blk_des)							// default is Max Entropy
				params.pp.m_req = params.pp.max_entropy; // Max Blk Design
			else
				params.pp.m_req = params.pp.m; // Max Weak Design
		} else if (params.pp.m <= (uint64_t)(params.pp.max_entropy / (M_E))) {
			if (params.blk_des) { // Entropy within Weak Design limits, don't need Blk Design
				params.blk_des = 0;
				cout << "WARNING: Block Design Unneccessary, Reverting to WEAK Design" << endl;
			}
		} else if (params.pp.m > (uint64_t)(params.pp.max_entropy / (M_E))) {
			if (params.blk_des == 0) { // Entropy exceeds Weak Design limits, must be Blk Design
				cerr << "ERROR: requested m is too large for WEAK Design, Terminating" << endl;
				exit(-1);
			}
			params.pp.m = (uint64_t)(params.pp.max_entropy / (M_E)); // set to Max WEAK Design Blk Size
			if (params.pp.m_req > params.pp.max_entropy) { // Entropy exceeds Blk Design limits, terminate
				cerr << "ERROR: requested m is too large for Block Design, Terminating" << endl;
				exit(-1);
			}
		}
		cout << "m=" << params.pp.m << ", m_req=" << params.pp.m_req << endl;
		params.pp.a = 0;
		bitext_rsh *btx_rsh = new bitext_rsh(r_interp);
//		params.pp.m += (uint64_t)((k-4*log2(1.0/params.pp.eps) - 6)/(2*M_E));
//		printf("eps=%10.9f, M_E=%f, mult_factor=%d\n", params.pp.eps, M_E, mult_factor);
//		printf("got to	bext_type::RSH:\n");
//		printf("global_rand_vector.at(0)=%ld\n", global_rand_vector.at(0) );
		btx_rsh->set_input_data(global_rand, params.pp);
		bext = btx_rsh;
		break;
	}
	default:
		cerr << "Internal error: Unknown 1-bit extractor requested" << endl;
		exit(-1);
	}

	alloc_init_weakdes(params.wdt, &wd, bext, params);

	if (params.verbose)
		show_params(params, bext, wd);

	// Make sure that the source provides a sufficient amount of entropy
	if (bext->compute_k() > params.pp.alpha * params.pp.n) { // is needed Weak Design entropy > avail Max entropy
		if (params.ignore_entropy)
			cerr << "Warning: ";
		else
			cerr << "Error: ";
		cerr << "Source does not contain sufficient entropy "
				 << "for specified extraction parameters!" << endl;
		if (!params.ignore_entropy) {
			cerr << "(Choose a smaller m, change eps or use a "
					 << "different weak design to satisfy the constraints," << endl
					 << "or specify --ignore-entropy-violation to "
					 << "ignore the constraint)" << endl;
			exit(-1);
		}
	}
	trevisan_dispatcher(wd, bext, params);

	if (params.streaming) {
		cout << " NEXT_STREAM " << endl;
		goto NEXT_STREAM;
	}
	delete bext;
	delete wd;
	delete r_interp;

	return 0;
}

int main(int argc, char **argv) {
	struct params params;
	parse_cmdline(params, argc, argv);
	int ret = dispatch(params);

	return (ret);
}
