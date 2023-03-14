#include "stdio.h"
#include "string.h"
#include "intFloat.h"
#include "math.h"
#include <stdlib.h>
#include "unwrapSource/ers1Code/ers1.h"
#include "unwrapSource/unWrap/unwrap.h"
#include "cRecipes/nrutil.h"

typedef struct intDataType {
        float x;
        float y;
} intData;

static void borderPixels(int lList[9], int **label,int *nLab,   int i,int j,int nr,int na, int fastFlag);
static void findPoints(int j,hole holes,intData *data,double *zTmp, double *dTmp,float **image,int *nPts,  float radius,float *minD);
static float wDistInterp(intData *data,double *zTmp,double *dTmp,
			 int nPts,float x,float y);

static void planeCoeffs(void *x,int i,double *afunc, int ma);
static void quadCoeffs(void *x,int i,double *afunc, int ma);

static float planeInterp(intData *data,double *zTmp,double *dTmp, int nPts, float x, float y, double **u,  double **v,double *w, double a[4], int update);

static float quadInterp(intData *data,double *zTmp,double *dTmp, int nPts, float x, float y, double **u,double **v,double *w, double a[7], int update);

static void clipBorders(unwrapPhaseImage *inputImage, char **mask, double minValue, int *width,int *height);
static void screenHoles(hole *holes, int maxLabel, int thresh,  int width, int height,    int interpLength,float ratThresh,int allowBreaks);


static int  findMaxLabel(int **labels, int nr, int na) {
	int i, j, maxLabel;
	maxLabel=0;   
	for(i=0; i < na; i++)  
		for(j=0; j < nr; j++ ) 
			if(labels[i][j] < LARGEINT) maxLabel=max(maxLabel,labels[i][j]);
	fprintf(stderr,"1maxLabel = %i \n",maxLabel);
	return maxLabel;
}

static hole *createHoles(int maxLabel) {
	int i, j;
	hole *holes;
	holes = (hole *)malloc(sizeof(hole)*(maxLabel+1));
	for(i=1; i <= maxLabel; i++ ) { 
		holes[i].nH = 0; holes[i].nB=0;
		holes[i].cH = 0; holes[i].cB=0;
	}
	return holes;
}

static void countHoles(hole *holes, int **labels, char **mask, int nr, int na, int *lList, int nLab, int fastFlag) {
	int i, j,k;
	for(i=0; i < na; i++)  
		for(j=0; j < nr; j++ ) {
			if(labels[i][j] < LARGEINT && validPixel(mask[i][j])==TRUE) {
				(holes[labels[i][j]].nH)++;
			} else if(validPixel(mask[i][j])) {
				borderPixels(lList,labels,&nLab,i,j,nr,na,fastFlag);
				if(nLab != 0) mask[i][j] |=BORDER;
				for(k=0; k < nLab; k++) (holes[lList[k]].nB)++;          
			}            
		}
}

