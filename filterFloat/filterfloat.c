#include "stdio.h"
#include"string.h"
#include "filterFloat.h"
#include <sys/types.h>
#include <sys/time.h>
#include <math.h>

/*
   This program takes a floating point images and interpolates values that
   are less than minVal.
*/

    static void readArgs(int argc,char *argv[], int *nr, int *na,
                         char **imageFile,double *minValue, 
                         int *nIterations,int *wa, int *wr, int *filterType, int *byteOrder);
   static void usage();

/* 
   Global variables definitions
*/
    int RangeSize=RANGESIZE;       /* Range size of complex image */
    int AzimuthSize=AZIMUTHSIZE;   /* Azimuth size of complex image */
    int BufferSize=BUFFERSIZE;     /* Size of nonoverlap region of the buffer */
    int BufferLines = 512;         /* # of lines of nonoverlap in buffer */
    double RangePixelSize=RANGEPIXELSIZE; /* Range PixelSize */
    double AzimuthPixelSize=AZIMUTHPIXELSIZE; /* Azimuth PixelSize */




    void main(int argc, char *argv[])
{   
    //paramData params;
    unwrapPhaseImage inputImage;
    char *imageFile, *paramFile;
    double minVal;
    int interpLength,padEdges;
    int filterType, filterWidth,wa,wr, nIterations;
    int nr,na;
    int i,j;                               /* LCV */
    int byteOrder;
/* 
    Read command line args and compute filenames
*/
    readArgs( argc, argv, &nr, &na, &imageFile, &minVal,
              &nIterations,&wa, &wr, &filterType, &byteOrder);
    inputImage.byteOrder = byteOrder;
/*
    Input image
*/
    getPhaseImage(imageFile, nr, na, &inputImage);
/*
   Filter image
*/   
    fprintf(stderr,"wa, wr, nIterations, filterType,minValue  %i  %i  %i  %i  %f\n",
    wa, wr, nIterations,filterType,minVal);

    filterFloatImage(&inputImage, filterType, nIterations, wr,wa, minVal);
 
    for(i=0; i < inputImage.azimuthSize; i++) {
        if(byteOrder == MSB)
        {
            fwriteBS(inputImage.phase[i],
                     inputImage.rangeSize,sizeof(float), stdout, FLOAT32FLAG);  
        }  
         else 
         {
            fwrite(inputImage.phase[i], sizeof(float), inputImage.rangeSize, stdout);
         } 
    }
    return; 
}


    static void readArgs(int argc,char *argv[], int *nr, int *na,
                         char **imageFile,double *minValue, 
                         int *nIterations,int *wa, int *wr, int *filterType, int *byteOrder)
{
    int filenameArg;
    char *argString;
    int w;
    int i,n;

    if( argc < 2 || argc > 13 ) usage();        /* Check number of args */ 
    *nr = 1248;
    *na = 1318;
    *nIterations = 1;
    *wa = 50;
    *wr = 50;
    *minValue = -2.0E9;
    *filterType = LOPASS;
    *byteOrder = MSB;
    n = argc - 1;
    for(i=1; i <= n; i++) {
        argString = strchr(argv[i],'-');  
        //fprintf(stderr,"argString %s\n", argString);
        if(strstr(argString,"nr") != NULL) {
             sscanf(argv[i+1],"%i",nr);  
             i++;
        } else if(strstr(argString,"na") != NULL) {
             sscanf(argv[i+1],"%i",na);
             i++;
        } else if(strstr(argString,"nIterations") != NULL) {
             sscanf(argv[i+1],"%i",nIterations);
             i++;
        } else if(strstr(argString,"w") != NULL) {             
             sscanf(argv[i+1],"%i",&w);
             i++;
             if(strstr(argString,"a") != NULL) *wa = w;
             else if(strstr(argString,"r") != NULL) *wr = w;
             else {*wr = w; *wa = w; }
        } else if(strstr(argString,"minValue") != NULL) {
             sscanf(argv[i+1],"%lf",minValue);
             i++;
        } else if(strstr(argString,"hiPass") != NULL) {
             *filterType = HIPASS;
        } else if(strstr(argString,"LSB") != NULL)  
            *byteOrder = LSB;
        else usage();  
    }
/*    *imageFile = argv[argc-1]; */
    *imageFile = NULL;
    //return;
}

 
    static void usage()
{ 
    error(
    "\n\n%s\n%s\n\n%s\n\n%s\n\n%s\n\n%s\n\n%s\n\n%s\n\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
    "Filter a floating point image ignoring values < minValue using ",
    "a rectangular kernel.",
    "Input and output are to stdio",
    "Usage:",
    " filterfloat -LSB -w w -nr nr -na na -minValue minValue -nIterations nIterations", 
    "or ",
     " filterfloat -wa wa -wr wr ...", 
    "where",
    "  nr,na       = range and azimuth size in pixels (default=1248x1318)",
    "  minValue    = values <= minValue ignored",
    "  w           = Width of filter kernel (default=50)",
    "  wr,wa       = Filter width in range and azimuth directions (default=50)",
    "  nIterations = Repeat filtering for nIterations to obtain other kernels",
    "                (i.e triangular for nIterations = 2)",
    "  hiPass      = Set to high-pass filter image as: image - lowpass(image)"
/*    "  imageFile   = floating-point image file" */
       );
}
