#include "stdio.h"
#include "string.h"
#include "filterFloat.h"
#include <sys/types.h>
#include <sys/time.h>
#include <math.h>
#include "utilsSource/intFloat/vrtIO.h"

/*
   This program takes a floating point images and interpolates values that
   are less than minVal.
*/

    static void readArgs(int argc, char *argv[], int *nr, int *na,
                         char **imageFile, double *minValue,
                         int *nIterations, int *wa, int *wr, int *filterType,
                         int *byteOrder, char **inputVRT, char **tiffOut,
                         char **radiusMap, double *pixRatio);
   static void usage();
   static unsigned char **readByteRaster(const char *file, int *xSize, int *ySize);

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
    unwrapPhaseImage inputImage;
    char *imageFile, *inputVRT, *tiffOut, *radiusMap;
    double geoTransform[6];
    char *projWKT;
    dictNode *metaData;
    double minVal, pixRatio;
    int interpLength, padEdges;
    int filterType, filterWidth, wa, wr, nIterations;
    int nr, na;
    int i, j;                               /* LCV */
    int byteOrder;
/*
    Read command line args
*/
    memset(geoTransform, 0, sizeof(geoTransform));
    projWKT  = NULL;
    metaData = NULL;
    readArgs(argc, argv, &nr, &na, &imageFile, &minVal,
             &nIterations, &wa, &wr, &filterType, &byteOrder,
             &inputVRT, &tiffOut, &radiusMap, &pixRatio);
    inputImage.byteOrder = byteOrder;
/*
    Input image — VRT or stdin
*/
    if (inputVRT) {
        readVRTIntoImage(inputVRT, &inputImage, geoTransform, &projWKT, &metaData);
        nr = inputImage.rangeSize;
        na = inputImage.azimuthSize;
    } else {
        getPhaseImage(imageFile, nr, na, &inputImage);
    }
/*
   Filter image
*/
    if (radiusMap) {
        /* Variable-radius path: radiusMap is a per-pixel byte raster (azimuth half-width
           in pixels, same grid as inputImage -- produced by siminsar's -minTol/-percentSpeed/
           -maxTol sweep or simoffsets.py's equivalent). Range half-width is derived via
           -pixRatio (azimuthPixelSize/rangePixelSize for this frame; default 1.0 = isotropic
           in pixel units), since radar-coordinate VRTs carry a dummy pixel-index geotransform,
           not real pixel spacing. */
        unsigned char **radius;
        int rxSize, rySize;
        int **wrMap, **waMap;

        radius = readByteRaster(radiusMap, &rxSize, &rySize);
        if (rxSize != nr || rySize != na)
            error("filterfloat: -radiusMap size %ix%i does not match image size %ix%i\n",
                  rxSize, rySize, nr, na);
        fprintf(stderr, "radiusMap, pixRatio, nIterations, minValue  %s  %f  %i  %f\n",
                radiusMap, pixRatio, nIterations, minVal);
        waMap = (int **)malloc(na * sizeof(int *));
        wrMap = (int **)malloc(na * sizeof(int *));
        for (i = 0; i < na; i++) {
            waMap[i] = (int *)malloc(nr * sizeof(int));
            wrMap[i] = (int *)malloc(nr * sizeof(int));
            for (j = 0; j < nr; j++) {
                waMap[i][j] = (int)radius[i][j];
                wrMap[i][j] = (int)(radius[i][j] * pixRatio + 0.5);
            }
        }
        filterFloatImageVariable(&inputImage, wrMap, waMap, nIterations, minVal);
    } else {
        fprintf(stderr, "wa, wr, nIterations, filterType, minValue  %i  %i  %i  %i  %f\n",
                wa, wr, nIterations, filterType, minVal);
        filterFloatImage(&inputImage, filterType, nIterations, wr, wa, minVal);
    }

