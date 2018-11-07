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

// A 1-bit extractor based on a concatenated Reed-Solomon-Hadamard code
// Algorithmic description and parameter calculations by Christoper Portmann

#include<iostream>
#include<fstream>
#include<cstddef>
#include<cstdlib>
#include<cstdio> // stderr
#include<gmp.h>
#include<vector>
#include<NTL/tools.h>
#include<NTL/GF2E.h>
#include <NTL/GF2XFactoring.h>
#include "GF2Es.h"
#include "timing.h"
#include "utils.hpp"
#include "irreps.h"
#include "1bitext_rsh.h"
#ifndef NO_DEBUG
#include "debug.h"
#endif

#ifdef HAVE_SSE4
#include <smmintrin.h> // popcnt
#endif

extern int debug_level;

#ifdef USE_NTL
NTL_CLIENT
#endif

using namespace std;

// NOTE: With NTL, it is not possible to work with two Galois fields
// of different sizes simultaneously.  So if both, the weak design and
// the 1-bit extractor, are based on Galois fields, we cannot use
// NTL... It is, however, possible to base one of the implementations
// on NTL. This allows for cross-checking the implementation, which is
// why we keep the code around.

bitext_rsh::~bitext_rsh() {
	if (global_rand != nullptr) {
		for (auto bignum : coeffs)
			BN_free(bignum);
	}
}

vertex_t bitext_rsh::num_random_bits() {
	// The number of random bits required for each 1-bit extractor run
	// is 2*l. For computational efficiency, the amount needs to be easily
	// divisible into two parts, we round the amount up so that it's
	// an even multiple of 8, i.e., the number of bits per char.

	uint64_t l_rounded = l + (8-l%8);
	return 2*l;
}

uint64_t bitext_rsh::compute_k() {
	// Caveat: r can mean the polynomial order and the overlap in this context.
//	return(static_cast<uint64_t>(bitext::r*pp.m + 4*log2(1.0/pp.eps) + 6));
	return(static_cast<uint64_t>(pp.r*pp.m + 4*log2(1.0/pp.eps) + 6));
}

void get_ssl_irrep(int p[], unsigned n) {

		GF2X P;
		BuildSparseIrred(P, n);

		cout << "case " << n << "= " << endl;

		int lidx = 0;
		for (long i = deg(P); i >= 0; i--) {
			if (coeff(P, i) == 1) {
				p[lidx] = i;
				cout << "p[" << (lidx++) << "] = " << i << ";" << endl;
			}
		}
		cout << endl;
		while (lidx < 5)
			p[lidx++] = 0;
}

void bitext_rsh::compute_r_l() {
	l = ceil(log2((long double)pp.n) + 2*log2(2.0/pp.eps)); // chg: ceil(log2((long double)pp.n) + 2*log2(2/pp.eps));
	r = ceil((1.0*pp.n)/(1.0*l));  			// chg:  r = ceil(pp.n/l);
	printf("l=%lu, r=%lu, n=%lu, log(n)=%Lf, log(2/err)=%f\n", l, r, pp.n, log2((long double)pp.n), log2(2.0/pp.eps) );
	
	uint64_t l_rounded = l + (8-l%8); // See comment in num_random_bits
	chars_per_half = l_rounded/BITS_PER_TYPE(char);
//	printf("l_rounded=%ld, l=%ld, chars_per_half=%ld\n", l_rounded, l, chars_per_half); 

	if (debug_level >= RESULTS) {
		cerr << "RSH EXT: eps: " << pp.eps << ", n: " << pp.n << endl;
		cerr << "RSH extractor: Computed l (Galois field order):" << l
		     << ", r (polynomial degree) " << r << endl;
		cerr << "Number of char instances in half of initial randomness: "
		     << chars_per_half << "(l_rounded=" << l_rounded << ")" << endl;
	}
	
	// Use the precomputed lookup table in dir "generated" (either NTL or SSL) 
	// to get the exponents of an an irreducible polynomial of order "l" (ltr "l" not # "1")
	// x**l + x**k(1) + ... + x**k(j) + 1, where j may be upto "4" (i.e., 5 elts) 
	// or as small as "0" (i.e., only x**l). Thus there may be up to 5 elts 
	// in "irred_poly", order by value (i.e., irred_poly[0] = l).
	// This is used as a modulus in the RHS poly expansion in the
	// "horner_poly_gf2n" below
//	get_ssl_irrep(irred_poly, l); // for OpenSSL
	set_irrep(irred_poly, l);	// get the irreducible poly def for either NTL or SSL
//	cout << "xxxirred_poly=0x" << irred_poly << endl;
//	cout << "***irred_poly=" << irred_poly[0] << ", " << irred_poly[1] <<  ", " << irred_poly[2]  <<  ", " << irred_poly[3] << endl;
#ifdef USE_NTL
	GF2E::init(irred_poly);	// set NTL modulus for current Poly (SSL uses it directly in the "Horner" fct)
	cout << "***using NTL" << endl;
#else
	cout << "***using SSL" << endl;
#endif
	
	if (debug_level >= INFO) {
#ifdef USE_NTL
		cerr << "Picked rsh bit extractor irreducible polynomial "
		     << irred_poly << endl;
#else
		cerr << "Picked rsh bit extractor irreducible polynomial ";
		for (auto i : irred_poly)
			cerr << i << " ";
		cerr << endl;
#endif
	}
}