static void saveHoles(hole *holes, int **labels, char **mask, int nr, int na, int *lList, int nLab, int fastFlag) {
	int i, j, k;
	fprintf(stderr,"Pass 3 -  saving holes and borders\n");
	for(i=0; i < na; i++)  
		for(j=0; j < nr; j++ ) {
			if(labels[i][j] < LARGEINT && validPixel(mask[i][j])==TRUE) {
				holes[labels[i][j]].xh[holes[labels[i][j]].cH]=j;
				holes[labels[i][j]].yh[holes[labels[i][j]].cH]=i;
				(holes[labels[i][j]].cH)++;
			} else if(validPixel(mask[i][j]) && (mask[i][j] & BORDER)) {
				borderPixels(lList,labels,&nLab,i,j,nr,na,fastFlag);
				for(k=0; k < nLab; k++) {
					holes[lList[k]].xb[holes[lList[k]].cB]=j;
					holes[lList[k]].yb[holes[lList[k]].cB]=i;
					(holes[lList[k]].cB)++;          
				}
			}            
		}
}
/*
  Interpolate floating point image.
*/
void interpFloatImage(unwrapPhaseImage *inputImage, int interpLength,  int padEdges,double minValue, int intType,
		      int thresh,float ratThresh, int fastFlag,  int allowBreaks)
{
	unwrapImageStructure  uwImage;     /* This is to fake out for using 
					      unwrap image connect components */
	hole *holes;
	int x,y;                /* Current location in hole */
	int xp,yp;              /* Hole point where last update occured */
	double a[8];
	char **mask;            /* Mask used for mark pixels for cc alg */
	intData *data;          /* xy points use in interpolation for curr pt */
	double *zTmp,*dTmp;     /* Data points use in interpolation for curr pt */
	double **u, **v, *w;    /* Tmp arrays use by svd */
	double dh;              /* Distance between consecutive points in hole */
	int **labels;           /* connect components labeled output */      
	int nr,na;              /* Size in range and azimuth */
	int ma;                 /* Number of parameters interpolater estimate */
	int i,j,k;              /* Loop variables */
	int width, height;
	int maxLabel;           /* Largest labels from cc */
	int *buf;               /* Temp variable used for mem allocation */
	int lList[25],nLab;      /* List of labels for border point */
	int nPts;  
	float minD;
	int skipValue;
	int maxBorders;         /* Maximum border points over all holes */
	int skip, update;       /* Flags used in interpolation loop */
	FILE *fp;
	/*
	  Check interpLength
	*/
	if(interpLength < 2) error("interpolateFloatImage -- interpLenth < 2\n");
	ma=3;
	if(intType==QUADRATIC) ma=6;
	skipValue=5;
	if(fastFlag == TRUE) skipValue=7;
	/*
	  Init  image stuff
	*/
	na = inputImage->azimuthSize;
	nr = inputImage->rangeSize;
	/*
	  Mask and label  images
	*/
	fprintf(stderr,"minvalue %f\n",minValue);
	mask  = (char **) malloc(na * sizeof(char *) );
	labels  = (int **) malloc(na * sizeof(int *) );
	for(i=0; i < na; i++) {
		mask[i] = (char *)  malloc(nr * sizeof(char) );
		labels[i] = (int *) malloc(nr * sizeof(int) );
		for( j=0; j < nr; j++ ) {
			mask[i][j]= VALIDPIXEL;
			if(inputImage->phase[i][j] <= minValue) mask[i][j] +=REGION;
			labels[i][j] = 0;
		}
        }
	if(padEdges == FALSE) clipBorders(inputImage,mask,minValue,&width,&height);
	/*
	  Copy to uw structure for connect components.
	*/
	uwImage.phase=inputImage->phase;
	uwImage.rangeSize=inputImage->rangeSize;
	uwImage.azimuthSize=inputImage->azimuthSize;
	uwImage.bCuts=mask;
	uwImage.labels=labels;
	/*
	  Run connected components
	*/
	fprintf(stderr,"Labeling regions\n");
	labelRegions(&uwImage);
	/*
	  Pass 1: Get max label
	*/
	maxLabel = findMaxLabel(labels, nr, na);
	fprintf(stderr,"Pass 1 -- checking labels\n");
	holes = createHoles(maxLabel);
	/*
	  Pass 2: Count holes and border pixels
	*/
	fprintf(stderr,"Pass 2 - counting holes and borders\n");
	countHoles(holes, labels, mask,  nr, na, lList,nLab, fastFlag);
	/*
	  Now allocate space for arrays
	*/
	maxBorders=0;
	for(i=1; i <= maxLabel; i++ ) {
		if(holes[i].nH > 0 ) {
			holes[i].xb=(int *)malloc(holes[i].nB*sizeof(int));
			holes[i].yb=(int *)malloc(holes[i].nB*sizeof(int));
			holes[i].xh=(int *)malloc(holes[i].nH*sizeof(int));
			holes[i].yh=(int *)malloc(holes[i].nH*sizeof(int));
			maxBorders=max(holes[i].nB,maxBorders);
		}           
	}
	/* Set up interpolator */
	data=(intData *)malloc(sizeof(intData)*maxBorders);
	zTmp=(double *)malloc(sizeof(double)*maxBorders);
	dTmp=(double *)malloc(sizeof(double)*maxBorders);
	u = dmatrix(1, maxBorders+1,1,ma);
	v = dmatrix(1,ma,1,ma);
	w = dvector(1,ma);
	/*
	  Pass 3: Save locations of hole and border pixels
	*/
	saveHoles(holes, labels, mask,  nr, na, lList, nLab, fastFlag);
	/*
	  screen Holes
	*/
	screenHoles(holes,maxLabel,thresh,width,height,interpLength,ratThresh,allowBreaks);
	/*
	  Now loop through holes and points interpolating
	*/
	fprintf(stderr,"Fixing holes\n");
	for(i=1; i < maxLabel; i++) {
		if(holes[i].nH > 0) {
			xp=-1000.; yp=-1000.;
			skip=1; minD=0;
			/* for large holels use subset */
			if(holes[i].nH > 100) skip=skipValue;/* find pts every skip pixls */
			for(j=0; j < holes[i].nH; j++) {
				x=holes[i].xh[j]; y=holes[i].yh[j];
				dh=sqrt( (x-xp)*(x-xp) + (y-yp)*(y-yp));
				update=FALSE;
				if(dh >= skip || minD < skip) { 
					findPoints(j,holes[i],data,zTmp,dTmp,
						   inputImage->phase,&nPts,(float)interpLength,&minD);
					update=TRUE;
					xp=x; yp=y; /* Location of last update */
				} 
				if(nPts > 3) {
					if(intType == WDIST || (nPts < 6 && intType==QUADRATIC)) 
						inputImage->phase[y][x]= wDistInterp(data,zTmp,dTmp,nPts,x,y);
					else if(intType==LINEAR)
						inputImage->phase[y][x]=planeInterp(data,zTmp,dTmp,nPts,x,y,u,v,w,a,update);
					else if(intType==QUADRATIC) 
						inputImage->phase[y][x]= quadInterp(data,zTmp,dTmp,nPts,x,y,u,v,w,a,update);
					else error("intFloatImage: Invalid interpolation type");
				}
			} 
		} /* End if(holes... */
	} /* end i */
}

