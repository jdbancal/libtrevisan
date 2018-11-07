/* -------------------------------------------------------------------------- */
// This file contains the Client fcts to test the Trevisan server.
// It operate in a continuous loop, kill the Trevisan server to stop
// evrything. Killing a client leaves all the rest hung waiting for data.
// this code can fct as any 1 of the 3 clients by passing its ID "d", 
// "s" or "o" via the cmd_line params on XEQ.
// This code sends the same cmd_line paramsto the Trevisan server, but 
// generates new weak data and seed data for each loop. 
// The output data is rcvd and discarded.
//
// Compile & XEQ guide
//
// Win7  Compile: gcc -DWIN7 -o stream_tst_data stream_tst_data.c -lws2_32
// Linux Compile: gcc -o stream_tst_data stream_tst_data.c 
//
// XEQ:  stream_tst_data -t [d,s,o] [-ip ip_addr/DNS_name]
/* -------------------------------------------------------------------------- */

TODO : Add a rule in the Makefile to compile this program

#ifdef WIN7		// if WIN7
#define _WIN32_WINNT 0x501
#include <ws2tcpip.h>
#include <winsock2.h>
#endif // WIN7

#ifndef WIN7	// else LINUX
#include <sys/socket.h> // Needed for the socket functions
#include <netdb.h>      // Needed for the socket functions
#include <netinet/in.h>
#endif // LINUX

#include <string.h>
#include <stdio.h>	// printf
#include <stdlib.h>  // exit
#include <stdint.h> // uint64_t

#define STREAM_PORT "5150" 

struct addrinfo host_info;       // The struct that getaddrinfo() fills up with data.
struct addrinfo *host_info_list; // Pointer to the to the linked list of host_info's.
struct sockaddr *addr_info;
struct hostent *old_host_struct;
char **list;

/* the following is only for Socket structs info

	struct addrinfo {
		int              ai_flags;
		int              ai_family;
		int              ai_socktype;
		int              ai_protocol;
		socklen_t        ai_addrlen;
		struct sockaddr *ai_addr;
		char            *ai_canonname;
		struct addrinfo *ai_next;
	};
	
	struct sockaddr {
		unsigned short    sa_family;    // address family, AF_xxx
		char              sa_data[14];  // 14 bytes of protocol address
	};
	
	struct hostent {
		char  *h_name;            // official name of host 
		char **h_aliases;         // alias list 
		int    h_addrtype;        // host address type 
		int    h_length;          // length of address 
		char **h_addr_list;       // list of addresses 
	}
*/ 
 
/* ------------------------------------------------------- */

int start_client(addr_strg, my_ID)
char *addr_strg;
char my_ID;
{
	int  status, i, socket_fd, new_sd;
	char hdr_strg[4], req_str[4];
	
#ifdef WIN7	// WIN7
	WSADATA wsaData;
	WSAStartup(0x0202, &wsaData);
#endif // WIN7

	memset(&host_info, 0, sizeof(host_info) ); // clr struct
	printf("Setting up the structs...\n");
	
#ifdef WIN7	// WIN7
	if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) { 
			fprintf(stderr,"WSAStartup() failed"); 
			exit(1); 
	}
