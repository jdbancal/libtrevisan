/* This is a dummy file put in the place of the unlocatable 
	and missing from GitHub file utils.hpp. 

	Andrea Rommal, 8/5/2013
	Version 3

*/
#include<fstream>
#include<sstream>
#include <stdio.h> // gets print, if needed
#include <iostream>

typedef unsigned long long uint64;
using namespace std;

class utils {

public:
	template<typename D>
	unsigned short BITS_PER_TYPE(D type) {
		return 8*sizeof(type);
	};

/* -------------------------------------------------------------------------- */
	
		// gets 64-bit values from a single line of binary bits from a specific user-provided text file
		
	void * load_data(string file_name, uint64_t bits_needed, vector<uint64_t> *data_vector){	// Chg 
		cout << "----------Using  " << file_name << endl;				//stores in a void pointer
		int size = 0;
		if(bits_needed % 64 == 0){
			size = bits_needed/64;
		}else{
			size = bits_needed/64 + 1;
		}
		data_vector->resize(size);		// Chg
		ifstream data_file (file_name);
		if(data_file.is_open()) {
			int counter =0;
			string line;
			getline (data_file,line);
			int string_length = line.length();
			if (string_length > 64) {		// if the input is one long line of thousands of bits, select them 64 at a time
				while((string_length >= 64) && (counter < size)) {		// and put them in an array to be set equal to global_rand when filled.
					if (string_length < 128) {
						string first64 (line.begin(), line.begin()+64);
						const char *temp_Array2;
						temp_Array2=first64.c_str();
						const char *char_array2 = (const char *) temp_Array2;
						data_vector->at(counter) = strtoull(char_array2,0,2);	// Chg
						break;
					}
					string first64 (line.begin(), line.begin()+64);
					const char *temp_Array2;
					temp_Array2=first64.c_str();
					const char *char_array2 = (const char *) temp_Array2;
					data_vector->at(counter) = strtoull(char_array2,0,2);	// Chg
					string remainder (line.begin()+64, line.begin()+(string_length));
					line = remainder;
					string_length = line.length();
					counter +=1;
				}
			}
			else {
				cerr << "Please format input in either 64 bit lines or in only one line!" << endl;
			}
			data_file.close();
		}		
		void *data_void_ptr = data_vector;	// Chg: puts the input file's bits into void *
		return data_void_ptr;
	};
};
	
/* -------------------------------------------------------------------------- */
	
template<typename C>
unsigned short numbits(C get_bits_from)
		{
			return ceil(log2(get_bits_from));
		};
		