/*
  Use various criteria to avoid fixing holes
*/
static void screenHoles(hole *holes, int maxLabel, int thresh, 
			int width, int height,  int interpLength,float ratThresh,int allowBreaks)
{
	int i,j;
	int minX,maxX,minY,maxY;
	float ratio;

	for(i=1; i < maxLabel; i++) {
		if(holes[i].nH >= thresh) {
			fprintf(stderr,"rejecting %i for size %i > %i \n",
				i,holes[i].nH,thresh);
			holes[i].nH=0;
		}
		minX=LARGEINT; maxX=-LARGEINT; minY=LARGEINT; maxY=-LARGEINT; 
		for(j=0; j < holes[i].nH; j++ ) {
			minX=min(minX,holes[i].xh[j]);
			maxX=max(maxX,holes[i].xh[j]);
			minY=min(minY,holes[i].yh[j]);
			maxY=max(maxY,holes[i].yh[j]);
		}
		/*
		  Breaks image in two - reject
		*/
		if(allowBreaks == FALSE) {
			if( (maxX-minX+1) >= width) { 
				if(holes[i].nH !=0) 
					fprintf(stderr,"rejecting %i for horiz break\n",i);
				holes[i].nH=0; 
			} if( (maxY-minY+1) >= height) {
				if(holes[i].nH !=0)
					fprintf(stderr,"rejecting %i for vert break\n",i);
				holes[i].nH=0;
			}
		}
		/*
		  Allows saving things that are long and skinny
		*/
		ratio=(float)(holes[i].nH)/(float)((maxX-minX+1)*(maxY-minY+1));
		if( holes[i].nH > (interpLength*interpLength) && (ratio > ratThresh)) {
			fprintf(stderr,"rejecting %i for ratio %f > %f \n",
				i,ratio,ratThresh);
			holes[i].nH=0;
		}
	}
}


static float planeInterp(intData *data,double *zTmp,double *dTmp,
			 int nPts, float x, float y, double **u,
			 double **v,double *w, double a[4], int update)
{
	int ma;
	int i;
	double chisq;
	float result;
	intData *dat;
    
	if(update==TRUE) {   
		ma=3;
		data--;
		zTmp--;
		dTmp--;
		svdfit((void *)data,zTmp,dTmp,nPts,a,ma,u,v,w, &chisq, &planeCoeffs);
	}
	result = (float)(a[1] + a[2]*x + a[3]*y);
	return result;    
}


static void planeCoeffs(void *x,int i,double *afunc, int ma) 
{
	/*
	  Plane equation
	*/
	intData *xy; 
	xy = (intData *)x;
	afunc[1] = 1;
	afunc[2] = (double)(xy[i].x);
	afunc[3] = (double)(xy[i].y); 
	return;
}


