# LibTrevisian

## Introduction

A modular, extensible and high-performance Trevisan extractor library. See [arXiv:1212.0520](https://arxiv.org/abs/1212.0520) for a detailed description.

## Compilation

This program relies on a number of extra libraries. Installation commands are provided below for ubuntu-type systems (>= 18.04).

 * First, a compiler and git are needed : `sudo apt install g++ git`
 * To activate support for libraries described in `/etc/ld.so.conf`, run `sudo ldconfig`
 * To install GMP: `sudo apt install libgmp-dev`
 * We need a specific version of OpenSSL: it can be downloaded with `cd ..; git clone -b OpenSSL_1_0_2-stable https://github.com/openssl/openssl`. To prepare it, run commands `./config` and `make` within the created folder. Keep this folder (it will be used during the compilation of LibTrevisian)
 * [gf2x](http://www.shoup.net/ntl/doc/tour-gf2x.html), install with the commands `./configure`, `make`, `make check`, `sudo make install`, `sudo ldconfig` within the downloaded folder
 * [NTL](http://www.shoup.net/ntl/doc/tour-unix.html), install with the commands `cd src`, `./configure NTL_GF2X_LIB=on`, `make`, `make check`, `sudo make install`
 * [Tclap](http://tclap.sourceforge.net), download and then run `./configure`, `make`, `make check` and `sudo make install` within the created folder.
  * Upon success, the downloaded folders of g2fx, NTL and Tclap can be removed.
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



