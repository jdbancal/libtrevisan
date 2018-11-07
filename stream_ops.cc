/* -------------------------------------------- */
// all values are char or interger during Tx, no floating pt

// request cmds, 8 bytes = 2 bytes, followed by 6 byte length req
// ?r - verification request at connect time to ID Client type:
//			current response is 4 Bytes: 1st Byte is 'd', 's' or 'o',  followd by ' ', '\0', '\0'
// cr - cmd param request, 6 bytes of zero padding
// dr - data request, 6 bytes of data size in bits (only using 4 bytes here)
// sr - seed request, 6 bytes of seed size in bits (only using 4 bytes here)

// response cmds, 2 bytes = 1 byte + 1 " " padding, followed by 6 byte msg length, followed by # of params 1 byte,
// 					followed by list of params & their values 
// c  - cmd  info, 6 bytes of cmd  size in bits (only using 4 bytes here)
// d  - data info, 6 bytes of data size in bits (only using 4 bytes here)
// s  - seed info, 6 bytes of seed size in bits (only using 4 bytes here)
// f  - fail run, no data sent

// cmd params, # of params, 1 byte param ID, followed by data, if any:
// n  - data size + 6 byte value
// a  - total entropy + 6 byte value (total bits possible, use to comput Entropy rate) params.pp.alpha = a/n (float)
// m  - output size + 6 byte value
// e  - eps, output error  + n byte value (char string of float value, zero termination)
// b  - block design + 1 byte value
/* -------------------------------------------- */

#include <sys/socket.h> // Needed for the socket functions
#include <netdb.h>      // Needed for the socket functions
// #include <netinet/in.h>
// #include <string.h>
// #include <stdio.h>	// printf
// #include <stdlib.h>  // exit
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring> // memset
#include <string>
#include <cstdint> // uint64_t

#include <sstream>
#include <stdio.h>   // printf
#include <stdlib.h>  // exit

// #include "stream_ops.h"

/*
#include "primitives.h" // params.pp
#include "utils.hpp"
#include "debug_levels.h"
#include "timing.h"
#include "extractor.h"
#include "prng.hpp"
*/
using namespace std;

#define FOREVER 	1
#define BLK_AMT	64*1024

/* ---------------------------------------------------- */
// converts a float or double to a char string
// but only has the precision of a "print statement"
/* ---------------------------------------------------- */

string my_ftoa(double value) 
{
  std::ostringstream o;
  if (!(o << value))
    return "";
  return o.str();
}

/* ---------------------------------------------------- */
/* fill int array "data_strg" of "len" with random data */
/* ---------------------------------------------------- */

void random_fill(int *data_strg, int len)
{
	int i;
	uint64_t l_wrd1;
	
	for (i=0; i<len; i++) {
		data_strg[i] = rand();
	}
}

/* ---------------------------------------------------- */
/* reorder int array "data_strg" of "len" with random data */
/* ---------------------------------------------------- */

void reorder_fill(int *data_strg, int len)
{
	int i, wrd1, wrd2;
	
	for (i=0; i<len; i=i+2) {
		wrd2 = data_strg[i+1];
		data_strg[i+1] = data_strg[i];
		data_strg[i] = wrd2;
	}
}

/* ---------------------------------------------------- */
/* server (Trevisan) request data & seed					  */
/* ---------------------------------------------------- */

