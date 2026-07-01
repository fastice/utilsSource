#ifndef VRTIO_H
#define VRTIO_H
/*
 * vrtIO.h — GDAL read/write helpers for filterfloat and intfloat.
 */
#include "intFloat.h"
#include "gdalIO/gdalIO/grimpgdal.h"

/*
 * readVRTIntoImage — open any single-band float GDAL dataset (VRT, GeoTIFF,
 * etc.) and populate an unwrapPhaseImage.  NaN pixels are replaced by
 * (-2e9 - 1) so they register as holes in downstream processing.
 */
int readVRTIntoImage(const char *file, unwrapPhaseImage *image,
                     double geoTransform[6], char **projWKT,
                     dictNode **metaData);

/*
 * writeImageAsTiff — write the image as a GeoTIFF only (no VRT wrapper).
 * Used for -outputVRT -tiff -noVRT mode.
 */
int writeImageAsTiff(const char *outTif, const unwrapPhaseImage *image,
                     const double geoTransform[6], const char *projWKT,
                     dictNode *metaData, float noDataValue);

/*
 * writeImageAsVRT — write a GeoTIFF + VRT wrapper.
 *   outVRT   e.g. "result.vrt" — also creates "result.tif"
 * Used by filterfloat and intfloat -outputVRT -tiff modes.
 */
int writeImageAsVRT(const char *outVRT, const unwrapPhaseImage *image,
                    const double geoTransform[6], const char *projWKT,
                    dictNode *metaData, float noDataValue);

/*
 * writeBinaryVRT — write the image as a raw big/little-endian float32 binary
 * file, with an optional VRT sidecar using VRTRawRasterBand.
 *   outVRT     e.g. "result.vrt" — data file is "result" (no extension)
 *   byteOrder  MSB (1) or LSB (0)
 *   noVRT      if non-zero, skip the VRT wrapper (write data file only)
 */
int writeBinaryVRT(const char *outVRT, const unwrapPhaseImage *image,
                   const double geoTransform[6], const char *projWKT,
                   dictNode *metaData, float noDataValue,
                   unsigned char byteOrder, int noVRT);

#endif /* VRTIO_H */
