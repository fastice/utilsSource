#include "stdio.h"
#include "string.h"
#include "filterFloat.h"
#include "math.h"
#include "stdlib.h"

static void filterSquare(unwrapPhaseImage *inputImage, float **tmpImage,
                            char **mask,int wr, int wa);
/*
    Filter floating point image.
*/
     void filterFloatImage(unwrapPhaseImage *inputImage,  
                           int filterType,int nIterations,
                           int wr,int wa,double minValue)
{
     float **tmpImage;
     float **originalImage;
     char **maskImage;
     int nr,na;
     int i,j,k;
 /*
    Init  image stuff
*/
    na = inputImage->azimuthSize;
    nr = inputImage->rangeSize;
/*
   Temp image
*/
    tmpImage  = (float **) malloc(na * sizeof(float *) ); 
    for(i=0; i < na; i++) 
        tmpImage[i] = (float *) malloc(nr * sizeof(float) );
/*
   Mask image
*/
    maskImage  = (char **) malloc(na * sizeof(char *) ); 
    for(i=0; i < na; i++) 
        maskImage[i] = (char *) malloc(nr * sizeof(char) );

    for(i=0; i < na; i++) 
        for(j=0; j < nr; j++) { 
            tmpImage[i][j] = 0.0;
            maskImage[i][j] = (char)0; 
            if(inputImage->phase[i][j] > minValue) maskImage[i][j] = (char)1;
        }    
/*
   Use odd filterwidth
*/
   if((wr - (wr/2)*2) == 0) wr++;
   if((wa - (wa/2)*2) == 0) wa++;

/*
   Save image if hipass
*/
    if(filterType == HIPASS) {
        originalImage  = (float **) malloc(na * sizeof(float *) ); 
        for(i=0; i < na; i++) 
        originalImage[i] = (float *) malloc(nr * sizeof(float) );
        for(i=0; i < na; i++)
            for(j=0; j < nr; j++) originalImage[i][j] = inputImage->phase[i][j];
    }
/*
   Low pass filter image
*/
    for(i = 0; i < nIterations; i++) {
        fprintf(stderr,"I = %i\n",i);
        filterSquare(inputImage,tmpImage,maskImage,wr,wa );
    }
    for(i=0; i < na; i++)  free(tmpImage[i]); 
    free(tmpImage);
/*
   Subract low from orig if hipass
*/
    if(filterType == HIPASS) {
        for(i=0; i < na; i++) {
            for(j=0; j < nr; j++)
                inputImage->phase[i][j] = originalImage[i][j] - 
                                          inputImage->phase[i][j];
            free(originalImage[i]);
        } /* End i */
        free(originalImage);
    }
/*
   Mark bad points
*/
    for(i=0; i < na; i++) 
        for(j=0; j < nr; j++)   
            if(maskImage[i][j] == 0.0) inputImage->phase[i][j] = minValue;
/*
   Free mem
*/
   for(i=0; i < na; i++) { free(maskImage[i]); }
   free(maskImage);

}