/*
   Output — GeoTIFF+VRT or binary stdout
*/
    if (tiffOut) {
        char buf[64];
        if (radiusMap) {
            insert_node(&metaData, "filterfloat_radiusMap", radiusMap);
            snprintf(buf, sizeof(buf), "%.6g", pixRatio);
            insert_node(&metaData, "filterfloat_pixRatio", buf);
        } else {
            snprintf(buf, sizeof(buf), "%d", wr);
            insert_node(&metaData, "filterfloat_wr", buf);
            snprintf(buf, sizeof(buf), "%d", wa);
            insert_node(&metaData, "filterfloat_wa", buf);
            insert_node(&metaData, "filterfloat_type",
                        filterType == HIPASS ? "hipass" : "lopass");
        }
        snprintf(buf, sizeof(buf), "%d", nIterations);
        insert_node(&metaData, "filterfloat_nIterations", buf);
        snprintf(buf, sizeof(buf), "%.6g", minVal);
        insert_node(&metaData, "filterfloat_minValue", buf);
        writeImageAsVRT(tiffOut, &inputImage, geoTransform, projWKT, metaData,
                        (float)minVal);
    } else {
        for (i = 0; i < inputImage.azimuthSize; i++) {
            if (byteOrder == MSB)
                fwriteBS(inputImage.phase[i],
                         inputImage.rangeSize, sizeof(float), stdout, FLOAT32FLAG);
            else
                fwrite(inputImage.phase[i], sizeof(float), inputImage.rangeSize, stdout);
        }
    }
    return;
}


    static void readArgs(int argc, char *argv[], int *nr, int *na,
                         char **imageFile, double *minValue,
                         int *nIterations, int *wa, int *wr, int *filterType,
                         int *byteOrder, char **inputVRT, char **tiffOut,
                         char **radiusMap, double *pixRatio)
{
    char *argString;
    int w;
    int i, n;

    if (argc < 2) usage();        /* Check number of args */
    *nr = 1248;
    *na = 1318;
    *nIterations = 1;
    *wa = 50;
    *wr = 50;
    *minValue = -2.0E9;
    *filterType = LOPASS;
    *byteOrder = MSB;
    *inputVRT = NULL;
    *tiffOut  = NULL;
    *radiusMap = NULL;
    *pixRatio = 1.0;
    n = argc - 1;
    for (i = 1; i <= n; i++) {
        argString = strchr(argv[i], '-');
        if (argString == NULL) { usage(); }
        if (strstr(argString, "inputVRT") != NULL) {
            *inputVRT = argv[++i];
        } else if (strstr(argString, "radiusMap") != NULL) {
            *radiusMap = argv[++i];
        } else if (strstr(argString, "pixRatio") != NULL) {
            sscanf(argv[++i], "%lf", pixRatio);
        } else if (strstr(argString, "tiff") != NULL) {
            *tiffOut = argv[++i];
        } else if (strstr(argString, "nr") != NULL) {
            sscanf(argv[i+1], "%i", nr);
            i++;
        } else if (strstr(argString, "na") != NULL) {
            sscanf(argv[i+1], "%i", na);
            i++;
        } else if (strstr(argString, "nIterations") != NULL) {
            sscanf(argv[i+1], "%i", nIterations);
            i++;
        } else if (strstr(argString, "w") != NULL) {
            sscanf(argv[i+1], "%i", &w);
            i++;
            if (strstr(argString, "a") != NULL) *wa = w;
            else if (strstr(argString, "r") != NULL) *wr = w;
            else { *wr = w; *wa = w; }
        } else if (strstr(argString, "minValue") != NULL) {
            sscanf(argv[i+1], "%lf", minValue);
            i++;
        } else if (strstr(argString, "hiPass") != NULL) {
            *filterType = HIPASS;
        } else if (strstr(argString, "LSB") != NULL) {
            *byteOrder = LSB;
        } else usage();
    }
    *imageFile = NULL;   /* filterfloat always reads from stdin in binary mode */
}

/*
  Read a single-band byte GeoTIFF/VRT (e.g. a .smr smoothing-radius map) into a row-major 2D
  array. Modeled on siminsar.c's readMaskFile() GDAL path -- a generic float reader like
  readRasterVRT() assumes GDT_Float32 internally and isn't safe to reuse for byte data.
*/
    static unsigned char **readByteRaster(const char *file, int *xSize, int *ySize)
{
    GDALDatasetH hDS;
    GDALRasterBandH hBand;
    unsigned char *storage;
    unsigned char **image;
    int i;

    /* filterfloat's main() never calls this (legacy stdin/stdout path doesn't need
       GDAL); -inputVRT/-tiff get it for free via vrtIO.c's readVRTIntoImage()/
       writeImageAsVRT(), but -radiusMap can be used without either, so register here.
       Idempotent -- safe even if already called. */
    GDALAllRegister();
    hDS = GDALOpen(file, GA_ReadOnly);
    if (hDS == NULL)
        error("readByteRaster: cannot open %s with GDAL\n", file);
    *xSize = GDALGetRasterXSize(hDS);
    *ySize = GDALGetRasterYSize(hDS);
    hBand = GDALGetRasterBand(hDS, 1);
    storage = (unsigned char *)malloc((size_t)(*xSize) * (*ySize));
    GDALRasterIO(hBand, GF_Read, 0, 0, *xSize, *ySize, storage, *xSize, *ySize, GDT_Byte, 0, 0);
    GDALClose(hDS);

    image = (unsigned char **)malloc(*ySize * sizeof(unsigned char *));
    for (i = 0; i < *ySize; i++)
        image[i] = &storage[(size_t)i * (*xSize)];
    return image;
}


    static void usage()
{
    error(
    "\n\n%s\n%s\n\n%s\n%s\n\n%s\n\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
    "Filter a floating point image ignoring values < minValue using a rectangular kernel.",
    "Input is stdin (binary) or a VRT; output is stdout (binary) or a GeoTIFF+VRT.",
    "Usage (binary):  filterfloat [opts] < input.bin > output.bin",
    "Usage (VRT):     filterfloat -inputVRT in.vrt -tiff out.vrt [opts]",
    "where",
    "  inputVRT    = read from georeferenced VRT (auto-detects nr/na)",
    "  tiff        = write GeoTIFF+VRT instead of binary stdout",
    "  radiusMap   = per-pixel byte raster (azimuth half-width in pixels, e.g. siminsar's",
    "                .smr) for variable-radius smoothing instead of fixed wr/wa",
    "  pixRatio    = azimuthPixelSize/rangePixelSize, used with -radiusMap to derive the",
    "                range half-width from the azimuth half-width (default=1.0, isotropic)",
    "  nr,na       = range and azimuth size in pixels (default=1248x1318)",
    "  minValue    = values <= minValue ignored (default=-2e9)",
    "  w           = width of filter kernel (default=50)",
    "  wr,wa       = filter width in range and azimuth directions (default=50)",
    "  nIterations = repeat filtering (triangular kernel for nIterations=2)",
    "  hiPass      = high-pass filter: image - lowpass(image)",
    "  LSB         = use LSB byte order for binary i/o (default MSB)"
    );
}
