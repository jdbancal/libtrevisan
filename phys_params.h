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

#ifndef PHYS_PARAMS_H
#define PHYS_PARAMS_H

// All physical parameters if the Trevisan process are collected here.
struct phys_params {
	uint64_t n; // Input length (bits)
	uint64_t m; // Output length (bits)

	 uint64_t m_req; // added variable for Output length for Block design (bits)
	 uint64_t l;     // added variable for half sub-seed length
	 uint64_t t_req; // added variable for sub-seed length = 2*l
	 uint64_t t_pri; // added variable for next prime of sub-seed length
	 uint64_t d;     // added variable for size of seed pool for gfp or single Blk (t_pri^2)
	 uint64_t d_req; // added variable for used size of seed pool for gfp or single Blk (t_pri*t_req)
	 uint64_t d_tot; // added variable for used size of total seed pool for gfp or single Blk (a+1)*(t_pri*t_req)
	 double   r;     // added variable for sub-seed overlap value
	 uint64_t a;     // added variable for additional Blks for Blk design (total =(a+1)*d)
	 uint64_t max_entropy;  // added variable to store max_entropy
//	 vector<uint32_t>  mi;	// added ptr to output per Block in Blk design
//	 vector<uint32_t>  base_mi; // added ptr to starting output base per Block in Blk design
	 void *  mi_ptr;      // added ptr to per Block output in Blk design
	 void *  base_mi_ptr; // added ptr to starting output base per Block in Blk design
	
	double alpha; // Source entropy ratio
	double eps;   // 1-bit error probability
	
	std::string i;  // added variable to store source bits file name
	std::string q;  // added variable to store seed file name
	std::string of; // added variable to store output data file name

	// Parameters for specific extractors (see the paper for details)
	double lu_nu;
};

#endif