#endif // WIN7

	host_info.ai_family 	 = AF_UNSPEC;    	 // IP version not specified. Can be both.
	host_info.ai_socktype = SOCK_STREAM; 	 // Use SOCK_STREAM for TCP or SOCK_DGRAM for UDP.
	printf("Connect Addr=%s\n", addr_strg );
	status = getaddrinfo(addr_strg, STREAM_PORT, &host_info, &host_info_list);
	printf("status=%d\n", status );
	if (status != 0) {
		printf("getaddrinfo error%s\n", gai_strerror(status) );
	} else {
		printf("len=%d\n", host_info_list->ai_addrlen );
		printf("cannon=%s\n", host_info_list->ai_canonname );
		addr_info = host_info_list->ai_addr;
		if (addr_info == NULL) {
			printf("addr_info is a NULL ptr\n");
			exit(-1);
		}
		printf("rtn addr=");
		for (i=0; i<14; i++) {
			printf("%d ", addr_info->sa_data[i]&0xFF);
		}
		printf("\n");
	}
	printf("Creating a socket...\n");
	socket_fd = socket(host_info_list->ai_family, host_info_list->ai_socktype, 
	host_info_list->ai_protocol);
	if (socket_fd == -1) {
		printf("socket error \n");
		exit(-1);
	}
	printf("Connect()ing...\n"); 
	status = connect(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
	if (status == -1) {
      perror("***ERROR connection fails ");
 		printf("connect error \n");
		exit(-1);
	}
   if ( recv(socket_fd, hdr_strg, 4, 0) != 4) {	// Rcv ID  Request
     perror("***ERROR rcving Socket ID Response ");
     printf("***ERROR rcving ID Response on socket=%d\n", socket_fd);
     exit(-1);
   }
	if (hdr_strg[0] != '?' && hdr_strg[1] != 'r') {
     printf("***ERROR unknown ID Request rcvd =%c%c\n", hdr_strg[0], hdr_strg[1]);
     exit(-1);
	}
	req_str[0] = my_ID;	// prep ID Request
	req_str[1] = ' ';
	req_str[2] = 0;
	req_str[3] = 0;
   if ( send(socket_fd, req_str, 4, 0) != 4) {	// send ID Response
     perror("***ERROR sending Socket ID Request ");
     printf("***ERROR sending ID Request on socket=%d\n", socket_fd);
     exit(-1);
   }
	return (socket_fd);
}

/* ------------------------------------------------------- */
/* fill uint64 array "data_strg" of "len" with random data */
/* ------------------------------------------------------- */

void random_fill_ordered(uint64_t *data_strg, int len)
{
	int i;
	
	for (i=0; i<len; i++) {
		data_strg[i] = (rand() & 0xFFFFFFFF);
		data_strg[i] = (data_strg[i]<<32) | (rand() & 0xFFFFFFFF);
	}
}

/* ------------------------------------------------------- */
/* build mask to keep upper part of partial 64-bit word    */
/* ------------------------------------------------------- */

uint64_t part_mask(uint64_t size){
	int loc = size % 64;
	uint64_t mask = 0LLU;  // init to zero, so if 64-bit aligned then mask is all 1's
	if (loc != 0) {
		mask = (1LLU<<(64-loc));
		mask -= 1; // mask for lower bits
	}
	mask = ~mask; // invert bits to get mask to clr out unwanted lower bits, if any
	return (mask);
}

/* -------------------------------------------- */

#define FOREVER 	1
#define BLK_AMT	64*1024

/* -------------------------------------------- */

