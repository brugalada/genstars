/* Generate stars following the Galactic model developed by Koshimoto, Baba & Bennett (2021).
 * Written by N. Koshimoto on Nov 5 2021 for simulations of the surveys by PRIME (I.Kondo+) and Roman (S.A.Johnson+).
 * E(J-Ks) value is read from the Gonzalez+12+Surot+20 extinction map and converted into A_lambda by Nishiyama+09's law. 
 * For V, I, and Z087, different slope derived by Koshimoto using Nataf+13's AI map is used.
 * Current version applies Mag=99 for white dwarfs. 
 * W146 mag for BD (< 0.09 Msun) is assumed to be ~J mag.
 * Mag= 99 is applied for Z087 mag and F213 mag for BD
 * *************************************************************************************
 *   This is a combined version of genstars_H (for Kondo+ who lead PRIME survey 
 *   simulation) and genstars_IR (for Johnson+ who lead Roman survey simulation).
 *   By default, it uses the PRIME mode that simulates V, I, J, H, and Ks mags.
 *   By adding "ROMAN 1" to the arguments, it uses the Roman mode that simulates
 *   J, H, Ks, Z087, W146, and F213 mags.
 *   Although the two modes both contain J-, H-, and Ks-mags, they are slightly 
 *   different because the two modes use different mass-luminosity relation due to 
 *   each authors' preference. The PRIME mode uses a hybrid mass-luminosity relation 
 *   that uses an empirical one for M < ~0.5 Msun and isochrone models for M > ~0.5 Msun.
 *   The Roman mode uses pure isochrone models for all mass range.
 * *************************************************************************************
 * Update on Feb 13-18 2022  
 *   1. Use Surot+20's E(J-Ks) map at |b| < 4 deg instead of Gonzalez+12 because G12 underestimates E(J-Ks) values at |b| < ~1.
 *      The original S20 map has 0.0025x0.0025 deg^2, 100 times higher resolution than previous ver.
 *      EXTMAP option is added for trade-off between resolution and computation time.
 *      Each EXTMAP value behaves differently for every basic grid of 0.025x0.025 deg^2.
 *          0 (slowest) : Use the finest 0.0025x0.0025 deg^2 map when E(J-Ks) varies > 0.2 mag or 
 *                        E(J-Ks)/<E(J-Ks)> varies > 50% within the 0.025x0.025 deg^2 grid.
 * Default->1 (middle)  : Same as 0 but use binned 0.0050x0.0050 deg^2 map instead of 0.0025x0.0025 deg^2 map.
 *          2 (fastest) : Use the average, <E(J-Ks)>, of the 100 subgrids included in a 0.025x0.025 deg^2 grid.
 *      The default is EXTMAP == 1 for a public version, in many cases EXTMAP == 2 may be sufficient.
 *   2. Extinction-law is updated so that it uses different A_J/A_Ks and A_H/A_Ks values every NE, NW, SE or SW quadrants.
 *      Previous version used different A_Ks/E(J-Ks) values, but applied the same A_lam/A_Ks ratios, which is inappropriate.
 *      Values are taken from Table 2 of Alonso-Garcia+17, ApJL, 849, L13 for EXTLAW==0, and Table 2 of
 *      Nishiyama+09, ApJ, 696, 1407 for EXTLAW==1 (default).
 *      For EXTLAW==0, Koshimoto derived the alphaJ2I values for conversion of AJ into AZ087 by comparing Nataf+13
 *      AI map and AJ map by each EXTLAW value. alphaJ2V is confirmed to be different from alphaJ2I, 
 *      so alphaJ2I can be applied down to ~I-band wavelength.
 * Update on Mar 30 2022 
 *   NSC is added for a test of its contribution.
 * Update on Apr 4 2022 
 *   EXTLAW == 2 (Wang & Chen (2019), ApJ, 877, 116) is added
 * Update on Apr 8 2022 
 *   CenSgrA option added to put SgrA* on the GC
 * Update on Feb 16 2023
 *   RenShu is increased to 12200 (from 9200) for increasing Dmax up to 20 kpc.
 *   Note that the disk model does not include the flare structure that is expected to begin rising outward from R ~ 8 kpc, so results with Dmax > 16 kpc would be affected by that.
 * */
#include <math.h> 
#include <stdio.h> 
#include <string.h> 
#include <stdarg.h>
#include "option.h"
#include <stdlib.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

#define EPS 1.2e-7
#define RNMX (1.0 - EPS)
#define       PI 3.1415926535897932385
#define NDATAMAX 8000000000 // take ~6hours?
#define STR2MIN2 8.461595e-08  // arcmin^2 in str = deg^2 in str / 3600
#define STR2DEG2 0.000304617419787  // deg^2 in str = deg^2 in str / 3600
#define    KAPPA 8.1439 // 
#define    KS2MY 210.949526569698696 // ([sec/yr]/[km/AU]) for km/sec/pc -> mas/yr
#define       GC 4.30091e-03 // Gravitational Constant in pc * Msun^-1 * (km/sec)^2 (Eng. Wikipedia)
#define     zsun 25.0
#define     srob 500.0
#define    vescd 550.0 // escape velo of disk
#define    vescb 600.0 // escape velo of bulge
#define  MBINMIN 0.05 // No binary for Mprim < 0.05 Msun
#define  MAXMULT 1.0 // 
#define MAXGAMMA 4.0 // 
#define MINGAMMA 0.0 // 
#define MAXSIGLOGA 1.8 // 
#define MINSIGLOGA 0.3 // 
#define MAXMEANLOGA 1.7 // 
#define MINMEANLOGA 0.6 // 


// /* Generate a random number between 0 and 1 (excluded) from a uniform distribution. */
const gsl_rng_type * T;
gsl_rng * r;
double ran1(){
    double u = gsl_rng_uniform(r);
    return u;
}
// 
// /* Generate a random number from a Gaussian distribution of mean 0, and std 
//    deviation 1.0. */
double gasdev(){
    return gsl_ran_ugaussian(r);
}

// --- define global parameters ------
static int ncomp = 10, nband; // 7xthin + thick + bar + NSD, J, H, Ks, Z087, W146, F213 
static double tSFR = 7.0;  // time scale of SFR, 7.0 Gyr
static double rhot0;

// --- for fit to tE --- (from get_chi2_for_tE.c)
static int agesD[250], agesB[50], agesND[10];
static double MinidieD[250], MinidieB[50], MinidieND[10];
static int nageD=0, nageB=0, nageND=0;
static double mageB = 9, sageB = 1, mageND = 7, sageND = 1;

// --- for Mass function ---
static int nm;
static double logMst, dlogM;

// --- Parameters for bulge ---
// Values here will be overwritten in store_IMF_nBs using given IMF
static double fb_MS   = 1.62/2.07; // MS mass / total mass in bulge / bar
static double m2nb_MS  = 1/0.227943; // Msun/star in bulge / bar
static double m2nb_WD  = 1/0.847318; // Msun/WD   in bulge / bar
static double nMS2nRGb = 2.33232e-03; // n_RG/n_MS for bulge / bar
static double rho0b, n0MSb, n0RGb, n0b;

// Nuclear Star Clustar (for |b| < 0.3 deg.)
static int NSC;
static double gammaNSC= 0.71, qNSC= 0.73, a0NSC= 5.9; // Chatzopoulos+15
static double rho0NSC = 0, n0MSNSC = 0, n0RGNSC = 0, n0NSC = 0;

// Nuclear disk (for |b| < 1 deg.)
static int ND, x0ND = 250, y0ND = 125, z0ND = 50;
static double fND_MS    = 0; // MS mass / total mass in NSD
static double m2nND_MS  = 0; // Msun/star in NSD
static double m2nND_WD  = 0; // Msun/WD   in NSD
static double nMS2nRGND = 0; // n_RG/n_MS for NSD
static double C1ND = 2, rho0ND = 0, n0MSND = 0, n0RGND = 0, n0ND = 0;

// --- Parameters for Disk ---
// Density values will be overwritten in store_IMF_nBs using given IMF
static double rho0d[8]   = {5.16e-03+3.10e-04, 5.00e-03+5.09e-04, 3.85e-03+5.42e-04, 3.18e-03+5.54e-04,
                            5.84e-03+1.21e-03, 6.24e-03+1.51e-03, 1.27e-02+3.49e-03, 1.68e-03+6.02e-04};
static double n0d[8]     = {1.51e-02+1.12e-04, 1.66e-02+3.22e-04, 1.40e-02+4.39e-04, 1.22e-02+5.15e-04, 
                            2.36e-02+1.25e-03, 2.63e-02+1.67e-03, 5.55e-02+4.08e-03, 7.91e-03+7.81e-04};
static double n0MSd[8]   = {1.51e-02, 1.66e-02, 1.40e-02, 1.22e-02, 2.36e-02, 2.63e-02, 5.55e-02, 7.91e-03};
static double n0RGd[8]   = {7.09e-06, 3.40e-05, 4.32e-05, 2.16e-05, 6.60e-05, 6.19e-05, 1.29e-04, 9.38e-06};
// Scale lengths and heights are fixed
static double y0d[3];
static int       Rd[3] = {5000, 2600, 2200};
static int        Rh = 3740, Rdbreak = 5300, nh = 1;
static double zd[8]   =  {61.47, 141.84, 224.26, 292.36, 372.85, 440.71, 445.37, 903.12};
static double zd45[8] =  {36.88,  85.10, 134.55, 175.41, 223.71, 264.42, 267.22, 903.12};
static int DISK, hDISK, addX, model;
static double R0, thetaD, x0_1, y0_1, z0_1=0, C1, C2, C3, Rc, frho0b, costheta, sintheta, zb_c;
static double x0_X, y0_X, z0_X=0, C1_X, C2_X, b_zX, fX, Rsin, b_zY, Rc_X;

//--- To give coordinate globally ---
static double *lDs, *bDs;

//--- For rough source mag and color constraint ----
static int nMIs;
static double **CumuN_MIs;

//--- For Circular Velocity ------
static int nVcs=0;
static double Rcs[60], Vcs[60];

//--- Sun kinematics ------
static double vxsun = -10.0, Vsun = 11.0, vzsun = 7.0, vysun = 243.0;

//--- For Disk kinematics ------
static double ****fgsShu, ****PRRgShus, ****cumu_PRRgs;
static int ***n_fgsShu, ****kptiles;
static double hsigUt, hsigWt, hsigUT, hsigWT, betaU, betaW, sigU10d, sigW10d, sigU0td, sigW0td;
static double medtauds[8] = {0.075273, 0.586449, 1.516357, 2.516884, 4.068387, 6.069263, 8.656024, 12};
/* The line of sight toward (lSIMU, bSIMU) until Dmax pc needs to be inside the cylinder defined by
 * R < RenShu and -zenShu < z < zenShu 
 * Please change the following zenShu and/or RenShu value when you want to extend 
 * the line of sight outside of the default cylinder. */
static int zstShu =   0, zenShu = 3600, dzShu = 200;
// static int RstShu = 500, RenShu = 9200, dRShu = 100; // use value @ RstShu for R < RstShu
static int RstShu = 500, RenShu = 12200, dRShu = 100; // use value @ RstShu for R < RstShu

//--- For Bulge kinematics ------
static int model_vb, model_vbz;
static double Omega_p, x0_vb, y0_vb, z0_vb, C1_vb, C2_vb, C3_vb, sigx_vb, sigy_vb, sigz_vb, vx_str, y0_str;
static double sigx_vb0, sigy_vb0, sigz_vb0;
static double x0_vbz, y0_vbz, z0_vbz, C1_vbz, C2_vbz, C3_vbz;

//--- For NSD (ND==3), to store values of input_files/NSD_moments.dat ------
static double **logrhoNDs, **vphiNDs, ***logsigvNDs, **corRzNDs;
static double zstND = 0, zenND =  400, dzND = 5;
static double RstND = 0, RenND = 1000, dRND = 5;
static int nzND, nRND;

//--- Parameters to put Sgr A* on the GC ------
static double xyzSgrA[3] = {};

// Declare functions
int    get_khi(int n, double *x, double xin);
double getx2y_khi(int n, double *x, double *y, double xin, int *khi);
double getx2y_ist(int n, double *x, double *y, double xin, int *ist);
double interp_x(int n, double *F, double xst, double dx, double xreq);
double interp_xquad(int n, double *F, double *f, double xst, double dx, double xreq);
double interp_xy(int nx, int ny, double **F, double xst, double yst, double dx, double dy, double xreq, double yreq);
void   interp_xy_coeff(int nx, int ny, double *as, double xst, double yst, double dx, double dy, double xreq, double yreq);
void Dlb2xyz(double D, double lD, double bD, double Rsun, double *xyz);



//----------------
double getAlamAV_WC19(double lam){ // Calculate Eqs.(9)-(10) of Wang & Chen (2019), ApJ, 877, 116
  if (lam < 1000){ // in nm
    double Y1 = 1000/lam - 1.82; // 1/um - 1.82
    double as[7] = {0.7499, -0.1086, -0.08909, 0.02905, 0.01069, 0.001707, -0.001002};
    double Yi = 1;
    double AlamAV = 1;
    for (int i=0; i <7; i++){
      Yi *= Y1; // Yi = Y1^(i+1)
      AlamAV += as[i]*Yi;
    }
    return AlamAV;
  }else{
    return 0.3722*pow(1000/lam, 2.07);
  }
}
void getEJK2Alams(int EXTLAW, int nlams, double *EJK2Alams, double *lameff, double l, double b){
  // return Alams/E(J-Ks)_VVV
  double lameff0[5], EJK2AK, EHK2AK, EJK2AI, AI2AV, alphaIR, alphaJ2I, alphaI2V, f2EJKVVV;

  if (EXTLAW == 2){ // Use Wang & Chen (2019)'s law, see their Eqs.(9)-(10)
    // Alam/E(J-K) = [Alam/AV] / [E(J-Ks)/AV]
    double EJKAV = getAlamAV_WC19(1254.0) - getAlamAV_WC19(2149.0); // 1254.0 = lamJ,VVV, 2149.0 = lamK,VVV 
    for (int ilam=0; ilam<nlams; ilam++){
      EJK2Alams[ilam] = getAlamAV_WC19(lameff[ilam]) / EJKAV;
    }
    return;
  }

  // From comparison between E(J-Ks) of Gonzalez+12 and AI, AV of Nataf+13
  lameff0[0] = 549.056, lameff0[1] = 805.988; // V, I from output of PARSEC isochrone webpage
  EJK2AI = (l > 0 && b > 0) ? 3.65  // NE, AI/E(J-Ks)_VVV
         : (l < 0 && b > 0) ? 3.77  // NW
         : (l > 0 && b < 0) ? 3.97  // SE
         : (l < 0 && b < 0) ? 3.82  // SW
         : 3.86; // All data points
  AI2AV  = (l > 0 && b > 0) ? 1.80  // NE, AV/AI
         : (l < 0 && b > 0) ? 1.81  // NW
         : (l > 0 && b < 0) ? 1.82  // SE
         : (l < 0 && b < 0) ? 1.82  // SW
         : 1.82; // All data points
  alphaI2V = (l > 0 && b > 0) ? 1.54  // NE, slope between lam_I and lam_V
           : (l < 0 && b > 0) ? 1.55  // NW
           : (l > 0 && b < 0) ? 1.57  // SE
           : (l < 0 && b < 0) ? 1.56  // SW
           : 1.56; // All data points
  double EJK2AV = EJK2AI * AI2AV;
  if (EXTLAW == 1){
    EJK2AK = (l > 0 && b > 0) ? 0.497  // N+ from Table2 of Nishiyama+09 
           : (l < 0 && b > 0) ? 0.494  // N- from Table2 of Nishiyama+09 
           : (l > 0 && b < 0) ? 0.534  // S+ from Table2 of Nishiyama+09 
           : (l < 0 && b < 0) ? 0.587  // S- from Table2 of Nishiyama+09 
           : 0.528; // All data points from Table 2 of Nishiyama+09
    EHK2AK = (l > 0 && b > 0) ? 1.64   // N+ from Table2 of Nishiyama+09 
           : (l < 0 && b > 0) ? 1.48   // N- from Table2 of Nishiyama+09 
           : (l > 0 && b < 0) ? 1.54   // S+ from Table2 of Nishiyama+09 
           : (l < 0 && b < 0) ? 1.63   // S- from Table2 of Nishiyama+09 
           : 1.61;  // All data points from Table 2 of Nishiyama+09
    alphaJ2I = (l > 0 && b > 0) ? 2.07   // NE, NK compared AJ with Nataf+13's AI map
             : (l < 0 && b > 0) ? 2.15   // NW, NK compared AJ with Nataf+13's AI map
             : (l > 0 && b < 0) ? 2.21   // SE, NK compared AJ with Nataf+13's AI map
             : (l < 0 && b < 0) ? 2.05   // SW, NK compared AJ with Nataf+13's AI map
             : 2.12;  // All data points, NK compared AJ with Nataf+13's AI map
    alphaIR = 2.0; // Nishiyama+06, 09
    lameff0[2] = 1240, lameff0[3] = 1664, lameff0[4] = 2164; // 2MASS J, H, Ks from Table 1 of Nishiyama+09, slightly differ from PARSEC output, but probably fine.
    // f2EJKVVV = 0.975; // (J - Ks)_VVV/(J - Ks)_2MASS, from http://casu.ast.cam.ac.uk/surveys-projects/vista/technical/photometric-properties
    f2EJKVVV = 0.970; // to convert E(J-K)_2MASS into VVV. middle of the above and 0.960 based on (A_J,V - A_K,V)/(A_J,2 - A_K,2) w/ extiction law of alpha=2.0
  }else{ // Use Alonso-Garcia+17's law
    EJK2AK = (l > 0 && b > 0) ? 0.390  // NE from Table 2 of Alonso-Garcia+17
           : (l < 0 && b > 0) ? 0.384  // NW from Table 2 of Alonso-Garcia+17
           : (l > 0 && b < 0) ? 0.464  // SE from Table 2 of Alonso-Garcia+17
           : (l < 0 && b < 0) ? 0.415  // SW from Table 2 of Alonso-Garcia+17
           : 0.428; // representative value from Table 1 of AG+17
    EHK2AK = (l > 0 && b > 0) ? 1.02   // NE from Table 2 of Alonso-Garcia+17 
           : (l < 0 && b > 0) ? 0.97   // NW from Table 2 of Alonso-Garcia+17 
           : (l > 0 && b < 0) ? 1.30   // SE from Table 2 of Alonso-Garcia+17 
           : (l < 0 && b < 0) ? 1.21   // SW from Table 2 of Alonso-Garcia+17 
           : 1.10;  // representative value from Table 1 of AG+17  
    alphaJ2I = (l > 0 && b > 0) ? 2.18   // NE, NK compared AJ with Nataf+13's AI map 
             : (l < 0 && b > 0) ? 2.26   // NW, NK compared AJ with Nataf+13's AI map 
             : (l > 0 && b < 0) ? 2.26   // SE, NK compared AJ with Nataf+13's AI map 
             : (l < 0 && b < 0) ? 2.25   // SW, NK compared AJ with Nataf+13's AI map 
             : 2.25;  // All data points, NK compared AJ with Nataf+13's AI map
    alphaIR = 2.47; // Alonso-Garcia+17
    lameff0[2] = 1254, lameff0[3] = 1646, lameff0[4] = 2149; // VVV J, H, Ks from Table 1 of Saito+12, A&A 537, A107
    f2EJKVVV = 1; // EJK is already given by VVV system
  }
  double EJK2AJ = EJK2AK + 1; // AJ/E(J-K) = AK/E(J-K) + 1
  double EJK2AH = EJK2AK*(1/EHK2AK + 1); // E(H-K)/AK + 1 = AH/AK
  double EJK2Alam0s[5] = {};
  // EJK2Alam0s[0] = EJK2AV / f2EJKVVV;  // Convert E(J-Ks)_2MASS in the denominator into E(J-Ks)_VVV   
  // EJK2Alam0s[1] = EJK2AI / f2EJKVVV;  // Convert E(J-Ks)_2MASS in the denominator into E(J-Ks)_VVV   
  EJK2Alam0s[0] = EJK2AV;  // Convert is not needed cuz EJK is based on VVV for both EXTLAW=0 & 1
  EJK2Alam0s[1] = EJK2AI;  // Convert is not needed cuz EJK is based on VVV for both EXTLAW=0 & 1
  EJK2Alam0s[2] = EJK2AJ / f2EJKVVV;  // Convert E(J-Ks)_2MASS in the denominator into E(J-Ks)_VVV   
  EJK2Alam0s[3] = EJK2AH / f2EJKVVV;  // Convert E(J-Ks)_2MASS in the denominator into E(J-Ks)_VVV 
  EJK2Alam0s[4] = EJK2AK / f2EJKVVV;  // Convert E(J-Ks)_2MASS in the denominator into E(J-Ks)_VVV
  // printf("AJ/EJK0= %f AH/EJK0= %f AK/EJK0= %f\n",EJK2Alam0s[0],EJK2Alam0s[1],EJK2Alam0s[2]);
  for (int ilam=0; ilam<nlams; ilam++){
    double EJK2Alam0=0, dlammin = 99999;
    int iMag0;
    for (int i = 0; i<5; i++){ // Pick closest lameff0
      double dlam = fabs(lameff[ilam] - lameff0[i]);
      if (dlam < dlammin){
        dlammin = dlam;
        iMag0 = i;
      }
    }
    double lamratio = lameff0[iMag0]/lameff[ilam]; // lam^-1 relative to lam[iMag0]
    // Roughly adjust from the A_iMag0 by a power-law as a function of lameff
    double alpha = (lameff[ilam] < lameff0[1]) ? alphaI2V // Use alphaJ2I for a band bluer than I-band
                 : (lameff[ilam] < 1000)       ? alphaJ2I // Use alphaJ2I for a band bluer than 1000 nm
                 : alphaIR;
    EJK2Alams[ilam] = pow(lamratio, alpha)*EJK2Alam0s[iMag0]; // A_lam propto lambda^-2.47 (AG+17)
  }
}