static float quadInterp(intData *data,double *zTmp,double *dTmp,
			int nPts, float x, float y, double **u,
			double **v,double *w, double a[7], int update)
{
	int ma;
	int i;
	double chisq;
	float result;
	intData *dat;
    
	if(update==TRUE) {   
		ma=6;
		data--;
		zTmp--;
		dTmp--;
		svdfit((void *)data,zTmp,dTmp,nPts,a,ma,u,v,w, &chisq, &quadCoeffs); 
	}
	result =(float)(a[1] + a[2]*x + a[3]*y + a[4]*x*y + a[5]*x*x + a[6]*y*y);
	return result;    
}


static void quadCoeffs(void *x,int i,double *afunc, int ma) 
{
	/*
	  quadratic function
	*/
	intData *xy; 
	xy = (intData *)x;
	afunc[1] = 1;
	afunc[2] = (double)(xy[i].x);
	afunc[3] = (double)(xy[i].y); 
	afunc[4] = (double)(xy[i].y * xy[i].x);
	afunc[5] = (double)(xy[i].x * xy[i].x);
	afunc[6] = (double)(xy[i].y * xy[i].y);
	return;
}


static float wDistInterp(intData *data,double *zTmp,double *dTmp, int nPts,float x,float y)
{
	int i;
	double weight;
	double sum;
	double w;
	float result;

	sum=0.; weight=0.;
	for(i=0; i < nPts; i++) {
		w = 1.0/(dTmp[i]*dTmp[i]);
		sum += w * zTmp[i];
		weight += w;
	}
	result=(float)(sum/weight);
	return result;
}


static void findPoints(int j,hole holes,intData *data,double *zTmp,
		       double *dTmp,float **image,int *nPts,
		       float radius,float *minD)
{
	int i;
	float r;
	double d;
	r=radius*radius;
	*nPts=0;
	*minD=(float)LARGEINT;
	for(i=0; i < holes.nB; i++) {
		d= (double)((holes.xh[j]-holes.xb[i])*(holes.xh[j]-holes.xb[i]));
		d += (double)((holes.yh[j]-holes.yb[i])*(holes.yh[j]-holes.yb[i]));
		if(d < r) {
			data[*nPts].x=holes.xb[i];
			data[*nPts].y=holes.yb[i];
			zTmp[*nPts]=image[holes.yb[i]][holes.xb[i]];
			dTmp[*nPts]=sqrt(d);
			*minD=min(*minD,dTmp[*nPts]);
			(*nPts)++;
		}
	}
}

     
/*
  Determine if pixel is a border pixel
*/
static void borderPixels(int lList[9], int **label,int *nLab,
			 int i,int j,int nr,int na, int fastFlag)
{
	int i1, j1,k;
	int exist;
	int nd;
   
	nd=2;
	if(fastFlag==TRUE) nd=1;

	for(k=0; k < 9; k++) lList[k]=LARGEINT;
	*nLab=0;
	for(i1=max(0,i-nd); i1 <= min(i+nd,na-nd); i1++) {
		for(j1=max(0,j-nd); j1 <= min(j+nd,nr-nd); j1++) {
			if(label[i1][j1] < LARGEINT) { /* Found label */               
				exist=FALSE;
				for(k=0; k < *nLab; k++)  /* Check label not found already */
					if(lList[k]==label[i1][j1]) exist=TRUE;
				/* New label so add to list */
				if(exist==FALSE) { 
					lList[*nLab]=label[i1][j1];
					(*nLab)++;    
				}
			} /* if(la.. */
		}/* j1 */
	} /* i1 */
}



static void clipBorders(unwrapPhaseImage *inputImage, char **mask, double minValue,int *width, int *height)
{
	int nr,na;
	int na1,nr1,na2,nr2,count;
	int i,j;
	/* 
	   Clip borders
	*/
	na = inputImage->azimuthSize;
	nr = inputImage->rangeSize;
	/*
	  Do top,bottome,left,right edges
	*/
	na1 = 0;  count=0;
	while(count ==  0 && na1 < na) { 
		for(j=0; j < nr; j++ ) if(inputImage->phase[na1][j] > minValue) count++;
		na1++;
	} 
	for(i=0; i < na1; i++)  for(j=0; j < nr; j++ ) mask[i][j] &= INVALIDPIXEL;

	na2 = na - 1; count=0;
	while(count == 0 && na2 >= 0) { 
		for(j=0; j < nr; j++ ) if(inputImage->phase[na2][j] > minValue) count++;
		na2--;
	} 
	for(i=na2+1; i < na; i++) for(j=0; j < nr; j++ ) mask[i][j] &= INVALIDPIXEL;

	nr1 = 0; count=0;
	while(count == 0 && nr1 < nr) { 
		for(i=0; i < na; i++ ) if(inputImage->phase[i][nr1] > minValue) count++;
		nr1++;	
	} 
	for(i=0; i < na; i++)  for(j=0; j < nr1; j++ ) mask[i][j] &= INVALIDPIXEL;
	nr2 = nr - 1; count=0;
	while(count == 0 && nr2 >=0) { 
		for(i=0; i < na; i++ ) if(inputImage->phase[i][nr2] > minValue) count++;
		nr2--;	
	} 
	for(i=0; i < na; i++) for(j=nr2+1; j < nr; j++ ) mask[i][j] &= INVALIDPIXEL;
	fprintf(stderr,"nr1,nr2,na1,na2 %i %i %i %i\n",nr1,nr2,na1,na2);
	*width=nr2-nr1+1; *height=na2-na1+1;
}