void socket_send_response_data_bits(int socket_num, char my_ID)
{	
	char req_str[8], hdr_strg[8], *data_strg;
	int 		i, len, accum, n, a, m, bit_len;
	size_t   incr;	// ~unsigned int
	ssize_t  amt;	// signed size_t
	int 	   msg_cntent_len, cmd_ptr, b, byte_len, seed_len, output_len;
	double 	e;
	int 		*wrd_data;
	uint64_t	*u64_data;
	
	data_strg = calloc(256, 1);	// dummy allocation for initial "free" call
	while (FOREVER) {
      if ( (len=recv(socket_num, req_str, 8, 0)) != 8) {	// wait for Request 
        perror("***ERROR rcving Socket Info Request ");
        printf("***ERROR rcving Info Request on socket=%d, len=%d\n", socket_num, len);
        exit(-1);
      }
		if ( req_str[0]=='o' && req_str[1]=='r' && my_ID=='o') {	// capture output sting
				// Need to consider output data Byte ordering if other than x86 architectures
			output_len = 0;		// assemble seed bit length from msg bytes
			for (i=4; i<8; i++) {	// limit to 32-bit length
				output_len = (output_len<<8) | req_str[i]&0xFF; // len in Bytes
			}
				// get output data in BLKs
			free (data_strg);	// clr any old data
			byte_len = (((output_len+31)/32) * 4); // round-up to 32-bit alignment 
			data_strg = calloc(byte_len, 1); 		// no hdr just data
			wrd_data = (int *) &data_strg[0];
			accum = 0;
				// this is where a buffer (data pool) should be inserted to store data for other Appl 
			while (accum < byte_len) {	// send msg data to socket in BLK sized chunks
				if ( (byte_len-accum) > BLK_AMT) 	// compute remaining bytes
					incr = BLK_AMT; // limit RECV request to BLK_AMT
				else 
					incr  = byte_len - accum;	
				if ( (amt=recv(socket_num, &data_strg[accum], incr, 0)) <= 0) {	// Send Response msg data
					perror("***ERROR rcving output data ");
					printf("***ERROR rcving output on socket=%d, got=%d of %d with last =%d\n", socket_num, accum, byte_len, amt);
					exit(-1);
				}
				accum += amt;	// update accumlated data amt
			}
				// This is where one should Insert code for these output bits into a bit pool or process them for another Appl
// printf("***RCVD Output HRD on socket=%d, len=%d\n", socket_num, byte_len);
			data_strg[0] = req_str[0];	// prep ACK hdr
			data_strg[1] = 'a';			//'a' for ACK
			data_strg[2] = 0;
			data_strg[3] = 0;
			for (i=4; i<8; i++) {	// pack ACK length into hdr bytes
				data_strg[i] = ( output_len>>(8*(7-i)) )&0xFF;
			}
			if ( (i=send(socket_num, data_strg, 8, 0)) != 8) {	// send output data ACK
				perror("***ERROR sending Socket output data ACK ");
				printf("***ERROR sending output data ACK on socket=%d, len=%d\n", socket_num, i);
				exit(-1);
			}
printf("O Done, Waiting for next Request\n");
			continue;  // get next request
		}
// cmd params in cmd info, 1 byte param ID, followed by data, if any:
// n  - data size + 6 byte value
// a  - total entropy + 6 byte value (total bits possible, use to comput Entropy rate) params.pp.alpha = a/n (float)
// m  - output size + 6 byte value
// e  - eps, output error  + n byte value (char string of float value, zero termination)
// b  - block design + 1 byte value
		if ( req_str[0]=='c' && req_str[1]=='r' && my_ID=='d') { 			// generate cmd line data
				// Cmd Line data explicitly Byte ordered & is independent of CPU architecture
			free (data_strg);	// clr any old data
			byte_len = 256; 	// should be more than enough for 5 params, if more are added consider increasing
			data_strg = calloc(byte_len, 1);
			cmd_ptr = 9;
			data_strg[8] = 5;  // 5 params being sent
			n = 2000017; // data size in bits
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
// printf("cmd_ptr %d bytes, before e\n", cmd_ptr);
			e = 1.54e-5;  // float value
			data_strg[cmd_ptr++] = 'e';
			len = sprintf(&data_strg[cmd_ptr], "%14.12f", e);
			if (len <= 0) {
				printf("***ftoa failed, terminating\n");
				exit(-1);
			}
			cmd_ptr += (len+1);
// printf("cmd_ptr %d bytes, after e\n", cmd_ptr);
//			std::string my_str = my_ftoa(e); // convert float to char strg
//			len = my_str.length();
//			for (i=0; i<len; i++)
//				data_strg[cmd_ptr++] = my_str[i];
//			data_strg[cmd_ptr++] = '\0'; // zero end strg
			b = 1;	// Block Design, 1 Byte 1=TRUE or 0=FALSE
			data_strg[cmd_ptr++] = 'b';
			data_strg[cmd_ptr++] = b;
// printf("cmd_ptr %d bytes, end cmd\n", cmd_ptr);
			cmd_ptr	= ((cmd_ptr+7)/8) * 8; 	// round up to 64-bit alignment
			byte_len = cmd_ptr - 8;  	// msg size (less hdr)
			bit_len  = byte_len*8;	
printf("sending %d cmd bytes, bit_len=%d\n", byte_len, bit_len);
		} else if ( req_str[0]=='d' && req_str[1]=='r' && my_ID=='d') { // generate data sting
			if (n > 0) {	// Need to consider data Byte ordering if other than x86 architectures
				free (data_strg);	// clr any old data
				bit_len  = n;
				byte_len = (((n+63)/64) * 8); // round-up to 64-bit word alignment 
				data_strg = calloc(byte_len+8, 1); 			// 8 Bytes for hdr
				u64_data = (uint64_t *) &data_strg[8];	
				random_fill_ordered(u64_data, byte_len/8);
				u64_data[(byte_len/8)-1] &= part_mask(n);  // clr out "empty" lower bits when bit len is not aligned at 64
			} else {
				printf("***ERROR*** no data size computed yet\n");
				exit(-1);
			}
printf("sending %d data bits, u64[0]=0x%lx\n", byte_len, u64_data[0]);
printf(" , u64[%d]=0x%lx , u64[-1]=0x%lx \n", (byte_len/8)-1, u64_data[(byte_len/8)-1], u64_data[((byte_len+7)/8)-2] );
		} else if ( req_str[0]=='s' && req_str[1]=='r' && my_ID=='s') {	// generate seed sting
				// Need to consider seed Byte ordering if other than x86 architectures
			seed_len = 0;		// assemble seed bit length from msg bytes
			for (i=4; i<8; i++) {	// limit to 32-bit length
				seed_len = (seed_len<<8) | req_str[i]&0xFF; // len in Bytes
			}
			free (data_strg);	// clr any old data
			bit_len  = seed_len;
			byte_len = (((seed_len+63)/64) * 8); // round-up to 64-bit alignment 
			data_strg = calloc(byte_len+8, 1); 			// 8 Bytes for hdr
			u64_data = (uint64_t *) &data_strg[8];	
			random_fill_ordered(u64_data, byte_len/8);
			u64_data[(byte_len/8)-1] &= part_mask(seed_len);  // clr out "empty" lower bits when bit len is not aligned at 64
printf("sending %d seed bits, u64[0]=0x%lx\n", byte_len, u64_data[0]);
printf(" , u64[%d]=0x%lx , u64[-1]=0x%lx \n", (byte_len/8)-1, u64_data[(byte_len/8)-1], u64_data[((byte_len+7)/8)-2] );
		} else {
			printf("***ERROR*** unknown Request %c%c type\n", req_str[0], req_str[1]);
			exit(-1);
		}
		data_strg[0] = req_str[0];	// prep Response hdr
		data_strg[1] = ' ';
		data_strg[2] = 0;
		data_strg[3] = 0;
		for (i=4; i<8; i++) {	// pack msg length into hdr bytes
			data_strg[i] = ( bit_len>>(8*(7-i)) )&0xFF;
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
           printf("***ERROR rcving data on socket=%d, got=%d of %d with last =%d\n", socket_num, accum, byte_len, amt);
           exit(-1);
         }
			accum += amt;	// update accumlated data amt
		}
printf("%c Done, Waiting for next Request\n", req_str[0]);
	} // end while
}
/* -------------------------------------------- */
/*     main()                                   */
/* -------------------------------------------- */

