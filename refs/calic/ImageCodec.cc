#include "ImageCodec.h"
// #include <sys/time.h>
#include <time.h>

/*************************************************************************

 Constructor: Initialize the codec.

------------------------------------------------------------------------*/
ImageCodec::ImageCodec() {
  symbolCount = DEFAULT_SYMBOL_COUNT;

  enc = NULL;
  dec = NULL; 
  debug = false;

  verbose = false;

  errImg = NULL;
  srcImg = NULL;
  refImg = NULL;
}

/************************************************************************

 Destructor.

------------------------------------------------------------------------*/
ImageCodec::~ImageCodec() {

  delete enc;
  delete dec;
  delete inFilename;
  delete outFilename;
  delete refFilename;
  delete srcImg;
  delete errImg;
  delete refImg;
}

/************************************************************************
  
  Provides programmer a convenient way to read reference image.

------------------------------------------------------------------------*/
void ImageCodec::readReferenceImage() {

  ASSERT(refFilename != NULL, "Reference image filename has not been specified.");
  ASSERT(dec != NULL, "Image decoder has not yet been initialize.");

  delete refImg;
  refImg = readPGMImage(refFilename);

  ASSERT(refImg != NULL, "Fail to read reference image.");
  ASSERT(refImg->w == dec->getWidth() && refImg->h == dec->getHeight(),
         "Input image and reference image have difference dimension.");
}

/************************************************************************

  Method: int ImageCodec::encode(char *filename);

  Return:
    Return 0 upon sucessful encoding. Otherwise return -1.

  Note: Source image (input) is specified using either setSourceImage() or
        readSourceImage().

  Description:
    Encode the source image and write the encoded image to a file called 
    'filename'.

------------------------------------------------------------------------*/
int ImageCodec::encode() {
  ASSERT(inFilename != NULL, "Input filename has not been specified.");
  ASSERT(outFilename != NULL, "Output filename has not been specified.");
  
  timespec fin,ini;
  clock_gettime(CLOCK_MONOTONIC, &ini);
    
  encInit();

  ASSERT(enc != NULL, "ImageEncoder is NULL");
  ASSERT(dec != NULL, "ImageDecoder is NULL");

  enc->startEncoding();
  dec->startDecoding();

  startEncode();

  dec->stopDecoding();
  enc->stopEncoding();

  encFinalize();

  clock_gettime(CLOCK_MONOTONIC, &fin);
   
  
  float enc_time = ((fin.tv_sec+fin.tv_nsec* 1E-9)-(ini.tv_sec+ini.tv_nsec* 1E-9));
  float enc_bw = dec->getWidth()*dec->getHeight()/(1024*1024*enc_time);
  printf("Encoder time: %.3f | BW: %.3f MP/s | \n", enc_time,enc_bw );

  return 0;
}

/************************************************************************

  Method: int ImageCodec::decode(char *filename);
  Parameters:
    filename: name of the file to be decoded.

  Return:
    Return 0 upon sucessful decoding. Otherwise return -1.

  Note: The decoded image will not be written to a file directly.
        After successful decoding, user will need to call writeSourceImage() 
        to write the decoded image to a file or
        retrieve the decoded image using getSourceImage().

  Description:
    Decode the file named 'filename' and keep the decoded image in 'src'.

------------------------------------------------------------------------*/
int ImageCodec::decode() {

  ASSERT(inFilename != NULL, "Input filename has not been specified.");
  ASSERT(outFilename != NULL, "Output filename has not been specified.");

  timespec fin,ini;
  clock_gettime(CLOCK_MONOTONIC, &ini);

  decInit();

  ASSERT(enc != NULL, "ImageEncoder is NULL");
  ASSERT(dec != NULL, "ImageDecoder is NULL");

  enc->startEncoding();
  dec->startDecoding();

  startDecode();

  dec->stopDecoding();
  enc->stopEncoding();

  decFinalize();

  clock_gettime(CLOCK_MONOTONIC, &fin);
   
  
  float elap_time = ((fin.tv_sec+fin.tv_nsec* 1E-9)-(ini.tv_sec+ini.tv_nsec* 1E-9));
  float comp_bw = dec->getWidth()*dec->getHeight()/(1024*1024*elap_time);
  printf("Decoder time: %.3f | BW: %.3f MP/s | \n", elap_time,comp_bw );

  return 0;
}