static void filterSquare(unwrapPhaseImage *inputImage, float **tmpImage,
                            char **mask,int wr, int wa)
{
   float **in;
   char **mask1;
   int i, j,k;
   int halfWidthR, halfWidthA;
   float *scale;
   int a;
   int nr,na;
FILE *fp1,*fp2;
/*
    Init image and filter stuff
*/
   in = inputImage->phase;
   halfWidthR = (wr-1)/2;
   halfWidthA = (wa -1)/2;
fprintf(stderr,"%i  %i %i\n",halfWidthR,halfWidthA,(max(wr,wa)+1));
   na = inputImage->azimuthSize;
   nr = inputImage->rangeSize;
/*
   Compute scale factor lookup table
*/
   scale = (float *) malloc((max(wr,wa)+1) * sizeof(float));
   scale[0] = 0.0;
   for(i=1; i <= max(wr,wa); i++) scale[i] = 1.0/(float)i; 
/*
    Initialize second mask image
*/
    mask1  = (char **) malloc(na * sizeof(char *) ); 
    for(i=0; i < na; i++) 
        mask1[i] = (char *) malloc(nr * sizeof(char) );
/* ************************************************************************
   Filter columns
***************************************************************************/
    for(i=0; i < na; i++) {   
/*
   First columns
*/
        a = 0;
        tmpImage[i][0] = 0.0;
        tmpImage[i][0] = in[i][0] * (float) mask[i][0];
        a = (int)mask[i][0]; 

        if(a != 0) mask1[i][0] = 1; else mask1[i][0] = 0; 

        tmpImage[i][0] *= scale[a];
        if(a != 0) mask1[i][0] = 1; else mask1[i][0] = 0;
        for(j=1; j < halfWidthR+1; j++) {
              tmpImage[i][j] = tmpImage[i][j-1] * (float)a
                            + in[i][2*j-1]* (float)mask[i][2*j-1]
                            + in[i][2*j] *  (float)mask[i][2*j];
            a += (int)mask[i][2*j-1] + (int)mask[i][2*j];
            if(a >= 0) tmpImage[i][j] *= scale[a];
            if(a != 0) mask1[i][j] = 1; else mask1[i][j] = 0;
         } /* End first j */       
/* 
   Middle columns
*/
        for(j=halfWidthR+1; j < nr-halfWidthR; j++) {
            tmpImage[i][j] = tmpImage[i][j-1] * (float)a
                            - in[i][j-halfWidthR-1] * mask[i][j-halfWidthR-1]
                            + in[i][j+halfWidthR] * mask[i][j+halfWidthR];
            a += mask[i][j+halfWidthR] - mask[i][j-halfWidthR-1];
            tmpImage[i][j] *= scale[a];
            if(a != 0) mask1[i][j] = 1; else mask1[i][j] = 0;
         } /* End middle j */
/*
    End columns
*/ 
        k = halfWidthR+1;
        for(j=nr-halfWidthR; j < nr; j++) {
            tmpImage[i][j] = tmpImage[i][j-1] * (float) a 
                             - in[i][j-k]   * mask[i][j-k]
                             - in[i][j-k+1] * mask[i][j-k+1];

            a -=  mask[i][j-k] +  mask[i][j-k+1];
            if(a >= 0) tmpImage[i][j] *= scale[a];
            k--;
            if(a != 0) mask1[i][j] = 1; else mask1[i][j] = 0;
        } /* End  end j  */
    }     /* End i */

/* ************************************************************************
   Filter rows
***************************************************************************/
   for(j=0; j < nr; j++) {   
/*
   First rows
*/
        a = 0;

        in[0][j] = tmpImage[0][j] * (float) mask1[0][j];
        a = (int)mask1[0][j]; 

        for(i=1; i < halfWidthA+1; i++) {
            in[i][j] = in[i-1][j] * (float)a
                    + tmpImage[2*i-1][j] * (float)mask1[2*i-1][j]
                    + tmpImage[2*i][j] * (float)mask1[2*i][j];
            a += (int)mask1[2*i-1][j] + (int)mask1[2*i][j];
            if(a >= 0) in[i][j] *= scale[a];
        } /* End first j */
/* 
   Middle columns
*/
        for(i=halfWidthA+1; i < na-halfWidthA; i++) {
            in[i][j] = in[i-1][j] * (float)a
                     - tmpImage[i-halfWidthA-1][j] * mask1[i-halfWidthA-1][j]
                     + tmpImage[i+halfWidthA][j] * mask1[i+halfWidthA][j];
            a += mask1[i+halfWidthA][j] - mask1[i-halfWidthA-1][j];
            in[i][j] *= scale[a];
        } /* End middle j */
/*
    End columns
*/
        k = halfWidthA+1; 
        for(i=na-halfWidthA; i < na; i++) {
            in[i][j] = in[i-1][j] * (float) a 
                     - tmpImage[i-k][j] * mask1[i-k][j]
                     - tmpImage[i-k+1][j] * mask1[i-k+1][j];
            a -=  mask1[i-k][j] + mask1[i-k+1][j];
            if(a >= 0) in[i][j] *= scale[a];
            k--;
         } /* End  end i  */
    }      /* End j */
/*
   Free mem
*/
   for(i=0; i < na; i++) free(mask1[i]);
   free(mask1);
}

