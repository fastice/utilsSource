#include "stdio.h"
#include "string.h"
#include "changeFlat.h"
#include <sys/types.h>
#include <sys/time.h>
#include <math.h>

/*
   This program takes an unwrapped image as input and change the flattening
   parameters.
*/

static void readArgs(int argc, char *argv[], int32_t *passType, int32_t *noRamp,
                     int32_t *nr, int32_t *na,
                     char **imageFile, char **paramFile, int32_t *dBpFlag);
static void usage();

/*
   Global variables definitions
*/
int32_t RangeSize = RANGESIZE;              /* Range size of complex image */
int32_t AzimuthSize = AZIMUTHSIZE;          /* Azimuth size of complex image */
int32_t BufferSize = BUFFERSIZE;            /* Size of nonoverlap region of the buffer */
int32_t BufferLines = 512;                  /* # of lines of nonoverlap in buffer */
double RangePixelSize = RANGEPIXELSIZE;     /* Range PixelSize */
double AzimuthPixelSize = AZIMUTHPIXELSIZE; /* Azimuth PixelSize */

int main(int argc, char *argv[])
{
    paramData params;
    unwrapPhaseImage inputImage;
    char *imageFile, *paramFile;
    int32_t passType;
    int32_t nr, na;
    int32_t noRamp, dBpFlag;
    int32_t i, j; /* LCV */
                  /*
                      Read command line args and compute filenames
                  */
    readArgs(argc, argv, &passType, &noRamp, &nr, &na, &imageFile, &paramFile, &dBpFlag);
    inputImage.passType = passType;
    params.noRamp = noRamp;
    params.dBpFlag = dBpFlag;
    if (noRamp == TRUE)
        fprintf(stderr, "NO RAMP\n");
    else
        fprintf(stderr, "RAMP\n");
    /*
        Input image
    */
    getPhaseImage(imageFile, nr, na, &inputImage);
    /*
        Get baseline and other params
    */
    inputParamFile(paramFile, &params);
    /*
        Add back in old phase ramps and subtract new
    */
    reflattenImage(&inputImage, params);
    for (i = 0; i < inputImage.azimuthSize; i++)
        fwriteBS(inputImage.phase[i],
                 inputImage.rangeSize, sizeof(float), stdout, FLOAT32FLAG);
}

static void readArgs(int argc, char *argv[], int32_t *passType, int32_t *noRamp,
                     int32_t *nr, int32_t *na,
                     char **imageFile, char **paramFile, int32_t *dBpFlag)
{
    int32_t filenameArg;
    char *argString;
    int32_t i, n;

    if (argc < 3 || argc > 12)
        usage();            /* Check number of args */
    *passType = DESCENDING; /* Default */
    *nr = 1248;
    *na = 1318;
    *noRamp = FALSE;
    *dBpFlag = FALSE;
    n = argc - 3;
    for (i = 1; i <= n; i++)
    {
        argString = strchr(argv[i], '-');
        if (strstr(argString, "descending") != NULL && *passType < 0)
            *passType = DESCENDING;
        else if (strstr(argString, "ascending") != NULL && *passType < 0)
            *passType = ASCENDING;
        else if (strstr(argString, "noRamp") != NULL)
            *noRamp = TRUE;
        else if (strstr(argString, "dBp") != NULL)
            *dBpFlag = TRUE;
        else if (strstr(argString, "rPix") != NULL)
        {
            sscanf(argv[i + 1], "%lf", &RangePixelSize);
            i++;
        }
        else if (strstr(argString, "nr") != NULL)
        {
            sscanf(argv[i + 1], "%i", nr);
            i++;
        }
        else if (strstr(argString, "na") != NULL)
        {
            sscanf(argv[i + 1], "%i", na);
            i++;
        }
        else
            usage();
    }
    *imageFile = argv[argc - 2];
    *paramFile = argv[argc - 1];
    return;
}

static void usage()
{
    error("\n\n%s\n\n%s\n\n%s\n\n%s\n\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
          "Change flattening parameters by adding back ramp and subtracting new ramp",
          "Usage:",
          " changeFlat -dBp -rPix rPix -passType -nr nr -na na imageFile baseLineFile",
          "where",
          "  dBp          = Set to use azimuth varying Bp",
          "  noRamp       = use exact expression for along track Bp variation ",
          "                 otherwise approximate as a linear azimuth phase ramp",
          "  passType     = DESCENDING (default) or ASCENDING",
          "  nr,na        = range and azimuth size in pixels (default=1248x1318)",
          "  imageFile    = unwrapped phase file",
          "  baselineFile = file with baseline info. (i.e.,output from tiepoints)");
}
