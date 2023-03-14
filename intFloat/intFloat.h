#define WDIST 1
#define LINEAR 2
#define QUADRATIC 3

typedef struct holeType {
	int label;
	int nB;       /* Number of border pixels */
	int nH;       /* Number of hole pixels */
	int cH;
	int cB;
	int *xb,*yb;  /* Border coords */
	int *xh,*yh;  /* Hole coords */
} hole;

typedef struct unwrapPhaseTypeDef {
	int rangeSize;
	int azimuthSize;
	int passType;
	float **phase;
} unwrapPhaseImage;
/*
  Reflatten image using new baselines.
*/
void interpFloatImage(unwrapPhaseImage *inputImage, int interpLength,  int padEdges,double minValue, int intType,
		      int thresh,float ratThresh, int fastFlag, int allowBreaks);
void getPhaseImage(char *inputFile, int nr, int na,  unwrapPhaseImage *inputImage);

void clipIslands(unwrapPhaseImage *inputImage, double minValue, int intType, int fastFlag,  int sizeThresh);