/************************************************************************

  Method: void ImageCodec::startEncode();

------------------------------------------------------------------------*/
void ImageCodec::startEncode() {

  int imgW = dec->getWidth();
  int imgH = dec->getHeight();

  int x, y; 		// Location of the current pixel in srcImg and errImg.

  delete srcImg;
  delete errImg;

  byte *srcBuf;
  srcBuf = new byte [imgW * imgH];
  srcImg = new ByteImage(srcBuf, imgW, imgH);
 
  short *errBuf;
  errBuf = new short [imgW * imgH];
  errImg = new ShortImage (errBuf, imgW, imgH);

  //----------------------------- First row --------------------------------
  setPredicted(predict_0_0());
  encode_0_0();
  setPredicted(predict_1_0());
  encode_1_0();
  for (x = 2; x < imgW; x++) {
    setPredicted(predict_x_0(x));
    encode_x_0(x);
  }

  //----------------------------- Second row --------------------------------
  setPredicted(predict_0_1());
  encode_0_1();
  setPredicted(predict_1_1());
  encode_1_1();
  for (x = 2; x < imgW - 2; x++) {
    setPredicted(predict_x_1(x));
    encode_x_1(x);
  }
  setPredicted(predict_2ndLast_1());
  encode_2ndLast_1();
  setPredicted(predict_Last_1());
  encode_Last_1();

  //----------------------------- All except the first two rows  --------------------------------
  for (y = 2; y < imgH; y++) {
    setPredicted(predict_0_y(y));
    encode_0_y(y);
    setPredicted(predict_1_y(y));
    encode_1_y(y);
    for (x = 2; x < imgW - 2; x++) {
      setPredicted(predict_x_y(x, y));
      encode_x_y(x, y);
    }
    setPredicted(predict_2ndLast_y(y));
    encode_2ndLast_y(y);
    setPredicted(predict_Last_y(y));
    encode_Last_y(y);
  }
}

/************************************************************************

  Method: void ImageCodec::startDecode();

------------------------------------------------------------------------*/
void ImageCodec::startDecode() {

  int imgW = dec->getWidth();
  int imgH = dec->getHeight();

  int x, y; 		// Location of the current pixel in srcImg and errImg.

  delete srcImg;
  delete errImg;

  byte *srcBuf;
  srcBuf = new byte [imgW * (imgH + 1)];
  srcImg = new ByteImage(srcBuf, imgW, imgH);
 
  short *errBuf;
  errBuf = new short [imgW * (imgH + 1)];
  errImg = new ShortImage (errBuf, imgW, imgH);

  //----------------------------- First row --------------------------------
  setPredicted(predict_0_0());
  decode_0_0();
  setPredicted(predict_1_0());
  decode_1_0();
  for (x = 2; x < imgW; x++) {
    setPredicted(predict_x_0(x));
    decode_x_0(x);
  }

  if (imgH < 2)
    return;

  //----------------------------- Second row --------------------------------
  setPredicted(predict_0_1());
  decode_0_1();
  setPredicted(predict_1_1());
  decode_1_1();
  for (x = 2; x < imgW - 2; x++) {
    setPredicted(predict_x_1(x));
    decode_x_1(x);
  }
  setPredicted(predict_2ndLast_1());
  decode_2ndLast_1();
  setPredicted(predict_Last_1());
  decode_Last_1();

  //----------------------------- All except the first two rows  --------------------------------
  for (y = 2; y < imgH; y++) {
    setPredicted(predict_0_y(y));
    decode_0_y(y);
    setPredicted(predict_1_y(y));
    decode_1_y(y);
    for (x = 2; x < imgW - 2; x++) {
      setPredicted(predict_x_y(x, y));
      decode_x_y(x, y);
    }
    setPredicted(predict_2ndLast_y(y));
    decode_2ndLast_y(y);
    setPredicted(predict_Last_y(y));
    decode_Last_y(y);
  }
}

int encRemapError(bool negativeModel, int symbolCount, int error, int predicted) {

  if (negativeModel)
    error = -error;

  int halfSymbolCount = (symbolCount >> 1) - 1;
  int absError = abs(error);

  if (error != 0) {
    if (predicted <= halfSymbolCount) {
      if (absError <= predicted)
        error = (error < 0) ? -2 * error : (error << 1) - 1;
      else
        error = predicted + absError;
    }
    else {
      if (absError <= (symbolCount - 1 - predicted))
        error = (error < 0) ? -2 * error : (error << 1) - 1; 
      else
        error = symbolCount - 1 - predicted + absError;
    }
  }

  return error;
}

int decRemapError(bool negativeModel, int symbolCount, int error, int predicted) {

  int halfSymbolCount = (symbolCount >> 1) - 1;

  if (error != 0) {
    if (predicted <= halfSymbolCount) {
      if ((2 * predicted) >= error)
        error = (error % 2 == 0) ? - (error >> 1) : (error + 1) >> 1;
      else  {
        if (negativeModel)
          error = predicted - error;	// - (error - predicted)
        else
          error = error - predicted;
      }
    }
    else {
      if ((2 * ((symbolCount - 1)- predicted)) >= error)
        error = (error % 2 == 0) ? - (error >> 1) : (error + 1) >> 1;
      else  {
        if (negativeModel)
          error = error + predicted - symbolCount + 1;
        else
          error = symbolCount - (error + predicted + 1);
      }
    }
  }

  return (negativeModel) ? -error : error;
}

