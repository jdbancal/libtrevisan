#ifndef UTILS_HPP
#define UTILS_HPP

// don't use namespace std -- we may want to use this in code
// where std is not imported into the main namespace

#include<cstddef>
#include<cassert>
#include<cstdlib>
#include<cmath> // floating point logarithm
#include<fstream>
#include<sstream>
#include <stdio.h> // gets print, if needed
#include<iostream>
#include<vector>
#include<limits>

#define BITS_PER_BYTE	8
#define BITS_PER_TYPE(_type)	(sizeof(_type)*BITS_PER_BYTE)

// Compute \lceil log_{2}(n)\rceil. We don't care about efficiency here.
template<class T>
std::size_t ceil_log2(T num) {
	assert(num > 0);

	if (num & ((T)1 << (BITS_PER_TYPE(T) - 1))) {
		if ((num & ((T)1 << (BITS_PER_TYPE(T) - 1) - 1)) > 0) {
			std::cerr << "Internal error: Overflow in floor_log2" 
				  << std::endl;
			std::exit(-1);
		}

		return BITS_PER_TYPE(T);
	}

	std::size_t log;
	for (log = BITS_PER_TYPE(T)-1; log--; log >= 0) {
		if (num & ((T)1 << log))
			if ((num & (((T)1 << log) - 1)) > 0)
				return log+1;
			else
				return log;
	}

	// Should never be reached
	assert(1==0);
	return 42;
}

// Determine how many bits are required to store a number num
// Could as well compute ceil_log2<T>(num+1)
template<class T>
std::size_t numbits(T num) {
	assert(num >= 0);
	std::size_t bits;

	if (num == 0)
	    return 1;

	bits = ceil_log2<T>(num);

	if (num & ((T)1 << bits)) {
		return bits + 1;
	}

	return bits;
}

// Same for floor.
template<class T>
std::size_t floor_log2(T num) {
	assert(num > 0);
	size_t log;

	for (log = BITS_PER_TYPE(T)-1; log--; log >= 0) {
		if (num & ((T)1 << log))
			return log;
	}
}

// Shannon entropy
template<class T>
T h(T x) {
	if (x < 2*std::numeric_limits<T>::epsilon())
		return(0);

	if (x > 1-2*std::numeric_limits<T>::epsilon())
		return(0);

	return(-x*log2(x) -(1-x)*log2(1-x));
}

template<class C, class OutIter>
OutIter copy_container(const C& c, OutIter result) {
	return std::copy(c.begin(), c.end(), result);
}

// gets 64-bit values from a single line of binary bits from a specific user-provided text file
inline void * load_data(std::string file_name, uint64_t bits_needed, std::vector<uint64_t> *data_vector){	// Chg 
	std::cout << "----------Using  " << file_name << std::endl;				//stores in a void pointer
	int size = 0;
	if(bits_needed % 64 == 0){
		size = bits_needed/64;
	}else{
		size = bits_needed/64 + 1;
	}
	data_vector->resize(size);		// Chg
	std::ifstream data_file (file_name);
	if(data_file.is_open()) {
		int counter =0;
		std::string line;
		std::getline (data_file,line);
  	int string_length = line.length();
			if (string_length > 64) {		// if the input is one long line of thousands of bits, select them 64 at a time
			while((string_length >= 64) && (counter < size)) {		// and put them in an array to be set equal to global_rand when filled.
				if (string_length < 128) {
					std::string first64 (line.begin(), line.begin()+64);
					const char *temp_Array2;
					temp_Array2=first64.c_str();
					const char *char_array2 = (const char *) temp_Array2;
					data_vector->at(counter) = strtoull(char_array2,0,2);	// Chg
					break;
				}
				std::string first64 (line.begin(), line.begin()+64);
				const char *temp_Array2;
				temp_Array2=first64.c_str();
				const char *char_array2 = (const char *) temp_Array2;
				data_vector->at(counter) = strtoull(char_array2,0,2);	// Chg
				std::string remainder (line.begin()+64, line.begin()+(string_length));
				line = remainder;
				string_length = line.length();
				counter +=1;
			}
		}
		else {
			std::cerr << "Please format input in either 64 bit lines or in only one line!" << std::endl;
		}
		data_file.close();
	}		
	void *data_void_ptr = data_vector;	// Chg: puts the input file's bits into void *
	return data_void_ptr;
};

#endif

