/*
 * vrtIO.c — GDAL read/write helpers for filterfloat and intfloat.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "vrtIO.h"
#include "clib/standard.h"

#ifndef MSB
#define MSB 1
#define LSB 0
#endif

/* ------------------------------------------------------------------ */
/* Fill out[6] from in[6], substituting a pixel-identity transform when
 * all elements are zero (dataset had no georeferencing). */
static void effectiveGT(const double in[6], double out[6])
{
    memcpy(out, in, 6 * sizeof(double));
    for (int k = 0; k < 6; k++)
        if (out[k] != 0.0) return;
    out[0] = 0.0; out[1] = 1.0; out[2] = 0.0;
    out[3] = 0.0; out[4] = 0.0; out[5] = -1.0;
}

/* ------------------------------------------------------------------ */
/* Write the VRT header block (dataset element, SRS, GeoTransform,
 * Metadata) to an already-open FILE. */
static void writeVRTHeader(FILE *fp, int nr, int na,
                            const double gt[6], const char *projWKT,
                            dictNode *metaData)
{
    double egt[6];
    effectiveGT(gt, egt);
    fprintf(fp, "<VRTDataset rasterXSize=\"%d\" rasterYSize=\"%d\">\n", nr, na);
    if (projWKT && projWKT[0])
        fprintf(fp, "  <SRS>%s</SRS>\n", projWKT);
    fprintf(fp, "  <GeoTransform>"
                " %.17g, %.17g, %.17g, %.17g, %.17g, %.17g"
                "</GeoTransform>\n",
            egt[0], egt[1], egt[2], egt[3], egt[4], egt[5]);
    if (metaData) {
        fprintf(fp, "  <Metadata>\n");
        for (dictNode *n = metaData; n; n = n->next)
            fprintf(fp, "    <MDI key=\"%s\">%s</MDI>\n", n->key, n->value);
        fprintf(fp, "  </Metadata>\n");
    }
}

/* ------------------------------------------------------------------ */
/*
 * appendSuffix — stub required because gdalIO.o references it (via
 * checkForVrt) even though we never call that function ourselves.
 */
char *appendSuffix(char *file, char *suffix, char *buf)
{
    strcpy(buf, file);
    strcat(buf, suffix);
    return buf;
}

/* ------------------------------------------------------------------ */
int readVRTIntoImage(const char *file, unwrapPhaseImage *image,
                     double geoTransform[6], char **projWKT,
                     dictNode **metaData)
{
    GDALAllRegister();

    GDALDatasetH hDS = GDALOpen(file, GA_ReadOnly);
    if (!hDS) {
        fprintf(stderr, "readVRTIntoImage: cannot open '%s'\n", file);
        return 1;
    }

    int xSize = GDALGetRasterXSize(hDS);
    int ySize = GDALGetRasterYSize(hDS);
    image->rangeSize   = (int32_t)xSize;
    image->azimuthSize = (int32_t)ySize;

    GDALGetGeoTransform(hDS, geoTransform);

    const char *wkt = GDALGetProjectionRef(hDS);
    *projWKT = (wkt && wkt[0]) ? CPLStrdup(wkt) : NULL;

    readDataSetMetaData(hDS, metaData);

    image->phase    = (float **)malloc((size_t)ySize * sizeof(float *));
    image->phase[0] = (float *)malloc((size_t)xSize * ySize * sizeof(float));
    for (int ia = 1; ia < ySize; ia++)
        image->phase[ia] = image->phase[0] + (size_t)ia * xSize;

    GDALRasterBandH hBand = GDALGetRasterBand(hDS, 1);
    for (int ia = 0; ia < ySize; ia++)
        GDALRasterIO(hBand, GF_Read, 0, ia, xSize, 1,
                     image->phase[ia], xSize, 1, GDT_Float32, 0, 0);

    /* Replace NaN with a sub-minValue sentinel so they count as holes. */
    for (int ia = 0; ia < ySize; ia++)
        for (int ir = 0; ir < xSize; ir++)
            if (isnan(image->phase[ia][ir]))
                image->phase[ia][ir] = (float)(-2.0e9 - 1.0);

    GDALClose(hDS);
    return 0;
}

/* ------------------------------------------------------------------ */
int writeImageAsTiff(const char *outTif, const unwrapPhaseImage *image,
                     const double geoTransform[6], const char *projWKT,
                     dictNode *metaData, float noDataValue)
{
    int nr = image->rangeSize;
    int na = image->azimuthSize;

    GDALAllRegister();
    GDALDriverH hDriver = GDALGetDriverByName("GTiff");
    if (!hDriver) {
        fprintf(stderr, "writeImageAsTiff: GTiff driver not available\n");
        return 1;
    }
    GDALDatasetH hDS = GDALCreate(hDriver, outTif, nr, na, 1, GDT_Float32, NULL);
    if (!hDS) {
        fprintf(stderr, "writeImageAsTiff: cannot create '%s'\n", outTif);
        return 1;
    }

    double gt[6];
    effectiveGT(geoTransform, gt);
    GDALSetGeoTransform(hDS, gt);
    if (projWKT && projWKT[0])
        GDALSetProjection(hDS, projWKT);

    GDALRasterBandH hBand = GDALGetRasterBand(hDS, 1);
    GDALSetRasterNoDataValue(hBand, (double)noDataValue);
    for (int ia = 0; ia < na; ia++)
        GDALRasterIO(hBand, GF_Write, 0, ia, nr, 1,
                     image->phase[ia], nr, 1, GDT_Float32, 0, 0);

    writeDataSetMetaData(hDS, metaData);
    GDALClose(hDS);
    return 0;
}