#ifndef USE_NTL
// Horner's rule for polynomial evaluation, using GF(2^n) arithmetic
// This is nearly c&p from weakdes_gf2p, but providing a unified
// version via templates would require another layer of indirection for
// the function calls, so I don't think the extra complexity pays off
// for this small amount of code.
BIGNUM *bitext_rsh::horner_poly_gf2n(BIGNUM *x) {
	BIGNUM *res; res = BN_new();
	BIGNUM tmp; BN_init(&tmp);
	BN_zero(res);

	// TODO: Ideally, This should be moved to the per-thread initialisation
	// (would require an API change that we could do during the migration
	// tp TBB). We can also allocate a new context in the copy operator
	// and the constructor to make sure each object is equipped with an
	// own one, and use one instance for each tbb thread (just provide
	// the instance as copy-by-value in the dispatcher)
	BN_CTX *ctx = BN_CTX_new();

	size_t len = (coeffs.size()-1);  // use if you want to reverse the coeff order
	for(size_t i = 0; i < coeffs.size(); i++) {	// reverse coeffs oder for Horners Rule
		BN_GF2m_mod_mul_arr(&tmp, res, x, irred_poly, ctx);	// tmp = (res*x) mod irred_poly
		BN_GF2m_add(res, &tmp, coeffs[len-i]);						// res = tmp + coeffs[i]
	}

	BN_CTX_free(ctx);

	return res;
}
#endif

// new fct replaces "b.get_bit_range" in bitfield.hpp
// gets a set of bits from the location range specified from the bitfield vector
// build the extract polynomial segments from the input data
void bitext_rsh::build_ext_poly_seg(uint64_t low_bound, uint64_t upper_bound, unsigned char* vect) { 

	uint     loc, wrd, pad;
	uint64_t val, new_upper_bound;
	string   temp;
	int 	 vec_wrd, vec_loc;
		// get the input data vector ptr
	vector<uint64_t> *input_rand_vector = (vector<uint64_t>* ) (global_rand);

	int len = upper_bound - low_bound; // len-1 in bits
	int byte_len = (len+sizeof(chunk_t))/sizeof(chunk_t); // len in bits
	for (int i=0; i<byte_len; i++) // int vect array to "0"
		vect[i] = 0;
	pad = 0;
	new_upper_bound = upper_bound;
	if (upper_bound >= pp.n) {
		pad = upper_bound - (pp.n-1);
		new_upper_bound = (pp.n-1);
		len = new_upper_bound - low_bound ; // len-1 in bits
	}
	#ifdef USE_NTL	 // NTL - LSB (LSB in Byte(0)) & LittleEndian (LSB in bit(0))
		vec_wrd =  len/sizeof(chunk_t); // start `wrd loc of MSB
	#else		 // SSL - MSB (MSB in Byte(0)) & BigEndian (MSB in bit(7))
		vec_wrd =  pad/sizeof(chunk_t); // start wrd loc of MSB
	#endif
	vec_loc = len%sizeof(chunk_t); // output bit loc within Byte the same for SSL & NTL
	wrd = low_bound/64;	// starting word
	loc = 63 - (low_bound % 64);	// starting bit loc within that word (MSB leftmost)
	val = input_rand_vector->at(wrd);  // get that data word
	for (uint64_t i=low_bound; i<=new_upper_bound; i++) {
//		temp += to_string(val&0x1LL); // add that bit to the string
		if ( ((val>>loc)&0x1LLU) == 1) {
			vect[vec_wrd] |= (chunk_t) (1<<vec_loc); // place "1"s in a field of "0"s
		}  // update input ptrs
		if (loc == 0) {
			loc = 63;
			wrd++;
			val = input_rand_vector->at(wrd);
		} else { // word overflow, get next word
			loc--;	      // decr bit cntr, moving left-to-right
		}  // udate output ptrs
		if (vec_loc == 0) {
			vec_loc = sizeof(chunk_t)-1; // 7;
			#ifdef USE_NTL  // LSB
				vec_wrd--;  // fill towards Byte "0"
			#else		// MSB
				vec_wrd++;  // fill towards Byte "N"
			#endif
		} else {
			vec_loc--;
		}
	}
}

