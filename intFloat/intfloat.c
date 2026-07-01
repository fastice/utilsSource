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
  This program takes a floating point32_t image and replaces values that
  are less than minVal with interpolated values.
*/

static void readArgs(int32_t argc, char *argv[], int32_t *nr, int32_t *na,
                     char **imageFile, double *minValue,
                     int32_t *interpLength, int32_t *padEdges, int32_t *intType,
                     int32_t *thresh, float *ratThresh,
                     int32_t *fastFlag, int32_t *allowBreaks,
                     int32_t *islandThresh, int32_t *islandAreaThresh,
                     unsigned char *byteOrder,
                     char **inputVRT,
                     char **outputVRT, int32_t *tiffMode, int32_t *noVRT);
static void usage();
#include "vrtIO.h"

/*
   Global variables
*/
int32_t RangeSize;
int32_t AzimuthSize;
int32_t BufferSize = BUFFERSIZE;
int32_t BufferLines = 512;
double RangePixelSize;
double AzimuthPixelSize;

int main(int argc, char *argv[])
{
    CullParams cullPar;
    time_t tloc;
    unwrapPhaseImage inputImage;
    char *imageFile;
    char *inputVRT;
    char *outputVRT;
    double geoTransform[6];
    char *projWKT;
    dictNode *metaData;
    unsigned char byteOrder;
    double minVal;
    int32_t interpLength, padEdges;
    int32_t intType;
    float ratThresh;
    int32_t fastFlag, allowBreaks;
    int32_t thresh, islandThresh, islandAreaThresh;
    int32_t tiffMode, noVRT;
    int32_t nr, na;
    int32_t i;

    tloc = time(NULL);
    memset(geoTransform, 0, sizeof(geoTransform));
    projWKT  = NULL;
    metaData = NULL;

    readArgs(argc, argv, &nr, &na, &imageFile, &minVal,
             &interpLength, &padEdges, &intType, &thresh, &ratThresh, &fastFlag,
             &allowBreaks, &islandThresh, &islandAreaThresh, &byteOrder,
             &inputVRT, &outputVRT, &tiffMode, &noVRT);

    if (intType == LINEAR)    fprintf(stderr, "\n*** Linear Interpolation ***\n\n");
    if (intType == QUADRATIC) fprintf(stderr, "\n*** quadratic Interpolation ***\n\n");
    if (intType == WDIST)     fprintf(stderr, "\n*** Weighted-Distance Interpolation ***\n\n");
    if (fastFlag == TRUE)     fprintf(stderr, "\n*** Fast Interpolation ***\n\n");

    inputImage.byteOrder = byteOrder;

    /*
      Input: GDAL when -inputVRT or -outputVRT is given, else binary file.
      -inputVRT takes precedence as the source; -outputVRT triggers GDAL mode
      when used without -inputVRT (legacy: intfloat -outputVRT out.vrt imageFile).
    */
    if (inputVRT || outputVRT) {
        char *readFrom = inputVRT ? inputVRT : imageFile;
        readVRTIntoImage(readFrom, &inputImage, geoTransform, &projWKT, &metaData);
        nr = inputImage.rangeSize;
        na = inputImage.azimuthSize;
    } else {
        getPhaseImage(imageFile, nr, na, &inputImage);
    }

    /*
      Interpolate holes.
    */
    interpFloatImage(&inputImage, interpLength, padEdges, minVal, intType,
                     thresh, ratThresh, fastFlag, allowBreaks);

    /*
      Cull islands.
    */
    if (islandAreaThresh > 0)
        fprintf(stderr, "Cull islands with area < %i\n", islandAreaThresh);
    clipIslands(&inputImage, minVal, intType, fastFlag, islandAreaThresh);
    if (islandThresh > 0) {
        fprintf(stderr, "Cull islands with width/height < %i\n", islandThresh);
        cullPar.islandThresh = islandThresh;
        cullPar.offRS = inputImage.phase;
        cullPar.offAS = NULL;
        cullPar.nA = inputImage.azimuthSize;
        cullPar.nR = inputImage.rangeSize;
        cullIslands(&cullPar);
    }

    /*
      Output.
    */
    if (outputVRT) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%d", interpLength);
        insert_node(&metaData, "intfloat_interpLength", buf);
        insert_node(&metaData, "intfloat_interpType",
                    intType == WDIST ? "wdist" : intType == QUADRATIC ? "quadratic" : "linear");
        snprintf(buf, sizeof(buf), "%.6g", minVal);
        insert_node(&metaData, "intfloat_minValue", buf);
        snprintf(buf, sizeof(buf), "%d", thresh);
        insert_node(&metaData, "intfloat_thresh", buf);

        if (tiffMode) {
            if (noVRT) {
                /* Derive .tif path from outputVRT name */
                char tifPath[4096];
                strncpy(tifPath, outputVRT, sizeof(tifPath) - 5);
                tifPath[sizeof(tifPath) - 5] = '\0';
                char *dot = strrchr(tifPath, '.');
                if (dot && strcmp(dot, ".vrt") == 0)
                    strcpy(dot, ".tif");
                else
                    strcat(tifPath, ".tif");
                writeImageAsTiff(tifPath, &inputImage, geoTransform, projWKT,
                                 metaData, (float)minVal);
            } else {
                writeImageAsVRT(outputVRT, &inputImage, geoTransform, projWKT,
                                metaData, (float)minVal);
            }
        } else {
            writeBinaryVRT(outputVRT, &inputImage, geoTransform, projWKT,
                           metaData, (float)minVal, byteOrder, noVRT);
        }
    } else {
        /* Binary stdout */
        for (i = 0; i < inputImage.azimuthSize; i++) {
            if (byteOrder == MSB)
                fwriteBS(inputImage.phase[i], inputImage.rangeSize * sizeof(float),
                         1, stdout, FLOAT32FLAG);
            else
                fwrite(inputImage.phase[i], inputImage.rangeSize * sizeof(float),
                       1, stdout);
        }
    }

    fprintf(stderr, "Interpolation time %li\n", -(tloc - time(NULL)));
    return 0;
}