//----------------
void store_NSDmoments(char *infile) // Read input_files/NSD_moments.dat
{
  // read moments of Sormani+21's NSD DF model 
  FILE *fp;
  char line[1000];
  char *words[100];
  if((fp=fopen(infile,"r"))==NULL){
     printf("can't open %s\n",infile);
     exit(1);
  }
  int iRz = 0;
  while (fgets(line,1000,fp) !=NULL){
     split((char*)" ", line, words);
     if (*words[0] == '#') continue;
     int iR = iRz % nRND;
     int iz = iRz / nRND;
     if (RstND + iR*dRND == 1000*atof(words[0]) && zstND + iz*dzND == 1000*atof(words[1])){
       logrhoNDs[iz][iR] = log10(atof(words[2])); // log [M_sun/pc^3]
       vphiNDs[iz][iR] = atof(words[3]); // vphi
       logsigvNDs[iz][iR][0] = log10(atof(words[4])); // sigphi
       logsigvNDs[iz][iR][1] = log10(atof(words[5])); // sigR
       logsigvNDs[iz][iR][2] = log10(atof(words[6])); // sigz
       corRzNDs[iz][iR] = atof(words[7]); // correlation coefficient between vR and vz
       // printf("iz=%d iR=%d %f %f %6.3f %5.1f\n", iz,iR,atof(words[1]),atof(words[0]),logrhoNDs[iz][iR], vphiNDs[iz][iR]);
     }else{
       printf("something goes wrong\n");
     }
     iRz++;
  } 
  fclose(fp);
}
//----------------
void store_IMF_nBs(int B, double *logMass, double *PlogM, double *PlogM_cum_norm, int *imptiles, double M0, double M1, double M2, double M3, double Ml, double Mu, double alpha1, double alpha2, double alpha3, double alpha4, double alpha0){
  /* Store IMF with a broken-power law form.
   * Update normalize factors for the density distribution if B == 1 
   * Updated for NSD on 20220207 */
  double *PlogM_cum, *Mass, *PMlogM_cum, *PMlogM_cum_norm;
  Mass            = (double *)calloc(nm+1, sizeof(double *));
  PlogM_cum       = (double *)calloc(nm+1, sizeof(double *));
  PMlogM_cum      = (double *)calloc(nm+1, sizeof(double *));
  PMlogM_cum_norm = (double *)calloc(nm+1, sizeof(double *));
  logMst = log10(Ml);
	dlogM = (double) (log10(Mu)-logMst)/nm;
  for (int i=0; i<=nm; i++){
    double Mp  = i*dlogM + logMst;
    logMass[i] = Mp;
    Mass[i]  = pow(10, Mp);
    double alpha = (Mass[i] < M3) ? alpha4 : (Mass[i] < M2) ? alpha3 : (Mass[i] < M1) ? alpha2 : (Mass[i] < M0) ? alpha1 : alpha0;
    double temp00    = pow(M0,alpha0+1.);
    double temp01    = pow(M0,alpha1+1.);
    double temp11    = pow(M1,alpha1+1.);
    double temp12    = pow(M1,alpha2+1.);
    double temp22    = pow(M2,alpha2+1.);
    double temp23    = pow(M2,alpha3+1.);
    double temp33    = pow(M3,alpha3+1.);
    double temp34    = pow(M3,alpha4+1.);
    double dPlogM = 1;
    if (Mass[i] <M0) dPlogM=temp01 / temp00; //dM=MdlogM
    if (Mass[i] <M1) dPlogM=temp12 / temp11*dPlogM; //dM=MdlogM
    if (Mass[i] <M2) dPlogM=temp23 / temp22*dPlogM;
    if (Mass[i] <M3) dPlogM=temp34 / temp33*dPlogM;
    double templogMF = pow(Mass[i], alpha+1.);
    PlogM[i] = templogMF / dPlogM;
    if (i>=1) {
      PlogM_cum[i]  = 0.5*(PlogM[i]+PlogM[i-1])*dlogM                   + PlogM_cum[i-1];  // Mass function
      PMlogM_cum[i] = 0.5*(Mass[i]*PlogM[i]+Mass[i-1]*PlogM[i-1])*dlogM + PMlogM_cum[i-1]; // Mass spectrum
    } else {
      PlogM_cum[i] = 0.0;
      PMlogM_cum[i] = 0.0;
    }
  }
  // PlogM: Percentage of logM stars in total number of stars born, PMlogM: in total mass of stars born
  for(int i=0;i<=nm;i++){
    PlogM_cum_norm[i]= PlogM_cum[i]/PlogM_cum[nm];
    PMlogM_cum_norm[i]= PMlogM_cum[i]/PMlogM_cum[nm];
    PlogM[i] /= PlogM_cum[nm]; // for getcumu2xist
    int intp = PlogM_cum_norm[i]*20;
    if (imptiles[intp] == 0)  imptiles[intp] = (intp==0) ? 1 : i+0.5;
  }
  if (B == 0) return;

  // Calc average mass-loss for WDs
  double *ageMloss;
  ageMloss       = (double *)calloc(nm+1, sizeof(double *));
  double cumMwt = 0, cumWDwt = 0;
  void Mini2Mrem (double *pout, double M, int mean); 
  for (int i=nm;i>=0;i--){
    double pout[2] = {};
    double M = pow(10, logMass[i]);
    double wt = PlogM[i];
    Mini2Mrem(pout, M, 1);  // 0 : random
    double MWD = pout[0];
    cumMwt  += M * wt;
    cumWDwt += MWD * wt;
    ageMloss[i] = cumWDwt/cumMwt; 
  }
  // Read minimum died initial mass as a function of age
  char line[1000];
  char *words[100];
  FILE *fp;
  char file1[] = "input_files/Minidie_IR.dat";
  double MRGstD[250], MRGenD[250], MRGstB[50], MRGenB[50], MRGstND[10], MRGenND[10];
  if((fp=fopen(file1,"r"))==NULL){
     printf("can't open %s\n",file1);
     exit(1);
  }
  nageD = 0, nageB = 0, nageND = 0;
  while (fgets(line,1000,fp) !=NULL){
     split((char*)" ", line, words);
     if (*words[0] == '#') continue;
     if (*words[0] == 'N'){
       agesND[nageND]    = atof(words[1]);
       MinidieND[nageND] = atof(words[2]);
       MRGstND[nageND] = atof(words[3]);
       MRGenND[nageND] = atof(words[4]);
       nageND++;
     }else if (*words[0] == 'B'){
       agesB[nageB]    = atof(words[1]);
       MinidieB[nageB] = atof(words[2]);
       MRGstB[nageB] = atof(words[3]);
       MRGenB[nageB] = atof(words[4]);
       nageB++;
     }else{
       agesD[nageD]    = atof(words[0]);
       MinidieD[nageD] = atof(words[1]);
       MRGstD[nageD] = atof(words[2]);
       MRGenD[nageD] = atof(words[3]);
       nageD++;
     }
  }
  fclose(fp);
  
  // for disks 
  double gamma = 1/tSFR;  // SFR timescale, 7 Gyr
  int agest = 1, ageen = 1000;
  int iages[7] = {15,100,200,300,500,700,1000};
  double wt_D[7] = {}, wtWD_D[7] = {}, sumM_D[7] = {}, sumMWD_D[7] = {}, sumstars_D[7] = {}, sumWDs_D[7] = {}, sumRGs_D[7] = {};
  for (int i=agest; i<=ageen; i++){
    int itmp = (i - agesD[0])/(agesD[1] - agesD[0]) + 0.5;
    if (itmp < 0) itmp = 0;
    double logMdie = log10(MinidieD[itmp]);
    double logMRG1 = log10(MRGstD[itmp]);
    double logMRG2 = log10(MRGenD[itmp]);
    double PM   = interp_x(nm+1, PMlogM_cum_norm, logMst, dlogM, logMdie);
    double P    = interp_x(nm+1, PlogM_cum_norm,  logMst, dlogM, logMdie);
    double PRG1 = interp_x(nm+1, PlogM_cum_norm,  logMst, dlogM, logMRG1);
    double PRG2 = interp_x(nm+1, PlogM_cum_norm,  logMst, dlogM, logMRG2);
    double PRG = PRG2 - PRG1; 
    double aveMloss = interp_x(nm+1, ageMloss, logMst, dlogM, logMdie);
    double PMWD = (1 - PM) * aveMloss;
    double PWD  = (1 - P);
    double wtSFR = exp(-gamma*(ageen-i)*0.01); // weight of this age
    P   *= wtSFR;
    PWD *= wtSFR;
    PM  *= wtSFR;
    PMWD *= wtSFR;
    PRG *= wtSFR;
    int idisk  = (i <= iages[0]) ? 0 
               : (i <= iages[1]) ? 1 
               : (i <= iages[2]) ? 2 
               : (i <= iages[3]) ? 3 
               : (i <= iages[4]) ? 4 
               : (i <= iages[5]) ? 5 
               : (i <= iages[6]) ? 6 : 0;
    wt_D[idisk]   += PM;
    wtWD_D[idisk] += PMWD;
    sumM_D[idisk] += PM*PMlogM_cum[nm];
    sumMWD_D[idisk]   += PMWD*PMlogM_cum[nm];
    sumstars_D[idisk] += P   *PlogM_cum[nm];
    sumWDs_D[idisk]   += PWD *PlogM_cum[nm];
    sumRGs_D[idisk]   += PRG *PlogM_cum[nm];
  }
  // Normalize
  double rho0thinMS = 0, rho0thinWD = 0, Sig2rho[8] = {}, aveMMS_D[8] = {}, aveMWD_D[8] = {}, nfracRG_D[8] = {}, aveM_D[8] = {};
  for (int i=0;i<8;i++){
    Sig2rho[i] = 0.5/zd[i]; // rho0/Sigma
    if (i < 7){
      int rd = (i==0) ? Rd[0] : Rd[1]; // because integrated mass depends on rd, SFR should be weight for the integrated mass  
      aveMMS_D[i] = sumM_D[i]/sumstars_D[i]; //  Msun/star for MainSequence
      aveMWD_D[i] = sumMWD_D[i]/sumWDs_D[i]; //  Msun/star for WhiteDwarf
      nfracRG_D[i]= sumRGs_D[i]/sumstars_D[i]; // RG to MS+RG ratio in number of stars
      aveM_D[i] = (sumM_D[i]+sumMWD_D[i])/(sumstars_D[i]+sumWDs_D[i]); // Msun/star for MS+WD
      // exp(-Rsun/rd)*wt[i]/rd is weight of rho at Sun position relative to the total mass wt[i] (but when ignoring hole)
      rho0thinMS += exp(-R0/rd)*wt_D[i]/rd * Sig2rho[i];
      rho0thinWD += exp(-R0/rd)*wtWD_D[i]/rd * Sig2rho[i];
    }
  }
  // double rhot0 = 0.042; // Msun/pc^3 @ z=0, rhot0 + rhoT0 = 0.042, (Bovy17: 0.042 +- 0.002 incl.BD)
  double rhoT0 = rhot0 * 0.04; //  Msun/pc^3, 4% of thin disk (Bland-Hawthorn & Gerhard (2016), f_rho = 4% +- 2%)
  for (int i=0;i<8;i++){
    int rd = (i == 0) ? Rd[0] : (i < 7) ? Rd[1] : (i == 7) ? Rd[2] : 0;
    if (i < 7){
      double norm = rhot0/rho0thinMS;
      double rhoMS  = norm * exp(-R0/rd) * wt_D[i]/rd * Sig2rho[i];
      double rhoWD  = norm * exp(-R0/rd) * wtWD_D[i]/rd * Sig2rho[i];
      rho0d[i] = rhoMS + rhoWD;
      n0MSd[i] = rhoMS/aveMMS_D[i];
      double n0WD = rhoWD/aveMWD_D[i];
      n0d[i]   = n0MSd[i] + n0WD;
      n0RGd[i] = n0MSd[i]*nfracRG_D[i];
    //   printf ("%d rho0= %.2e + %.2e = %.2e, n0= %.2e + %.2e = %.2e, n0RG= %.2e\n",i,rhoMS,rhoWD,rho0d[i],n0MSd[i],n0WD,n0d[i],n0RGd[i]);
    }else{  // Thick disk
      double logMdie = log10(MinidieD[nageD - 2]);
      double logMRG1 = log10(MRGstD[nageD - 2]);
      double logMRG2 = log10(MRGenD[nageD - 2]);
      double PM   = interp_x(nm+1, PMlogM_cum_norm, logMst, dlogM, logMdie);
      double P    = interp_x(nm+1, PlogM_cum_norm,  logMst, dlogM, logMdie);
      double PRG1 = interp_x(nm+1, PlogM_cum_norm,  logMst, dlogM, logMRG1);
      double PRG2 = interp_x(nm+1, PlogM_cum_norm,  logMst, dlogM, logMRG2);
      double PRG = PRG2 - PRG1; 
      double aveMloss = interp_x(nm+1, ageMloss, logMst, dlogM, logMdie);
      double PMWD = (1 - PM) * aveMloss;
      double PWD  = (1 - P);
      double aveMMS = PM   * PMlogM_cum[nm] / P   / PlogM_cum[nm]; // MSun/star for main sequence
      double aveMWD = PMWD * PMlogM_cum[nm] / PWD / PlogM_cum[nm]; // MSun/star for WD
      double aveM   = (PM*PMlogM_cum[nm]+PMWD*PMlogM_cum[nm])/(P*PlogM_cum[nm]+PWD*PlogM_cum[nm]);
      double norm = rhoT0/PM;
      double rhoMS = rhoT0;
      double rhoWD = norm * PMWD;
      rho0d[i] = rhoMS + rhoWD;
      n0MSd[i] = rhoMS/aveMMS;
      double n0WD = rhoWD/aveMWD;
      n0d[i]   = n0MSd[i] + n0WD;
      n0RGd[i] = n0MSd[i]* PRG/P;
      // printf ("%d rho0= %.2e + %.2e = %.2e, n0= %.2e + %.2e = %.2e, n0RG= %.2e\n",i,rhoMS,rhoWD,rho0d[i],n0MSd[i],n0WD,n0d[i],n0RGd[i]);
    }
  }
  // for Bar
  // Use 9+-1 Gyr to calculate the conversion factors (e.g., for total mass -> MS).
  // This is because the K21 fit was done with this assamption.
  // In the calculation of magnitude or PDMF, only the isochrone with 9Gyr is used, though.
  // So, a small discrepancy exists in the total mass to MS mass ratio between fb_MS value and MC simulation
  double wt_B = 0, wtWD_B = 0, sumM_B = 0, sumMWD_B = 0, sumstars_B = 0, sumWDs_B = 0, sumRGs_B = 0;
  for (int i= 0; i< nageB; i++){
    double tau = 0.01*agesB[i];
    double wtSFR = (tau - mageB)/sageB;
    wtSFR = exp(-0.5*wtSFR*wtSFR);
    double logMdie = log10(MinidieB[i]);
    double logMRG1 = log10(MRGstB[i]);
    double logMRG2 = log10(MRGenB[i]);
    double PM   = interp_x(nm+1, PMlogM_cum_norm, logMst, dlogM, logMdie);
    double P    = interp_x(nm+1, PlogM_cum_norm,  logMst, dlogM, logMdie);
    double PRG1 = interp_x(nm+1, PlogM_cum_norm,  logMst, dlogM, logMRG1);
    double PRG2 = interp_x(nm+1, PlogM_cum_norm,  logMst, dlogM, logMRG2);
    double PRG = PRG2 - PRG1; 
    double aveMloss = interp_x(nm+1, ageMloss, logMst, dlogM, logMdie);
    double PMWD = (1 - PM) * aveMloss;
    double PWD  = (1 - P);
    P   *= wtSFR;
    PWD *= wtSFR;
    PM  *= wtSFR;
    PMWD *= wtSFR;
    PRG *= wtSFR;
    wt_B   += PM;
    wtWD_B += PMWD;
    sumM_B += PM*PMlogM_cum[nm];
    sumMWD_B   += PMWD*PMlogM_cum[nm];
    sumstars_B += P   *PlogM_cum[nm];
    sumWDs_B   += PWD *PlogM_cum[nm];
    sumRGs_B   += PRG *PlogM_cum[nm];
  }
  double aveMMS = sumM_B/sumstars_B;
  double aveMWD = sumMWD_B/sumWDs_B;
  double aveM   = (sumM_B+sumMWD_B)/(sumstars_B+sumWDs_B);
  m2nb_MS  = 1/aveMMS;
  m2nb_WD  = 1/aveMWD;
  nMS2nRGb = sumRGs_B/sumstars_B; // RG to MS+RG ratio in number of stars
  fb_MS    = wt_B/(wt_B+wtWD_B);

  // for NSD
  double wt_ND = 0, wtWD_ND = 0, sumM_ND = 0, sumMWD_ND = 0, sumstars_ND = 0, sumWDs_ND = 0, sumRGs_ND = 0;
  // As of 20220207, nageND = 1.
  for (int i= 0; i< nageND; i++){
    double tau = 0.01*agesND[i];
    double wtSFR = (tau - mageND)/sageND;
    wtSFR = exp(-0.5*wtSFR*wtSFR);
    double logMdie = log10(MinidieND[i]);
    double logMRG1 = log10(MRGstND[i]);
    double logMRG2 = log10(MRGenND[i]);
    double PM   = interp_x(nm+1, PMlogM_cum_norm, logMst, dlogM, logMdie);
    double P    = interp_x(nm+1, PlogM_cum_norm,  logMst, dlogM, logMdie);
    double PRG1 = interp_x(nm+1, PlogM_cum_norm,  logMst, dlogM, logMRG1);
    double PRG2 = interp_x(nm+1, PlogM_cum_norm,  logMst, dlogM, logMRG2);
    double PRG = PRG2 - PRG1; 
    double aveMloss = interp_x(nm+1, ageMloss, logMst, dlogM, logMdie);
    double PMWD = (1 - PM) * aveMloss;
    double PWD  = (1 - P);
    P   *= wtSFR;
    PWD *= wtSFR;
    PM  *= wtSFR;
    PMWD *= wtSFR;
    PRG *= wtSFR;
    wt_ND   += PM;
    wtWD_ND += PMWD;
    sumM_ND += PM*PMlogM_cum[nm];
    sumMWD_ND   += PMWD*PMlogM_cum[nm];
    sumstars_ND += P   *PlogM_cum[nm];
    sumWDs_ND   += PWD *PlogM_cum[nm];
    sumRGs_ND   += PRG *PlogM_cum[nm];
  }
  aveMMS = sumM_ND/sumstars_ND;
  aveMWD = sumMWD_ND/sumWDs_ND;
  aveM   = (sumM_ND+sumMWD_ND)/(sumstars_ND+sumWDs_ND);
  m2nND_MS  = 1/aveMMS;
  m2nND_WD  = 1/aveMWD;
  nMS2nRGND = sumRGs_ND/sumstars_ND; // RG to MS+RG ratio in number of stars
  fND_MS    = wt_ND/(wt_ND+wtWD_ND);
  // printf("ND: %d %f %f %f %f %f\n",nageND,MinidieND[0], m2nND_MS,m2nND_WD,nMS2nRGND,fND_MS);
  free(Mass          );
  free(PlogM_cum     );
  free(PMlogM_cum    );
  free(PMlogM_cum_norm);
}

