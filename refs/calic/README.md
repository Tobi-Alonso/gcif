## Modifications included in this fork
- Some support to near-lossless compression (the near paramenter is not yet included in the compress image header, then it needs to be explicitly provided to the decoder. the impact in avg compression is negligible )
- Some performance optimizations


## Original README
This is my attempt to implement CALIC based on the following paper:

  Wu, Xiaolin and Memon, Nasir,"Context-Based, Adaptive, Lossless Image Coding", 
  IEEE Transactions on Communications, Vol. 45, No. 4, April 1997.

This implemention of CALIC algorithm is solely targeted for research and experimental 
purpose.

The compression gain achieved by this codec is within +/- small percentage of
what the 'actual' CALIC (available via ftp://ftp.csd.uwo.ca/pub/from_wu/) achieves. 

Possible causes of small offset in compression gains are:
  1. Different approaches used to encode/decode border pixels.
  2. Incorrect implementation of CALIC algorithm 
     (most likely in 'entropy coding of pixels in binary mode').
  3. Some part of CALIC has not yet been implemented.
  4. Different parameters setting.

Use this image codec at your on risk. I will not hold responsibilities for the
result produced by this codec. 

-------------------------------------------------------------------------------------

There package contains two executables: 'encode' (encoder) and 'decode' (decoder).

The encoder takes 8-bit PGM image (grayscale image) as input and produces a compressed 
version of the image. The decoder takes the compressed image and produces 8-bit PGM image.

Usage (for encoding image):
 
  encode -in <input: PGM image> -o <output: encoded image> [options]
 
Usage (for decoding image):
	
  decode -in <input: encoded image> -o <output: PGM image> [options]


Note: Some of the codes are irrelavant to CALIC. 
--------------------------------------------------------------------------------------
About the arithmetic codec:

The following files contain source code ported from third party source:

	AdaptiveModel.cc
	Arithm.h
	ArithmeticDecoder.cc
	ArithmeticEncoder.cc

The file arith.README is the README file that comes with the original source.
