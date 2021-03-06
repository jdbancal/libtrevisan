# LibTrevisian

## Introduction

A modular, extensible and high-performance Trevisan extractor library. See [arXiv:1212.0520](https://arxiv.org/abs/1212.0520) for a detailed description.

## Compilation

This program relies on a number of extra libraries. Installation commands are provided below for ubuntu-type systems (>= 18.04).

 * First, a compiler and git are needed : `sudo apt install g++ git`
 * To activate support for libraries described in `/etc/ld.so.conf`, run `sudo ldconfig`
 * To install GMP: `sudo apt install libgmp-dev`
 * We need a specific version of OpenSSL: it can be downloaded with `cd ..; git clone -b OpenSSL_1_0_2-stable https://github.com/openssl/openssl`. To prepare it, run commands `./config` and `make` within the created folder. Keep this folder (it will be used during the compilation of LibTrevisian)
 * Install `gf2x`, `NTL` and `Tclap` libraries on the system:
	- `sudo apt install libgf2x-dev libntl-dev libtclap-dev`
	- If you rather need to do this manually:
		- Download [gf2x-1.2](https://gforge.inria.fr/frs/download.php/file/36934/gf2x-1.2.tar.gz) somewhere. Install it with the commands `./configure`, `make`, `make check`, `sudo make install`, `sudo ldconfig` within the downloaded folder. This folder can then be deleted.
		- Download [NTL](http://www.shoup.net/ntl/doc/tour-unix.html) somewhere. Install with the commands `cd src`, `./configure NTL_GF2X_LIB=on`, `make`, `make check`, `sudo make install`. Delete the NTL folder.
		- Download [Tclap](http://tclap.sourceforge.net/) somwehere. Install by running `mkdir build`, `cd build`, `cmake ..`, `cmake --build .`, `sudo cmake --install` within the downloaded folder. Then delete this folder.
		- Upon success, the downloaded folders of g2fx, NTL and Tclap can be removed.
 * Rcpp, RInside, and TBB are needed to enable block weak design: they can be installed with the command `sudo apt install r-cran-devtools r-cran-rcpp r-cran-rinside libtbb-dev`

After these libraries are installed, simply run `make` within the libtrevisian folder. This should create the targets `extractor` and `gen_irreps`.

## Optional

Several additional rules are available in the Makefile. Here is additional software needed to run them

 * `make src-pdf` needs enscript. It can be installed with the command `sudo apt install enscript`
 * `make figures` needs some R libraries. To install them, run the command following commands:

 ```
     sudo R
     install.packages("ggplot2")
```