//----------------
void Mini2Mrem (double *pout, double Mini, int mean) {  // mean = 1: give mean, 0: give random
  /* Return remnant mass for a given initial mass following the initial-final mass relation by Lam et al. 2020, ApJ, 889, 31 */
  double MiniWDmax= 9;  // To make it continuous boundary between WD & NS
  double Mrem, fREM; 
  // Below is from Table 1 of Lam et al. 2020, ApJ, 889, 31
  double PNS = (Mini < MiniWDmax) ? 0  // 100% WD
             : (Mini < 15.0) ? 1  // 100% NS
             : (Mini < 17.8) ? 0.679
             : (Mini < 18.5) ? 0.833
             : (Mini < 21.7) ? 0.500
             : (Mini < 25.2) ? 0  // 100% BH
             : (Mini < 27.5) ? 0.652 
             : (Mini < 60.0) ? 0  // 100% BH
             : 0.4;
  // IFMRs for NS and BH are from Appendix C of Lam et al. 2020, ApJ, 889, 31 or from Raithel+18
  if (Mini < MiniWDmax){
     Mrem = 0.109 * Mini + 0.394; // IFMR from Kalirai+08
     fREM = 1; // WD
  }else{ 
     // NS (Eqs.(11)-(16) of Raithel+18)
     double MNS = 
            (Mini < 13.0) ? 2.24 + 0.508 *(Mini - 14.75) 
                                 + 0.125 *(Mini - 14.75)*(Mini - 14.75) 
                                 + 0.011 *(Mini - 14.75)*(Mini - 14.75)*(Mini - 14.75)
          : (Mini < 15.0) ?  0.123 + 0.112 * Mini
          : (Mini < 17.8) ?  0.996 + 0.0384* Mini
          : (Mini < 18.5) ? -0.020 + 0.10  * Mini
          : (Mini < 21.7 && mean == 0) ? 1.60 + 0.158*gasdev()
          : (Mini < 21.7 && mean == 1) ? 1.60 
          : (Mini < 27.5) ?  3232.29 - 409.429*(Mini - 2.619) 
                                     + 17.2867*(Mini - 2.619)*(Mini - 2.619) 
                                     - 0.24315*(Mini - 2.619)*(Mini - 2.619)*(Mini - 2.619)
          : (mean == 0) ? 1.78 + 0.02*gasdev()
          : 1.78;
          // print "Mini=Mini, MNS = MNS\n";

     // BH
      double Mcore = (Mini < 42.21) ? -2.049 + 0.4140 * Mini
                  : 5.697 + 7.8598 * 1e+8 * pow(Mini, -4.858);
      double Mall = 15.52 - 0.3294*(Mini - 25.97) 
                        - 0.02121*(Mini - 25.97)*(Mini - 25.97) 
                       + 0.003120*(Mini - 25.97)*(Mini - 25.97)*(Mini - 25.97);
     double fej = (Mini < 42.21) ? 0.9 : 1.0;
     double MBH = fej*Mcore + (1-fej)*Mall;
     // print "Mini=Mini, MBH = MBH\n";

     // Mean or Rand
     if (mean == 1){
       Mrem = PNS*MNS + (1-PNS)*MBH;
       fREM = PNS*2 + (1-PNS)*3;
     }else{
       double ran = ran1();
       Mrem = (ran < PNS) ? MNS : MBH;
       fREM = (ran < PNS) ?    2 :    3;
     }
  }
  pout[0] = Mrem;
  pout[1] = fREM;
}
//----------------
double fLF_detect(int nMIs, double Magst, double dMag, double extI, double Imin, double Imax, int idisk){
  double imaxd = (Imax - extI - Magst)/dMag;
  double imind = (Imin - extI - Magst)/dMag;
  if (imaxd < 0)      imaxd = 0;
  if (imaxd > nMIs-1) imaxd = nMIs - 1;
  if (imind < 0)      imind = 0;
  if (imind > nMIs-1) imind = nMIs - 1;
  int imax = imaxd;
  int imin = imind;
  double fmax = CumuN_MIs[idisk][imax+1]*(imaxd-imax) 
              + CumuN_MIs[idisk][imax]  *(1 - (imaxd-imax)); 
  double fmin = CumuN_MIs[idisk][imin+1]*(imind-imin) 
              + CumuN_MIs[idisk][imin]  *(1 - (imind-imin)); 
  return (fmax - fmin);
}
//----------------
void store_cumuP_Shu(char *infile) // calculate cumu prob dist of fg = Rg/R following Shu DF
{
  // read circular velocity
  FILE *fp;
  char line[1000];
  char *words[100];
  if (nVcs == 0){
    if((fp=fopen(infile,"r"))==NULL){
       printf("can't open %s\n",infile);
       exit(1);
    }
    nVcs = 0;
    while (fgets(line,1000,fp) !=NULL){
       split((char*)" ", line, words);
       if (*words[0] == '#') continue;
       Rcs[nVcs]  = 1000*atof(words[0]); // kpc -> pc
       Vcs[nVcs] =      atof(words[1]); // km/sec
       nVcs++;
    } 
    fclose(fp);
  }
  // Store CPD of fg following Shu DF
  // v[iz][iR][idisk]
  double getx2y(int n, double *x, double *y, double xin);
  double calc_PRRg(int R, int z, double fg, double sigU0, double hsigU, int rd);
  void get_PRRGmax2(double *pout, int R, int z, double fg1, double sigU0, double hsigU, int rd);
  for (int z = zstShu; z <= zenShu; z+=dzShu){
    int iz = (z - zstShu)/dzShu;
    double facVcz = 1 + 0.0374*pow(0.001*abs(z), 1.34); // Eq. (22) of Sharma et al. 2014, ApJ, 793, 51
    for (int R = RstShu; R <= RenShu; R+=dRShu){
      int iR = (R - RstShu)/dRShu;
      double vcR  = getx2y(nVcs, Rcs, Vcs, R);
      for (int idisk=0; idisk<8; idisk++){
        double tau = medtauds[idisk];
        double hsigU = (idisk < 7) ? hsigUt : hsigUT;
        int    rd = (idisk == 0) ? Rd[0] : (idisk <  7) ? Rd[1] : Rd[2];
        double sigU0 = (idisk < 7) ? sigU10d * pow((tau+0.01)/10.01, betaU) : sigU0td;
        double Rgmin = R0 - hsigU*log(vcR/sigU0); // which gives c = 0.5 if vcR = vcRg
        if (Rgmin > R) Rgmin = R0 - hsigU*log(240.0/sigU0); // vcmax = 240
        double fgmin0 = Rgmin/R;
        double fg1 = (fgmin0 > 1.5) ? fgmin0 : 1; // initial value of Newton method in get_PRRGmax
        double pout[4] = {};
        get_PRRGmax2(pout, R, z, fg1, sigU0, hsigU, rd);
        double   Pmax = pout[0];
        double  fgmin = pout[1];
        double  fgmax = pout[2];
        double    fgc = pout[3];
        if ((fgmin > 1 && R > 1000) || Pmax == 0) 
          printf ("# PERROR!! get_PRRGmax2(pout, %5d, %4d, %.3f, %.2f, %.2f, %d)\n",R, z, fg1, sigU0, hsigU, rd);
        // if (fgmin < 0.1 && fgc > 0.5) fgmin = 0.1;
        int swerror = ((fgmin > 1 && R > 1000) || Pmax == 0) ? 1 : 0;
        double fg   = fgmin;
        double dfg0 = (fgc - fgmin)*0.025; // divided by 40
        int ifg = 0; 
        double dfg =0;
        while(fg <= fgmax){
          fgsShu[iz][iR][idisk][ifg] = fg;
          double PRRg = calc_PRRg(R,z,fg,sigU0,hsigU,rd);
          PRRgShus[iz][iR][idisk][ifg] = PRRg;
          cumu_PRRgs[iz][iR][idisk][ifg] = (ifg==0) ? 0 : cumu_PRRgs[iz][iR][idisk][ifg-1] + 0.5*(PRRgShus[iz][iR][idisk][ifg-1] + PRRgShus[iz][iR][idisk][ifg])*dfg;
          dfg = (PRRg/Pmax < 0.05) ? 4*dfg0 : (PRRg/Pmax < 0.25 || PRRg/Pmax > 0.7) ? dfg0 : 2*dfg0;
          //  idfg = (abs(fgc-fg) <= 0.10) ? 0.02 : 0.06;
          //  printf "%2d (%.3f)  %.4f %.5e %.5e\n",ifg,fgmin,fg,PRRg,cumu_PRRgs[iz][iR][idisk][ifg]; 
          ifg++;
          fg = fg + dfg;
        }
        n_fgsShu[iz][iR][idisk] = ifg;
        // normalize and store percentiles
        double norm = cumu_PRRgs[iz][iR][idisk][ifg-1];
        for (int ktmp=0; ktmp<ifg;ktmp++){
          PRRgShus[iz][iR][idisk][ktmp]   /= norm;
          cumu_PRRgs[iz][iR][idisk][ktmp] /= norm;
          int intp = cumu_PRRgs[iz][iR][idisk][ktmp]*20;
          if (kptiles[iz][iR][idisk][intp]==0) kptiles[iz][iR][idisk][intp] = (intp==0) ? 1 : ktmp+0.5;
          // printf("(%4d-%4d-%d) ktmp= %3d (< %3d), fg= %.3f PRRg= %.4e (f= %.4f) cumu_PRRg= %.4e intp= %2d, kptile[intp]= %2d\n",z,R,idisk,ktmp,ifg,fgsShu[iz][iR][idisk][ktmp],PRRgShus[iz][iR][idisk][ktmp],PRRgShus[iz][iR][idisk][ktmp]/(Pmax/norm),cumu_PRRgs[iz][iR][idisk][ktmp], intp, kptiles[iz][iR][idisk][intp]);
        }
        if (swerror == 1) 
           printf("# i=%d, tau=%5.2f fg= %7.4f - %7.4f, fgc= %6.4f Pmax= %.3e\n",idisk,tau,fgmin,fgmax,fgc,Pmax);
      }
    }
  }
}
//---- calc Pmax, fgmin, fgmax, fgc -------
void get_PRRGmax2(double *pout, int R, int z, double fg1, double sigU0, double hsigU, int rd){
  if (fg1 < 1) fg1 = 1;
  double dfg = 0.001;
  double fgc = 1e+3, Pmax = 1e-200, dPdfgc = 0;
  double Ptmp = 0;
  double fg, fg2, fg3, fg4, dPdfg1, dPdfg2, d2Pdfg, dPdfg3, dPdfg4, d2Pdfg2, P1, P2, P3, P4;
  double jj;
  int    nj = 0, ntry = 0, sw = 0;
  double calc_PRRg(int R, int z, double fg, double sigU0, double hsigU, int rd);
  void calc_dpdfg(double *pout, int R, int z, double fg1, double sigU0, double hsigU, int rd);
  if (hsigU/rd/sigU0 < 0.1){  // eg hsigU/rd = 4 && sigU0 > 40 or hsigU/rd = 3.5 && sigU0 > 35
    for (fg=0.15;fg<1.0;fg+=0.05){ // determine fgmin & fgmax from Newton's method (search P = 0)
      P1 = calc_PRRg(R,z,fg,sigU0,hsigU,rd);
      if (P1 > Ptmp){
        Ptmp = P1;
        fg1 = fg;
      }
    }
  }
  while(1){
    int ncalc = 0;
    double pout1[2] = {}, pout2[2]= {};
    for (int j=0;j<3;j++){ // Find Pmax by Newton's method (search dP/dfg = 0)
      fg2 = fg1 + dfg;
      calc_dpdfg(pout1,R,z,fg1,sigU0,hsigU,rd); // d(PRRg)/d(fg)
      calc_dpdfg(pout2,R,z,fg2,sigU0,hsigU,rd);
      dPdfg1 = pout1[0], dPdfg2 = pout2[0];
      P1     = pout1[1], P2     = pout2[1];
      d2Pdfg = (dPdfg2-dPdfg1)/dfg; // d2(PRRg)/d(fg)
      if (P1 > Pmax){ fgc    = fg1    ;
                      dPdfgc = dPdfg1 ;
                      Pmax   = P1     ;}
      // printf "# R=%5d, now(j, ncalc, fg1, dPdfg1, dPdfg2, d2Pdfg, P)= (j, %3d, %.3f, %8.1e, %8.1e, %8.1e, %.4e), best(fgc, dPdfgc, Pmax)= (%.3f, %8.1e, %.4e)\n",R,ncalc,fg1,dPdfg1,dPdfg2,d2Pdfg,P1,fgc,dPdfgc,Pmax;
      ncalc++;
      if (ncalc > 15){
        if (nj > 0){
          break;
        }else if(ntry < 2){
          if (fgc > 900) fgc = (ntry == 0) ? fg1 : 0.9;
          fg1 = (ntry == 0) ? fgc - 0.4 : fgc + 0.4;
          if (fg1 < 0) fg1 = 0.2*ran1();
          ncalc = 0; 
          j = -1;
          ntry++;
          continue;
        }else{
          // printf ("# break!!\n");
          break;
        }
      }
      if (j==2 && fabs(dPdfgc/Pmax) > 0.1){
        nj++;
        fg1 = (dPdfgc > 0) ? fgc + 0.05/nj*ran1() : fgc - 0.05/nj*ran1();
        j = -1;
        continue;
      }
      if (dPdfg1 == 0){ // too left or too right
        jj = (dPdfgc == 0) ? 0.5 : 0.2*ran1();
        fg1 = (fg1 < fgc) ? fg1 + jj : fg1 - jj;
        j=-1;
        continue;
      }
      if (d2Pdfg > 0 && dPdfg1 < 0){ // to confirm too right or marume
        fg3 = fg2 + 0.04; // 0.04 ha tekitou
        fg4 = fg3 + dfg;
        calc_dpdfg(pout1,R,z,fg3,sigU0,hsigU,rd);
        calc_dpdfg(pout2,R,z,fg4,sigU0,hsigU,rd);
        dPdfg3 = pout1[0], dPdfg4 = pout2[0];
        P3     = pout1[1], P4     = pout2[1];
        d2Pdfg2 = (dPdfg4-dPdfg3)/dfg;
        if (d2Pdfg2 > 0 || dPdfg3 == 0){ // too right
          fg1 -= (0.02 + 0.10*ran1());
          j=-1;
          continue;
        }else{
          d2Pdfg = d2Pdfg2; // d2Pdfg > 0 was marume and replaced with d2Pdfg2 
        }
      }
      if (d2Pdfg > 0 && dPdfg1 > 0){ // to confirm too left or marume
        fg3 = fg1 - 0.04; // 0.04 ha tekitou
        fg4 = fg3 + dfg;
        calc_dpdfg(pout1,R,z,fg3,sigU0,hsigU,rd);
        calc_dpdfg(pout2,R,z,fg4,sigU0,hsigU,rd);
        dPdfg3 = pout1[0], dPdfg4 = pout2[0];
        P3     = pout1[1], P4     = pout2[1];
        d2Pdfg2 = (dPdfg4-dPdfg3)/dfg;
        if (d2Pdfg2 > 0 || dPdfg3 == 0){ // too left
          fg1 += (0.02 + 0.10*ran1());
          j=-1;
          continue;
        }else{
          d2Pdfg = d2Pdfg2; // d2Pdfg > 0 was marume and replaced with d2Pdfg2 
        }
      }
      // printf "# R=%5d, fg1(j)= %.3f, dPdfg1= %8.1e, dPdfg2= %8.1e, d2Pdfg= %8.1e, P=%.4e",R,fg1,dPdfg1,dPdfg2,d2Pdfg,P1;
      if (d2Pdfg != 0) fg1 = fg1 - dPdfg1/d2Pdfg;
      if (fg1 < 0) fg1  = 0.1;
      if (fabs(dPdfg1/d2Pdfg) > 0.5){
        jj = (dPdfgc > 0) ? 0.10 : -0.10;
        fg1 = fgc + jj*ran1();
        j = -1;
        continue;
      }
      // printf " fgc=fgc, Pmax=Pmax, nextfg1= %.3f\n",fg1;
    }
    ncalc = 0; sw = 0;
    for (fg1=fgc-0.2;fg1>0.1;fg1-=0.2){ // determine fgmin & fgmax from Newton's method (search P = 0)
      P1 = calc_PRRg(R,z,fg1,sigU0,hsigU,rd);
      ncalc++;
      if (P1 > Pmax*1.05){
        // printf "# P1 (P1 @ fg1) > Pmax (Pmax @ fgc)!! Calc again!\n";
        Pmax = P1;
        fgc = fg1;
        sw = 1;
      }
      // printf "fg1 -> %.2e (%.2e)\n",P1,P1/Pmax;
      if (P1/Pmax < 1e-02) break;
    }
    if (sw == 1) fg1 = fgc;
    // print "# next w/ fg1 = fg1 = fgc\n" if sw ==1;
    if (sw == 1) continue;
    sw = 0;
    for (fg2=fgc+0.2;fg2<4.0;fg2+=0.2){ // determine fgmin & fgmax from Newton's method (search P = 0)
      P2 = calc_PRRg(R,z,fg2,sigU0,hsigU,rd);
      ncalc++;
      if (P2 > Pmax*1.05){
        Pmax = P2;
        fgc = fg2;
        sw = 1;
        break;
      }
      // printf "fg2 -> %.2e (%.2e)\n",P2,P2/Pmax;
      if (P2/Pmax < 1e-02) break;
    }
    if (sw == 1) fg1 = fgc;
    // print "# next w/ fg1 = fg1 = fgc\n" if sw ==1;
    if (sw == 1) continue;
    if (fg1 < 0) fg1 = 0.1;
    // printf "#fg1= fg1, fg2=fg2, ncalc=ncalc, fP1=%.5f, fP2= %.5f\n",P1/Pmax,P2/Pmax;
    pout[0] = Pmax;
    pout[1] =  fg1;
    pout[2] =  fg2;
    pout[3] =  fgc;
    break;
  }
}
//---- calc P(Rg|R) following Shu distribution ( Eq.(16) of BG16 ) -------
void calc_dpdfg(double *pout, int R, int z, double fg1, double sigU0, double hsigU, int rd){
  double calc_PRRg(int R, int z, double fg, double sigU0, double hsigU, int rd);
  double dfg = 0.001;
  double fg2 = fg1 + dfg;
  double PRRg1 = calc_PRRg(R,z,fg1,sigU0,hsigU,rd);
  double PRRg2 = calc_PRRg(R,z,fg2,sigU0,hsigU,rd);
  double dPdfg = (PRRg2-PRRg1)/dfg;
  if (PRRg2 <= 0 || PRRg1 <= 0){
    dPdfg = PRRg1 = 0;  
  }
  // print "fg1=fg1, vc1=vc1, a01=a01, a1= a1, P=PRRg1\n";
  // print "fg2=fg2, vc2=vc2, a02=a02, a2= a2, P=PRRg2\n";
  pout[0] = dPdfg;
  pout[1] = PRRg1;
}
//----------------
void get_vxyz_ran(double *vxyz, int i, double tau, double D, double lD, double bD) //
{
  void Dlb2xyz(double D, double lD, double bD, double Rsun, double *xyz);
  double getcumu2xist (int n, double *x, double *F, double *f, double Freq, int ist, int inv);
  double getx2y(int n, double *x, double *y, double xin);
  double xyz[3]={};
  Dlb2xyz(D, lD, bD, R0, xyz);
  double x = xyz[0], y = xyz[1], z = xyz[2];
  double R = sqrt(x*x + y*y);
  double vx = 0, vy = 0, vz = 0;
  if (i < 8){
    double sigW0 = (i < 7) ? sigW10d * pow((tau+0.01)/10.01, betaW) : sigW0td;
    double sigU0 = (i < 7) ? sigU10d * pow((tau+0.01)/10.01, betaU) : sigU0td;
    double hsigW = (i < 7) ? hsigWt : hsigWT;
    double hsigU = (i < 7) ? hsigUt : hsigUT;
    double sigW  = sigW0*exp(-(R - R0)/hsigW);
    double sigU  = sigU0*exp(-(R - R0)/hsigU);
    int iz = (fabs(z) - zstShu)/dzShu;
    int iR = (R > RstShu) ? (R - RstShu)/dRShu : 0; // R = RstShu if R < RstShu
    do{
      double ran = ran1();
      int inttmp = ran*20;
      int kst1 = 1, kst2 = 1, kst3 = 1, kst4 = 1; // to avoid bug when inttmp = 0
      for (int itmp = inttmp; itmp > 0; itmp--){
        if (kst1 == 1) kst1 = kptiles[iz][iR][i][itmp];
        if (kst1 > 0 && kst2 > 0 && kst3 > 0 && kst4 > 0) break;
      }
      double fg1= getcumu2xist(n_fgsShu[iz][iR][i]    , fgsShu[iz][iR][i]    ,cumu_PRRgs[iz][iR][i]    ,PRRgShus[iz][iR][i]    ,ran,kst1,0);
      double fg = fg1;
      double Rg = fg*R;
      double vc = getx2y(nVcs, Rcs, Vcs, Rg) / (1 + 0.0374*pow(0.001*fabs(z), 1.34));
      double vphi = vc*fg;
      double vR =    0 + gasdev()*sigU; // radial velocity
      vx = -vphi * y/R + vR * x/R; // x/R = cosphi, y/R = sinphi
      vy =  vphi * x/R + vR * y/R; // x/R = cosphi, y/R = sinphi
      vz =    0 + gasdev()*sigW; // vertical velocity
    }while (vx*vx + vy*vy + vz*vz > vescd*vescd);
  }else if (i == 9 && ND == 3){ // NSD (when ND == 3)
    if (R > RenND || fabs(z) > zenND){
      printf("ERROR: NSD comp exists where it must not exist. (R,z)= (%f, %f)!!\n",R,z);
      exit(1);
    }
    // Bilinear interpolation of Sormani+21's NSD DF moments
    double as[4] = {}; // coeffs for interpolation
    double m_vphi = 0, logsigphi = 0, logsigR = 0, logsigz = 0, corRz = 0;
    interp_xy_coeff(nzND, nRND, as, zstND, RstND, dzND, dRND, fabs(z), R);
    int iz0  = (fabs(z) - zstND)/dzND;
    int iR0  = (R - RstND)/dRND;
    for (int j = 0; j < 4; j++){
      int iz = (j == 0 || j == 2) ? iz0 : iz0 + 1;
      int iR = (j == 0 || j == 1) ? iR0 : iR0 + 1;
      if (as[j] > 0){
        m_vphi    += as[j]*vphiNDs[iz][iR];
        logsigphi += as[j]*logsigvNDs[iz][iR][0];
        logsigR   += as[j]*logsigvNDs[iz][iR][1];
        logsigz   += as[j]*logsigvNDs[iz][iR][2];
        corRz     += as[j]*corRzNDs[iz][iR];
        // printf ("%d %d %d %f %f %f %f %f\n",j,iz,iR,m_vphi,logsigphi,logsigR,logsigz,corRz);
      }
    }
    // Random velocity with correlation coeff between vR and vz
    // ref: https://www.sas.com/offices/asiapacific/japan/service/technical/faq/list/body/stat034.html
    double sigphi = pow(10.0, logsigphi);
    double sigR   = pow(10.0, logsigR);
    double sigz   = pow(10.0, logsigz);
    double facR   = sigz/sigR * corRz;
    double sigz_R = sigz*sqrt(1 - corRz*corRz);
    do{
      double vphi = m_vphi + gasdev()*sigphi; // Assume vphi distribution is symmetrical (which is not true)
      double vR = gasdev()*sigR;
      vx = -vphi * y/R + vR * x/R; // x/R = cosphi, y/R = sinphi
      vy =  vphi * x/R + vR * y/R; // x/R = cosphi, y/R = sinphi
      vz =  facR * vR  + gasdev()*sigz_R; // random w/ correlation coeff
      // printf("%f %f %f %f %f %f %f %f %f %f\n",R,z,vphi,vR,vz,corRz,m_vphi,sigphi,sigR,sigz);
    }while (vx*vx + vy*vy + vz*vz > vescb*vescb);
  }else{ // bar & NSD (when ND <= 2)
    double vrot = 0.001 * Omega_p * R; // km/s/kpc -> km/s/pc
    double xb =  x * costheta + y * sintheta;
    double yb = -x * sintheta + y * costheta;
    double zb =  z;                          
    double sigvbs[3] = {}, sigx, sigy, sigz;
    void calc_sigvb(double xb, double yb, double zb, double *sigvbs);
    calc_sigvb(xb, yb, zb, sigvbs);
    sigx = sqrt(sigvbs[0]*sigvbs[0] * costheta*costheta + sigvbs[1]*sigvbs[1] * sintheta*sintheta);
    sigy = sqrt(sigvbs[0]*sigvbs[0] * sintheta*sintheta + sigvbs[1]*sigvbs[1] * costheta*costheta);
    sigz = sigvbs[2];
    double avevxb   = (yb > 0) ? -vx_str : vx_str;
    if (y0_str > 0){
      double tmpyn = fabs(yb/y0_str);
      avevxb  *=  (1 - exp(-tmpyn*tmpyn));
    }
    do{
      vx = - vrot * y/R + avevxb * costheta + sigx * gasdev();
      vy =   vrot * x/R + avevxb * sintheta + sigy * gasdev();
      vz =                                    sigz * gasdev();
    }while (vx*vx + vy*vy + vz*vz > vescb*vescb);
  }
  vxyz[0] = vx;
  vxyz[1] = vy;
  vxyz[2] = vz;
}
//---------------
void getaproj(double *pout, double M1, double M2, int coeff)  { // pick up aproj  
   double Mprim = (M2>M1) ? M2 : M1;
   double meanloga  = 0.57 + 1.02 * Mprim; // Table 2 of Koshimoto+20, AJ, 159, 268
   if (meanloga > MAXMEANLOGA) meanloga = MAXMEANLOGA; // avoid too small meanmaloga 
   if (meanloga < MINMEANLOGA) meanloga = MINMEANLOGA; // avoid too small meanmaloga 
   double sigmaloga = 1.61 + 1.15 * log(Mprim)/log(10); // Table 2 of Koshimoto+20, AJ, 159, 268
   if (sigmaloga > MAXSIGLOGA) sigmaloga = MAXSIGLOGA; // avoid too small sigmaloga 
   if (sigmaloga < MINSIGLOGA) sigmaloga = MINSIGLOGA; // avoid too small sigmaloga 
   double ran = coeff * fabs(gasdev());
   double loga = meanloga + ran * sigmaloga;
   // logaproj = loga - 0.133; // 0.133 = <log(a/aproj)>
   double a = pow(10.0, loga);
   ran = ran1();
   double aproj = sqrt(1 - ran*ran) * a; //probability of rproj/a from Gould&Loeb 1992
   pout[0] = loga;
   pout[1] = aproj;
}

