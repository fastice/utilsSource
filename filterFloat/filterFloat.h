//#include "ers1/makeDEM_p/makeDEM.h"
#include "mosaicSource/common/common.h"
#include "utilsSource/intFloat/intFloat.h"
#define LOPASS 0
#define HIPASS 1
//#define MSB 1
//#define LSB 0 
/*
  Interpolate floating point image.
*/
     void filterFloatImage(unwrapPhaseImage *inputImage, int filterType,
                           int nIterations, int wr,int wa,double minValue);
/*
  Per-pixel variable-half-width box average (see filterFloatImage.c for details). wr/wa are
  literal per-pixel half-widths in pixels (full window = 2*w+1), matching the convention
  mosaicSource/simInSAR/computeSmoothRadius.c uses to produce a smoothing-radius map.
*/
     void filterFloatImageVariable(unwrapPhaseImage *inputImage, int **wr, int **wa,
                           int nIterations, double minValue);
