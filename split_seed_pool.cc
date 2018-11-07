/*
 * This code reads the input stream "cin" (which should be a redirected file name)
 * and splits it into 2 new output files "file_seed" and "file_remainder". The amt 
 * of 1's & 0's from input to "file_seed" must be specified (any other embedded 
 * chars are also copied, the remaining data is copied to "file_remainder".
 *
 * The intent is to extract the seed data for the current QRNG experiment into
 * "file_seed" and the remaing seed data copied to "file_remainder", so the seed
 * data won't be used again. The seed amt can be obtain from running the Trevisan
 * code for the current experiment.
 */

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstring> // memset
using namespace std;

/* -------------------------------------------------------------------------- */

// fct to read "1" & "0" data from files & ignore any other chars

int copy_part_ascii_bit_file(ofstream & out_file_handle, int bits_needed){
	char c;
	int cnt = 0;
	int num = 0;

	if (bits_needed <= 0) { // choose big num with intension to read remainder of file
		 bits_needed = 0x7FFFFFFF;
	}
	while (std::cin>>c && cnt<bits_needed) {
		if (c=='1' || c=='0') {
			cnt++;
		}
		else {
			num++;
		}
		out_file_handle << c;
	}
	if ( num > 0 )
		std::cout << "***num of other events=" << num << endl;
	return (cnt);
}

/* -------------------------------------------------------------------------- */



main(int argc, char *argv[])
{
	int seed_len, argcCount;
	char *file_seed, *file_remainder;

	seed_len = 0;
	file_seed = "file_seed";
	file_remainder = "file_remainder";
	if (argc >= 2) {
     	      argcCount = 1;
     		   // keep extracting params from the cmd line
     	      while(argcCount < argc) {
	 	  if (argv[argcCount][0] == '-' && argv[argcCount][1] == 'n' ) {
       		        seed_len = atoi(&argv[argcCount+1][0]);		// specify ID type
          		argcCount += 2;
			printf("seed_len =%d\n", seed_len);
            	   } else if (argv[argcCount][0] == '-' && ((argv[argcCount][1] == 'f' && argv[argcCount][2] == 's')
							 || (argv[argcCount][1] == 'f' && argv[argcCount][2] == '1')) ) {
              		file_seed = argv[argcCount+1];    // file_seed
              		argcCount += 2;
              		printf("file_seed=%s\n", file_seed);
            	   } else if (argv[argcCount][0] == '-' && ((argv[argcCount][1] == 'f' && argv[argcCount][2] == 'r')
							 || (argv[argcCount][1] == 'f' && argv[argcCount][2] == '2')) ) {
              		file_remainder = argv[argcCount+1];    // file_seed
              		argcCount += 2;
              		printf("file_remainder=%s\n", file_remainder);
            	  }  else  {
              		printf("INCORRECT Param usage:\n");
              		printf("%s <-n seed_len> <-fs or -f1 file_seed_name> <-fr or -f2 file_remainder_name>\n", argv[0]);
              		exit(0);
        	  }   
	       } // end WHILE
    	}	// end IF
	if (seed_len <= 0 ) {
		printf("no seed length to extract, terminating\n");
		exit(-1);
	}
    	ofstream seed_file (file_seed);	// opend output file_1
	if(seed_file.is_open()) {
			// copy "seed_len" 1's & 0's along with any other chars from input strem to file_1
		int len = copy_part_ascii_bit_file(seed_file, seed_len); 
	} else {
		std::cout << "****Can't open specified Seed file " << file_seed << ", Terminating Program" << endl;
		exit(-1);
	}

    	ofstream remainder_file (file_remainder); 	/// opend output file_2
	if(remainder_file.is_open()) {
			// copy remaining data from input stream to file_2
	//	int len = copy_part_ascii_bit_file(remainder_file, 0); 
		remainder_file << std::cin.rdbuf();
	} else {
		std::cout << "****Can't open specified Seed file " << file_remainder << ", Terminating Program" << endl;
		exit(-1);
	}
	std::cout << "done" << endl; 
}
