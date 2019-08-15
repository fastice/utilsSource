#include "stdio.h"
#include "string.h"
#include "changeFlat.h"
#include "math.h"

/*
   Reflatten image using new baselines.
*/
     void reflattenImage(unwrapPhaseImage *inputImage, paramData params)
{
     double Bn,Bp,dBn,dBp;
     double BnNew, BpNew, dBnNew,dBpNew;     
     double phiAzimuth,dPhi, phiAzimuthOrig, dPhiOrig;
     double thetaFixed,thetaCfixed,thetaDfixed;
/*     double theta,thetaCfixed,thetaDfixed;*/
     double c1,c2 ,deltaOld, deltaNew;
     double r0, dr,rOffset;
     double xAz,dxAz;
     double deltaToPhase;
     double lambda;
     int i,j;
/*
   Constants and initialization
*/
   lambda=params.lambda;
   fprintf(stderr,"%f %f %f %f\n",params.dBp1,params.dBp2,params.dBpEst, lambda); 

    deltaToPhase = (4.0 * PI)/lambda;  
    if(params.dBpFlag == FALSE) {
        dPhi = params.omegaA * params.nla;
        phiAzimuth =  -inputImage->azimuthSize*0.5*dPhi;
/* 
   Note negative sign because for consistency because of sign switch
   from correlate to tiepoints
*/
         dPhiOrig = -(params.omegaA1 + params.omegaA2) * params.nla;
         phiAzimuthOrig = -inputImage->azimuthSize*0.5*dPhiOrig;
    }
/*
   Ramp parameters.
*/
    c1 = 2.0 * params.H * params.Re + pow(params.H,2.0);
    c2 = 2.0 * (params.Re + params.H);
    thetaCfixed = acos( (pow(params.Rc,2.0) + c1) / (c2*params.Rc) );
/*
    Compute baseline params.
*/
   if(params.nBaselines == 3) {       /* Dual baselines */
        Bn = params.Bn1 + params.Bn2;   Bp = params.Bp1 + params.Bp2;
        dBn = params.dBn1 + params.dBn2;
        dBp = params.dBp1 + params.dBp2;
        Bn = Bn - dBn*0.5;
        dBn = dBn/inputImage->azimuthSize;
        if(params.dBpFlag == TRUE) {
            Bp = Bp - dBp*0.5;
            dBp = dBp/inputImage->azimuthSize;
        }
/*
   For virtual baseline add baseline correction 2b1b2/r0 term.
   Note this value is not updated along track, but this should be ok anyway.
*/
    } else if(params.nBaselines == 2) { /* Single baseline case */
        Bn = params.Bn1;
        Bp = params.Bp1;
        dBn = params.dBn1;
        dBp = params.dBp1;
        Bn = Bn - dBn*0.5;
        dBn = dBn/inputImage->azimuthSize;
        if(params.dBpFlag == TRUE) {
             Bp = Bp - dBp*0.5;
             dBp = dBp/inputImage->azimuthSize;
        }
    } else {  /* Unflattened image */
        Bn = 0.0;  Bp = 0.0;
        dBn = 0.0;
    }
/*
    New baseline 
*/
     xAz = -0.5;
     dxAz = 1.0/inputImage->azimuthSize;
     dBpNew = 0;
     if(params.dBpFlag == FALSE) {
        BpNew = params.BpEst;
        dBnNew = params.dBnEst;
        dBpNew = params.dBpEst;
        BnNew = BnNew - 0.5 * dBnNew;
        dBnNew = dBnNew/inputImage->azimuthSize;
     }
/*
     if(params.dBpFlag == TRUE) {
         BpNew = BpNew - 0.5 * dBpNew;
         dBpNew = dBpNew/inputImage->azimuthSize;
     } */

/*
     Set up range counter depending on pass type.
*/
    dr =  RangePixelSize * params.nlr;
    if(inputImage->passType == DESCENDING) {
        rOffset = 0.0;
    } else {
        error("phaseToDelta ASCENDING PASS not implemented yet\n");
    }
/*
   Convert phase values to delta values.
*/
fprintf(stderr,"Old Bn,Bp %f %f %f %f\n",Bn,Bp,dBp,params.RNear);
/*fprintf(stderr,"New Bn,Bp %f %f %f\n",BnNew,BpNew,dBpNew);*/
    for(i=0; i< inputImage->azimuthSize; i++) {
        r0 = params.RNear + rOffset;
        if(params.dBpFlag==TRUE) {
           BnNew = params.BnEst + xAz*params.dBnEst + xAz*xAz*params.dBnEstQ; 
           BpNew = params.BpEst + xAz*params.dBpEst + xAz*xAz*params.dBpEstQ; 
        }
        if(i % 100 == 0) 
            fprintf(stderr,"At line %i %f %f %f %f %f\n",i,Bn,Bp,BnNew,BpNew,inputImage->phase[i][inputImage->rangeSize/2]);
/*
    Loop over image
*/
        for(j=0; j< inputImage->rangeSize; j++) {   
            if(inputImage->phase[i][j] > (-LARGEINT/10)) { /*unwrapped values*/

/*
    Add back in previously removed phase ramp.
*/
                thetaFixed = acos( (pow(r0,2.0) + c1) / (c2*r0) );
                thetaDfixed = (thetaFixed - thetaCfixed);
/*
if((i % 100 == 0)&& (j % 100 == 0)) fprintf(stderr,"%f %f  %f %f  %f\n",thetaFixed*RTOD,thetaDfixed*RTOD,thetaCfixed*RTOD,r0,RangePixelSize);
*/
/*
   Remove azimuth phase ramp
*/
                if(params.dBpFlag == FALSE) {
                    if(params.noRamp == FALSE) {
		    phiAzimuth=phiAzimuth;
                        inputImage->phase[i][j] -= phiAzimuth;
                   }  else {
                        inputImage->phase[i][j] -= phiAzimuth * cos(thetaDfixed);
/*
   Remove old azimuth ramp first
*/
                       inputImage->phase[i][j] += phiAzimuthOrig;
                    }
                }
/*
   Compute delta old
*/
                deltaOld = -Bn * sin(thetaDfixed) - Bp * cos(thetaDfixed)   + (Bn*Bn + Bp*Bp)/(2.0*r0);
                inputImage->phase[i][j] += deltaToPhase * deltaOld;
/*
   Subtract phase ramp for new baseline
*/
                deltaNew = -BnNew * sin(thetaDfixed) - BpNew * cos(thetaDfixed)  + (BnNew*BnNew + BpNew*BpNew)/(2.0*r0);
                inputImage->phase[i][j] -= deltaToPhase * deltaNew;
/*if(i %100 == 0 && j% 1000 == 0) fprintf(stderr,"%f %f %f %f %f %f %f\n",inputImage->phase[i][j] ,deltaToPhase ,deltaNew,r0,BpNew,BnNew, thetaDfixed); */

/*
   Added 12/14/94 so that reflattening doesn't change mean value of phase
   significantly
*/
              /*   inputImage->phase[i][j] += deltaToPhase * (-BpNew+Bp);*/
            } else inputImage->phase[i][j] =-LARGEINT; /* end if */
            r0 += dr;
        } /* End for j */
        phiAzimuth += dPhi;
        phiAzimuthOrig += dPhiOrig;
        Bn += dBn;
        xAz += dxAz;
        if(params.dBpFlag==TRUE) { Bp +=dBp; /*BpNew += dBpNew;*/}
        else BnNew += dBnNew; 
     }  /* End for i */
fprintf(stderr,"Old Bn,Bp %lf %lf %lf\n",Bn,Bp,dBp);
fprintf(stderr,"New Bn,Bp %lf %lf %lf\n",BnNew,BpNew,dBpNew);

}
