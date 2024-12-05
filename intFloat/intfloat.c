#include "stdio.h"
#include "string.h"
#include "mosaicSource/common/common.h"
#include "intFloat.h"
#include <sys/types.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include "gdalIO/gdalIO/grimpgdal.h"
#include "speckleSource/Cullst/cullst.h"
/*
  This program takes a floating point32_t images and replaces values that
  are less than minVal with interpolated values.
*/

static void readArgs(int32_t argc, char *argv[], int32_t *nr, int32_t *na, char **imageFile, double *minValue, 
					int32_t *interpLength, int32_t *padEdges, int32_t *intType, int32_t *thresh, float *ratThresh, 
					int32_t *fastFlag, int32_t *allowBreaks, int32_t *islandThresh, int32_t *islandAreaThresh, unsigned char *byteOrder);
static void usage();

/*
   Global variables definitions
*/
int32_t RangeSize;				 /* Range size of complex image */
int32_t AzimuthSize;			 /* Azimuth size of complex image */
int32_t BufferSize = BUFFERSIZE; /* Size of nonoverlap region of the buffer */
int32_t BufferLines = 512;		 /* # of lines of nonoverlap in buffer */
double RangePixelSize;			 /* Range PixelSize */
double AzimuthPixelSize;		 /* Azimuth PixelSize */

int main(int argc, char *argv[])
{
	CullParams cullPar;
	time_t tloc;
	unwrapPhaseImage inputImage;
	char *imageFile, *paramFile;
	unsigned char byteOrder;
	double minVal;
	int32_t interpLength, padEdges;
	int32_t intType;
	float ratThresh;
	int32_t fastFlag, allowBreaks;
	int32_t thresh, islandThresh, islandAreaThresh;
	int32_t nr, na;
	int32_t i, j; /* LCV */
	/*
	   Read command line args and compute filenames
	*/
	tloc = time(NULL);
	readArgs(argc, argv, &nr, &na, &imageFile, &minVal,
			 &interpLength, &padEdges, &intType, &thresh, &ratThresh, &fastFlag,
			 &allowBreaks, &islandThresh, &islandAreaThresh,  &byteOrder);
	if (intType == LINEAR)
		fprintf(stderr, "\n*** Linear Interpolation ***\n\n");
	if (intType == QUADRATIC)
		fprintf(stderr, "\n*** quadratic Interpolation ***\n\n");
	if (intType == WDIST)
		fprintf(stderr, "\n*** Weighted-Distance Interpolation ***\n\n");
	if (fastFlag == TRUE)
		fprintf(stderr, "\n*** Fast Interpolation ***\n\n");
	inputImage.byteOrder = byteOrder;
	/*
	  Input image
	*/
	getPhaseImage(imageFile, nr, na, &inputImage);
	/*
	 Interp image
	*/
	interpFloatImage(&inputImage, interpLength, padEdges, minVal, intType, thresh, ratThresh, fastFlag, allowBreaks);
	/*
	  Cull islands (isolated areas of valid data that are may be bad data
	*/
	if (islandAreaThresh > 0)
		fprintf(stderr, "Cull islands with area < %i\n", islandAreaThresh);
	clipIslands(&inputImage, minVal, intType, fastFlag, islandAreaThresh);
	if (islandThresh > 0)
	{
		fprintf(stderr, "Cull islands with width/height < %i\n", islandThresh);
		cullPar.islandThresh = islandThresh;
		cullPar.offRS = inputImage.phase;
		cullPar.offAS = NULL;
		cullPar.nA = inputImage.azimuthSize;
		cullPar.nR = inputImage.rangeSize;
		cullIslands(&cullPar);
	}
	for (i = 0; i < inputImage.azimuthSize; i++)
		if(byteOrder == MSB)
			fwriteBS(inputImage.phase[i], inputImage.rangeSize * sizeof(float), 1, stdout, FLOAT32FLAG);
		else
			fwrite(inputImage.phase[i], inputImage.rangeSize * sizeof(float), 1, stdout);
	fprintf(stderr, "Interpolation time %li\n", -(tloc - time(NULL)));
	return 0;
}