double getcumu2xist(int n, double *x, double *F, double *f, double Freq, int ist, int inv){ 
  // for cumulative distribution (assuming linear interpolation for f(x) when cumu = F = int f(x))
  double Fmax = F[n-1];
  double Fmin = F[0];
  if (Fmin > Freq) return 0;
  if (Fmax < Freq) return 0;
  if (ist < 1) ist = 1;
  if (inv==0){
    for(int i=ist;i<n;i++){
       if (F[i] <= Freq && F[i-1] >Freq || F[i] >=Freq && F[i-1] < Freq){
          double a = 0.5*(f[i]-f[i-1])/(x[i]-x[i-1]);
          double b = f[i-1] - 2*a*x[i-1];
          double c = a*x[i-1]*x[i-1] - f[i-1]*x[i-1] + F[i-1] - Freq;
          double xreq = (a != 0) ? (-b + sqrt(b*b - 4*a*c)) * 0.5/a  // root of ax^2 +bx + c
                                 : (x[i]-x[i-1])/(F[i]-F[i-1])*(Freq-F[i-1]) + F[i-1];
          return xreq;
       }
    }
  }else{
    for(int i=ist;i>0;i--){
       if (F[i] <= Freq && F[i-1] >Freq || F[i] >=Freq && F[i-1] < Freq){
          double a = 0.5*(f[i]-f[i-1])/(x[i]-x[i-1]);
          double b = f[i-1] - 2*a*x[i-1];
          double c = a*x[i-1]*x[i-1] - f[i-1]*x[i-1] + F[i-1] - Freq;
          double xreq = (a != 0) ? (-b + sqrt(b*b - 4*a*c)) * 0.5/a  // root of ax^2 +bx + c
                                 : (x[i]-x[i-1])/(F[i]-F[i-1])*(Freq-F[i-1]) + F[i-1];
          return xreq;
       }
    }
  }
  return 0;
}
//---------------
void get_MAG_MLfiles(int ROMAN, char **MAG, char **MLfiles, double *lameff){
  if (ROMAN == 1){
    char M[][8] = {"J", "H", "Ks", "Z087", "W146", "F213"};
    char F[][60] = { "input_files/isochrone_thin1.dat", "input_files/isochrone_thin2.dat",
                     "input_files/isochrone_thin3.dat", "input_files/isochrone_thin4.dat",
                     "input_files/isochrone_thin5.dat", "input_files/isochrone_thin6.dat",
                     "input_files/isochrone_thin7.dat", "input_files/isochrone_thick.dat",
                     "input_files/isochrone_bar.dat"  , "input_files/isochrone_NSD.dat"   };
    double lams[] = {1240, 1664, 2164, 867.590, 1367.793, 2112.465}; // J,H,Ks from Table 1 of Nishiyama+09, Roman-bands are based on output of PARSEC isochrone webpage, nband should be used
    for (int i = 0; i < nband; i++){
      strcpy(MAG[i], M[i]);
      lameff[i] = lams[i];
    }
    for (int icomp=0; icomp<ncomp; icomp++){
      strcpy(MLfiles[icomp], F[icomp]);
    }
    // printf("MAG %s %s %s %s %s %s\n", MAG[0], MAG[1], MAG[2], MAG[3], MAG[4], MAG[5]);
  }else{
    char M[][8] = {"V", "I", "J", "H", "Ks"};
    char F[][60] = { "input_files/isoemp_thin1.dat", "input_files/isoemp_thin2.dat",
                     "input_files/isoemp_thin3.dat", "input_files/isoemp_thin4.dat",
                     "input_files/isoemp_thin5.dat", "input_files/isoemp_thin6.dat",
                     "input_files/isoemp_thin7.dat", "input_files/isoemp_thick2.dat",
                     "input_files/isoemp_bar.dat"  , "input_files/isoemp_NSD.dat"   };
    // double lams[] = {549.056, 805.988, 1237.560, 1647.602, 2162.075}; // based on output of PARSEC isochrone webpage
    double lams[] = {549.056, 805.988, 1240, 1664, 2164}; // V, I are based on output of PARSEC isochrone webpage, J,H,Ks from Table 1 of Nishiyama+09
    for (int i = 0; i < nband; i++){
      strcpy(MAG[i], M[i]);
      lameff[i] = lams[i];
    }
    for (int icomp=0; icomp<ncomp; icomp++){
      strcpy(MLfiles[icomp], F[icomp]);
    }
  }
}

