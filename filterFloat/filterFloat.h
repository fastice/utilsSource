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