/*
  Throw out valid data regions with areas < sizeThresh
*/
void clipIslands(unwrapPhaseImage *inputImage, double minValue, int intType, int fastFlag,  int sizeThresh)
{
	unwrapImageStructure  uwImage;     /* This is to fake out for using 
					      unwrap image connect components */
	hole *holes;
	int x, y;                /* Current location in hole */
	char **mask;            /* Mask used for mark pixels for cc alg */
	int **labels;           /* connect components labeled output */      
	int nr, na;              /* Size in range and azimuth */
	int i, j, k;              /* Loop variables */
	int maxLabel;           /* Largest labels from cc */
	int lList[25], nLab;      /* List of labels for border point */
	int nPts;  
	int maxBorders;         /* Maximum border points over all holes */
	/*
	  Init  image stuff
	*/
	na = inputImage->azimuthSize;
	nr = inputImage->rangeSize;
	/*
	  Mask image - set for valid regions (as opposed to holes).*/
	fprintf(stderr,"minvalue %f\n",minValue);
	/*
	  Mask and label  images
	*/
	fprintf(stderr,"minvalue %f\n",minValue);
	mask  = (char **) malloc(na * sizeof(char *) );
	labels  = (int **) malloc(na * sizeof(int *) );
	for(i=0; i < na; i++) {
		mask[i] = (char *)  malloc(nr * sizeof(char) );
		labels[i] = (int *) malloc(nr * sizeof(int) );
		for( j=0; j < nr; j++ ) {
			mask[i][j]= VALIDPIXEL;
			if(inputImage->phase[i][j] > minValue) mask[i][j] +=REGION;
			labels[i][j] = 0;
		}
        }	
	/*
	  Copy to uw structure for connect components.
	*/
	uwImage.phase=inputImage->phase;
	uwImage.rangeSize=inputImage->rangeSize;
	uwImage.azimuthSize=inputImage->azimuthSize;
	uwImage.bCuts=mask;
	uwImage.labels=labels;
	/*
	  Run connected components
	*/
	fprintf(stderr,"Labeling regions\n");
	labelRegions(&uwImage);
	/*
	  Pass 1: Get max label
	*/
	maxLabel = findMaxLabel(labels, nr, na);
	fprintf(stderr,"Pass 1 -- checking labels\n");
	holes = createHoles(maxLabel);
	/*
	  Pass 2: Count holes and border pixels
	*/
	fprintf(stderr,"Pass 2 - counting holes and borders\n");
	countHoles(holes, labels, mask,  nr, na, lList,nLab, fastFlag);
	/*
	  Now allocate space for arrays
	*/
	maxBorders=0;
	for(i=1; i <= maxLabel; i++ ) {
		if(holes[i].nH > 0 ) {
			holes[i].xb=(int *)malloc(holes[i].nB*sizeof(int));
			holes[i].yb=(int *)malloc(holes[i].nB*sizeof(int));
			holes[i].xh=(int *)malloc(holes[i].nH*sizeof(int));
			holes[i].yh=(int *)malloc(holes[i].nH*sizeof(int));
			maxBorders=max(holes[i].nB,maxBorders);
		}           
	}
	/*
	  Pass 3: Save locations of hole and border pixels
	*/
	saveHoles(holes, labels, mask,  nr, na, lList, nLab, fastFlag);
	/*
	  Set small island regions to no data
	*/
	for(i=1; i < maxLabel; i++) {
		if(holes[i].nH < sizeThresh ) {
			for(j=0; j < holes[i].nH; j++) {
				inputImage->phase[holes[i].yh[j]][holes[i].xh[j]] = minValue;
			}
		}
	}
}
