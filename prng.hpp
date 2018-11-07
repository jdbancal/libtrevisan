#ifndef PRNG_H
#define PRNG_H

#include <stdio.h>
#include <stdlib.h> 
#include <vector>
#include <bitset>
#include <math.h>
#include <random>
#include <cstdint>
#include <functional>


// Variant 1
template<class T>
void *create_randomness(uint64_t n, std::vector<T> &rand) {
	// Prepare n bits of randomness using a pseudo-random number generator.
	// A vector instead of a raw array is used to store the data because
	// this way, it's easier for the caller to keep track of the allocation.
	std::uniform_int_distribution<T>
		distribution(0, std::numeric_limits<T>::max());
	std::mt19937 mt_engine(42);
	auto generator = std::bind(distribution, mt_engine);
	
	uint64_t num_chunks = n/sizeof(rand[0])+1;
	rand.reserve(num_chunks);
	
	for (auto i = 0; i < num_chunks; i++) {
		rand[i] = generator(); 
	}

	return (&rand[0]);
}


// Variant 2

using namespace std;
class prng {

public:
	
	//creates # of 64-bit values that have n bits in total
	void *create_randomness(uint64_t size_of_string, vector<uint64_t> *vector_to_fill) { 
		vector_to_fill->resize(size_of_string/(8*sizeof(uint64_t))+1);
		for(uint n=0;n < (size_of_string/(8*sizeof(uint64_t))+1);n++){
			uint64_t rand_num = get_rand_num();
			vector_to_fill->at(n)=rand_num;
		}
		void *vector_filled = vector_to_fill;
		return vector_filled;
	}

	//gets a 64-bit random number
	uint64_t get_rand_num() {	 // Mink 9-10-2014, new, more eff ver
		uint64_t rand_val = (rand() & 0xFFFFFFFF);	 // revised 
		rand_val <<= 32;
		rand_val |= (rand() & 0xFFFFFFFF);
		return (rand_val);
	}
};

#endif
