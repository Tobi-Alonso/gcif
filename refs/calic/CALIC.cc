#include "CALIC.h"


void CALICImageCodec::encInit() {
  delete dec;
  delete enc;

  dec = pgmDec = new PGMImageDecoder(inFilename);
  enc = calicEnc = new CALICImageEncoder(outFilename, dec->getWidth(), dec->getHeight(), 
                                         symbolCount);
  if (resetModel) {
    errorModel->reset();
    entropyModel->reset();
  }

  calicEnc->setEntropyModel(entropyModel);

  binaryModeCount = 0;
  escapedBinaryModeCount = 0;
  regularModeCount = 0;

}

void CALICImageCodec::decInit() {
  delete dec;
  delete enc;

  dec = calicDec = new CALICImageDecoder(inFilename, symbolCount);
  enc = pgmEnc = new PGMImageEncoder(outFilename, dec->getWidth(), dec->getHeight());

  if (resetModel) {
    errorModel->reset();
    entropyModel->reset();
  }

  calicDec->setEntropyModel(entropyModel);

  binaryModeCount = 0;
  escapedBinaryModeCount = 0;
  regularModeCount = 0;
}

int CALICImageCodec::predict_0_0() {
  return 0;
}

void CALICImageCodec::encode_0_0() {
  int pixel = dec->read();
  updateImageBuffer(0, 0, pixel, pixel);
  calicEnc->writeBits(8, pixel);
}

void CALICImageCodec::decode_0_0() {
  int pixel = calicDec->readBits(8);
  updateImageBuffer(0, 0, pixel, pixel);
  enc->write(pixel);
}

int CALICImageCodec::predict_1_0() {
  return srcImg->getPixel(0, 0);
}

int CALICImageCodec::predict_x_0(int x) {
  return srcImg->getPixel(x-1, 0);
}

int CALICImageCodec::predict_0_1() {
  return srcImg->getPixel(0, 0);
}

int CALICImageCodec::predict_1_1() {
  return predict_LOCO(1, 1);
}

int CALICImageCodec::predict_x_1(int x) {
  return predict_LOCO(x, 1);
}

int CALICImageCodec::predict_2ndLast_1() {
  return predict_LOCO(dec->getWidth() - 2, 1);
}

int CALICImageCodec::predict_Last_1() {
  return predict_LOCO(dec->getWidth() - 1, 1);
}

int CALICImageCodec::predict_0_y(int y) {
  return srcImg->getPixel(0, y-1);
}

int CALICImageCodec::predict_1_y(int y) {
  return predict_LOCO(1, y);
}

int CALICImageCodec::predict_Last_y(int y) {
  return predict_LOCO(dec->getWidth() - 1, y);
}

int CALICImageCodec::predict_LOCO(int x, int y) {
  int a = srcImg->getPixel(x-1, y);
  int b = srcImg->getPixel(x, y-1);
  int c = srcImg->getPixel(x-1, y-1);

  if (c >= ((a > b) ? a : b))
    return (a > b) ? b : a;
  else
  if (c <= ((a > b) ? b : a))
    return (a > b) ? a : b;
  else
    return a + b - c;
}

void CALICImageCodec::encode_border(int x, int y) {
  int pixel = dec->read();
  int pred = getPredicted();
  int error = pixel - pred;

  #if NEAR_LOSSLESS
  error = UQ(error,delta,near);
  pixel = ensureBounds(pred+error*delta);
  #endif

  updateImageBuffer(x, y, pixel, error);
  error = encRemapError(false, symbolCount, error, getPredicted());
  calicEnc->writeContinuousModeSymbol(entropyModel->continuousModelCount-1, error);
}

void CALICImageCodec::decode_border(int x, int y) {
  int error = calicDec->readContinuousModeSymbol(entropyModel->continuousModelCount-1);
  error = decRemapError(false, symbolCount, error, getPredicted());

  // Recover the pixel value
  #if NEAR_LOSSLESS
  int pixel = error*delta + getPredicted();
  pixel = ensureBounds(pixel);
  #else
  int pixel = error + getPredicted();
  #endif

  updateImageBuffer(x, y, pixel, error);
  enc->write(pixel);
}

/************************************************************************

------------------------------------------------------------------------*/
void CALICImageCodec::encode_x_y(int x, int y) {

  srcImg->setPixel(x, y, dec->read());

  // Switch to binary mode if necessary.
  // See pp. 441-442, Section 'VI'.

  if (!encBinaryMode(x, y))
    encContinuousMode(x, y);
}