uint64_t socket_get_request_data_bits(int socket_num, char char1, char char2, uint64_t length_req, char *data_array)
{	
	char req_str[8], hdr_strg[8];
	int		i;
	uint64_t len, accum, bit_len;
	size_t  incr;	// ~unsigned int
	ssize_t amt;	// signed size_t
	
	req_str[0] = char1;	// prep Request
	req_str[1] = char2;
	req_str[2] = (length_req>>40)&0xFF;	// length in bits
	req_str[3] = (length_req>>32)&0xFF;
	req_str[4] = (length_req>>24)&0xFF;
	req_str[5] = (length_req>>16)&0xFF;
	req_str[6] = (length_req>>8 )&0xFF;
	req_str[7] = (length_req    )&0xFF;
   if ( (i=send(socket_num, req_str, 8, 0)) != 8) {	// send Request
     perror("***ERROR sending Socket Info Request ");
     printf("***ERROR sending Info Request on socket=%d, len=%d\n", socket_num, i);
     exit(-1);
   }
   if ( (i=recv(socket_num, hdr_strg, 8, 0)) != 8) {	// Rcv Response header
     perror("***ERROR rcving Socket Info Response ");
     printf("***ERROR rcving Info Response on socket=%d, len=%d\n", socket_num, i);
     exit(-1);
   }
	if (hdr_strg[0]!=char1 || hdr_strg[1]!=' ') {	// verify Response type
     perror("***ERROR in rcving Socket Info Response Header ");
     printf("***ERROR in rcving Info Response Header on socket=%d\n", socket_num);
     exit(-1);
	}
	bit_len = 0;		// assemble msg bit_length from bytes
	for (i=2; i<8; i++) {
		bit_len = (bit_len<<8) | hdr_strg[i]&0xFF;
	}
	len = ((bit_len+63)/64) * 8; // len in bytes, assuming full 64-bit words TX
	accum = 0;
	while (accum < len) {	// retrive msg data from socket in BLK sizes increments
		if ( (len-accum) > BLK_AMT) 	// compute remaining bytes
			incr = BLK_AMT; // limit RECV request to BLK_AMT
		else 
			incr  = len - accum;	
      if ( (amt=recv(socket_num, &data_array[accum], incr, 0)) <= 0) {	// Rcv Response msg data
        perror("***ERROR rcving Socket data ");
        printf("***ERROR rcving data on socket=%d, got=%ld of %ld  with last =%ld\n", socket_num, accum, len, amt);
        exit(-1);
      }
		accum += amt;	// update accumlated data amt
	}
	return (accum);
}	
	
/* ---------------------------------------------------- */
/* server (Trevisan) send output data 							  */
/* ---------------------------------------------------- */

uint64_t socket_send_output_data_bits(int socket_num, char char1, char char2, uint64_t length_req, char *data_array)
{	
	char req_str[8], hdr_strg[8];
	int		i;
	uint64_t len, accum, bit_len;
	size_t  incr;	// ~unsigned int
	ssize_t amt;	// signed size_t
	
	req_str[0] = char1;	// prep header
	req_str[1] = char2;
	req_str[2] = (length_req>>40)&0xFF;	// length in bits
	req_str[3] = (length_req>>32)&0xFF;
	req_str[4] = (length_req>>24)&0xFF;
	req_str[5] = (length_req>>16)&0xFF;
	req_str[6] = (length_req>>8 )&0xFF;
	req_str[7] = (length_req    )&0xFF;
   if ( (i=send(socket_num, req_str, 8, 0)) != 8) {	// send header
     perror("***ERROR sending Socket Output Header ");
     printf("***ERROR sending Output Header on socket=%d, len=%d\n", socket_num, i);
     exit(-1);
   }
	len = ((length_req+31)/32) * 4; // len in bytes, assuming full 32-bit words TX
	accum = 0;
	while (accum < len) {	// send msg data via socket in BLK sizes increments
		if ( (len-accum) > BLK_AMT) 	// compute remaining bytes
			incr = BLK_AMT; // limit SEND to BLK_AMT
		else 
			incr  = len - accum;	
      if ( (amt=send(socket_num, &data_array[accum], incr, 0)) <= 0) {	// Send output msg data
        perror("***ERROR Sending Socket Output data ");
        printf("***ERROR Sending Output data on socket=%d, sent=%ld of %ld with last =%ld\n", socket_num, accum, len, amt);
        exit(-1);
      }
		accum += amt;	// update accumlated data amt
	}
 printf("***OKAY Sending Output data on socket=%d, sent=%ld of %ld with last =%ld\n", socket_num, accum, len, amt);
	
	if ( (i=recv(socket_num, hdr_strg, 8, 0)) != 8) {	// Rcv Response ACK
     perror("***ERROR rcving Socket Info Response ");
     printf("***ERROR rcving Info Response on socket=%d, len=%d\n", socket_num, i);
     exit(-1);
   }
	if (hdr_strg[0]!=char1 || hdr_strg[1]!='a') {	// verify ACK type
     perror("***ERROR in rcving Socket Info Response Header ");
     printf("***ERROR in rcving Info Response Header on socket=%d\n", socket_num);
     exit(-1);
	}
	len = 0;		// assemble msg length from bytes
	for (i=2; i<8; i++) {
		len = (len<<8) | hdr_strg[i]&0xFF;
	}
	if (len != length_req) {
     printf("***ERROR in ACK, ack_size =%ld, sent size=%ld, ACK on socket=%d\n", len, length_req, socket_num);
     exit(-1);
	}
	return (accum);
}	