//---------------
int get_ML_LF(int calcLF, int ROMAN, char **MLfiles, int iMag, int *nMLrel, double **Minis, double **MPDs, double ***Mags, double **Rstars, double *Minvs, int Magst, int Magen, double dMag, double **CumuLFs, double *logMass, double *PlogM_cum_norm, double *PlogM) 
/* Read mass-luminosity relation and make LF in iMag-band for each component. 
 * Update for NSD on 20220207 */
{
   char *infile;
   FILE *fp;
   char line[1000];
   char *words[100];
   int i=0;

   // Make LFs in H for each component
   int Nbin = (Magen - Magst)/dMag;
   for (int icomp=0; icomp<ncomp; icomp++){
     if((fp=fopen(MLfiles[icomp],"r"))==NULL){
       printf("can't open %s\n", MLfiles[icomp]);
       exit(1);
     }
     int narry = 0;
     double Magpre = 9999;
     while (fgets(line,1000,fp) !=NULL){
       int nwords = split(" ", line, words);
       if (*words[0] == '#') continue;
       if (log10(atof(words[0])) < logMst) continue; // Skip if Mini < Mmin considered
       if (atof(words[2]) == 0) continue; // Skip the line for WD (Rad==0) 
       Minis[icomp][narry] = atof(words[0]);
       MPDs[icomp][narry] = atof(words[1]);
       Rstars[icomp][narry] = atof(words[2]);
       for (int j=0; j < nband; j++){
         Mags[j][icomp][narry] = atof(words[j+3]);
       }
       if (Mags[iMag][icomp][narry] > Magpre && Minvs[icomp] == 0) Minvs[icomp] = Minis[icomp][narry-1];
       Magpre = Mags[iMag][icomp][narry];
       narry ++;
     }
     fclose(fp);
     nMLrel[icomp] = narry;
     // printf ("%d (iagest= %4d iageen= %4d dtau= %3d iage= %2d nmax= %4d)  read %30s\n",icomp,iagest,iageen,dtau,iage,nmax,file1);
     // Store interpolated Mini vs Mags
     // void spline_coeffs (int n, double *x, double *y, double *d); // n here needs to be [number of array - 1]
     // double spline_x2y (int n, int ist, double *x, double *y, double *d, double xreq);
     // double *dsp;
     // dsp = calloc(narry, sizeof(double *));
     // spline_coeffs(narry-1, Minis[icomp], Mags[iMag][icomp], dsp);

     if (calcLF == 0) continue; // for lens catalog

     // Make LF for iMag-band
     double *pIs, Ptotal = 0;
     pIs = calloc(Nbin+1, sizeof(double *));
     double logMini = log10(Minis[icomp][0]); // minimum mass considered in MLrelation 0.0005 - 0.003 Msun
     // double PBD = interp_x(nm+1, PlogM_cum_norm, logMst, dlogM, logMini);
     double PBD = interp_xquad(nm+1, PlogM_cum_norm, PlogM, logMst, dlogM, logMini);
     pIs[Nbin - 3] += PBD;
     Ptotal += PBD;
     for (int k=0; k< narry - 1; k++){
       if (Minis[icomp][k+1] == 0) continue;
       double Mini1 = Minis[icomp][k];
       double Mini2 = (Minis[icomp][k+1] > 0) ? Minis[icomp][k+1] : 0;
       if (Mini1 > Mini2) printf ("Warning!! Mini1 > Mini2 (icomp= %d k= %d) !!!!\n",icomp,k);
       double logMini1 = log10(Minis[icomp][k]);
       double logMini2 = log10(Minis[icomp][k+1]);
       int nii = 10;
       double dlogMini = (logMini2-logMini1)/nii;
       for (int ii= 0; ii < nii; ii++){
         double logMini = (ii + 0.5) * dlogMini + logMini1;
         double Mini = pow(10.0, logMini);
         // double Mag = spline_x2y(narry-1, k+1, Minis[icomp], Mags[iMag][icomp], dsp, Mini);
         int khi = k + 1;
         double Mag = getx2y_khi(narry, Minis[icomp], Mags[iMag][icomp], Mini, &khi);
         if (Mini < 0.09 && (iMag == 3 || iMag == 5) && ROMAN == 1) Mag = 99; // for MZ087 and MF213
         // printf ("%d %.9f %7.3f\n",icomp, Mini,Mag);
         double P1 = interp_xquad(nm+1, PlogM_cum_norm, PlogM, logMst, dlogM, logMini - 0.5 * dlogMini);
         double P2 = interp_xquad(nm+1, PlogM_cum_norm, PlogM, logMst, dlogM, logMini + 0.5 * dlogMini);
         double wtM = P2 - P1;
         int pI = (Mag - Magst)/dMag;
         if (pI < 0) pI = 0;
         if (pI >= Nbin) pI = Nbin - 3;
         pIs[pI] += wtM;
         Ptotal += wtM;
       }
       // printf ("icomp= %d j=%d, tau= %5.2f k=%3d, M1=%.9f, M2=%.9f, P1=%.6f, P2=%.6f, wtM=%.6f MI= %.6f pI= %3d wtSFR= %.6f ( %.6f ) \n",icomp,j,tau,k,Mini1,Mini2,P1,P2,wtM,MIc,pI,wtSFR, exp(-gamma*(10-tau)));
     }

     for (int pI=0;pI<=Nbin;pI++){
        double Mag = Magst + pI * dMag;
        // printf ("%d %6.2f",icomp, Mag);
        if (pI>=1) {
          CumuN_MIs[icomp][pI] = 0.5*(pIs[pI] + pIs[pI-1])/Ptotal  + CumuN_MIs[icomp][pI-1];
        } else {
          CumuN_MIs[icomp][pI] = 0.0;
        }
        // printf (" %.6e %.6e\n",pIs[pI],CumuN_MIs[icomp][pI]);
     }
     free (pIs);
   }

   //----------------------------------
   // Normalize cumulative distirbution
   for (int k=0; k<ncomp; k++){
   for (int j=0; j<=Nbin; j++){
      CumuN_MIs[k][j] /= CumuN_MIs[k][Nbin];
      double Mag = Magst + j * dMag;
      // printf ("%1d %03d %5.2f %.6e\n",k,j,Mag,CumuN_MIs[k][j]);
   }}
   // printf ("#N= %d read from %s\n",i, infile);


   return Nbin+1;
}
//---- calc P(Rg|R) following Shu distribution ( Eq.(14) of Sharma et al. 2014, ApJ, 793, 51) -------
double calc_PRRg(int R, int z, double fg, double sigU0, double hsigU, int rd){ 
  double getx2y(int n, double *x, double *y, double xin);
  if (fg <= 0) return 0;
  double  calc_faca(double Rg, double hsigU, int rd, double a0);
  double calc_SigRg(double Rg, double hsigU, int rd, double a0);
  double    calc_gc(double c);
  double Rg = R*fg;
  double vc = getx2y(nVcs, Rcs, Vcs, Rg) / (1 + 0.0374*pow(0.001*abs(z), 1.34));
  double a0 = sigU0/vc * exp(R0/hsigU);
  double a  = sigU0/vc * exp(-(Rg - R0)/hsigU);
  double faca = calc_faca(Rg,hsigU,rd,a0);
  a *= faca;
  double c = 0.5/a/a;
  if (c <= 0.5) return 0;
  double SigRg = calc_SigRg(Rg,hsigU,rd,a0);
  double gc = calc_gc(c);
  double x = c*(2*log(fg) + 1 - fg*fg);
  double PRRg = SigRg * exp(x)/gc;
  // printf ("(calc_PRRg) R=%d, Rg=%f, vc=%f a0= sigU0(%f)/vc(%f) * exp(R0(%f)/hsigU(%f))(%.3e)= %.3e, a= %.3e, c= %.3e, SigRg= %.3e, gc= %.3e, x= %.3e, PRRg= SigRg*exp(x)/gc = %.3e * %.3e = %.3e\n",R,Rg,vc,sigU0,vc,R0,hsigU,exp(R0/hsigU),a0,a,c,SigRg,gc,x,SigRg,exp(x)/gc,PRRg);
  if (PRRg < 0) PRRg = 0;
  return PRRg;
}
/*----------------------------------------------------------------*/
double calc_gc(double c){ // Eq.(16) of Sharma et al. 2014, ApJ, 793, 51
   if (c < 0.5) return 0;
  double c2, gamma, c3, gc;
  if (c < 10){
    c2 = c - 0.5;
    gamma = tgamma(c2);
    c3 = 2 * pow(c, c2);
    gc = exp(c) * gamma/c3;
  }else{
    gc = sqrt(0.5*PI/(c - 0.913)); // approximation Eq. (14) of Schonrich & Binney 2012
  }
  return gc;
}
/*----------------------------------------------------------------*/
double calc_SigRg(double Rg, double hsigU, int rd, double a0){ // Rd**2 * Eq.(20) of Sharma et al. 2014, ApJ, 793, 51
  double k = 31.53, a = 0.6719, b = 0.2743;
  // my (c1, c2, c3, c4) = (3.740, 0.523, 0.00976, 2.29); # for flat vc
  double c1 = 3.822, c2 = 0.524, c3 = 0.00567, c4 = 2.13; // for rising vc from Table 1 of Sharma & Bland-Hawhorn (2013), ApJ, 773, 183
  double q = rd/hsigU;
  double Rgmax = c1*rd/(1+q/c2); // Eq.32 of Sharma & Bland-Hawhorn (2013), ApJ, 773, 183
  // x = Rg/3.74/Rd/(1+q/0.523); # This is form in Sharma+14, but wrong
  double x = Rg/Rgmax;  // x = Rg/Rgmax in Sharma & Bland-Hawhorn (2013), ApJ, 773, 183
  double s = k*exp(-x/b)*((x/a)*(x/a) - 1); // Eq. (21) of Sharma et al. 2014, ApJ, 793, 51
  double SigRg = 0.5*exp(-Rg/rd)/PI - c3*pow(a0,c4) * s;
  return SigRg;
}
/*----------------------------------------------------------------*/
double calc_faca(double Rg, double hsigU, int rd, double a0){ // Eq.(39) of Sharma & Bland-Hawhorn (2013), ApJ, 773, 183
  double q = rd/hsigU;
  double bunsi = 0.25*pow(a0, 2.04);
  double bumbo = pow(q, 0.49);
  double as[12] = {-0.028476,-1.4518,12.492,-21.842,19.130,-10.175,3.5214,-0.81052,0.12311,-0.011851,0.00065476,-1.5809e-05};
  double x = Rg*q/rd;
  double fpoly = as[0] + as[1]*x + as[2]*pow(x,2.) + as[3]*pow(x,3.) + as[4]*pow(x,4.) + as[5]*pow(x,5.) + as[6]*pow(x,6.) + as[7]*pow(x,7.) + as[8]*pow(x,8.) + as[9]*pow(x,9.) + as[10]*pow(x,10.) + as[11]*pow(x,11.);
  double faca = (1 - bunsi/bumbo * fpoly);
  return faca;
}


