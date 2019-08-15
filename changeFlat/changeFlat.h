#include "mosaicSource/common/common.h"
#include "utilsSource/intFloat/intFloat.h"
typedef struct paramDataType {
    int nBaselines;
    int nlr;
    int nla;
    int noRamp;
    int dBpFlag;
    double H;
    double Re;
    double Rc;
    double RNear;
    double Bn1;
    double Bp1;
    double dBn1;
    double dBp1;
    double Bn2;
    double Bp2;
    double dBn2;
    double dBp2;
    double BnEst;
    double BpEst;
    double dBnEst;
    double dBnEstQ;
    double dBpEst;
    double dBpEstQ;
    double omegaA;
    double omegaA1;
    double omegaA2;
     double lambda;
} paramData;
/*
   Reflatten image using new baselines.
*/
     void reflattenImage(unwrapPhaseImage *inputImage, paramData params);
     void inputParamFile(char *paramFile, paramData *params);

