#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "intFloat.h"
#include "clib/standard.h"
#include "stdlib.h"
/*
   Input phase image.
*/
void getPhaseImage(char *inputFile, int32_t nr, int32_t na, unwrapPhaseImage *inputImage)
{
    FILE *fp;
    int32_t i;
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
        freadBS(inputImage->phase[i], nr * sizeof(float), 1, fp, FLOAT32FLAG);
    return;
}