void CALICImageCodec::encContinuousMode(int x, int y) {

  #ifdef ANALYSIS
  regularModeCount++;
  #endif

  // ---------- Perform prediction ------------

  static int predicted, energy, pattern, context, dh, dv, error, currentPixel;

  gradientAdjustedPrediction(x, y, &dh, &dv, &predicted); 

  energy = computeEnergy(x, y, dh, dv); 
  pattern = quantizePattern(x, y, predicted);

  context = (pattern << 2) + (energy >> 1); // Form the context C(energy, pattern).

  // refine the predicted value via context modeling of prediction error.
  predicted += errorModel->getExpectation(context);

  // Make sure the predicted value is in the range [0, symbolCount-1].
  predicted = ensureBounds(predicted);

  // ---------- End of prediction -------------

  currentPixel = srcImg->getPixel(x, y);
  error = currentPixel - predicted;

  //nearlossless quantization
  #if NEAR_LOSSLESS
  error = UQ(error,delta,near);
  // error = l*delta;
  currentPixel = ensureBounds(predicted+error*delta);
  #endif
  updateImageBuffer(x, y, currentPixel, error);

  // Remap and encode error. 
  int remappedError = encRemapError(errorModel->isNegative(context), symbolCount, error, predicted);
  calicEnc->writeContinuousModeSymbol(energy, remappedError);

  errorModel->update(context, error);
}

/************************************************************************
  Method: void CALICImageCodec::decPredict(int x, int y);
  Parameter:
    x, y: Location of the pixel to be decoded. (x, y) is the location of
          pixel in srcImg and errImg. The location of pixel in 'src' is
          (x-2, y-2).

  Description:
    Decode and restore the pixel value at position (x-2, y-2) in 'src'.

  Post condition:
    The pixel at location x,y of srcImg and errImg will be filled.

------------------------------------------------------------------------*/
void CALICImageCodec::decode_x_y(int x, int y) {

  // Switch to binary mode if necessary
  // See pp. 441-442, section 'VI'.

  if (!decBinaryMode(x, y))
    decContinuousMode(x, y);
}

void CALICImageCodec::decContinuousMode(int x, int y) {

  // ------------ Perform prediction --------------
  static int predicted, energy, pattern, context, dh, dv, error, currentPixel;

  gradientAdjustedPrediction(x, y, &dh, &dv, &predicted);

  energy = computeEnergy(x, y, dh, dv);
  pattern = quantizePattern(x, y, predicted);

  context = (pattern << 2) + (energy >> 1);

  predicted += errorModel->getExpectation(context);

  predicted = ensureBounds(predicted);

  // ------------ End of prediction --------------

  // Recover the prediction error.
  error = calicDec->readContinuousModeSymbol(energy);

  error = decRemapError(errorModel->isNegative(context), symbolCount, error, predicted);

  #if NEAR_LOSSLESS
  //near-lossless recovery
  // error = error*delta;

  // Recover the pixel value
  currentPixel = error*delta + predicted;
  currentPixel = ensureBounds(currentPixel);
  #else
  // Recover the pixel value
  currentPixel = error + predicted;

  #endif

  updateImageBuffer(x, y, currentPixel, error);

  enc->write(currentPixel);

  errorModel->update(context, error);
}

bool CALICImageCodec::encBinaryMode(int x, int y) {
  static int s1, s2;
  int binaryPattern = computeBinaryPattern(srcImg, x, y, &s1, &s2);
  if (binaryPattern == -1)
    return false;

  int currentPixel = srcImg->getPixel(x, y);

  #ifdef ANALYSIS
  binaryModeCount++;
  #endif

  if (currentPixel == s1) {
    updateImageBuffer(x, y, currentPixel, 0);
    calicEnc->writeBinaryModeSymbol(binaryPattern, 0);
    //enc->write(0);
  }
  else
  if (currentPixel == s2) {
    updateImageBuffer(x, y, currentPixel, 0);
    calicEnc->writeBinaryModeSymbol(binaryPattern, 1);
    //enc->write(1);
  }
  else {
    escapedBinaryModeCount++;
    calicEnc->writeBinaryModeSymbol(binaryPattern, 2);
    //enc->write(2);
    return false;
  }
  return true;
}