main(int argc, char *argv[])
{
	char  my_ID, *Server_IP;
	int	socket_fd, argcCount;

 	 my_ID = 'd'; 	// default data type
	 Server_IP = "andrea.nist.gov"; // default server addr
	 if (argc >= 2) {
     		argcCount = 1;
     		   // keep extracting params from the cmd line
				// -t <type_char - d, s or o>
				// -ip <Tervisan_server_addr_strg>
      	while(argcCount < argc) {
//      		printf("argcCount=%d\n", argcCount);
				if (argv[argcCount][0] == '-' && argv[argcCount][1] == 't' ) {
               my_ID = argv[argcCount+1][0];		// specify ID type
               argcCount += 2;
					printf("new ID type =%c\n", my_ID);
					if (my_ID!='d' && my_ID!='s' && my_ID!='o') {
						printf("***ERROR incorrect ID type\n"); 
						exit(-1);
					}						
            } else if (argv[argcCount][0] == '-' && argv[argcCount][1] == 'i' && argv[argcCount][2] == 'p') {
              	Server_IP = argv[argcCount+1];    // IP Address or DNS name
              	argcCount += 2;
              	printf("Alice_IP_addr=%s\n", Server_IP);
            } else  {
              		printf("INCORRECT Param usage:\n");
              		printf("%s <-t ID_char>  <-ip IP Address or DNS name>\n", argv[0]);
              		exit(0);
        		}
			} // end WHILE
      } else  {
         	printf("INCORRECT Param usage:\n");
         	printf("%s <-t ID_char>  <-ip IP Address or DNS name>\n", argv[0]);
         	exit(0);
   	}	// end IF

	socket_fd = start_client(Server_IP, my_ID); // open socket for data stream to Trevisan
	
	socket_send_response_data_bits(socket_fd, my_ID); // simple response fct, acting as spified ID

}