/* ---------------------------------------------------- */
/* Client (Data, Seed & bit pool) respond to requests	  */
/* ---------------------------------------------------- */

void socket_send_response_data(int socket_num, char my_ID)
{	
	char req_str[8], hdr_strg[8], *data_strg;
	int 		i, len, accum, n, a, m;
	size_t   incr;	// ~unsigned int
	ssize_t  amt;	// signed size_t
	int 	   msg_cntent_len, cmd_ptr, b, byte_len, seed_len;
	double 	e;
	
	while (FOREVER) {
      if ( recv(socket_num, req_str, 8, 0) != 8) {	// wait for Request 
        perror("***ERROR rcving Socket Info Request ");
        printf("***ERROR rcving Info Request on socket=%d\n", socket_num);
        exit(-1);
      }
// cmd params in cmd info, 1 byte param ID, followed by data, if any:
// n  - data size + 6 byte value
// a  - total entropy + 6 byte value (total bits possible, use to comput Entropy rate) params.pp.alpha = a/n (float)
// m  - output size + 6 byte value
// e  - eps, output error  + n byte value (char string of float value, zero termination)
// b  - block design + 1 byte value
		if ( req_str[0]=='c' && req_str[1]=='r' && my_ID=='d') { 			// generate cmd line data
			free (data_strg);	// clr any old data
			byte_len = 256; 
			data_strg = (char *) calloc(byte_len, 1);
			cmd_ptr = 9;
			data_strg[8] = 5;  // 5 params being sent
			n = 2000001; // data size in bits
			data_strg[cmd_ptr++] = 'n';
			data_strg[cmd_ptr++] = 0;
			data_strg[cmd_ptr++] = 0;
			data_strg[cmd_ptr++] = (n>>24)&0xFF;
			data_strg[cmd_ptr++] = (n>>16)&0xFF;
			data_strg[cmd_ptr++] = (n>>8 )&0xFF;
			data_strg[cmd_ptr++] = (n    )&0xFF;
			a = 641;		// entropy size in bits
			data_strg[cmd_ptr++] = 'a';
			data_strg[cmd_ptr++] = 0;
			data_strg[cmd_ptr++] = 0;
			data_strg[cmd_ptr++] = (a>>24)&0xFF;
			data_strg[cmd_ptr++] = (a>>16)&0xFF;
			data_strg[cmd_ptr++] = (a>>8 )&0xFF;
			data_strg[cmd_ptr++] = (a    )&0xFF;
			m = 512;		// output size in bits
			data_strg[cmd_ptr++] = 'm';
			data_strg[cmd_ptr++] = 0;
			data_strg[cmd_ptr++] = 0;
			data_strg[cmd_ptr++] = (m>>24)&0xFF;
			data_strg[cmd_ptr++] = (m>>16)&0xFF;
			data_strg[cmd_ptr++] = (m>>8 )&0xFF;
			data_strg[cmd_ptr++] = (m    )&0xFF;
			e = 1.54e-5;  // float value
			data_strg[cmd_ptr++] = 'e';
			std::string my_str = my_ftoa(e); // convert float to char strg
			len = my_str.length();
			for (i=0; i<len; i++)
				data_strg[cmd_ptr++] = my_str[i];
			data_strg[cmd_ptr++] = '\0'; // zero end strg
			b = 1;	// Block Design, 1 Byte 1=TRUE or 0=FALSE
			data_strg[cmd_ptr++] = 'b';
			data_strg[cmd_ptr++] = b;
			byte_len = cmd_ptr - 8;  // msg size (less hdr)
		} else if ( req_str[0]=='d' && req_str[1]=='r' && my_ID=='d') { // generate data sting
			if (n > 0) {
				free (data_strg);	// clr any old data
				byte_len = (((n+31)/32) * 4); // round-up to 32-bit alignment 
				data_strg = (char *) calloc(byte_len+8, 1); 
				random_fill( (int *) &data_strg[8], byte_len/4);  // len in "int" size
			} else {
				printf("***ERROR*** no data size computed yet\n");
				exit(-1);
			}
			byte_len = ((n+7)/8); // recompute to correct Byte length
		} else if ( req_str[0]=='s' && req_str[1]=='r' && my_ID=='s') {	// generate seed sting
			seed_len = 0;		// assemble seed bit length from msg bytes
			for (i=4; i<8; i++) {	// limit to 32-bit length
				seed_len = (seed_len<<8) | req_str[i]&0xFF;
			}
			free (data_strg);	// clr any old data
			byte_len = (((seed_len+31)/32) * 4); // round-up to 32-bit alignment 
			data_strg = (char *) calloc(byte_len+8, 1);
			random_fill( (int *) &data_strg[8], byte_len/4);	// len in "int" size
			byte_len = ((seed_len+7)/8); // recompute to correct Byte length
		} else {
			printf("***ERROR*** unknown Request %c%c type\n", req_str[0], req_str[1]);
			exit(-1);
		}
		data_strg[0] = req_str[0];	// prep Response hdr
		data_strg[1] = ' ';
		data_strg[2] = 0;
		data_strg[3] = 0;
		for (i=4; i<8; i++) {	// pack msg length into hdr bytes
			data_strg[i] = (byte_len>>(8*(7-i)))&0xFF;
		}
			// send data in BLKs
		byte_len = byte_len + 8;	// add hdr size
		accum = 0;
		while (accum < byte_len) {	// send msg data to socket in BLK sized chunks
			if ( (byte_len-accum) > BLK_AMT) 	// compute remaining bytes
				incr = BLK_AMT; // limit RECV request to BLK_AMT
			else 
				incr  = byte_len - accum;	
         if ( (amt=send(socket_num, &data_strg[accum], incr, 0)) <= 0) {	// Send Response msg data
           perror("***ERROR rcving Socket data ");
           printf("***ERROR rcving data on socket=%d, got=%d of %d with last =%ld\n", socket_num, accum, byte_len, amt);
           exit(-1);
         }
			accum += amt;	// update accumlated data amt
		}
	} // end while
}