bool CALICImageCodec::decBinaryMode(int x, int y) {
  static int s1, s2;
  int binaryPattern = computeBinaryPattern(srcImg, x, y, &s1, &s2);
  if (binaryPattern == -1)
    return false;

  int symbol = calicDec->readBinaryModeSymbol(binaryPattern);

  if (symbol == 0) {
    updateImageBuffer(x, y, s1, 0);
    enc->write(s1);
  }
  else
  if (symbol == 1) {
    updateImageBuffer(x, y, s2, 0);
    enc->write(s2);
  }
  else
    return false;

  return true;
}


/************************************************************************
  Method: void CALICImageCodec::gradientAdjustedPrediction(
                 int x, int y, int *dh, int *dv, int *predicted);
  Parameters:
    x, y: Location of the pixel to be decoded. (x, y) is the location of
          pixel in srcImg and errImg (not in 'src'). 
    dh, dv: On return, they contain the estimated gradient at location (x, y).
    predicted: On return, it contains the predicted pixel value at 
                  location (x, y).
 
  Description:
    Perform gradient-adjusted prediction. See pp. 439, eqn. 2 and bottom 
    of pp. 439.

------------------------------------------------------------------------*/
void CALICImageCodec::gradientAdjustedPrediction(int x, int y, 
                                                 int *dh, int *dv, 
                                                 int *predicted) {
  #define GAP_LE
  #ifdef GAP_LE
    const int W = srcImg->W(x, y);
    const int N = srcImg->N(x, y);
    const int NE = srcImg->NE(x, y);
    const int WW = srcImg->WW(x, y);
    const int NW = srcImg->NW(x, y);
    const int NN = srcImg->NN(x, y);
    const int NNE = srcImg->NNE(x, y);

    *dh = abs(W - WW) +
          abs(N - NW) +
          abs(NE - N);

    *dv = abs(W - NW) +
          abs(N - NN) +
          abs(NE - NNE);

    int grad_diff = *dv - *dh;

    if (grad_diff > 80){
      *predicted = W;
    } else if (grad_diff < -80){
      *predicted = N;
    } else {
      *predicted = ((W + N) >>1 ) +
                      ((NE - NW) / 4);

      if (grad_diff > 32){
        *predicted = (*predicted + W) >>1;
      } else if (grad_diff > 8){
        *predicted = (3 * *predicted + W) >>2;
      } else if (grad_diff < -32){
        *predicted = (*predicted + N) >>1;
      } else if (grad_diff < -8){
        *predicted = (3 * *predicted + N) >>2;
      }
    }

  #else // original prediction
    // Map of pixels:
    // |NNWW|NNW|NN|NNE|NNEE|
    // |NWW |NW |N |d  |
    // |WW  |W  |x |

    int W  = srcImg->W(x,y);
    int NW  = srcImg->NW(x,y); 
    int N  = srcImg->N(x,y); 
    int NE  = srcImg->NE(x,y); 
    int WW  = srcImg->WW(x,y); 
    int NWW  = srcImg->NWW(x,y); 
    int NNWW  = srcImg->NNWW(x,y); 
    int NNW  = srcImg->NNW(x,y); 
    int NN = srcImg->NN(x,y); 
    int NNE = srcImg->NNE(x,y); 
    int NNEE  = srcImg->NNEE(x,y); 

    int d_h = abs(WW-W)+abs(NW-N)+abs(N-NE);
    int d_v = abs(W-NW)+abs(NW-NNW)+abs(N-NN);
    int d_45 = abs(W-NWW)+abs(NW-NNWW)+abs(N-NNW);
    int d_135 = abs(W-N)+abs(N-NNE)+abs(NE-NNEE);

    int aux_pred;
    if(d_v + d_h > 32) { //sharp edge
      aux_pred = (d_v*W + d_h*N )/(d_v+d_h)+(NE-NW)/8;
    }else if (d_v - d_h > 12){ // horizontal edge
      aux_pred = ((W<<1) + N)/3 + (NE - NW)/8 ;
    }else if (d_h - d_v > 12){ // vertical edge
      aux_pred = (W + (N<<1))/3 + (NE - NW)/8 ;
    }else{ //smooth area
      aux_pred = ((W + N)>>1) + (NE - NW)/8;
    }

    if (d_45 - d_135 > 32){ // sharp 135-deg diagonal edge
      aux_pred += (NE - NW)/8;
    }else if (d_45 - d_135 > 16){ // 135-deg diagonal edge
      aux_pred += (NE - NW)/16;
    }else if (d_135 - d_45 > 32){ // sharp 45-deg diagonal edge
      aux_pred += (NW - NE)/8;
    }else if (d_135 - d_45 > 16){ // 45-deg diagonal edge
      aux_pred += (NW - NE)/16;
    }

    *predicted = aux_pred; 
    *dh = d_h;
    *dv = d_v;
  #endif
}

