#include <stdint.h>
#define WDIST 1
#define LINEAR 2
#define QUADRATIC 3

typedef struct holeType
{
	int32_t label;
	int32_t nB; /* Number of border pixels */
	int32_t nH; /* Number of hole pixels */
	int32_t cH;
	int32_t cB;
	int32_t *xb, *yb; /* Border coords */
	int32_t *xh, *yh; /* Hole coords */
} hole;

typedef struct unwrapPhaseTypeDef
{
	int32_t rangeSize;
	int32_t azimuthSize;
	int32_t passType;
	float **phase;
} unwrapPhaseImage;
/*
  Reflatten image using new baselines.
*/
void interpFloatImage(unwrapPhaseImage *inputImage, int32_t interpLength, int32_t padEdges, double minValue, int32_t intType,
					  int32_t thresh, float ratThresh, int32_t fastFlag, int32_t allowBreaks);
void getPhaseImage(char *inputFile, int32_t nr, int32_t na, unwrapPhaseImage *inputImage);

void clipIslands(unwrapPhaseImage *inputImage, double minValue, int32_t intType, int32_t fastFlag, int32_t sizeThresh);