/* ---------------------------------------------------- */
/* server (Trevisan) request cmd line & parse it		  */
/* ---------------------------------------------------- */

void stream_get_cmd_line_bits(int socket_num, uint64_t *pp_n, uint64_t *pp_m, double *pp_eps, double *pp_alpha, bool *blk_des)
{
	int 		i, j, byte_ptr, param_num;
	uint64_t k, len, length_req;
	char		cmd_string[256];

	length_req  = 0;
	*pp_n = 0;	// set defaults
	*pp_m = 0;
	k = 0;
	if ( (len = socket_get_request_data_bits(socket_num, 'c', 'r', length_req, cmd_string)) < 14) {
		printf("Failed stream_get_cmd_line request rtn %ld, too little data\n", len);
	
	} else {
		byte_ptr = 1;
		param_num = cmd_string[0]&0xff;
		for (i=0; i<param_num; i++) {
			if (cmd_string[byte_ptr] == 'n') {	// num of input bits, 6 byte value
				byte_ptr++;
				for (j=0; j<6; j++) {
					*pp_n = (*pp_n<<8) | (cmd_string[byte_ptr]&0xFF);
					byte_ptr++;
				}
			} else if (cmd_string[byte_ptr] == 'a') {	// num of total entropy bits, 6 byte value
				byte_ptr++;
				for (j=0; j<6; j++) {
					k = (k<<8) | (cmd_string[byte_ptr]&0xFF);
					byte_ptr++;
				}
			} else if (cmd_string[byte_ptr] == 'm') {	// num of output bits requested, 6 byte value
				byte_ptr++;
				for (j=0; j<6; j++) {
					*pp_m = (*pp_m<<8) | (cmd_string[byte_ptr]&0xFF);
					byte_ptr++;
				}
			} else if (cmd_string[byte_ptr] == 'e') {	// eps, output error est, n byte value (char string of float value, zero termination)
				byte_ptr++;
				*pp_eps = atof(&cmd_string[byte_ptr]); // convert char string to float
				while (cmd_string[byte_ptr++] != '\0') // advance ptr past char string
					continue;
			} else if (cmd_string[byte_ptr] == 'b') {	// block design, 1 byte value
				byte_ptr++;
				if ( (cmd_string[byte_ptr]&0xFF) == 0)
					*blk_des = 0; 	// FALSE;
				else
					*blk_des = 1;	// TRUE;
				byte_ptr++;
			} else {
				printf("***ERROR*** unknown cmd param recvd %c(%x) \n", cmd_string[byte_ptr], cmd_string[byte_ptr]&0xFF );
				exit(-1);
			}
		}
	}
	if (*pp_n==0 || k==0) {
		printf("***ERROR*** no n-data size or a-total entropy cmd param recvd \n");
		exit(-1);
	}
printf("Trev rcvd %d param_num, k=%ld and n= %ld\n", param_num, k, *pp_n);
	*pp_alpha = (1.0*k+1.0)/(1.0*(*pp_n));  // compute Entropy rate 
}	
	