static void readArgs(int32_t argc, char *argv[], int32_t *nr, int32_t *na,
                     char **imageFile, double *minValue,
                     int32_t *interpLength, int32_t *padEdges, int32_t *intType,
                     int32_t *thresh, float *ratThresh,
                     int32_t *fastFlag, int32_t *allowBreaks,
                     int32_t *islandThresh, int32_t *islandAreaThresh,
                     unsigned char *byteOrder,
                     char **inputVRT,
                     char **outputVRT, int32_t *tiffMode, int32_t *noVRT)
{
    char *argString;
    int32_t i, n;

    if (argc < 2) usage();

    *nr               = 1248;
    *na               = 1318;
    *interpLength     = 50;
    *padEdges         = FALSE;
    *minValue         = -2.0E9;
    *intType          = LINEAR;
    *ratThresh        = .2;
    *islandThresh     = -1;
    *islandAreaThresh = 0;
    *allowBreaks      = FALSE;
    *thresh           = 50000;
    *fastFlag         = FALSE;
    *byteOrder        = MSB;
    *inputVRT         = NULL;
    *outputVRT        = NULL;
    *tiffMode         = FALSE;
    *noVRT            = FALSE;

    /* imageFile is always the last positional argument (unused in VRT mode) */
    n = argc - 2;
    for (i = 1; i <= n; i++) {
        argString = strchr(argv[i], '-');
        if (argString == NULL) usage();

        if (strstr(argString, "inputVRT") != NULL) {
            *inputVRT = argv[++i];
        } else if (strstr(argString, "outputVRT") != NULL) {
            *outputVRT = argv[++i];
        } else if (strstr(argString, "noVRT") != NULL) {
            *noVRT = TRUE;
        } else if (strstr(argString, "tiff") != NULL) {
            /* New style: -tiff out.vrt  sets output path and tiff mode.
               Also sets outputVRT so the output section knows to write. */
            *outputVRT = argv[++i];
            *tiffMode  = TRUE;
        } else if (strstr(argString, "nr") != NULL) {
            sscanf(argv[++i], "%i", nr);
        } else if (strstr(argString, "na") != NULL) {
            sscanf(argv[++i], "%i", na);
        } else if (strstr(argString, "thresh") != NULL) {
            sscanf(argv[++i], "%i", thresh);
        } else if (strstr(argString, "interpLength") != NULL) {
            sscanf(argv[++i], "%i", interpLength);
        } else if (strstr(argString, "minValue") != NULL) {
            sscanf(argv[++i], "%lf", minValue);
        } else if (strstr(argString, "ratThresh") != NULL) {
            sscanf(argv[++i], "%f", ratThresh);
        } else if (strstr(argString, "islandAreaThresh") != NULL) {
            sscanf(argv[++i], "%i", islandAreaThresh);
        } else if (strstr(argString, "islandThresh") != NULL) {
            sscanf(argv[++i], "%i", islandThresh);
        } else if (strstr(argString, "linear") != NULL) {
            *intType = LINEAR;
        } else if (strstr(argString, "allowBreaks") != NULL) {
            *allowBreaks = TRUE;
        } else if (strstr(argString, "wdist") != NULL) {
            *intType = WDIST;
        } else if (strstr(argString, "quadratic") != NULL) {
            *intType = QUADRATIC;
        } else if (strstr(argString, "fast") != NULL) {
            *fastFlag = TRUE;
        } else if (strstr(argString, "LSB") != NULL) {
            *byteOrder = LSB;
        } else {
            usage();
        }
    }
    *imageFile = argv[argc - 1];
}

static void usage()
{
    error("\n%s\n%s\n\n%s\n%s\n%s\n%s\n%s\n\n%s\n\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n\n",
          "Interpolate values < minValue in a floating-point image.",
          "Usage:",
          "  intfloat [opts] imageFile",
          "    binary input imageFile, interpolated binary to stdout",
          "  intfloat -inputVRT in.vrt -tiff out.vrt [opts]",
          "    VRT input, GeoTIFF+VRT output (nr/na auto-detected)",
          "  intfloat -inputVRT in.vrt -outputVRT out.vrt [opts]",
          "    VRT input, binary+VRT output",
          "Options:",
          "  inputVRT         = read input from VRT (auto-detects nr/na)",
          "  outputVRT        = output VRT path for binary+VRT output",
          "  tiff <file>      = write GeoTIFF+VRT to <file> (sets outputVRT)",
          "  noVRT            = skip VRT wrapper, write data file only",
          "  LSB              = use LSB byte order for binary I/O (default MSB)",
          "  minValue         = values <= minValue are holes (default -2e9)",
          "  linear           = linear interpolation (default)",
          "  quadratic        = quadratic interpolation",
          "  wdist            = weighted-distance interpolation",
          "  interpLength     = max distance (pixels) to search for valid data (default 50)",
          "  thresh           = largest hole size to interpolate (default 50000)",
          "  ratThresh        = hole size/bbox ratio threshold (default 0.2)",
          "  islandThresh     = cull islands narrower than this many pixels in result",
          "  islandAreaThresh = cull islands smaller than this area (pixels)",
          "  allowBreaks      = interpolate across a hole that spans the image",
          "  nr, na           = range/azimuth size for binary input (default 1248x1318)",
          "  padEdges         = pad border with nearest-neighbour values");
}