/*----------------------------------------------------------------*/
/*                   for Normalize rho or sigv                    */
/*----------------------------------------------------------------*/
double crude_integrate(double xmax, double ymax, double zmax, int nbun)  // for normalize rho_b
{
  double calc_rhoB(double xb, double yb, double zb);
  int get_p_integral(int nji, double *ls, double *ks);
  double *ls, *ks;
  int nmin, narry, nji, ncalc;
  nji  =   2;
  // nbun =  30;
  narry = (nji <= 1) ?  1 :
          (nji <= 2) ?  3 :
          (nji <= 4) ?  9 :
          (nji <= 6) ? 18 :
          (nji <= 8) ? 30 : 42;
  ls = (double *)malloc(sizeof(double *) * narry);
  ks = (double *)malloc(sizeof(double *) * narry);
  nmin = get_p_integral(nji, ls, ks);
  if (nbun < nmin) nbun = nmin;
  ncalc = nbun + 1 + 2*narry - 2*nji;  // ls[narry] includes i <= nji - 1
  double xb, xb0, yb, yb0, zb, zb0, rho, rho0, *rhosumz, *rhosumyz;
  rhosumz  = (double *)malloc(sizeof(double *) * ncalc);
  rhosumyz = (double *)malloc(sizeof(double *) * ncalc);
  double dx, dy, dz, dxtmp, dytmp, dztmp;
  // int xmax = 2200, ymax = 1400, zmax = 1200;
  dx = (double) (xmax - 0)/nbun;
  dy = (double) (ymax - 0)/nbun;
  dz = (double) (zmax - 0)/nbun;
  // printf ("dx= %.1f dy= %.1f dz= %.1f\n",dx,dy,dz);
  double totalmass = 0, massVVVbox = 0;
  int ixtmp, iytmp, iztmp;
  for (int ix = 0; ix < ncalc; ix++){
    ixtmp = ix - 2*narry + nji; // ixtmp = nji - nbun - nji
    xb = (ix>=2*narry) ? 0 + dx * ixtmp 
        : (ix % 2 == 0) ? 0 + dx * ls[ix/2] : xmax - dx * ls[ix/2];
    for (int iy = 0; iy < ncalc; iy++){
       iytmp = iy - 2*narry + nji; // iytmp = nji - nbun - nji
       yb = (iy>=2*narry) ? 0 + dy * iytmp 
          : (iy % 2 == 0) ? 0 + dy * ls[iy/2] : ymax - dy * ls[iy/2];
       rhosumz[iy] = 0;
       for(int j=0;j< narry;j++){
           dztmp = dz*ls[j];
           zb0   =    0 + dztmp;
           zb    = zmax - dztmp;
           rho0       = calc_rhoB(xb, yb,  zb0);
           rho        = calc_rhoB(xb, yb,   zb);
           rhosumz[iy] += (rho0 + rho)*ks[j];
       }
       for(int j=nji;j<=nbun-nji;j++){
           zb  =    0 + dz*j;
           rho = calc_rhoB(xb, yb, zb);
           rhosumz[iy] += rho;
       }
       rhosumz[iy] *= dz;
       // printf ("iy=%d iytmp=%d yb= %f ndy= %f ls[%d]= %f rhosumz= %f\n",iy,iytmp,yb,yb/dy,iy/2,ls[iy/2],rhosumz[iy]);
    }
    rhosumyz[ix] = 0;
    for(int j=0;j< narry;j++){
        rho0  = rhosumz[2*j];
        rho   = rhosumz[2*j+1];
        // printf ("iy=%d rhosumz= %f\n",2*j  ,rhosumz[2*j]);
        // printf ("iy=%d rhosumz= %f\n",2*j+1,rhosumz[2*j+1]);
        rhosumyz[ix] += (rho0 + rho)*ks[j];
    }
    for(int j=2*narry;j<ncalc;j++){
        rhosumyz[ix] += rhosumz[j];
    }
    rhosumyz[ix] *= dy;
    // printf ("ix=%d ixtmp=%d ndx= %f ls[%d]= %f rhosumyz= %f\n",ix,ixtmp,xb/dx,ix/2,ls[ix/2],rhosumyz[ix]);
  }
  for(int j=0;j< narry;j++){
      rho0  = rhosumyz[2*j];
      rho   = rhosumyz[2*j+1];
      // printf ("ix=%d rhosumyz= %f\n",2*j  ,rhosumyz[2*j]);
      // printf ("ix=%d rhosumyz= %f\n",2*j+1,rhosumyz[2*j+1]);
      totalmass += (rho0 + rho)*ks[j];
  }
  for(int j=2*narry;j<ncalc;j++){
      totalmass += rhosumyz[j];
  }
  totalmass *= dx*8;
  massVVVbox = totalmass;
  free (ls);
  free (ks);
  free (rhosumz);
  free (rhosumyz);
  return totalmass;
}
//---------------
void calc_sigvb(double xb, double yb, double zb, double *sigvbs)
{
  double xn, yn, zn, Rs, rs, facsig, facsigz = 0;
  xn = fabs(xb/x0_vb), yn = fabs(yb/y0_vb), zn = fabs(zb/z0_vb);
  Rs = pow((pow(xn, C1_vb) + pow(yn, C1_vb)), 1/C1_vb);
  rs = pow(pow(Rs, C2_vb) + pow(zn, C2_vb), 1/C2_vb);
  if (rs==0 && model_vb == 8) rs = 0.0001; // to avoid infty
  facsig = (model_vb == 5) ? exp(-rs)  // exponential
         : (model_vb == 6) ? exp(-0.5*rs*rs) // Gaussian
         : (model_vb == 7) ? pow( 2.0/(exp(rs)+exp(-rs)), 2) // sech2
         : (model_vb == 4) ? exp(-pow(rs,C3_vb))  
         : 0;
  if (model_vbz >= 4){
    xn = fabs(xb/x0_vbz), yn = fabs(yb/y0_vbz), zn = fabs(zb/z0_vbz);
    Rs = pow((pow(xn, C1_vbz) + pow(yn, C1_vbz)), 1/C1_vbz);
    rs = pow(pow(Rs, C2_vbz) + pow(zn, C2_vbz), 1/C2_vbz);
    if (rs==0 && model_vbz == 8) rs = 0.0001; // to avoid infty
    facsigz = (model_vbz == 5) ? exp(-rs)  // exponential
            : (model_vbz == 6) ? exp(-0.5*rs*rs) // Gaussian
            : (model_vbz == 7) ? pow( 2.0/(exp(rs)+exp(-rs)), 2) // sech2
            : (model_vbz == 4) ? exp(-pow(rs,C3_vbz))  
            : 0;
  }else{
    facsigz = facsig;
  }
  sigvbs[0] = sigx_vb * facsig + sigx_vb0;
  sigvbs[1] = sigy_vb * facsig + sigy_vb0;
  sigvbs[2] = sigz_vb * facsigz + sigz_vb0;
}
/*----------------------------------------------------------------*/
/*                      for general use                           */
/*----------------------------------------------------------------*/
void calc_rho_each(double D, int idata, double *rhos, double *xyz, double *xyb){  // return rho for each component 
  void Dlb2xyz(double D, double lD, double bD, double Rsun, double *xyz);
  double calc_rhoB(double xb, double yb, double zb);
  double x, y, z, R, xb, yb, zb, xn, yn, zn, rs, zdtmp, rhotmp;
  double lD, bD;
  lD = lDs[idata];
  bD = bDs[idata];
  Dlb2xyz(D, lD, bD, R0, xyz);
  x = xyz[0], y = xyz[1], z = xyz[2];
  R = sqrt(x*x + y*y);
  // i = 0-6: thin disk, i=7: thick disk, i=8: bulge, i=9: long bar, i = 10: super thin bar
  // for (int i = 0; i<ncomp; i++){rhos[i] = 0;} // shokika
  for (int i = 0; i<ncomp+1; i++){rhos[i] = 0;} // shokika
  // Disk
  int idisk, itmp, ist;
  if (DISK > 0){
    double ftmp = (hDISK == 0) ? 0.005 : (hDISK == 1) ? 0.01 : 0;
    // ist = (fabs(z) < 400) ? ftmp*fabs(z)  : (fabs(z) <= 1200) ?  4 : 7;
    ist = 0;
    for (idisk = ist; idisk < 8; idisk++){ // ignore disk0 - disk3
      zdtmp =(hDISK == 0) ? zd[idisk] :
              (R > 4500)  ? zd[idisk] + (R-R0)*(zd[idisk] - zd45[idisk])/(R0 - 4500) 
                          : zd45[idisk];
      rhotmp  = (idisk < 7) ? 4.0/(exp(2*z/zdtmp)+exp(-2*z/zdtmp)+2)
                            : exp(-fabs(z)/zd[idisk]);
      itmp = (idisk == 0) ? 0 : (idisk <  7) ? 1 : 2;
      rhotmp *= zd[idisk]/zdtmp;  // zd/zdtmp is to keep Sigma(R) as exponential 
      if (DISK == 1) rhotmp = rhotmp * exp(-R/Rd[itmp] - pow(((double)Rh/R),nh));
      if (DISK == 2) rhotmp = (R > Rdbreak) ? rhotmp * exp(-R/Rd[itmp])
                                            : rhotmp * exp(- (double)Rdbreak/Rd[itmp]); // const. in R < 5300
      if (DISK == 3) rhotmp = rhotmp * exp(-R/Rd[itmp]);
      rhos[idisk]  = rhotmp/y0d[itmp]; // Number density of BD + MS
    }
  }
  // Bar
  xb =  x * costheta + y * sintheta;
  yb = -x * sintheta + y * costheta;
  zb =  z;                          
  rhos[8] = calc_rhoB(xb,yb,zb);
  // ND 
  if (ND > 0){
    if (ND == 3){
      if (R <= RenND - 30 && fabs(z) <= zenND - 20){
        rhos[9] = pow(10.0, interp_xy(nzND, nRND, logrhoNDs, zstND, RstND, dzND, dRND, fabs(z), R));
      }else{
        rhos[9] = 0;
      }
    }else{
      // See Eq. (28) of Portail et al. 2017
      xn = fabs(xb/x0ND), yn = fabs(yb/y0ND), zn = fabs(zb/z0ND);
      rs = pow((pow(xn, C1ND) + pow(yn, C1ND)), 1/C1ND) + zn;
      rhos[9]  = exp(-rs);  
    }
  }
  // NSC 
  if (NSC > 0){
    double zq = z/qNSC;
    double aNSC = sqrt(R*R + zq*zq);
    if (aNSC < 200){
      double bunbo = pow(aNSC, gammaNSC) * pow(aNSC+a0NSC, 4-gammaNSC);
      rhos[10] = a0NSC/bunbo;
    }
  }
  xyb[0] = xb;
  xyb[1] = yb;
}
//---------------
double calc_rhoB(double xb, double yb, double zb)
{
  double xn, yn, zn, R, Rs, rs, rho, rho2, rhoX;
   R = sqrt(xb*xb + yb*yb);
  // 1st  Bar
  if (model >= 4 && model <= 8){
    xn = fabs(xb/x0_1), yn = fabs(yb/y0_1), zn = fabs(zb/z0_1);
    Rs = pow((pow(xn, C1) + pow(yn, C1)), 1/C1);
    rs = pow(pow(Rs, C2)     + pow(zn, C2), 1/C2);
    if (rs==0 && model == 8) rs = 0.0001; // to avoid infty
    rho = (model == 5) ? exp(-rs)  // exponential for 4 or 5
        : (model == 6) ? exp(-0.5*rs*rs) // Gaussian
        : (model == 7) ? pow( 2.0/(exp(rs)+exp(-rs)), 2) // sech2
        : (model == 4) ? exp(-pow(rs, C3))
        : 0;
  }
  if (R  >= Rc) rho *= exp(-0.5*(R-Rc)*(R-Rc)/srob/srob);
  if (fabs(zb) >= zb_c) rho *= exp(-0.5*(fabs(zb)-zb_c)*(fabs(zb)-zb_c)/200.0/200.0);

  // X-shape
  if (addX >= 5){
    xn = fabs((xb-b_zX*zb)/x0_X), yn = fabs((yb-b_zY*zb)/y0_X), zn = fabs(zb/z0_X);
    rs = pow(pow((pow(xn, C1_X) + pow(yn, C1_X)), C2_X/C1_X) + pow(zn, C2_X), 1/C2_X);
    rhoX  = (addX == 5) ? exp(-rs)  // exponential
          : (addX == 6) ? exp(-0.5*rs*rs) // Gaussian
          : (addX == 7) ? pow( 2.0/(exp(rs)+exp(-rs)), 2) // sech2
           : 0;
    xn = fabs((xb+b_zX*zb)/x0_X), yn = fabs((yb-b_zY*zb)/y0_X);
    rs = pow(pow((pow(xn, C1_X) + pow(yn, C1_X)), C2_X/C1_X) + pow(zn, C2_X), 1/C2_X);
    rhoX += (addX == 5) ? exp(-rs)  // exponential
          : (addX == 6) ? exp(-0.5*rs*rs) // Gaussian
          : (addX == 7) ? pow( 2.0/(exp(rs)+exp(-rs)), 2) // sech2
           : 0;
    if (b_zY > 0.0){
      xn = fabs((xb-b_zX*zb)/x0_X), yn = fabs((yb+b_zY*zb)/y0_X);
      rs = pow(pow((pow(xn, C1_X) + pow(yn, C1_X)), C2_X/C1_X) + pow(zn, C2_X), 1/C2_X);
      rhoX += (addX == 5) ? exp(-rs)  // exponential
            : (addX == 6) ? exp(-0.5*rs*rs) // Gaussian
            : (addX == 7) ? pow( 2.0/(exp(rs)+exp(-rs)), 2) // sech2
             : 0;
      xn = fabs((xb+b_zX*zb)/x0_X), yn = fabs((yb+b_zY*zb)/y0_X);
      rs = pow(pow((pow(xn, C1_X) + pow(yn, C1_X)), C2_X/C1_X) + pow(zn, C2_X), 1/C2_X);
      rhoX += (addX == 5) ? exp(-rs)  // exponential
            : (addX == 6) ? exp(-0.5*rs*rs) // Gaussian
            : (addX == 7) ? pow( 2.0/(exp(rs)+exp(-rs)), 2) // sech2
             : 0;
    }
    rhoX *= fX;
    if (R >= Rc_X) rhoX *= exp(-0.5*(R-Rc_X)*(R-Rc_X)/srob/srob);
  }
  if (addX >=5) rho += rhoX;
  return rho;
}
//---------------
void Dlb2xyz(double D, double lD, double bD, double Rsun, double *xyz)
/*------------------------------------------------------------*/
/*  Give (x,y,z) for a given D, lD, bD  
 *  Updated to give the x, y, z relative to xyzSgrA on 2022 04 08 */
/*------------------------------------------------------------*/
{
  double cosbsun = cos(zsun/Rsun), sinbsun = sin(zsun/Rsun);
  double cosb = cos(bD/180.0*PI), sinb = sin(bD/180.0*PI), 
         cosl = cos(lD/180.0*PI), sinl = sin(lD/180.0*PI);
  double xtmp = Rsun - D * cosb * cosl;
  double ytmp = D * cosb * sinl;       
  double ztmp = D * sinb;              
  // xyz[0] = -ztmp * sinbsun + xtmp * cosbsun;
  xyz[0] =  xtmp - xyzSgrA[0]; 
  xyz[1] =  ytmp - xyzSgrA[1];                            
  xyz[2] =  ztmp * cosbsun + xtmp * sinbsun - xyzSgrA[2]; 
}

//---------------
double elongation(double azi1, double alt1, double azi2, double alt2)
/*------------------------------------------------------------*/
/*  Copy-pasted of elongation from sky2ccd.pl  */
/*------------------------------------------------------------*/
{

  double a1 = azi1 / 180.0 * PI;
  double h1 = alt1 / 180.0 * PI;
  double a2 = azi2 / 180.0 * PI;
  double h2 = alt2 / 180.0 * PI;

  double sh1 = sin(h1);
  double ch1 = cos(h1);
  double sh2 = sin(h2);
  double ch2 = cos(h2);
  double sa  = sin(a2 - a1);
  double ca  = cos(a2 - a1);
  double sd2 = ch2 * ch2 * sa * sa + ch1 * ch1 * sh2 * sh2 + sh1 * sh1 * ch2 * ch2 * ca * ca - 2.0 * ch1 * sh1 * ch2 * sh2 * ca;

  double sd = sqrt(sd2);
  double cd = sh1 * sh2 + ch1 * ch2 * ca;

  double d = atan2(sd, cd) / PI * 180.0;

  return d;
}