/* -------------------------------------------- */
/* -------------------------------------------- */

#define STREAM_PORT "5150" 

struct addrinfo host_info;       // The struct that getaddrinfo() fills up with data.
struct addrinfo *host_info_list; // Pointer to the to the linked list of host_info's.
/*
struct sockaddr {
    unsigned short    sa_family;    // address family, AF_xxx
    char              sa_data[14];  // 14 bytes of protocol address
};
*/
 
int start_server(int *socket_fd_data, int *socket_fd_seed, int *socket_fd_out)
{
	int clients_accepted, socketfd;
	char req_str[4], hdr_strg[4];
	
	memset(&host_info, 0, sizeof host_info); // clr struct
//	std::cout << "Setting up the structs..."  << std::endl;
	
	host_info.ai_family 	 = AF_UNSPEC;    	 // IP version not specified. Can be both.
	host_info.ai_socktype = SOCK_STREAM; 	 // Use SOCK_STREAM for TCP or SOCK_DGRAM for UDP.
	host_info.ai_flags   = AI_PASSIVE;	 	 // get addr suitable for binding
//	std::cout << "Addr="  << argv[1] << std::endl;
	int status = getaddrinfo(NULL, STREAM_PORT, &host_info, &host_info_list);
	if (status != 0) { 
		std::cout << "getaddrinfo error " << gai_strerror(status) ;
		exit(-1);
	} 
//	std::cout << "Creating a socket..."  << std::endl;
   socketfd = socket(host_info_list->ai_family, host_info_list->ai_socktype,host_info_list->ai_protocol);
   if (socketfd == -1) { 
		std::cout << "socket error " << std::endl;
		exit(-1);
	} 
//	std::cout << "Binding socket..."  << std::endl;
		// we make use of the setsockopt() function to make sure the port is not in use.
		// by a previous execution of our code. (see man page for more information)
	int yes = 1;
	status = setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	if (status == -1) {
		std::cout << "setsockopt error " << gai_strerror(status) << std::endl;
		std::cout << "setsockopt error" << std::endl ;  
		exit(-1);
	}
	status = bind(socketfd, host_info_list->ai_addr, host_info_list->ai_addrlen);
	if (status == -1) {
		std::cout << "bind error " << gai_strerror(status) << std::endl;
		std::cout << "bind error" << std::endl ;  
		exit(-1);
	}
//	std::cout << "Listen()ing for connections..."  << std::endl;
	status =  listen(socketfd, 5);
	if (status == -1) {
		std::cout << "listen error " << gai_strerror(status) << std::endl;
		std::cout << "listen error " << std::endl ;
		exit(-1);
	}
	
	clients_accepted = 0;
	while (clients_accepted < 3) {	// wait for all 3 clients to connect & verify
		int new_sd;
       struct sockaddr_storage their_addr;
       socklen_t addr_size = sizeof(their_addr);
       new_sd = accept(socketfd, (struct sockaddr *)&their_addr, &addr_size);
       if (new_sd == -1) {
           std::cout << "accept error" << std::endl ;
       } else {
           std::cout << "Connection accepted. Using new socketfd : "  <<  new_sd << std::endl;
       }
			// since all clients use the same Port number, send a request to determine client ID
			// and then assign this socket_fd to predefined global variables
		req_str[0] = '?';	// prep ID Request
		req_str[1] = 'r';
		req_str[2] = 0;
		req_str[3] = 0;
      if ( send(new_sd, req_str, 4, 0) != 4) {	// send ID Request
        perror("***ERROR sending Socket ID Request ");
        printf("***ERROR sending ID Request on socket=%d\n", new_sd);
        exit(-1);
      }
      if ( recv(new_sd, hdr_strg, 4, 0) != 4) {	// Rcv ID Response 
        perror("***ERROR rcving Socket ID Response ");
        printf("***ERROR rcving ID Response on socket=%d\n", new_sd);
        exit(-1);
      }
		if (hdr_strg[0]=='d' && hdr_strg[1]==' ') {
	   	if (*socket_fd_data != 0) {
	   		std::cout << "***DUP***, Data client already connected, Terminating " << std::endl ;  
	   		exit(-1);
	   	}
	   	*socket_fd_data = new_sd;  // Data/Cmd Line socket_fd
	   	clients_accepted++;	
	   	std::cout << "Data client now connected " << std::endl ;  
	   } else if (hdr_strg[0]=='s' && hdr_strg[1]==' ') {
			if (*socket_fd_seed != 0) {
				std::cout << "***DUP***, Seed client already connected, Terminating " << std::endl ;  
				exit(-1);
			}
			*socket_fd_seed = new_sd;	// Seed socket_fd
	   	clients_accepted++;	
	   	std::cout << "Seed client now connected " << std::endl ;  
	   } else if (hdr_strg[0]=='o' && hdr_strg[1]==' ') {
	   	if (*socket_fd_out != 0) {
	   		std::cout << "***DUP***, Output client already connected, Terminating " << std::endl ;  
	   		exit(-1);
	   	}
	   	*socket_fd_out = new_sd;	// Output data socket_fd
	   	clients_accepted++;	
	   	std::cout << "Output client now connected " << std::endl ;  
		} else {
	   	std::cout << "Unknown client of " << hdr_strg[0] << hdr_strg[1] << " type" << std::endl ;  
	   	exit(-1);
		}
	}	// end while
	std::cout << "All 3 clients are now connected " << std::endl ;  
}     
      
