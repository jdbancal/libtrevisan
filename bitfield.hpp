
/* This is a dummy file put in the place of the unlocatable 
	and missing from GitHub file bitfield.hpp. 

	Andrea Rommal, 8/5/2013
	Version 5
*/

#ifndef BITFIELD_H
#define BITFIELD_H


#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <bitset>

typedef unsigned long long uint64;
using namespace std;

template <typename T, typename P>
class bitfield {

public:
	#define LARGE_NUM 60000							

/* -------------------------------------------------------------------------- */
		//creates a generic-sized vector large enough for all values
	bitfield() {			
		//	bitfield_vector.resize(LARGE_NUM);    // deleted
	};
	
/* -------------------------------------------------------------------------- */
		//gets one bit from the location specified from the bitfield vector
	uint64_t get_bit(uint64_t value) {	
		int bit_vector = value / 64;
		int bit_placement = value % 64;
		uint64_t vector_value = (uint64_t) bitfield_vector->at(bit_vector);  // chg from  .at(bit_vector);
		uint64_t bit_value = ((vector_value>>bit_placement)&0x1LLU);  // chg from (uint64_t) get_binary(vector_value, bit_placement);
		return bit_value;
	};
	
/* -------------------------------------------------------------------------- */
	
	//gets a set of bits from the location range specified from the bitfield vector
	void get_bit_range(uint64_t low_bound, uint64_t upper_bound, const unsigned char* vect[0]){ 
		int count = 0;
		string temp;
		for (int i = low_bound; i <= upper_bound; i++) { // changed from "<" to "<=" to be inclusive
			temp += to_string((unsigned long long int)get_bit(i));
			count++;
		}
		const char *temp_charArray;
		temp_charArray=temp.c_str();
		const unsigned char *final_char_Array = (const unsigned char *) temp_charArray;
		vect[0] = final_char_Array;
	};
	
/* -------------------------------------------------------------------------- */
	
	  //stores the values given by create_randomness or input text file as a bitfield vector (64 bit storage)
	template <typename Q>
	void set_raw_data(Q para_vector, uint64 size) {	  // chg to ptr rather that a separate vector to be copied			
	
		bitfield_vector= static_cast<vector<uint64_t>* >(para_vector);		// added to just save ptr
	};
	
/* -------------------------------------------------------------------------- */

	unsigned int get_binary(uint64_t vector_value, int i) {	//gets the binary value of the number, returns based on bit number
		bitset<64> bin_x(vector_value);
		bitset<64> temp_bit = (bin_x << (64-(i+1))) >> 63;
		unsigned int bit = ((unsigned int) (temp_bit.to_ulong()));
		return bit;
	}
	
	protected:
		vector<uint64_t> * bitfield_vector;    // chg to ptr rather that a separate vector to be copied
};

#endif