/* ------------------------------------------------------------------ */
int writeImageAsVRT(const char *outVRT, const unwrapPhaseImage *image,
                    const double geoTransform[6], const char *projWKT,
                    dictNode *metaData, float noDataValue)
{
    int nr = image->rangeSize;
    int na = image->azimuthSize;

    /* Derive .tif path from outVRT */
    char tifPath[4096];
    strncpy(tifPath, outVRT, sizeof(tifPath) - 5);
    tifPath[sizeof(tifPath) - 5] = '\0';
    char *dot = strrchr(tifPath, '.');
    if (dot && strcmp(dot, ".vrt") == 0)
        strcpy(dot, ".tif");
    else
        strcat(tifPath, ".tif");

    if (writeImageAsTiff(tifPath, image, geoTransform, projWKT, metaData, noDataValue) != 0)
        return 1;

    /* VRT basename for relativeToVRT reference */
    const char *tifBase = strrchr(tifPath, '/');
    tifBase = tifBase ? tifBase + 1 : tifPath;

    FILE *fp = fopen(outVRT, "w");
    if (!fp) {
        fprintf(stderr, "writeImageAsVRT: cannot open '%s'\n", outVRT);
        return 1;
    }

    double gt[6];
    effectiveGT(geoTransform, gt);
    writeVRTHeader(fp, nr, na, gt, projWKT, metaData);

    fprintf(fp, "  <VRTRasterBand dataType=\"Float32\" band=\"1\">\n");
    fprintf(fp, "    <NoDataValue>%.17g</NoDataValue>\n", (double)noDataValue);
    fprintf(fp, "    <SimpleSource>\n");
    fprintf(fp, "      <SourceFilename relativeToVRT=\"1\">%s</SourceFilename>\n", tifBase);
    fprintf(fp, "      <SourceBand>1</SourceBand>\n");
    fprintf(fp, "      <SourceProperties RasterXSize=\"%d\" RasterYSize=\"%d\""
                " DataType=\"Float32\" BlockXSize=\"256\" BlockYSize=\"256\" />\n", nr, na);
    fprintf(fp, "      <SrcRect xOff=\"0\" yOff=\"0\" xSize=\"%d\" ySize=\"%d\" />\n", nr, na);
    fprintf(fp, "      <DstRect xOff=\"0\" yOff=\"0\" xSize=\"%d\" ySize=\"%d\" />\n", nr, na);
    fprintf(fp, "    </SimpleSource>\n");
    fprintf(fp, "  </VRTRasterBand>\n");
    fprintf(fp, "</VRTDataset>\n");
    fclose(fp);
    return 0;
}

/* ------------------------------------------------------------------ */
int writeBinaryVRT(const char *outVRT, const unwrapPhaseImage *image,
                   const double geoTransform[6], const char *projWKT,
                   dictNode *metaData, float noDataValue,
                   unsigned char byteOrder, int noVRT)
{
    int nr = image->rangeSize;
    int na = image->azimuthSize;

    /* Derive binary data path by stripping .vrt */
    char binPath[4096];
    strncpy(binPath, outVRT, sizeof(binPath) - 1);
    binPath[sizeof(binPath) - 1] = '\0';
    char *dot = strrchr(binPath, '.');
    if (dot && strcmp(dot, ".vrt") == 0)
        *dot = '\0';

    /* Write binary float32 data */
    FILE *fp = fopen(binPath, "wb");
    if (!fp) {
        fprintf(stderr, "writeBinaryVRT: cannot open '%s'\n", binPath);
        return 1;
    }
    for (int ia = 0; ia < na; ia++) {
        if (byteOrder == MSB)
            fwriteBS(image->phase[ia], nr * sizeof(float), 1, fp, FLOAT32FLAG);
        else
            fwrite(image->phase[ia], sizeof(float), nr, fp);
    }
    fclose(fp);

    if (noVRT) return 0;

    /* VRT with VRTRawRasterBand pointing to the binary file */
    const char *binBase = strrchr(binPath, '/');
    binBase = binBase ? binBase + 1 : binPath;

    double gt[6];
    effectiveGT(geoTransform, gt);

    FILE *vfp = fopen(outVRT, "w");
    if (!vfp) {
        fprintf(stderr, "writeBinaryVRT: cannot open '%s'\n", outVRT);
        return 1;
    }
    writeVRTHeader(vfp, nr, na, gt, projWKT, metaData);

    fprintf(vfp, "  <VRTRasterBand dataType=\"Float32\" band=\"1\""
                 " subClass=\"VRTRawRasterBand\">\n");
    fprintf(vfp, "    <NoDataValue>%.17g</NoDataValue>\n", (double)noDataValue);
    fprintf(vfp, "    <SourceFilename relativeToVRT=\"1\">%s</SourceFilename>\n", binBase);
    fprintf(vfp, "    <ImageOffset>0</ImageOffset>\n");
    fprintf(vfp, "    <PixelOffset>4</PixelOffset>\n");
    fprintf(vfp, "    <LineOffset>%d</LineOffset>\n", 4 * nr);
    fprintf(vfp, "    <ByteOrder>%s</ByteOrder>\n", byteOrder == MSB ? "MSB" : "LSB");
    fprintf(vfp, "  </VRTRasterBand>\n");
    fprintf(vfp, "</VRTDataset>\n");
    fclose(vfp);
    return 0;
}