/* -------------------------------------------------------------------------- */

void stream_get_data_bits(uint64_t bit_len, int socket_num, uint64_t *data_array)
{
	uint64_t  len, byte_size;
	
	byte_size = ((bit_len + 63)/64) * 8; 	// round-up to next 64-bit-aligned Byte length
	if ( (len = socket_get_request_data_bits(socket_num, 'd', 'r', bit_len, (char *) data_array)) < byte_size ) {
		printf("Failed stream_get_data request rtn %ld Bytes when expecting %ld, too little data\n", len, byte_size);
		exit(-1);
	} 
printf("Trev rcvd %ld data bytes on socket=%d, u64[0]=0x%lx ", len,  socket_num, data_array[0]);
printf(", u64[%ld]=0x%lx, [-1]=0x%lx\n", ((byte_size+7)/8)-1, data_array[((byte_size+7)/8)-1],  data_array[((byte_size+7)/8)-2]);
}

/* -------------------------------------------------------------------------- */

void stream_get_seed_bits(uint64_t bit_len, int socket_num, uint64_t *data_array)
{
	uint64_t  len, byte_size;
	
	byte_size = ((bit_len + 63)/64) * 8; 	// round-up to next 64-bit-aligned Byte length
	if ( (len = socket_get_request_data_bits(socket_num, 's', 'r', bit_len, (char *) data_array)) < byte_size ) {
		printf("Failed stream_get_seed request rtn %ld Bytes when expecting %ld, too little data\n", len, byte_size);
		exit(-1);
	} 
printf("Trev requested %ld for d_bits %ld on socket=%d, rcvd %ld seed bytes, u64[0]=0x%lx ", byte_size, bit_len,  socket_num, len, data_array[0]);
printf(", u64[%ld]=0x%lx, [-1]=0x%lx\n", ((byte_size+7)/8)-1, data_array[((byte_size+7)/8)-1],  data_array[((byte_size+7)/8)-2]);
}

