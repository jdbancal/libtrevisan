
/* This is a dummy file put in the place of the unlocatable 
	and missing from GitHub file prng.hpp. 
	
	This file's purpose is assumed to be the selection and 
	concatenation of pseudo-random bits into strings.

	Andrea Rommal, 8/5/2013
	Version 3

*/

#include <stdio.h>
#include <stdlib.h> 
#include <vector>
#include <bitset>
#include <math.h>

using namespace std;
class prng {

public:
	
/* -------------------------------------------------------------------------- */

			 //creates # of 64-bit values that have n bits in total
	void * create_randomness(uint64_t size_of_string, vector<uint64_t> *vector_to_fill) { 
		vector_to_fill->resize(size_of_string/(8*sizeof(uint64_t))+1);
		for(uint n=0;n < (size_of_string/(8*sizeof(uint64_t))+1);n++){
			uint64_t rand_num = get_rand_num();
			vector_to_fill->at(n)=rand_num;
		}
		void *vector_filled = vector_to_fill;
		return vector_filled;
	}

/* -------------------------------------------------------------------------- */

		//gets a 64-bit random number
	uint64_t get_rand_num() {	 // Mink 9-10-2014, new, more eff ver
		uint64_t rand_val = (rand() & 0xFFFFFFFF);	 // revised 
		rand_val <<= 32;
		rand_val |= (rand() & 0xFFFFFFFF);
		return (rand_val);
	}
};