static void readArgs(int32_t argc, char *argv[], int32_t *nr, int32_t *na, char **imageFile, double *minValue,
					 int32_t *interpLength, int32_t *padEdges, int32_t *intType, int32_t *thresh, float *ratThresh, int32_t *fastFlag, int32_t *allowBreaks,
					 int32_t *islandThresh, int32_t *islandAreaThresh, unsigned char *byteOrder)
{
	int32_t filenameArg;
	char *argString;
	int32_t i, n;

	if (argc < 3 || argc > 17)
		usage(); /* Check number of args */
	*nr = 1248;
	*na = 1318;
	*interpLength = 50;
	*padEdges = FALSE;
	*minValue = -2.0E9;
	*intType = LINEAR;
	*ratThresh = .2;
	*islandThresh = -1;
	*islandAreaThresh = 0;
	*allowBreaks = FALSE;
	*thresh = 50000;
	*fastFlag = FALSE;
	*byteOrder = MSB;
	n = argc - 2;
	for (i = 1; i <= n; i++)
	{
		argString = strchr(argv[i], '-');
		if (strstr(argString, "nr") != NULL)
		{
			sscanf(argv[i + 1], "%i", nr);
			i++;
		}
		else if (strstr(argString, "padEdges") != NULL)
			*padEdges = TRUE;
		else if (strstr(argString, "na") != NULL)
		{
			sscanf(argv[i + 1], "%i", na);
			i++;
		}
		else if (strstr(argString, "thresh") != NULL)
		{
			sscanf(argv[i + 1], "%i", thresh);
			i++;
		}
		else if (strstr(argString, "interpLength") != NULL)
		{
			sscanf(argv[i + 1], "%i", interpLength);
			i++;
		}
		else if (strstr(argString, "minValue") != NULL)
		{
			sscanf(argv[i + 1], "%lf", minValue);
			i++;
		}
		else if (strstr(argString, "ratThresh") != NULL)
		{
			sscanf(argv[i + 1], "%f", ratThresh);
			i++;
		}
		else if (strstr(argString, "islandThresh") != NULL)
		{
			sscanf(argv[i + 1], "%i", islandThresh);
			i++;
		}
		else if (strstr(argString, "islandAreaThresh") != NULL)
		{
			sscanf(argv[i + 1], "%i", islandAreaThresh);
			i++;
		}
		else if (strstr(argString, "linear") != NULL)
		{
			*intType = LINEAR;
		}
		else if (strstr(argString, "allowBreaks") != NULL)
		{
			*allowBreaks = TRUE;
		}
		else if (strstr(argString, "wdist") != NULL)
		{
			*intType = WDIST;
		}
		else if (strstr(argString, "quadratic") != NULL)
		{
			*intType = QUADRATIC;
		}
		else if (strstr(argString, "fast") != NULL)
		{
			*fastFlag = TRUE;
		}
		else if (strstr(argString, "LSB") != NULL)
		{
			*byteOrder = LSB;
		}
		else
			usage();
	}
	*imageFile = argv[argc - 1];
	return;
}

static void usage()
{
	error("\n%s\n%s\n\n%s\n\n%s\n\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n\n",
		  "Interpolate values < minValue in a Floating Point image ",
		  "Usage:",
		  " interpfloat -islandThresh islandThresh -islandAreaThresh islandAreaThresh  -thresh thresh -ratThresh -padEdges -nr nr -na na imageFile",
		  "where",
		  "  LSB = Use LSB byte order for i/o instead of default MSB",
		  "  minValue = use linear interpolation (-2.e8)",
		  "  linear       = use linear interpolation (default)",
		  "  quadratic    = use quadratic interpolation",
		  "  wdist        = use weighted distance interpolation",
		  "  interpLength = use points within this distance of hole",
		  "  thresh       = The largest hole size to try and interpolate",
		  "  ratThresh    = Threshhold for ratio of size to bbox area (def=0.2)",
		  "  islandThresh  = cull islands of width < islandThresh in result",
		  "  islandAreaThresh  = cull islands of area in pixels < islandAreaThresh in result",
		  "  allowBreaks  = interpolate accross hole that spans image",
		  "  nr,na        = range and azimuth size in pixels (default=1248x1318)",
		  "  padEdges     = Set flag to pad border values nearest neighbor",
		  "  imageFile    = floatint-point image file");
}