/* -------------------------------------------------------------------------- */

void stream_send_output_bits(uint64_t bit_len, int socket_num, int *data_array)
{
	uint64_t  len, byte_size;
	
	byte_size = ((bit_len + 31)/32) * 4; 	// round-up to next 32-bit-aligned Byte length
	if ( (len = socket_send_output_data_bits(socket_num, 'o', 'r', bit_len, (char *) data_array)) < byte_size ) {
		printf("Failed stream_send_output request rtn %ld Bytes when expecting %ld, too little data\n", len, byte_size);
		exit(-1);
	} 
printf("Trev sent %ld Bytes for %ld bits on socket=%d, sent %ld output bytes, u32[0]=0x%x \n", byte_size, bit_len, socket_num, len, data_array[0]);
printf(", u32[%ld]=0x%x, [-1]=0x%x\n", (byte_size+3)/4, data_array[((byte_size+3)/4)-1], data_array[((byte_size+3)/4)-2]);
}

/* -------------------------------------------- */
/* -------------------------------------------- */

void pack_val(char *c_strg, uint64_t val)
{
	c_strg[0] = (val>>40)&0xFF;
	c_strg[1] = (val>>32)&0xFF;
	c_strg[2] = (val>>24)&0xFF;
	c_strg[3] = (val>>16)&0xFF;
	c_strg[4] = (val>>8 )&0xFF;
	c_strg[5] = (val    )&0xFF;
		// for (i=0; i<6; i++) {
		//		hdr_strg[i] = ((val>>(8*(5-i))&0xFF;
}

/* -------------------------------------------- */

void unpack_val(char *c_strg, uint64_t *val)
{
	int i;
	
	*val = 0;		// assemble msg length from bytes
	for (i=0; i<6; i++) 
		*val = (*val<<8) | c_strg[i]&0xFF;
}

/* -------------------------------------------- */
/* -------------------------------------------- */

void stream_info(int fd, int data_len)
{
	;
}

/* -------------------------------------------- */

void stream_output(int fd, int data_len)
{
	;
}
