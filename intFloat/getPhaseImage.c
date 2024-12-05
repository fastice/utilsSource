#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "intFloat.h"
#include "clib/standard.h"
#include "stdlib.h"
#include "mosaicSource/common/common.h"
/*
   Input phase image.
*/
void getPhaseImage(char *inputFile, int32_t nr, int32_t na, unwrapPhaseImage *inputImage)
{
    FILE *fp;
    int32_t i;
    size_t retValue;
    /*
       Init image
    */
    inputImage->phase = (float **)malloc(na * sizeof(float *));
    for (i = 0; i < na; i++)
        inputImage->phase[i] = (float *)malloc(nr * sizeof(float));
    inputImage->rangeSize = nr;
    inputImage->azimuthSize = na;
    /*
        Input image
    */
    if (inputFile != NULL)
        fp = openInputFile(inputFile);
    else
        fp = stdin;
    for (i = 0; i < na; i++)
        if(inputImage->byteOrder == MSB)
            freadBS(inputImage->phase[i], nr * sizeof(float), 1, fp, FLOAT32FLAG);
        else if(inputImage->byteOrder == LSB)
            retValue = fread(inputImage->phase[i], nr * sizeof(float), 1, fp);
        else
            error("invalid byte order");
    return;
}