void bitext_rsh::create_coefficients() {
	if (global_rand == NULL) {
		cerr << "Internal error: Cannot compute coefficients for uninitialised "
		     << "RSH extractor" << endl;
		exit(-1);
	}

	// TODO: Some padding may need to be required
	uint64_t elems_per_coeff = ((l+sizeof(chunk_t)-1)/sizeof(chunk_t)) ;	// since the "vec" is in Bytes, its size should be also
	vector<unsigned char> vec(elems_per_coeff);

#ifndef USE_NTL
	coeffs.resize(r); // alloc the "r" coeffs
#endif		// build the extract polynomial segments from the input data
	for (uint64_t i = 0; i < r; i++) {
		build_ext_poly_seg(i*l, (i+1)*l-1, &vec[0]);
/*				// Debug info
			cout << "**vect[]=" ;
			for (int j=0; j<elems_per_coeff; j++) {
				std::bitset<8> y(vec[j]); // map int value to 32 separate bits
				cout << y;		      // output those 32-bits as chars
			}
			cout << endl;
*/
		// TODO: In the paper, coefficient i is for the exponent r-i. This
		// is not what we do. Why are the values mingled this way? Since we
		// are creating the coefficients from randomness, their relative order
		// should not matter because the numerical values are, well, random...
#ifdef USE_NTL
// cout << "**** Using NTL ****" << endl;
		GF2Es val;
			// LSB - assumes bit(0) is 1st bit of input_Byte and assumes Byte(0) is LSB
			// thus must reverse bits in Bytes as well as reverse Byte order
		GF2XFromBytes(val.LoopHole(), (const unsigned char*) &vec[0] , elems_per_coeff);  // convert to NTL format
		SetCoeff(poly, i, val);	// load ith coeff into poly
/*				// Debug info
			cout << "RSH Coefficient (NTL) " << i << ": " << val << endl;
			cout << "sizeof(val)=" << sizeof(val) << endl;	
			GF2E val_test = coeff(poly, i);
			cout <<"RSH Coefficient_test (NTL) " << i << ": " << val_test << endl;;
*/
#else
// cout << "**** Using SSL ****" << endl;
		coeffs[i] = BN_new();	// def a new BN struct for coeffs[i]
		BN_bin2bn(reinterpret_cast<const unsigned char*>(&vec[0]), // convert to SSL format & store as ith coeff 
			  elems_per_coeff, coeffs[i]);	// replace len info
			 	// load the char strg "vec" into BN coeffs[i]
/*				// Debug info
			cout << "RSH Coefficient (SSL) " << i << "(" << r << "): "
			     << BN_bn2hex(coeffs[i]) << endl;
*/
#endif
	}
/*				// Debug info
		if (debug_level >= EXCESSIVE_INFO) {
			vector<uint64_t> *input_rand_vector = (vector<uint64_t>* ) (global_rand);
			int len = input_rand_vector->size();
			int last = pp.n/64;
			cout << "len = " << len << ", last = " << last << endl;
			cout << "global_rand[last] =" << hex << input_rand_vector->at(last) << endl;
			cout << "global_rand[ --] ="  << hex << input_rand_vector->at(last-1) << dec << endl;
		}
*/
}