/************************************************************************

  Method: int CALICImageCodec::computeEnergy(int x, int y, int dh, int dv);
  Parameters:
    x, y: Location of the current pixel. (x, y) is the location of
          pixel in srcImg and errImg (not in 'src'). 
    dh, dv: Estimated gradient at location (x, y).

  Return: Quantized energy (See pp. 440, eqn. 5).
 
  Description:
    Compute the quantized energy level at pixel location (x, y). 
    See pp. 439, section 'IV'.

------------------------------------------------------------------------*/
int CALICImageCodec::computeEnergy(int x, int y, int dh, int dv) {
  #if 1 // Original algorithm 
  return quantizeEnergy(dh + dv + (abs(errImg->getPixel(x-1, y))<<1));
  #else
  return quantizeEnergy(dh + dv + 
                        abs(errImg->getPixel(x-1, y)) + 
                        abs(errImg->getPixel(x, y-1)));
  #endif
                                                              //                        
}

/************************************************************************

  Method: int CALICImageCodec::quantizeEnergy(int energy);
  Parameters:
    energy: the computed energy.

  Return: Quantized energy (See pp. 440, eqn. 5).
 
  Description:
    Quantize the energy to 8 levels according to the threshold values
    defined in pp. 440, eqn 5.

------------------------------------------------------------------------*/
int CALICImageCodec::quantizeEnergy(int energy) {

  if (energy < 42) {
    if (energy < 15)
      return (energy < 5) ? 0 : 1;
    else
      return (energy < 25) ? 2 : 3;
  }
  else { 
    if (energy < 85)
      return (energy < 60) ? 4 : 5;
    else
      return (energy < 140) ? 6 : 7;
  }
}

/************************************************************************

  Method: int CALICImageCodec::quantizePattern(int x, int y, int predicted);
  Parameters:
    x, y: Location of the current pixel. (x, y) is the location of
          pixel in srcImg and errImg (not in 'src'). 
    predicted: The GAP predicted value at location (x, y).

  Return: Quantized pattern around pixel at location (x, y).
          See pp. 440, eqn. 6, 7.
 
  Description:
    Compute the quantized pattern around pixel at location (x, y).
    See pp. 440, eqn. 6, 7.

------------------------------------------------------------------------*/
int CALICImageCodec::quantizePattern(int x, int y, int predicted) {

  int q = 0;
  if (srcImg->N(x, y) < predicted) q |= 1;
  if (srcImg->W(x, y) < predicted) q |= 2;
  if (srcImg->NW(x, y) < predicted) q |= 4;
  if (srcImg->NE(x, y) < predicted) q |= 8;
  if (srcImg->NN(x, y) < predicted) q |= 16;
  if (srcImg->WW(x, y) < predicted) q |= 32;
  if ((2 * srcImg->N(x, y) - srcImg->NN(x, y)) < predicted) q |= 64;
  if ((2 * srcImg->W(x, y) - srcImg->WW(x, y)) < predicted) q |= 128;

  return q;
}

// Post condition:
//   If returned value is not -1, then s1[0] and s2[0] holds the two (or one)
//   distinct values among all the pixels that form the context.

int CALICImageCodec::computeBinaryPattern(ByteImage *img, int x, int y, int *s1, int *s2){

  static int tmp1[6], tmp2[6];

  if (!binaryModeEnabled)
    return -1;

  *s1 = tmp1[0] = img->W(x, y);
  tmp1[1] = img->N(x, y);
  tmp1[2] = img->NW(x, y);
  tmp1[3] = img->NE(x, y);
  tmp1[4] = img->WW(x, y);
  tmp1[5] = img->NN(x, y);

  // tmp1[] keeps the pixels that form the context 

  // Locate all the pixels that are distinct from 's1'
  int i, j;
  for (i = 1, j = 0; i < 6; i++)
    if (tmp1[i] != *s1)
      tmp2[j++] = tmp1[i];  

  // tmp2[] now keeps the pixels that are distinct from 's1'.

  // j is the number of pixels that are distinct from 's1'.
  // Check to see if there are more than 2 distinct values.
  // 's1' holds one value.
  // tmp2[0] holds the other one.
  // Need to check to see if tmp2[i], i > 0, holds the third one.
  if (j > 1) {
    *s2 = tmp2[0];
    for (i = 1; i < j; i++) {
      if (tmp2[i] != *s2)
        return -1;
    }
  }
  else { // All pixels have the same value.
    *s2 = *s1;
    return 0; 
  }

  int pattern = 0;
  // First bit is always 0, so don't include it into the pattern.
  for (i = 1; i < 6; i++) {
    if (tmp1[i] != *s1)
      pattern |= (1 << i);
  }
  pattern >>= 1; 

  return pattern;
}

