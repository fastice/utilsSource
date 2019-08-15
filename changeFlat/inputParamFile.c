#include "stdio.h"
#include "string.h"
#include "changeFlat.h"
#include "math.h"

/*
   Input baseline and other parameter info.
*/
     void inputParamFile(char *paramFile, paramData *params)
{
    extern double RangePixelSize;
    FILE *fp;
    double H,Re,Rc,RNear;
    double Bn1, Bn2, Bp1,Bp2,dBn1,dBn2,dBp1,dBp2;
    double BnEst,BpEst,dBnEst,dBpEst,dBpEstQ,dBnEstQ,omegaA;
    double omegaA1, omegaA2;
    double dum;
    double lambda;
    int nlr,nla, nBaselines,nPar;
    int lineCount=0, eod;
    int i;
    char line[256];
    params->omegaA=0.0;
/*
   Input parm info
*/
    fp = openInputFile(paramFile);
/*
   Get non baseline params
*/
    lineCount=getDataString(fp,lineCount,line,&eod);
    if( sscanf(line,"%lf%lf%lf%lf%i%i%lf%lf",&Re,&RNear,&Rc,&nlr,&nla, &RangePixelSize,&lambda) != 8) {
      lambda=LAMBDAERS1; /* For backwards compatability */
      if( sscanf(line,"%lf%lf%lf%lf%i%i%lf",&H,&Re,&RNear,&Rc,&nlr,&nla, &RangePixelSize) != 7) {
         error("%s  %i","inputParamFile--^ Missing params at line:",lineCount);
      }
    }
    /*    fprintf(stderr,"Lambda = %f\n",lambda); exit(-1);*/

    params->H = H * KMTOM; params->Re = Re*KMTOM; 
    params->RNear = RNear * KMTOM; params->Rc = Rc * KMTOM;
    params->nlr = nlr; params->nla = nla;
    params->lambda=lambda;
/*
    Get n lines baseline info.
*/
    lineCount=getDataString(fp,lineCount,line,&eod);
    if( sscanf(line,"%i",&nBaselines) != 1)
        error("%s  %i %s",  "inputParamFile--* Missing baseline params at line:",lineCount,line);
    params->nBaselines = nBaselines;
    if(nBaselines < 1 || nBaselines > 3)
       error("inputParamFile -- invalid number of baselines at line %i\n",
          lineCount);
/*
   Input Baselines used to flatten image.
*/
    params->omegaA1 = 0.0;
    params->omegaA2 = 0.0;
    params->dBp1 = 0.0;
    params->dBp2 = 0.0;

    if(nBaselines > 1) { /* First baseline */
        lineCount=getDataString(fp,lineCount,line,&eod);
        if(params->noRamp == TRUE) {
             if( sscanf(line,"%lf%lf%lf%lf",&Bn1,&Bp1,&dBn1,&omegaA1) != 4)
                 error("%s %i",
                       "inputParamFile  --+ Missing baseline params at line:",
                       lineCount);
             params->omegaA1 = omegaA1;

         } else if(params->dBpFlag ==TRUE) {
             if( sscanf(line,"%lf%lf%lf%lf",&Bn1,&Bp1,&dBn1,&dBp1) != 4)
                 error("%s %i",
                       "inputParamFile  -- Missing baseline params at line:",
                       lineCount);
              params->dBp1 = dBp1;
         } else if( sscanf(line,"%lf%lf%lf",&Bn1,&Bp1,&dBn1) != 3)
             error("%s %i",
                   "inputParamFile  --& Missing baseline params at line:",
                   lineCount);

        params->Bn1 = Bn1; 
        params->Bp1 = Bp1;
        params->dBn1 = dBn1;    
    }   /* End if nBase ... */
  
/*
    Second baseline in double difference case 
*/
   if(nBaselines > 2) {
        lineCount=getDataString(fp,lineCount,line,&eod);
        if(params->noRamp == TRUE) {
             if( sscanf(line,"%lf%lf%lf%lf",&Bn2,&Bp2,&dBn2,&omegaA2) != 4)
                 error("%s %i",
                       "inputParamFile  -- Missing baseline params at line:",
                       lineCount);
             params->omegaA2 = omegaA2;

         } else if(params->dBpFlag ==TRUE) {
             if( sscanf(line,"%lf%lf%lf%lf",&Bn2,&Bp2,&dBn2,&dBp2) != 4)
                 error("%s %i",
                       "inputParamFile  -- Missing baseline params at line:",
                       lineCount);
              params->dBp2 = dBp2;
         } else if( sscanf(line,"%lf%lf%lf",&Bn2,&Bp2,&dBn2) != 3)
             error("%s %i",
                   "inputParamFile  -- Missing baseline params at line:",
                   lineCount);

        params->Bn2 = Bn2; 
        params->Bp2 = Bp2;
        params->dBn2 = dBn2;    
    }
/*
    Input baseline estimated with tiepoints.
*/
        lineCount=getDataString(fp,lineCount,line,&eod);
        dBpEstQ=0.0;
        dBnEstQ=0.0;
        if(params->dBpFlag == TRUE) {
           nPar = sscanf(line,"%lf%lf%lf%lf%lf%lf",
                         &dum,&dum,&dum,&dum,&dum,&dum );
           if(nPar == 6) {
               sscanf(line,"%lf%lf%lf%lf%lf%lf",
                      &BnEst,&BpEst,&dBnEst,&dBpEst,&dBnEstQ,&dBpEstQ);
          } else {
              if(sscanf(line,"%lf%lf%lf%lf",&BnEst,&BpEst,&dBnEst,&dBpEst) != 4)
                  error("%s %i",
                     "inputParamFile  -- Missing baseline params at line:",
                      lineCount);
              }
              params->dBpEst = dBpEst;
              params->dBpEstQ = dBpEstQ;
              params->dBnEstQ = dBnEstQ;  
        } else {
            if( sscanf(line,"%lf%lf%lf%lf",&BnEst,&BpEst,&dBnEst,&omegaA) != 4)
               error("%s %i",
                     "inputParamFile  -- Missing baseline params at line:",
                      lineCount);
               params->omegaA = omegaA;
        }
        params->BnEst = BnEst; params->BpEst = BpEst;
        params->dBnEst = dBnEst; 
fprintf(stderr,"nlr %i\n nla %i \nH %f \n Re %f \n Rc %f\n RNear %f\n",params->nlr,params->nla,params->H,params->Re, params->Rc,params->RNear);
   
fprintf(stderr, 
"\n Bn1 %f \n Bp1 %f \n dBn1 %f \n Bn2 %f \n Bp2 %f \n dBn2 %f \n BnEst %f\n BpEst %f \n dBnEst %f \n omegaA %f  \n dBnQ %f \n dBpQ %f\n\n"
    ,params->Bn1
    ,params->Bp1
    ,params->dBn1
    ,params->Bn2
    ,params->Bp2
    ,params->dBn2
    ,params->BnEst
    ,params->BpEst
    ,params->dBnEst
    ,params->omegaA    
    ,params->dBnEstQ
    ,params->dBpEstQ);
}