/*----------------------------------------------------------------*/
/*                      for general use                           */
/*----------------------------------------------------------------*/
//---- get parameters for trapezoidal integral -------
// Values from http://midarekazu.g2.xrea.com/Newton-Cotes.html
int get_p_integral(int nji, double *ls, double *ks)
{
   int nmin, i = 0, j=0, k=0;
   nji = (nji <= 1) ? 1 :
         (nji <= 2) ? 2 :
         (nji <= 4) ? 4 :
         (nji <= 6) ? 6 :
         (nji <= 8) ? 8 : 10;
   if (nji == 1){ // Trapezoid
      ls[i++]= 0.0;
      ks[j++]= 0.5;
      nmin = 1;
   }
   if (nji == 2){ // simpson
      ls[i++]= 0.0,    ls[i++]= 0.5,    ls[i++]=  1.0;
      ks[j++]= 3.0/12, ks[j++]= 4.0/12, ks[j++]= 11.0/12;
      nmin = 3;
   }
   if (nji == 4){ // boole
      ls[i++]=0.0,
      ls[i++]=1.0/4, ls[i++]=1.0/2, ls[i++]=3.0/4, ls[i++]=1.0, ls[i++]=3.0/2, ls[i++]=2.0, 
      ls[i++]=9.0/4, ls[i++]=3.0;
      ks[j++]= 70./360,  
      ks[j++]= 32./360, ks[j++]= 76./360, ks[j++]= 128./360, ks[j++]=187./360, ks[j++]= 100./360, 
      ks[j++]=218./360, ks[j++]= 96./360, ks[j++]= 353./360; 
      nmin = 7;
   }
   if (nji == 6){
      ls[i++]=0.0,
      ls[i++]=1.0/6, ls[i++]=1.0/3, ls[i++]=1.0/2, ls[i++]=2.0/3, ls[i++]=5.0/6, ls[i++]=1.0, 
      ls[i++]=4.0/3, ls[i++]=3.0/2, ls[i++]=5.0/3, ls[i++]=2.0,   ls[i++]=5.0/2, ls[i++]=8.0/3, 
      ls[i++]=3.0,   ls[i++]=10.0/3,ls[i++]=4.0,   ls[i++]=25.0/6,ls[i++]=5.0;
      ks[j++]= 861./5040,  
      ks[j++]= 216./5040,  ks[j++]= 459./5040,  ks[j++]= 920./5040,  ks[j++]= 945./5040,  
      ks[j++]=1296./5040,  ks[j++]=2208./5040,  ks[j++]= 162./5040,  ks[j++]= 816./5040,   
      ks[j++]= 567./5040,  ks[j++]=2955./5040,  ks[j++]=2008./5040,  ks[j++]= 108./5040,   
      ks[j++]=3459./5040,  ks[j++]= 999./5040,  ks[j++]=3662./5040,  ks[j++]=1080./5040,  
      ks[j++]=4999./5040;
      nmin = 11;
   }
   if (nji == 8){
      ls[i++]=0.0,
      ls[i++]=1.0/8,  ls[i++]=1.0/4, ls[i++]=3.0/8, ls[i++]=1.0/2, ls[i++]=5.0/8,  ls[i++]=3.0/4, 
      ls[i++]=7.0/8,  ls[i++]=1.0,   ls[i++]=9.0/8, ls[i++]=5.0/4, ls[i++]=3.0/2,  ls[i++]=7.0/4, 
      ls[i++]=15.0/8, ls[i++]=2.0,   ls[i++]=9.0/4, ls[i++]=5.0/2, ls[i++]=21.0/8, ls[i++]=3.0,  
      ls[i++]=25.0/8, ls[i++]=7.0/2, ls[i++]=15.0/4,ls[i++]=4.0,   ls[i++]=35.0/8, ls[i++]=9.0/2, 
      ls[i++]=5.0,    ls[i++]=21.0/4,ls[i++]=6.0,   ls[i++]=49.0/8,ls[i++]=7.0;
      ks[j++]= 35604./226800,  
      ks[j++]=  5888./226800, ks[j++]= 10848./226800, ks[j++]= 28160./226800, 
      ks[j++]= 17156./226800, ks[j++]= 39936./226800, ks[j++]= 52608./226800, 
      ks[j++]= 47104./226800, ks[j++]= 43213./226800, ks[j++]= 31488./226800, 
      ks[j++]= 16352./226800, ks[j++]= 20940./226800, ks[j++]=  5280./226800, 
      ks[j++]= 83968./226800, ks[j++]= 31410./226800, ks[j++]= 60192./226800, 
      ks[j++]= 19284./226800, ks[j++]= 91136./226800, ks[j++]=103575./226800, 
      ks[j++]= 52480./226800, ks[j++]= -8228./226800, ks[j++]= 58336./226800, 
      ks[j++]= 99196./226800, ks[j++]=102912./226800, ks[j++]= -5568./226800, 
      ks[j++]=184153./226800, ks[j++]= 28832./226800, ks[j++]=177718./226800, 
      ks[j++]= 41216./226800, ks[j++]=225811./226800;
      nmin = 15;
   }
   if (nji == 10){
      ls[i++]=0.0,
      ls[i++]=1.0/10, ls[i++]=1.0/5,  ls[i++]=3.0/10, ls[i++]=2.0/5,  ls[i++]=1.0/2,  
      ls[i++]=3.0/5,  ls[i++]=7.0/10, ls[i++]=4.0/5,  ls[i++]=9.0/10, ls[i++]=1.0,   
      ls[i++]=6.0/5,  ls[i++]=7.0/5,  ls[i++]=3.0/2,  ls[i++]=8.0/5,  ls[i++]=9.0/5, 
      ls[i++]=2.0,    ls[i++]=21.0/10,ls[i++]=12.0/5, ls[i++]=5.0/2,  ls[i++]=27.0/10, 
      ls[i++]=14.0/5, ls[i++]=3.0,    ls[i++]=16.0/5, ls[i++]=7.0/2,  ls[i++]=18.0/5, 
      ls[i++]=4.0,    ls[i++]=21.0/5, ls[i++]=9.0/2,  ls[i++]=24.0/5, ls[i++]=49.0/10,
      ls[i++]=5.0,    ls[i++]=27.0/5, ls[i++]=28.0/5, ls[i++]=6.0,    ls[i++]=63.0/10, 
      ls[i++]=32.0/5, ls[i++]=7.0,    ls[i++]=36.0/5, ls[i++]=8.0,    ls[i++]=81.0/10, 
      ls[i++]=9.0;
      ks[j++]=  883685./5987520,
      ks[j++]=  106300./5987520, ks[j++]=  164075./5987520, ks[j++]=  591300./5987520, 
      ks[j++]=   67600./5987520, ks[j++]=  958868./5987520, ks[j++]=  776475./5987520, 
      ks[j++]= 1016500./5987520, ks[j++]=   86675./5987520, ks[j++]= 1880200./5987520, 
      ks[j++]= 1851848./5987520, ks[j++]= -504300./5987520, ks[j++]=  205125./5987520, 
      ks[j++]= 2644104./5987520, ks[j++]=-1527450./5987520, ks[j++]=  628625./5987520, 
      ks[j++]= 1177276./5987520, ks[j++]= 2724000./5987520, ks[j++]= -571875./5987520, 
      ks[j++]= 2136840./5987520, ks[j++]= 2770500./5987520, ks[j++]= -734250./5987520, 
      ks[j++]= 4772079./5987520, ks[j++]=-2278500./5987520, ks[j++]= 4353576./5987520, 
      ks[j++]=-3483050./5987520, ks[j++]= 4097507./5987520, ks[j++]= -189450./5987520, 
      ks[j++]= 4377812./5987520, ks[j++]=-2375550./5987520, ks[j++]= 1906800./5987520, 
      ks[j++]= 5210935./5987520, ks[j++]=-1707150./5987520, ks[j++]= 1839525./5987520, 
      ks[j++]= 2621502./5987520, ks[j++]= 3195700./5987520, ks[j++]= -388200./5987520, 
      ks[j++]= 5361569./5987520, ks[j++]=  413675./5987520, ks[j++]= 4892386./5987520, 
      ks[j++]=  956700./5987520, ks[j++]=5971453./5987520;
      nmin = 19;
   }
   return nmin;
}
/*----------------------------------------------------------------*/
//---- getx2y for linear interpolation
double getx2y(int n, double *x, double *y, double xin)
{
   int i;
   double xmin,xmax;
   if (x[0] < x[n-1]){xmin=x[0];xmax=x[n-1];}else{xmin=x[n-1];xmax=x[0];}
   //printf("n=%d %f %f %f\n",n, xmin,xmax,logM);

   if (xmin > xin) return 0;
   if (xmax < xin) return 0;
   //printf("n=%d %f %f %f\n",n, xmin,xmax,logM);

   for(i=0;i<n;i++){
      //printf("i= %d x= %f logM= %f\n",i, x[i],logM);
      if (i == 0) continue;
      if (x[i] <= xin && x[i-1] >=xin || x[i] >=xin && x[i-1] <= xin){
         double yreq = (y[i]-y[i-1])/(x[i]-x[i-1])*(xin -x[i-1]) + y[i-1];
         return yreq;
      }
   }
   return 0;
}
//---- getx2y_ist for linear interpolation
double getx2y_ist(int n, double *x, double *y, double xin, int *ist)
{
   int i;
   /* The followings are commented cuz Mag vs Mini  */
   // double xmin,xmax;
   // if (x[0] < x[n-1]){xmin=x[0];xmax=x[n-1];}else{xmin=x[n-1];xmax=x[0];}
   // //printf("n=%d %f %f %f\n",n, xmin,xmax,logM);

   // if (xmin > xin) return 0;
   // if (xmax < xin) return 0;
   // //printf("n=%d %f %f %f\n",n, xmin,xmax,logM);

   for(i=*ist;i<n;i++){
      //printf("i= %d x= %f logM= %f\n",i, x[i],logM);
      if (i == 0) continue;
      if (x[i] <= xin && x[i-1] >=xin || x[i] >=xin && x[i-1] <= xin){
         double yreq = (y[i]-y[i-1])/(x[i]-x[i-1])*(xin -x[i-1]) + y[i-1];
         *ist = i;
         return yreq;
      }
   }
   return 0;
}
//---- get_khi for linear interpolation
int get_khi(int n, double *x, double xin)
{
   int i;
   double xmin,xmax;
   if (x[0] < x[n-1]){xmin=x[0];xmax=x[n-1];}else{xmin=x[n-1];xmax=x[0];}
   //printf("n=%d %f %f %f\n",n, xmin,xmax,logM);

   if (xmin > xin) return  0;
   if (xmax < xin) return  n;
   int klo = 0;
   int khi = n-1;
   while (khi-klo > 1) {
     int k=(khi+klo) >> 1;
     if (x[k] > xin) khi=k; // xin = 10 mag  xk= 11 mag 0.08 Msun x[k-1]
     else klo=k;
   }
   return khi;
}
//---- getx2y_khi for linear interpolation
double getx2y_khi(int n, double *x, double *y, double xin, int *khi)
{
   int i;
   double xmin,xmax;
   if (x[0] < x[n-1]){xmin=x[0];xmax=x[n-1];}else{xmin=x[n-1];xmax=x[0];}
   //printf("n=%d %f %f %f\n",n, xmin,xmax,logM);

   if (xmin > xin) return 0;
   if (xmax < xin) return 0;
   //printf("n=%d %f %f %f\n",n, xmin,xmax,logM);
   int klo;
   if (*khi > 0){
     klo = *khi - 1;
   }else{
     klo = 0;
     *khi = n-1;
     while (*khi-klo > 1) {
       int k=(*khi+klo) >> 1;
       if (x[k] > xin) *khi=k; // xin = 10 mag  xk= 11 mag 0.08 Msun x[k-1]
       else klo=k;
     }
   }
   double h = x[*khi] - x[klo];
   if (h == 0.0) return 0;
   double a = (x[*khi]-xin)/h;
   double b = (xin-x[klo])/h;
   double yreq = a*y[klo] + b*y[*khi];
   return yreq;
}
//---------------
double interp_x(int n, double *F, double xst, double dx, double xreq) // just for this code
{
  int    ix   = (xreq - xst)/dx;
  double xres = (xreq - xst)/dx - ix;
  if (ix < 0 || ix > n - 1) return 0;
  if (ix+1 > n - 1) return F[ix];
  double F1 = F[ix];
  double F2 = F[ix+1];
  return F1 * (1 - xres) + F2 * xres;
}
//---------------
double interp_xquad(int n, double *F, double *f, double xst, double dx, double xreq) // just for this code
{
  int    ix   = (xreq - xst)/dx;
  double xres = (xreq - xst)/dx - ix;
  if (ix < 0 || ix > n - 1) return 0;
  if (ix+1 > n - 1) return F[ix];
  return 0.5*(f[ix+1] - f[ix])*xres*xres*dx + f[ix]*xres*dx + F[ix];
}
//---------------
double interp_xy(int nx, int ny, double **F, double xst, double yst, double dx, double dy, double xreq, double yreq) // just for this code
{
  int    ix   = (xreq - xst)/dx;
  double xres = (xreq - xst)/dx - ix;
  int    iy   = (yreq - yst)/dy;
  double yres = (yreq - yst)/dy - iy;
  if (ix < 0 || ix > nx - 1 || iy < 0 || iy > ny -1) return 0;
  if (ix+1 > nx - 1 && iy+1 > ny - 1) return F[ix][iy]; // return edge value
  if (ix+1 > nx - 1) return F[ix][iy] * (1 - yres) + F[ix][iy+1] * yres; // only interpolate y
  if (iy+1 > ny - 1) return F[ix][iy] * (1 - xres) + F[ix+1][iy] * xres; // only interpolate x
  double a1 = (1 - xres) * (1 - yres), F1 = F[ix][iy]    ;
  double a2 =      xres  * (1 - yres), F2 = F[ix+1][iy]  ;
  double a3 = (1 - xres) *      yres , F3 = F[ix][iy+1]  ;
  double a4 =      xres  *      yres , F4 = F[ix+1][iy+1];
  return a1 * F1 + a2 * F2 + a3 * F3 + a4 * F4;
}
//---------------
void interp_xy_coeff(int nx, int ny, double *as, double xst, double yst, double dx, double dy, double xreq, double yreq) // just for this code
/* Return coefficients as[0], as[1], as[2], as[3] of bilinear interpolation.
 * Interpolated value is given by
 *   as[0]*F[ix][iy] + as[1]*F[ix+1][iy] + as[2]*F[ix][iy+1] + as[3]*F[ix+1][iy+1]  */
{
  int    ix   = (xreq - xst)/dx;
  double xres = (xreq - xst)/dx - ix;
  int    iy   = (yreq - yst)/dy;
  double yres = (yreq - yst)/dy - iy;
  if (ix < 0 || ix > nx - 1 || iy < 0 || iy > ny -1){ // Out of range
    as[0] = as[1] = as[2] = as[3] = 0;
  }else if (ix+1 > nx - 1 && iy+1 > ny - 1){ // return edge value
    as[1] = as[2] = as[3] = 0;
    as[0]= 1; 
  }else if (ix+1 > nx - 1){ // only interpolate y
    as[0] = (1 - yres);
    as[2] = yres;
    as[1] = as[3] = 0;
  }else if (iy+1 > ny - 1){ // only interpolate x
    as[0] = (1 - xres);
    as[1] = xres;
    as[2] = as[3] = 0;
  }else{ // when either ix or iy is not at the edge
    as[0] = (1 - xres) * (1 - yres);
    as[1] =      xres  * (1 - yres);
    as[2] = (1 - xres) *      yres ;
    as[3] =      xres  *      yres ;
  }
}