void CALICImageCodec::printStat() {
  binaryModeCount -= escapedBinaryModeCount;
  double total = (double)(binaryModeCount + regularModeCount);

  printf("# of pixels encoded in binary mode: %d (%f%%)\n",
         binaryModeCount, (double)binaryModeCount * 100.0 / total);
  printf("# of pixels encoded in continuous mode: %d (%f%%)\n",
         regularModeCount, (double)regularModeCount * 100.0 / total);
  printf("# of escaped symbols encoded in binary mode: %d\n", escapedBinaryModeCount);

}

int CALICImageCodec::writeImageCodecModel(char *filename) {
  FILE *fp = fopen(filename, "w");
  if (fp == NULL) {
    perror(filename);
    return -1;
  }

  int ret = 0;
  if (errorModel->write(fp) == -1  ||
      entropyModel->write(fp) == -1) {
    fprintf(stderr, "%s: write error.\n", filename);
    ret = -1;
  }

  fclose(fp); 
  return ret;
}

int CALICImageCodec::readImageCodecModel(char *filename) {
  resetModel = false;
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    perror(filename);
    resetModel = true;
    return -1;
  }

  int ret = 0;
  if (errorModel->read(fp) == -1 || entropyModel->read(fp) == -1) {
    fprintf(stderr, "%s: read error.\n", filename);
    resetModel = true;
    ret = -1;
  }

  fclose(fp); 
  return ret;
}



// ---------------------------- Image Encoder/Decoder/Entropy Model -------------------

void CALICImageEncoder::init() {
  ArithmeticEncoder::start(filename);
  ArithmeticEncoder::writeBits(16, width);
  ArithmeticEncoder::writeBits(16, height);
}

void CALICImageDecoder::init() {
  ArithmeticDecoder::start(filename);
  width = readBits(16);
  height = readBits(16);
}

void CALICEntropyModel::reset() {
  for (int i = 0; i < continuousModelCount; i++)
    continuousModel[i].reset();

  for (int i = 0; i < ternaryModelCount; i++)
    ternaryModel[i].reset();
}

CALICEntropyModel::CALICEntropyModel() {

  continuousModelCount = 8;

  continuousModelSize = new int [continuousModelCount];
  continuousModelSize[0] = 18;
  continuousModelSize[1] = 26;
  continuousModelSize[2] = 34;
  continuousModelSize[3] = 50;
  continuousModelSize[4] = 66;
  continuousModelSize[5] = 82;
  continuousModelSize[6] = 114;
  continuousModelSize[7] = 256;

#ifdef DEFAULT_ADAPTIVE_MODEL
  continuousModel = new AdaptiveModel [continuousModelCount];
#else
  continuousModel = new ContinuousModeModel [continuousModelCount];
#endif
  for (int i = 0; i < continuousModelCount; i++)
    continuousModel[i].init(continuousModelSize[i]);

  ternaryModelCount = 32;

  ternaryModel = new BinaryModeModel [ternaryModelCount];
  for (int i = 0; i < ternaryModelCount; i++)
    ternaryModel[i].init(3);
}

int CALICEntropyModel::write(FILE *fp) {
  for (int i = 0; i < continuousModelCount; i++)
    continuousModel[i].write(fp);

  for (int i = 0; i < ternaryModelCount; i++)
    ternaryModel[i].write(fp);

  return 0;
}

int CALICEntropyModel::read(FILE *fp) {
  for (int i = 0; i < continuousModelCount; i++)
    if (continuousModel[i].read(fp) == -1)
      return -1;

  for (int i = 0; i < ternaryModelCount; i++)
    if (ternaryModel[i].read(fp) == -1)
      return -1;

  return 0;
}

CALICEntropyModel *CALICEntropyModel::clone() {
  CALICEntropyModel *model = new CALICEntropyModel();
  model->copy(this);
  return model;
}

void CALICEntropyModel::copy(CALICEntropyModel *model) {
  for (int i = 0; i < continuousModelCount; i++)
    continuousModel[i].copy(&(model->continuousModel[i]));

  for (int i = 0; i < ternaryModelCount; i++)
    ternaryModel[i].copy(&(model->ternaryModel[i]));
}
