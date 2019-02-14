#include "stdio.h"
#include"string.h"
#include "mosaicSource/common/common.h"
#include "intFloat.h"
#include <sys/types.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>

#include "speckleSource/Cullst/cullst.h"
/*
   This program takes a floating point images and replaces values that
   are less than minVal with interpolated values.
*/

    static void readArgs(int argc,char *argv[], int *nr, int *na, char **imageFile,double *minValue,  int *interpLength,int *padEdges, int *intType, 
                         int *thresh,float *ratThresh,int *fastFlag,     int *allowBreaks, int *islandThresh);
    static void usage();


/* 
   Global variables definitions
*/
    int RangeSize;       /* Range size of complex image */
    int AzimuthSize;   /* Azimuth size of complex image */
    int BufferSize=BUFFERSIZE;     /* Size of nonoverlap region of the buffer */
    int BufferLines = 512;         /* # of lines of nonoverlap in buffer */
    double RangePixelSize; /* Range PixelSize */
    double AzimuthPixelSize; /* Azimuth PixelSize */



    void main(int argc, char *argv[])
{   
    CullParams cullPar;
    time_t tloc;
    unwrapPhaseImage inputImage;
    char *imageFile, *paramFile;
    double minVal;
    int interpLength,padEdges;
    int intType;
    float ratThresh;
    int fastFlag,allowBreaks;
    int thresh, islandThresh;
    int nr,na;
    int i,j;                               /* LCV */
/* 
    Read command line args and compute filenames
*/
    tloc = time(NULL);
    readArgs( argc, argv, &nr, &na, &imageFile, &minVal,
              &interpLength,&padEdges, &intType,&thresh,&ratThresh,&fastFlag,
              &allowBreaks,&islandThresh);
    if(intType==LINEAR) 
        fprintf(stderr,"\n*** Linear Interpolation ***\n\n");
    if(intType==QUADRATIC) 
        fprintf(stderr,"\n*** quadratic Interpolation ***\n\n");
    if(intType==WDIST)
        fprintf(stderr,"\n*** Weighted-Distance Interpolation ***\n\n");
    if(fastFlag==TRUE)
        fprintf(stderr,"\n*** Fast Interpolation ***\n\n");
/*
    Input image
*/
    getPhaseImage( imageFile, nr, na, &inputImage);
/*
    Add back in old phase ramps and subtract new
*/
    interpFloatImage(&inputImage,interpLength,padEdges,minVal,intType, thresh,ratThresh,fastFlag,allowBreaks); 
/*
    kill of islands
*/
     if(islandThresh > 0) {
       fprintf(stderr,"Kill off islands with width < %i\n",islandThresh);
       cullPar.islandThresh=islandThresh;
       cullPar.offRS = inputImage.phase;
       cullPar.offAS = NULL;
       cullPar.nA=inputImage.azimuthSize;
       cullPar.nR=inputImage.rangeSize;
       cullIslands(&cullPar);
     }
     for(i=0; i < inputImage.azimuthSize; i++)
        fwriteBS(inputImage.phase[i],
               inputImage.rangeSize*sizeof(float),1,stdout,FLOAT32FLAG);  
     fprintf(stderr,"Interpolation time %li\n",-(tloc - time(NULL)));
    return; 
}


    static void readArgs(int argc,char *argv[], int *nr, int *na,
                         char **imageFile,double *minValue,
                         int *interpLength,int *padEdges, int *intType, 
                         int *thresh,float *ratThresh, int *fastFlag,
                         int *allowBreaks,int *islandThresh)
{
    int filenameArg;
    char *argString;
    int i,n;

    if( argc < 3 || argc > 17 ) usage();        /* Check number of args */ 
    *nr = 1248;
    *na = 1318;
    *interpLength = 50;
    *padEdges = FALSE;
    *minValue = -2.0E6;
    *intType=LINEAR;
    *ratThresh=.2;
    *islandThresh=-1;
    *allowBreaks=FALSE;
    *thresh=50000;
    *fastFlag=FALSE;
    n = argc - 2;
    for(i=1; i <= n; i++) {
        argString = strchr(argv[i],'-');  
        if(strstr(argString,"nr") != NULL) {
             sscanf(argv[i+1],"%i",nr);  
             i++;
        } else if(strstr(argString,"padEdges") != NULL)
             *padEdges = TRUE;
        else if(strstr(argString,"na") != NULL) {
             sscanf(argv[i+1],"%i",na);
             i++;
        } else if(strstr(argString,"thresh") != NULL) {
             sscanf(argv[i+1],"%i",thresh);
             i++;
        } else if(strstr(argString,"interpLength") != NULL) {
             sscanf(argv[i+1],"%i",interpLength);
             i++;
        } else if(strstr(argString,"minValue") != NULL) {
             sscanf(argv[i+1],"%lf",minValue);
             i++;
        } else if(strstr(argString,"ratThresh") != NULL) {
             sscanf(argv[i+1],"%f",ratThresh);
             i++;
        } else if(strstr(argString,"islandThresh") != NULL) {
             sscanf(argv[i+1],"%i",islandThresh);
             i++;
        } else if(strstr(argString,"linear") != NULL) {
             *intType=LINEAR;
       } else if(strstr(argString,"allowBreaks") != NULL) {
             *allowBreaks=TRUE;
        } else if(strstr(argString,"wdist") != NULL) {
             *intType=WDIST;
        } else if(strstr(argString,"quadratic") != NULL) {
             *intType=QUADRATIC;
        } else if(strstr(argString,"fast") != NULL) {
             *fastFlag=TRUE;    
        } else usage();  
    }
    *imageFile = argv[argc-1];
    return;
}

 
    static void usage()
{ 
    error("\n%s\n%s\n\n%s\n\n%s\n\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n\n",
      "Interpolate values < minValue in a Floating Point image ",
      "Usage:",
    " interpfloat -islandThresh islandThresh -thresh thresh -ratThresh -padEdges -nr nr -na na imageFile",
      "where",
      "  linear       = use linear interpolation (default)", 
      "  quadratic    = use quadratic interpolation",
      "  wdist        = use weighted distance interpolation",
      "  interpLength = use points within this distance of hole",
      "  thresh       = The largest hole size to try and interpolate",
      "  ratThresh    = Threshhold for ratio of size to bbox area (def=0.2)",
      "  islandThresh  = kill of islands < islandThresh in result",
      "  allowBreaks  = interpolate accross hole that spans image",
      "  nr,na        = range and azimuth size in pixels (default=1248x1318)",
      "  padEdges     = Set flag to pad border values nearest neighbor",
      "  imageFile    = floatint-point image file"
       );
}