int main(int argc,char **argv)
{
  //--- read parameters ---
  long seed    = getOptioni(argc,argv,"seed", 1, 12304357); // seed of random number
  gsl_rng_env_setup();
  T = gsl_rng_default;
  r = gsl_rng_alloc (T);
  gsl_rng_set(r, seed); 
  long seed0 = seed;
  //--- Set params for Galactic model (default: E+E_X model in Koshimoto+2021) ---
  double M0_B      = getOptiond(argc,argv,"M0", 1, 1.0);
  double M1_B      = getOptiond(argc,argv,"M1", 1, 0.859770466578045);
  double M2_B      = getOptiond(argc,argv,"M2", 1, 0.08);
  double M3_B      = getOptiond(argc,argv,"M3", 1, 0.01);
  double Ml        = getOptiond(argc,argv,"Ml", 1, 0.001); // default : w/o planetary mass
  double Mu        = getOptiond(argc,argv,"Mu", 1, 120); // need to be fixed!!!. Affect normalizing bulge coeffs 
  double alpha1_B  = getOptiond(argc,argv,"alpha1", 1, -2.32279457078378);
  double alpha2_B  = getOptiond(argc,argv,"alpha2", 1, -1.13449983242887);
  double alpha3_B  = getOptiond(argc,argv,"alpha3", 1, -0.175862190587576);
  double alpha0_B  = getOptiond(argc,argv,"alpha0", 1,  alpha1_B);
  double alpha4_B  = getOptiond(argc,argv,"alpha4", 1,  alpha3_B);
  DISK     = getOptiond(argc,argv,"DISK",   1,    2); // 0: wo disk, 1: w/ disk+hole, 2: w/ disk like P17
  rhot0    = getOptiond(argc,argv,"rhot0",   1,   0.042); // local thin disk density, Msun/pc^3 (Bovy17: 0.042 +- 0.002 incl.BD)
  hDISK     = getOptiond(argc,argv,"hDISK",  1,    0); // 0: const scale height, 1: linear scale height
  addX      = getOptiond(argc,argv,"addX",   1,    5); // 0: no X-shape,  >=5: use model==addX as X-shape 
  model     = getOptiond(argc,argv,"model",  1,    5); // 
  R0     = getOptiond(argc,argv,"R0",     1,   8160); // 
  thetaD = getOptiond(argc,argv,"thetaD", 1,     27); // 
  frho0b = getOptiond(argc,argv,"frho0b", 1,  0.839014514507754); // 
  Rc     = getOptiond(argc,argv,"Rc", 1,  2631.78535429573); //
  zb_c   = getOptiond(argc,argv,"zb_c", 1,  1e+6); //
  if (model >= 4 && model <= 8){
    x0_1     = getOptiond(argc,argv,"x0", 1,  930.623146993329); // 
    y0_1     = getOptiond(argc,argv,"y0", 1,  370.784386649364); // 
    z0_1     = getOptiond(argc,argv,"z0", 1,  239.547516030578); // 
    C1     = getOptiond(argc,argv,"C1", 1,  1.20011972384328); // 
    C2     = getOptiond(argc,argv,"C2", 1,  4.09326795684828); // 
    C3     = getOptiond(argc,argv,"C3", 1,  1.0000); // 
  }
  if (addX >= 5){
    x0_X = getOptiond(argc,argv,"x0_X", 1,  278.027059842233); // 
    y0_X = getOptiond(argc,argv,"y0_X", 1,  176.318528789193); // 
    z0_X = getOptiond(argc,argv,"z0_X", 1,  286.791941602401); // 
    C1_X = getOptiond(argc,argv,"C1_X", 1,  1.3087131258784); // 
    C2_X = getOptiond(argc,argv,"C2_X", 1,  2.21745322869032); // 
    b_zX = getOptiond(argc,argv,"b_zX", 1,  1.37774815817195); // b_zX, slope of "X" of X-shape
    fX   = getOptiond(argc,argv,"fX",   1,  1.43975636704683); // fraction of X-shape
    Rc_X = getOptiond(argc,argv,"Rc_X",  1,  1301.63829617294); // 
  }
  b_zY   = getOptiond(argc,argv,"b_zY", 1, 0); //

  // ----- Kinematic parameters ------
  // for bar kinematic
  Omega_p  = getOptiond(argc,argv,"Omega_p",  1, 47.4105844018699);
  model_vb = getOptiond(argc,argv,"model_vb", 1,    5); // 
  x0_vb    = getOptiond(argc,argv,"x0_vb"  ,  1, 858.106595717275);
  y0_vb    = getOptiond(argc,argv,"y0_vb"  ,  1, 3217.04987721548);
  z0_vb    = getOptiond(argc,argv,"z0_vb"  ,  1, 950.690583433628);
  C1_vb    = getOptiond(argc,argv,"C1_vb"  ,  1, 4.25236641149869);
  C2_vb    = getOptiond(argc,argv,"C2_vb"  ,  1, 1.02531652066343);
  C3_vb    = getOptiond(argc,argv,"C3_vb"  ,  1, 1);
  sigx_vb  = getOptiond(argc,argv,"sigx_vb",  1, 151.854794853683);
  sigy_vb  = getOptiond(argc,argv,"sigy_vb",  1, 78.0278905748233);
  sigz_vb  = getOptiond(argc,argv,"sigz_vb",  1, 81.9641955092164);
  sigx_vb0 = getOptiond(argc,argv,"sigx_vb0",  1,  63.9939241108675);
  sigy_vb0 = getOptiond(argc,argv,"sigy_vb0",  1,  75.8180486866697);
  sigz_vb0 = getOptiond(argc,argv,"sigz_vb0",  1,  71.2336430487113);
  vx_str   = getOptiond(argc,argv,"vx_str" ,  1,    43.0364707040617);
  y0_str   = getOptiond(argc,argv,"y0_str" ,  1,    406.558313420815);
  model_vbz = getOptiond(argc,argv,"model_vbz",  1,    5); // 
  x0_vbz    = getOptiond(argc,argv,"x0_vbz"  ,  1, 558.430182718529);
  y0_vbz    = getOptiond(argc,argv,"y0_vbz"  ,  1, 2003.21703656302);
  z0_vbz    = getOptiond(argc,argv,"z0_vbz"  ,  1, 3823.20855045157);
  C1_vbz    = getOptiond(argc,argv,"C1_vbz"  ,  1, 3.71001266000693);
  C2_vbz    = getOptiond(argc,argv,"C2_vbz"  ,  1, 1.07455173734341);
  C3_vbz    = getOptiond(argc,argv,"C3_vbz"  ,  1, 1);

  // for disk kinematic (default: all-z + flat z_d^{thin} model in Koshimoto+21)
  hsigUt    = getOptiond(argc,argv,"hsigUt"  ,  1,  14300);// scale len of velo disp R (sigU) for thin
  hsigWt    = getOptiond(argc,argv,"hsigWt"  ,  1,   5900);// scale len of velo disp Z (sigW) for thin
  hsigUT    = getOptiond(argc,argv,"hsigUT"  ,  1, 180000);// scale len of velo disp R (sigU) for thick
  hsigWT    = getOptiond(argc,argv,"hsigWT"  ,  1, 9400);  // scale len of velo disp Z (sigW) for thick
  betaU     = getOptiond(argc,argv,"betaU"   ,  1, 0.32);  //  slope of age-sigU for thin
  betaW     = getOptiond(argc,argv,"betaW"   ,  1, 0.77);  //  slope of age-sigW for thin
  sigU10d   = getOptiond(argc,argv,"sigU10d" ,  1, 42.0);  // sigU for 10Gyr thin @Sunposi 
  sigW10d   = getOptiond(argc,argv,"sigW10d" ,  1, 24.4);  // sigW for 10Gyr thin @Sunposi
  sigU0td   = getOptiond(argc,argv,"sigU0td" ,  1, 75.0);  // sigU for thick @Sunposi
  sigW0td   = getOptiond(argc,argv,"sigW0td" ,  1, 49.2);  // sigW for thick @Sunposi

  // Use one of named models in Koshimoto+21
  int E_fg0 = getOptiond(argc,argv,"E_fg0", 1, 0);
  int G_fg0 = getOptiond(argc,argv,"G_fg0", 1, 0);
  int EXE_fg0 = getOptiond(argc,argv,"EXE_fg0", 1, 0);
  int GXG_fg0 = getOptiond(argc,argv,"GXG_fg0", 1, 0);
  if (E_fg0 == 1){ // E model
    model = 5, addX = 0;
    M0_B = 1.0, M1_B = 0.843651488650385, M2_B = 0.08, M3_B = 0.01;
    alpha1_B = -2.30708461042964, alpha2_B = -1.09811414023325, alpha3_B = -0.176687444667866;
    alpha0_B = alpha1_B, alpha4_B = alpha3_B;
    R0= 8160, thetaD = 27, 
    frho0b = 0.847695765083198, Rc = 2804.94024639663;
    x0_1 = 668.323640191308, y0_1 = 277.674592258175, z0_1 = 235.344943180979, 
    C1 = 1.40903573470129, C2 = 3.3497118832179, C3 = 1;
    model_vb = 5, model_vbz = 5;
    Omega_p = 49.5149910609312, vx_str = 48.7482280102778, y0_str = 392.515724264323,
    sigx_vb  = 156.055410564041, sigy_vb  = 83.8197043324931, sigz_vb  = 86.3564038759999,
    sigx_vb0 = 63.8292191277825, sigy_vb0 = 74.9469462226124, sigz_vb0 = 72.3085487545662,
    x0_vb  = 823.387929122523, y0_vb  = 9288.51482678556, z0_vb  = 864.479916419292,
    C1_vb  = 3.82820123451928, C2_vb  = 1.00573720627546,
    x0_vbz = 511.063328964278, y0_vbz = 2896.01606378595, z0_vbz = 2189.7664883434,
    C1_vbz = 3.04214421342047, C2_vbz = 1.00609904766722;
  }
  if (G_fg0 == 1){ // G model
    model = 6, addX = 0;
    M0_B = 1.0, M1_B = 0.896557393600988, M2_B = 0.08, M3_B = 0.01;
    alpha1_B = -2.39628188518525, alpha2_B = -1.18451896148506, alpha3_B = 0.168672130848533;
    alpha0_B = alpha1_B, alpha4_B = alpha3_B;
    R0= 8160, thetaD = 27, 
    frho0b = 0.777347874844233, Rc = 4838.85613149588;
    x0_1 = 1025.42128394916, y0_1 = 457.419718281149, z0_1 = 396.048253079423, 
    C1 = 2.00928445577057, C2 = 3.9678518191928, C3 = 1;
    model_vb = 5, model_vbz = 5;
    Omega_p = 40.5174879673548, vx_str = 11.9026090372449, y0_str = 20.1384817812277,
    sigx_vb  = 136.435675357212, sigy_vb  = 109.313291840218, sigz_vb  = 101.291432907346,
    sigx_vb0 = 76.0453005937702, sigy_vb0 = 67.9783132842431, sigz_vb0 = 74.7117386554542,
    x0_vb  = 1031.18302251324, y0_vb  = 2145.45565210108, z0_vb  = 727.233943973984,
    C1_vb  = 4.9302429910108, C2_vb  = 1.04038121792228,
    x0_vbz = 517.854475368706, y0_vbz = 1436.21008855387, z0_vbz = 1095.79181359292,
    C1_vbz = 2.3091601785779, C2_vbz = 1.03832670354301;
  }
  if (EXE_fg0 == 1){ // E+E_X model
    model = 5, addX = 5;
    M0_B = 1.0, M1_B = 0.859770466578045, M2_B = 0.08, M3_B = 0.01;
    alpha1_B = -2.32279457078378, alpha2_B = -1.13449983242887, alpha3_B = -0.175862190587576;
    alpha0_B = alpha1_B, alpha4_B = alpha3_B;
    R0= 8160, thetaD = 27, 
    frho0b = 0.839014514507754, Rc = 2631.78535429573;
    x0_1 = 930.623146993329, y0_1 = 370.784386649364, z0_1 = 239.547516030578, 
    C1 = 1.20011972384328, C2 = 4.09326795684828, C3 = 1;
    model_vb = 5, model_vbz = 5;
    Omega_p = 47.4105844018699, vx_str = 43.0364707040617, y0_str = 406.558313420815,
    sigx_vb  = 151.854794853683, sigy_vb  = 78.0278905748233, sigz_vb  = 81.9641955092164,
    sigx_vb0 = 63.9939241108675, sigy_vb0 = 75.8180486866697, sigz_vb0 = 71.2336430487113,
    x0_vb  = 858.106595717275, y0_vb  = 3217.04987721548, z0_vb  = 950.690583433628,
    C1_vb  = 4.25236641149869, C2_vb  = 1.02531652066343,
    x0_vbz = 558.430182718529, y0_vbz = 2003.21703656302, z0_vbz = 3823.20855045157,
    C1_vbz = 3.71001266000693, C2_vbz = 1.07455173734341;
    x0_X = 278.027059842233, y0_X = 176.318528789193, z0_X = 286.791941602401,
    C1_X = 1.3087131258784, C2_X = 2.21745322869032, 
    b_zX = 1.37774815817195, fX = 1.43975636704683, Rc_X = 1301.63829617294;
  }
  if (GXG_fg0 == 1){ // G+G_X model
    model = 6, addX = 6;
    M0_B = 1.0, M1_B = 0.901747918318042, M2_B = 0.08, M3_B = 0.01;
    alpha1_B = -2.32055781291126, alpha2_B = -1.16146692073597, alpha3_B = -0.222751835826612;
    alpha0_B = alpha1_B, alpha4_B = alpha3_B;
    R0= 8160, thetaD = 27, 
    frho0b = 0.861982105059042, Rc = 2834.43172768484;
    x0_1 = 1564.78976595399, y0_1 = 721.729645984158, z0_1 = 494.669973292979, 
    C1 = 1.20141097225, C2 = 3.09254667088709, C3 = 1;
    model_vb = 5, model_vbz = 5;
    Omega_p = 45.9061365175252, vx_str = 28.250608437116, y0_str = 11.4387290790323,
    sigx_vb  = 154.984185643613, sigy_vb  = 78.4783157632334, sigz_vb  = 83.2424209150283,
    sigx_vb0 = 63.3834790223473, sigy_vb0 = 75.1951371572303, sigz_vb0 = 69.6076680158332,
    x0_vb  = 939.470002303028, y0_vb  = 4228.61947632437, z0_vb  = 883.716365308057,
    C1_vb  = 4.59067123072475, C2_vb  = 1.00961963171066,
    x0_vbz = 699.073733500672, y0_vbz = 1729.91970395558, z0_vbz = 2028.24030134845,
    C1_vbz = 4.84589813971794, C2_vbz = 1.01718557457505;
    x0_X = 755.975821023038, y0_X = 312.17136920671, z0_X = 399.287597819655,
    C1_X = 1.21131134854495, C2_X = 1.30388556329566,
    b_zX = 1.37711800325276, fX = 2.99985800759016, Rc_X = 5174.00544959931;
  }

  costheta = cos(thetaD/180.0*PI) , sintheta = sin(thetaD/180.0*PI);

  // To put Sgr A* on the GC
  int CenSgrA = getOptioni(argc,argv, "CenSgrA", 1, 1);
  double lSgrA = -0.056;
  double bSgrA = -0.046;
  if (CenSgrA == 1){
    Dlb2xyz(R0, lSgrA, bSgrA, R0, xyzSgrA);
    // printf("# SgrA*(x,y,z)= ( %.3f , %.3f , %.3f ) pc", xyzSgrA[0], xyzSgrA[1], xyzSgrA[2]);
    // double xyz[3] = {};
    // Dlb2xyz(R0, lSgrA, bSgrA, R0, xyz);
    // printf("# SgrA*(x,y,z)= ( %.3f , %.3f , %.3f ) pc", xyz[0], xyz[1], xyz[2]);
  }

  // Store Mass Function and calculate normalization factors for density distributions
  void store_IMF_nBs(int B, double *logMass, double *PlogM, double *PlogM_cum_norm, int *imptiles, double M0, double M1, double M2, double M3, double Ml, double Mu, double alpha1, double alpha2, double alpha3, double alpha4, double alpha0);
  nm = 1000;
  double *logMass_B, *PlogM_cum_norm_B, *PlogM_B;
  int *imptiles_B;
  logMass_B        = (double*)calloc(nm+1, sizeof(double *));
  PlogM_B          = (double*)calloc(nm+1, sizeof(double *));
  PlogM_cum_norm_B = (double*)calloc(nm+1, sizeof(double *));
  imptiles_B       = (int*)calloc(22, sizeof(int *));
  store_IMF_nBs(1, logMass_B, PlogM_B, PlogM_cum_norm_B, imptiles_B, M0_B, M1_B, M2_B, M3_B, Ml, Mu, alpha1_B, alpha2_B, alpha3_B, alpha4_B, alpha0_B);

  // Read mass-luminosity relation and make LF for each component
  double Isst   = getOptiond(argc,argv,"Magrange", 1, 0.0); // 
  double Isen   = getOptiond(argc,argv,"Magrange", 2, 0.0); // 
  int    ROMAN  = getOptiond(argc,argv,"ROMAN", 1, 0); // 
  int    HWBAND = getOptiond(argc,argv,"HWBAND", 1, 0); // 
  int    iMag0 = (ROMAN == 1) ? 4 : 3;
  nband = (ROMAN == 1) ? 6 : 5; // (J, H, K, Z087, W146, F213) for Roman, (V, I, J, H, K) otherwise.
  int iMag  = getOptiond(argc,argv,"iMag",  1, iMag0); // ROMAN 0: (0, 1, 2, 3, 4)= (V, I, J, H, K), default: H 
                                                       //       1: (0, 1, 2, 3, 4, 5)= (J, H, K, Z087, W146, F213), default: W146 
  if (iMag < 0 || iMag > nband) iMag = iMag0; 
  double **Minis, **MPDs, **Rstars, *Minvs, ***Mags;
  char **MAG, **MLfiles;
  double lameff[6] = {};
  int  nMLrel[10] = {490, 646, 790, 501, 373, 325, 291, 220, 301, 330}; // ncomp, data number of MLrelation file, later updated in get_ML_LF
  Minis  = malloc(sizeof(double *) * ncomp);  // initial mass
  MPDs   = malloc(sizeof(double *) * ncomp);  // current (present-day) mass
  Rstars = malloc(sizeof(double *) * ncomp); // Stellar radius
  Minvs  = calloc(ncomp, sizeof(double *)); // minimum initial mass after which mag gets fainter
  MLfiles = malloc(sizeof(char *) * ncomp); // Path of MLfile for each comp
  for (int i=0; i<ncomp; i++){
    Minis[i] = calloc(nMLrel[i], sizeof(double *));
    MPDs[i] = calloc(nMLrel[i], sizeof(double *));
    Rstars[i]  = calloc(nMLrel[i], sizeof(double *));
    MLfiles[i] = malloc(sizeof(char) * 61); // 60 is max number of characters of path for MLfile
  }
  MAG    = malloc(sizeof(char *) * nband); // Name of each band
  Mags   = malloc(sizeof(double *) * nband);  // absolute mag, J, H, Ks, Z087, W146, F213
  for (int j=0; j<nband; j++){
    MAG[j]  = malloc(sizeof(char) * 8); // 7 is max characters of path for MLfile
    Mags[j] = malloc(sizeof(double *) * ncomp);
    for (int i=0; i<ncomp; i++){
      Mags[j][i] = calloc(nMLrel[i], sizeof(double *));
    }
  }
  void get_MAG_MLfiles(int ROMAN, char **MAG, char **MLfiles, double *lameff);
  get_MAG_MLfiles(ROMAN, MAG, MLfiles, lameff);
  int get_ML_LF(int calcLF, int ROMAN, char **MLfiles, int iMag, int *nMLrel, double **Minis, double **MPDs, double ***Mags, double **Rstars, double *Minvs, int Magst, int Magen, double dMag, double **CumuLFs, double *logMass, double *PlogM_cum_norm, double *PlogM); 
  int Magst = -10;
  int Magen =  Isen - 5;
  if (Magen >  40) Magen =  40;
  double dMag = 0.02;
  int nLF = (Magen - Magst)/dMag + 1;
  CumuN_MIs = malloc(sizeof(double *) * ncomp);
  for (int i=0; i<ncomp; i++){
     CumuN_MIs[i] = calloc(nLF, sizeof(double *));
  }
  int calcLF = (Isen - Isst > 0) ? 1 : 0;
  nMIs = get_ML_LF(calcLF, ROMAN, MLfiles, iMag, nMLrel, Minis, MPDs, Mags, Rstars, Minvs, Magst, Magen, dMag, CumuN_MIs, logMass_B, PlogM_cum_norm_B, PlogM_B);
  // for (int icomp=0; icomp < ncomp; icomp++){
  //   printf("icomp= %d Minv= %.10f\n",icomp,Minvs[icomp]);
  // }


  // Store Cumu P_Shu
  int nfg = 100;
  int nz = (zenShu - zstShu)/dzShu + 1;
  int nR = (RenShu - RstShu)/dRShu + 1;
  int ndisk = 8;
  fgsShu      = (double****)malloc(sizeof(double *) * nz);
  PRRgShus    = (double****)malloc(sizeof(double *) * nz);
  cumu_PRRgs  = (double****)malloc(sizeof(double *) * nz);
  n_fgsShu  = (int***)malloc(sizeof(int *) * nz);
  kptiles   = (int****)malloc(sizeof(int *) * nz);
  for (int i=0; i<nz; i++){
    fgsShu[i]  = (double***)malloc(sizeof(double *) * nR);
    PRRgShus[i] = (double***)malloc(sizeof(double *) * nR);
    cumu_PRRgs[i] = (double***)malloc(sizeof(double *) * nR);
    n_fgsShu[i]  = (int**)malloc(sizeof(int *) * nR);
    kptiles[i]   = (int***)malloc(sizeof(int *) * nR);
    for (int j=0; j<nR; j++){
      fgsShu[i][j]  = (double**)malloc(sizeof(double *) * ndisk);
      PRRgShus[i][j] = (double**)malloc(sizeof(double *) * ndisk);
      cumu_PRRgs[i][j] = (double**)malloc(sizeof(double *) * ndisk);
      kptiles[i][j] = (int**)malloc(sizeof(int *) * ndisk);
      n_fgsShu[i][j]  = (int*)calloc(ndisk, sizeof(int *));
      for (int k=0; k<ndisk; k++){
        fgsShu[i][j][k]   = (double*)calloc(nfg, sizeof(double *));
        PRRgShus[i][j][k] = (double*)calloc(nfg, sizeof(double *));
        cumu_PRRgs[i][j][k] = (double*)calloc(nfg, sizeof(double *));
        kptiles[i][j][k]  = (int*)calloc(22, sizeof(int *));
      }
    }
  }
  char *fileVc = (char*)"input_files/Rotcurve_BG16.dat";
  void store_cumuP_Shu(char *infile);
  store_cumuP_Shu(fileVc);
    
  //for (int i=0; i<nz; i++){
  //  for (int j=0; j<nR; j++){
  //    n_fgsShu[i][j]  = (int*)calloc(ndisk, sizeof(int *));
  //      for (int k=0; k<ndisk; k++){
  //        fgsShu[i][j][k]   = (double*)calloc(nfg, sizeof(double *));
  //        PRRgShus[i][j][k] = (double*)calloc(nfg, sizeof(double *));
  //        cumu_PRRgs[i][j][k] = (double*)calloc(nfg, sizeof(double *));
  //        kptiles[i][j][k]  = (int*)calloc(22, sizeof(int *));
  //      }
  //  }
  //}

  printf("fgsShu start\n");
  printf("%i,%i,%i,%i\n",nz,nR,ndisk,nfg);
  for (int i=0; i<nz; i++){
    for (int j=0; j<nR; j++){
      for (int k=0; k<ndisk; k++){
        // printf("N(%i,%i,%i) = %i\n",i,j,k,n_fgsShu[i][j][k]);
        for (int kk=0; kk<n_fgsShu[i][j][k];kk++){
          printf("%i,%i,%i,%i,%.4f,%.4f,%.4f\n",i,j,k,kk,fgsShu[i][j][k][kk],cumu_PRRgs[i][j][k][kk],PRRgShus[i][j][k][kk]);
        }
      }
    }
  }
  printf("fgsShu end\n");
    
 
 
  // printf ("# sumM_MS/sumN_MS= %9.2f / %6.0f = %.6f Msun/*\n", allmass, allstars, allmass/allstars);
  // printf ("# nerror= %d\n", nerror);
  // if (BINARY == 1) printf ("# (n_single n_binwide n_binclose)/n_all= ( %6.0f %6.0f %6.0f ) / %6.0f = ( %.6f %.6f %.6f )\n", ncnts, ncntbWD, ncntbCD, ncntall,ncnts/ncntall,ncntbWD/ncntall,ncntbCD/ncntall);
  // printf ("# (n_thin1-7 n_thick n_bar n_nsd)/n_all= ( %6.0f %6.0f %6.0f %6.0f %6.0f %6.0f %6.0f %6.0f %6.0f %6.0f ) / %6.0f = ( %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f )\n", ncntcomp[0], ncntcomp[1], ncntcomp[2], ncntcomp[3], ncntcomp[4], ncntcomp[5], ncntcomp[6], ncntcomp[7], ncntcomp[8], ncntcomp[9], ncntall, ncntcomp[0]/ncntall, ncntcomp[1]/ncntall, ncntcomp[2]/ncntall, ncntcomp[3]/ncntall, ncntcomp[4]/ncntall, ncntcomp[5]/ncntall, ncntcomp[6]/ncntall, ncntcomp[7]/ncntall, ncntcomp[8]/ncntall, ncntcomp[9]/ncntall);
  // printf ("# (n_BD n_MS n_WD n_NS n_BH)/n_all= ( %6.0f %6.0f %6.0f %6.0f %6.0f ) / %6.0f = ( %.6f %.6f %.6f %.6f %.6f )\n", nBD, nMS, nWD, nNS, nBH,ncntall, nBD/ncntall, nMS/ncntall, nWD/ncntall, nNS/ncntall, nBH/ncntall);
  if (Isen - Isst > 0){
    for (int i=0; i<ncomp; i++){
       free(CumuN_MIs[i]);
    }
    free(CumuN_MIs);
  }
  
  free(logMass_B       );
  free(PlogM_cum_norm_B);
  free(PlogM_B         );
  free(imptiles_B      );
  free(lDs);
  free(bDs);
  for (int i=0; i<nz; i++){
    for (int j=0; j<nR; j++){
      for (int k=0; k<ndisk; k++){
        free(fgsShu[i][j][k]);
        free(PRRgShus[i][j][k]);
        free(cumu_PRRgs[i][j][k]);
        free(kptiles[i][j][k]);
      }
      free(fgsShu[i][j]);
      free(PRRgShus[i][j]);
      free(cumu_PRRgs[i][j]);
      free(kptiles[i][j]);
      free(n_fgsShu[i][j]);
    }
    free(fgsShu[i]);
    free(PRRgShus[i]);
    free(cumu_PRRgs[i]);
    free(kptiles[i]);
    free(n_fgsShu[i]);
  }
  free(fgsShu);
  free(PRRgShus);
  free(cumu_PRRgs);
  free(kptiles);
  free(n_fgsShu);
  for (int i=0; i<ncomp; i++){
    free(Minis[i]);
    free(MPDs[i]);
    free(Rstars[i]);
    free(MLfiles[i]);
  }
  free(Minis);
  free(MPDs);
  free(Rstars);
  free(Minvs);
  free(MLfiles);
  for (int i=0; i<nband; i++){
    for (int j=0; j<ncomp; j++){
      free(Mags[i][j]);
    }
    free(Mags[i]);
    free(MAG[i]);
  }
  free(Mags);
  free(MAG);
  return 0;
} // end main
