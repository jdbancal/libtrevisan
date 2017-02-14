/* ------------------------------------------------------- */
// fct calls for the Trevisan streaming option located 
// in stream_ops.cc
/* ------------------------------------------------------- */

#ifndef STREAM_H
#define STREAM_H

string my_ftoa(double value); 
void stream_get_cmd_line_bits(int socket_num, uint64_t *pp_n, uint64_t *pp_m, double *pp_eps, double *pp_alpha, bool *blk_des);
uint64_t socket_get_request_data_bits(int socket_num, char char1, char char2, uint64_t length_req, char *data_array);
uint64_t socket_send_output_data_bits(int socket_num, char char1, char char2, uint64_t length_req, char *data_array);
int start_server(int *socket_fd_data, int *socket_fd_seed, int *socket_fd_out);
void stream_get_data_bits(uint64_t bit_len, int socket_num, uint64_t *data_array);
void stream_get_seed_bits(uint64_t bit_len, int socket_num, uint64_t *data_array);
void stream_send_output_bits(uint64_t bit_len, int socket_num, int *data_array);

#endif