bool bitext_rsh::extract(void *sub_seed_a, void *sub_seed_b) {	// separated sub-seed into 2 parts

	// Reed-Solomon part
	// Select the first l bits from the initial randomness,
	// initialise an element of the finite field with them, and
	// evaluate the polynomial on the value
#ifdef USE_NTL
// cout << "**** Using NTL ****" << endl;
	GF2Es val;
	// Assign l bits to val, the parameter for the polynomial
	unsigned char re_res_byte_conv[chars_per_half+7]; 	// a crude way of quickly rounding up past 8-Byte alingment
	unsigned char seed_rev[chars_per_half+7];
	unsigned char * seed = (unsigned char*) sub_seed_a;
	for (int j=0; j<chars_per_half; j++) { // reverse chars to get LSB/LittleEndian from MSB/BigEndian
		seed_rev[chars_per_half-1-j] = seed[j];
	}
	for (int j=chars_per_half; j<(chars_per_half+7); j++) { // clear upper char
		seed_rev[j] = 0;
	}
  // global_rand changed to inital_rand
	GF2XFromBytes(val.LoopHole(), seed_rev, chars_per_half);	// reverse Bytes to LSB order for conv
/*				// Debug info
			cout << "**SEED_a[]=" ;
			seed = (unsigned char*) sub_seed_a;
			for (int j=0; j<chars_per_half; j++) {
				std::bitset<8> y(seed[j]); // map int value to 8 separate bits
				cout << y << "_";      // output those 8-bits as chars
			}
			cout << endl << "**SEED_a_REV[]=" ;
			seed = seed_rev;
			for (int j=0; j<chars_per_half; j++) {
				std::bitset<8> y(seed[j]); // map int value to 8 separate bits
				cout << y << "_";      // output those 8-bits as chars
			}
			cout << endl;
			cout << "RSH Seed_a_rev (NTL) " << ": " << val << endl;
*/
  // ... and evaluate the polynomial
	GF2E rs_res;
	eval(rs_res, poly, val); // rs_res = poly(val)
	//		cout << "***RSH Poly (NTL) " << ": " << rs_res << endl;
	BytesFromGF2X(re_res_byte_conv, (const NTL::GF2X&) rs_res, chars_per_half);	// add conv from NTL to LSB number form
/*				// Debug info
			cout << "***RSH data_conv (NTL) : " ;
			for (int j=0; j<chars_per_half; j++) {
				std::bitset<8> y(re_res_byte_conv[j]); // map int value to 8 separate bits
				cout << y << "_";      // output those 8-bits as chars
			}
			cout << endl;
*/
#else
// cout << "**** NOT - Using NTL ****" << endl;
	BIGNUM *val = BN_new();
	BIGNUM *rs_res = BN_new();
	BN_bin2bn(reinterpret_cast<unsigned char*>(sub_seed_a), chars_per_half, val);	//ROMMAL EDIT 7-22-13 global_rand changed to inital_rand
	//		cout << "RSH Poly (SSL) X= " << BN_bn2hex(val) << endl;
	
	rs_res = horner_poly_gf2n(val);
	BN_free(val);
#endif

	// Hadamard part
	// Take the second l bits of randomness, compute a logical AND with the result,
	// and compute the parity
	data_t *data;
	idx_t data_len;
#ifdef USE_NTL
	data = (data_t *) &re_res_byte_conv;
	data_len =  (chars_per_half+3)/4; // len in 4-Byte words, round up to next 4 Bytes
#else
	data = reinterpret_cast<BN_ULONG*>(rs_res->d);
	data_len = rs_res->dmax;
#endif

  // global_rand changed to inital_rand
	data_t *seed_half = reinterpret_cast<data_t*>(sub_seed_b) ;
	bool parity = 0;
	unsigned short bitcnt;
	for (idx_t count = 0; count < data_len; count++) {
		*(data+count) &= *(seed_half + count);
		// If an even number of bits is set in the current subset,
		// the global parity is XORed with one.
		// TODO: Ensure that this is really alright (does always seem
		// to end up in the != case).
#ifdef HAVE_SSE4
		bitcnt = _mm_popcnt_u64(*(data+count));
#else
		bitcnt = __builtin_popcountll(*(data+count));
#endif
		// Check if the bitcount is divisible by two (without using
		// a modulo division to speed the test up)
		//if (((bitcnt >> 2) << 2) == bitcnt) {
		if ((bitcnt & 1) == 1) { // chg to odd parity (instead of even) & chg: ((bitcnt >> 2) << 2) == bitcnt change to
			parity ^= 1;			 // ((bitcnt & 1) == 0) .
		}
	}

#ifndef USE_NTL
	BN_free(rs_res);
#endif
	return parity;
}
