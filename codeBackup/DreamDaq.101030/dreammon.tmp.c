#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string>
#include <TH1.h>
#include <TF1.h>
#include <TFile.h>
#include <TH2.h>
#include "myhbook.h"
#include "myRawFile.h"
#include "dreammon.h"
#include <iostream>
#include <iomanip>
#include <map>
#include <string>
#define MAX_SINGLE_HISTO 100

using namespace std;
#define HEXWORD(m) "0x" << std::hex << std::setfill('0') << std::setw(8) << m << std::dec
TFile* ped_file=0;

float get_pedestal(TH1F* h){
  if(ped_file==0 || ped_file->IsZombie() || h==0)
    return 0.;
  //printf("ped file exists\n");
 
  static std::map<std::string, float> cache;

  std::string name = Form("%s",h->GetName());
  //printf("h_ped: %s\n",name.c_str());
  float ped = cache[name];

  if(ped == 0.){ //not in cache
    //printf("h_ped: %s\n",name.c_str());
    //printf("not in cache\n");
    //printf("ped_file: %s\n",ped_file->GetName());
    TH1F* h_ped = 0;
    ped_file->GetObject(name.c_str(),h_ped);
    if (h_ped==0 ){
      name = Form("Debug/%s",h->GetName());
      ped_file->GetObject(name.c_str(),h_ped);
    }
    if (h_ped==0 ) {
	//printf("not found\n");
	ped = 0;
	cache[name] = -10000.;
    } else /* if(h_ped->GetEntries()<10){ */
/*       //printf("not enough stat\n"); */
/*       ped = 0; */
/*       cache[name] = -10000.; */
/*     } else  */{
      //printf("found!\n");
      ped = h_ped->GetMean();
      cache[name] = ped;
    } 
  } else if(ped == - 10000.){ //we already know it's not there
    //printf("we already know it's missing!\n");
    ped = 0;
  }
  //printf("ped: %f\n",ped);
  return ped;
}

int dreammon_init(char ** argv, unsigned int run_nr, bool drs,int drs_setup,bool phys_h)
{
  char * myWorkDir;

  if (getenv("WORKDIR") == NULL)
    myWorkDir = getenv("/home/dreamtest/working/");
  else
    myWorkDir = getenv("WORKDIR");
  
  // Create Input/Output filenames
  if (getenv("HISTODIR") == NULL)
    sprintf(ntdir, "%s/%s", myWorkDir, "hbook");
  else
    sprintf(ntdir, "%s", getenv("HISTODIR"));
  
  Nrunnumber= run_nr;
  if (phys_h){
    sprintf(ntfilename, "%s/datafile_histo_run%d.root", ntdir, Nrunnumber);
  }else if (!phys_h){
    sprintf(ntfilename, "%s/datafile_histo_pedestal_run%d.root", ntdir, Nrunnumber);
  }

  if (getenv("PEDDIR") == NULL)
    sprintf(peddir, "%s/%s", myWorkDir, "ped");
  else
    sprintf(peddir, "%s", getenv("PEDDIR"));
   
  // ped_run_name -- file name for the pedestal data
  if (getenv("PED_RUN") == NULL){ 
    sprintf(ped_run_name, "%s/pedestal_run0.cal",peddir);
    ped_run_number=0;
    if(phys_h){
      ped_file = TFile::Open(Form("%s/datafile_histo_pedestal_run%d.root",
				  ntdir, Nrunnumber));
      if(ped_file==0){
	ped_file = TFile::Open(Form("%s/datafile_histo_pedestal_run0.root",
				    ntdir));
      }
      
    }
  }else{ 
    sprintf(ped_run_name, "%s/pedestal_run%s.cal", peddir, getenv("PED_RUN"));
    ped_run_number=atol(getenv("PED_RUN"));
    if(phys_h)ped_file = TFile::Open(Form("%s/datafile_histo_pedestal_run%d.root", ntdir, ped_run_number));
  }
  
  if(!phys_h) ped_file = 0;
  else {
    printf ("using ped_file %s\n", ped_file->GetName()); 
  }
  if (getenv("MAXEVT") == NULL)
    evt_max= 100000;
  else
    evt_max = atol(getenv("MAXEVT"));                                                                                         
 
  //Read pedestal file
  // Pedestal  file have AT THE MOMENT 64 lines:
  // 32 lines for ADCN0 (32 channels)
  // 32 lines for ADCN1 (32 channels)
  //32 lines  for ADCN2 (32 channels)  
  //and 4 columns: Module_number (0 or 1 or 2) Channel Mean Rms
  FILE *peddatafp;
  peddatafp = fopen(ped_run_name, "r"); 
  if (!peddatafp) {
    fprintf(stderr, "Cannot open pedestal data file %s\n", ped_run_name);
    return -1;
  }
  int a,b;
  float c,d;
  int il;
  for(il=0;il<96;il++)  {
    fscanf(peddatafp,"%d %d %f %f",&a,&b,&c,&d);
    pmean[il]=c;
  }



  // Declare histograms
  //Histograms with the number of the run and with the number of the pedestal run used
  
  // not a useful plot
  hrn = new TH1F("runnumb","Run Number", 500,0.,500.);

  // not a very useful plot
  hprn = new TH1F("pedrunnumb","Pedestal Run Number",500,0.,500.); 

  unsigned int i=0,j=0,k=0,l=0;
  char histo[20],histop[20];
  char name[50];
  //Histograms for the 2 Dream ADC toghether
  for(int i_dreamadc=0; i_dreamadc<2; i_dreamadc++){
    for(int ch=0;ch<32;ch++){
      sprintf(histo,"Dream_ADC%d_ch%d", i_dreamadc,ch);
      sprintf(name,"Dream (ADC%d ch%d)", i_dreamadc,ch);
      hadc_dream[i_dreamadc][ch] = new TH1F (histo,name, 4096,-0.5,4095.5);
    }
  }

  //Muon ADC histograms
  for(int ch=0;ch<32;ch++){
    sprintf(histo,"Muon_ADC0_ch%d",ch);
    sprintf(name,"Muon (ADC0 ch%d)",ch);
    hadc_muons[ch] = new TH1F (histo,name, 4096,-0.5,4095.5);
  }

  //Init hodoscope histograms
  h_hodo_x  = new TH1F ("Hodo_X","Hodoscope X cohordinate", 96,0,96);
  h_hodo_y  = new TH1F ("Hodo_Y","Hodoscope Y cohordinate", 96,0,96);
  h_hodo_xy  = new TH2F ("Hodo_XY","Hodoscope beam profile", 96,0,96,96,0,96);
  for(int i_hodadc=0; i_hodadc<6; i_hodadc++){
    for(int ch=0;ch<32;ch++){
      sprintf(histo,"Hodoscope_%d_ADC%d_ch%d",ch+i_hodadc*32, i_hodadc,ch);
      sprintf(name,"Hodoscope %d (ADC%d ch%d)",ch+i_hodadc*32, i_hodadc,ch);
      hadc_hod[i_hodadc][ch] = new TH1F (histo,name, 4096,-0.5,4095.5);
    }
  }

  for(int i_hod_event=0; i_hod_event<100; i_hod_event++){
    hadc_hod_1ev[i_hod_event]=0;
  }

  //Histograms for the ADC LECROY L1182 (used also in  TB 2006)- Module 0 - 8 Channels  
  // (used only the first 2 channels)
  // ADC Spectra without pedestal subtraction (histo) and with pedestal substraction (histop)

  // Histograms for the ADC V792AC MODULE 0 (called in the code ADN0) - 32 Channels  
  // ADC Spectra without pedestal subtraction (histo) and with pedestal substraction (histop)

  //ADCN0  
  k=0;
  if ((!drs)||((drs)&&(drs_setup==2))) {
    for( j=0;j<32;j++) {
      //2009 mapping
/*       if((j<19)&&(j!=13)&&(j!=1)) { */
/*         // books s histos from 1 to 19i */
/* 	sprintf(histo,"s_%d",j+1); */
/* 	sprintf(histop,"s_ped_%d",j+1); */
/*       }else if (j==22) {  */
/* 	sprintf(histo,"s_2"); */
/* 	sprintf(histop,"s_ped_2"); */
/*       }else if (j==20) { */
/* 	sprintf(histo,"s_14"); */
/* 	sprintf(histop,"s_ped_14"); */
/*       } else { */
/* 	sprintf(histo,"adcn0_ch%d",j); */
/* 	sprintf(histop,"adcn0_ped_ch%d",j); */
/*       } */

      //2010 mapping
      if(j>0 && j<16) {
        // books s histos from 1 to 15
	sprintf(histo,"s_%d",j);
	sprintf(histop,"s_ped_%d",j);
      }else if (j>16 && j<22) { 
	sprintf(histo,"s_%d",j-1);
	sprintf(histop,"s_ped_%d",j-1);
      } else {
	sprintf(histo,"adcn0_ch%d",j);
	sprintf(histop,"adcn0_ped_ch%d",j);
      }
      sprintf(name,"RUN%d - ADCN0 ",run_nr);  
      hadc0n[k] = new TH1F (histo, name, 4096,-0.5,4095.5);
      sprintf(name,"RUN%d - ADCN0 (ped sub)",run_nr);  
      hadc0np[k] = new TH1F (histop, name,4100,-100.5,3999.5);
      k++;
    }
  
    sprintf(name,"RUN%d - ADCN0 Overflow Map",run_nr);  
    hadc0ovmap = new TH1F ("adcn0_ov_map", name, 32, -.5,31.5);
  } 

  //ADCN1
  k=0;
  if ((!drs)||((drs)&&(drs_setup==2))) {
    for( j=0;j<32;j++) {

      //2009 mapping
/*       if((j<19)&&(j!=16)&&(j!=17)) { */
/*         // books s histos from 1 to 19 */
/* 	sprintf(histo,"q_%d",j+1); */
/* 	sprintf(histop,"q_ped_%d",j+1); */
/*       } else if (j==20){ */
/* 	sprintf(histo,"q_17"); */
/* 	sprintf(histop,"q_ped_17"); */
/*       } else if (j==22) { */
/* 	sprintf(histo,"q_18"); */
/* 	sprintf(histop,"q_ped_18"); */
/*       } else { */
/* 	sprintf(histo,"adcn1_ch%d",j); */
/* 	sprintf(histop,"adcn1_ped_ch%d",j); */
/*       } */
      //2010 mapping
      if(j>0 && j<16) {
        // books s histos from 1 to 15
	sprintf(histo,"q_%d",j);
	sprintf(histop,"q_ped_%d",j);
      }else if (j>16 && j<22) { 
	sprintf(histo,"q_%d",j-1);
	sprintf(histop,"q_ped_%d",j-1);
      } else {
	sprintf(histo,"adcn1_ch%d",j);
	sprintf(histop,"adcn1_ped_ch%d",j);
      }

      sprintf(name,"RUN%d - ADCN1 (ped sub)",run_nr);
      hadc1n[k] = new TH1F (histo, name, 4096,-0.5,4095.5);
      sprintf(name,"RUN%d - ADCN1 (ped sub)",run_nr);
      hadc1np[k] = new TH1F (histop, name,4100,-100.5,3999.5);
      k++;
    }

    sprintf(name,"RUN%d - ADCN1 Overflow Map",run_nr);
    hadc1ovmap = new TH1F ("adcn1_ov_map", name, 32, -.5,31.5);

  }
 
  // Histograms for the ADC V792AC MODULE 1 (called in the code ADN0) - 32 Channels 
  // used only channels larger than 18 
  // ADC Spectra without pedestal subtraction (histo) and with pedestal substraction (histop)

  // The channel numbering on the second QDC starts
  // from 18, not from 0 -- CONFIRM WITH WINER
  //ADCN2
  k=0;
  for (l=0;l<32;l++) { 
    if ((!drs)||((drs)&&(drs_setup==2))) {
      if (l<6) {
	sprintf(histo,"L_%d",l+1);
	sprintf(histop,"L_ped_%d",l+1);
      }

      else if (l==6) {
	sprintf(histo,"muon");
	sprintf(histop,"muon_ped");
      }
      else if (l==7) {
	sprintf(histo,"IT");
	sprintf(histop,"IT_ped");
      } else { 
	// books crystal ADCs. The channel # starts with 18.
	sprintf(histo,"adcn2_ch%d",l);
	sprintf(histop,"adcn2_p_ch%d",l);
      }
    }
    if ((drs)&&(drs_setup==1)) {
      if (l==6) {
	sprintf(histo,"muon");
	sprintf(histop,"muon_ped");
      }
      else if (l==7) {
	sprintf(histo,"IT");
	sprintf(histop,"IT_ped");
      }else {
	sprintf(histo,"adcn2_ch%d",l);
	sprintf(histop,"adcn2_p_ch%d",l);
      }
    }
    sprintf(name,"RUN%d - ADCN2 (no ped sub)",run_nr);  
    hadc2n[l] = new TH1F (histo,name, 4096,-0.5,4095.5);
    sprintf(name,"RUN%d - ADCN2 (ped sub)",run_nr);  
    hadc2np[l] = new TH1F (histop,name,4100,-100.5,3999.5);
    k++;
  }
  sprintf(name,"RUN%d - ADCN2 Overflow Map",run_nr);  
  hadc2ovmap = new TH1F ("adcn2_ov_map",name, 32, -.5,31.5);

  // Higrograms for Dream Total Signal S and Dream Total Signal Q
  // Units are QDC counts.
  sprintf(name,"RUN%d - DREAM TOTAL S (no ped sub)",run_nr);  
  hs=new TH1F("S_TOT",name,500,0.,10000.);

  sprintf(name,"RUN%d - DREAM TOTAL S (ped sub)",run_nr);  
  hsp= new TH1F("S_TOT_P",name,500,0.,10000.);

  sprintf(name,"RUN%d - DREAM TOTAL Q (no ped sub)",run_nr);  
  hq=new TH1F("Q_TOT",name,500,0.,10000.);
  
  sprintf(name,"RUN%d - DREAM TOTAL Q (ped sub)",run_nr);  
  hqp=new TH1F("Q_TOT_P",name,500,0.,10000.);


  //DRS 2008  }

  /*
 /// Histrograms for FLASH ADC  (used in TB 2006)
 for( i=1;i<9;i++) {
 hi=900+i;
 sprintf(histo,"h%d",hi);
 hfadc[i-1] = new TH1F (histo," FADC ", 1000,0.,5000.);
 }
  */

  /// Histograms for TDC MODULE L1176 with 16 channel -used only 8 channel (from 1 to 8) 
  // TDC Spectra
  // Channel 0 is unused -- it is noisy
  sprintf(name,"RUN%d - TDC",run_nr);  
  ///TB 2007

  /*  for ( i=1;i<9;i++) {
      if (i==1) {sprintf(histo,"tdc_dwc_up_l");}
      if (i==2) {sprintf(histo,"tdc_dwc_up_r");}
      if (i==3) {sprintf(histo,"tdc_dwc_up_u");}
      if (i==4) {sprintf(histo,"tdc_dwc_up_d");}
      if (i==5) {sprintf(histo,"tdc_dwc_dw_l");}
      if (i==6) {sprintf(histo,"tdc_dwc_dw_r");}
      if (i==7) {sprintf(histo,"tdc_dwc_dw_u");}
      if (i==8) {sprintf(histo,"tdc_dwc_dw_d");}

      htdc[i]=new TH1F (histo,name,100,0.,1000.);
      }
  */

  for ( i=0;i<16;i++) {
    htdc[i]=0;
  }
  for ( i=1;i<4;i++) {
    if (i==1) {
      sprintf(histo,"tdc_dwc_up_l");
      sprintf(name,"RUN%d - TDC left",run_nr);  
    }
    if (i==2) {
      sprintf(histo,"tdc_dwc_up_r");
      sprintf(name,"RUN%d - TDC right",run_nr);  
    }
    if (i==3) {
      sprintf(histo,"tdc_dwc_up_u");
      sprintf(name,"RUN%d - TDC up",run_nr);  
    }
    if (i==4) {
      sprintf(histo,"tdc_dwc_dw_d");
      sprintf(name,"RUN%d - TDC down",run_nr);  
    }
    htdc[i]=new TH1F (histo,name,1000,0.,1000.);
  }
  for ( int i_tdc_ch=0;i_tdc_ch<16;i_tdc_ch++) {
    sprintf(histo,"TDC_ch%d",i_tdc_ch);  
    sprintf(name,"TDC ch %d",i_tdc_ch);  
    htdc_debug[i_tdc_ch]=new TH1F (histo,name,4096,-0.5,4095.5);
  }

  // Histogram of the number of corrupted hits (valid=0) in the event
  sprintf(name,"RUN%d - TDC Corrupted",run_nr);  
  htdccorr=new TH1F ("tdccorr",name,65,-0.5,64.5);

  sprintf(name,"RUN%d - DWC UP U-D",run_nr);  
  htdcupud=new TH1F ("tdc_dwc_up_ud",name,500,-250.5,249.5);
  
  sprintf(name,"RUN%d - DWC UP L-R",run_nr);  
  htdcuplr=new TH1F ("tdc_dwc_up_lr",name,500,-250.5,249.5);
  
  sprintf(name,"RUN%d - DWC DW U-D",run_nr);  
  htdcdwud=new TH1F ("tdc_dwc_dw_ud",name,500,-250.5,249.5);
  
  sprintf(name,"RUN%d - DWC DW L-R",run_nr);  
  htdcdwlr=new TH1F ("tdc_dwc_dw_lr",name,500,-250.5,249.5);

  sprintf(name,"RUN%d - X_DW ",run_nr);
  h_x_dw=new TH1F ("X_DW",name,100,-50.,50.);

  sprintf(name,"RUN%d - Y_DW ",run_nr);
  h_y_dw=new TH1F ("Y_DW",name,100,-50.,50.);

  sprintf(name,"RUN%d - Y_DW vs X_DW ",run_nr);
  h_xy_dw=new TH2F ("Y_vs_X_DW",name,100,-50.,50.,100,-50.,50.);

  sprintf(name,"RUN%d - X_UP ",run_nr);
  h_x_up=new TH1F ("X_UP",name,100,-50.,50.);

  sprintf(name,"RUN%d - Y_UP ",run_nr);
  h_y_up=new TH1F ("Y_UP",name,100,-50.,50.);

  sprintf(name,"RUN%d - Y_UP vs X_UP ",run_nr);
  h_xy_up=new TH2F ("Y_vs_X_UP",name,100,-50.,50.,100,-50.,50.);

  // Histograms for TEMPERATURE MODULE TH03 with 3 Channels
  // x axis is the T in deg C.
  sprintf(name,"RUN%d - TEMP",run_nr);  
  for (i=0;i<3;i++) {
    sprintf(histo,"temp%d",i);
    htemp[i]=new TH1F (histo,name,200,-100.,100.); 
    sprintf(histo,"temptrend%d",i);
    htemptrend[i] = new TH1F(histo,name,1000,-0.5,99999.5);
  }


  // Histograms for the OSCILLOSCOPE- 4 Channels
  // 1-Dim Histo 
  // This is just a histogram of the channel output
  // values, about 250 fills per event
  if ((!drs)||(drs&&(drs_setup==1))) {
    sprintf(name,"RUN%d - OSC",run_nr);  
    for( i=1;i<5;i++) {
      sprintf(histo,"osc%d",i);
      hosc[i-1] = new TH1F (histo,name, 256,-128.5,127.5);
    }
  
    sprintf(name,"RUN%d - Scope Underflow Count",run_nr);  
    for( i=1;i<5;i++) {
      sprintf(histo,"underflow%d",i);
      hunder[i-1] = new TH1F (histo,name, 2,-0.5,1.5);
    }

    // Time Structure of 100 events (event number:10001,20001....100001) 
    sprintf(name,"RUN%d - Time Structure",run_nr);  
    for (l=1;l<101;l++) {
      for ( k=1; k<5; k++) {
	sprintf(histo,"time_str_osc%d_n%d",k,l);
	hosc2[l-1][k-1]=new TH1F (histo,name,282,-0.5,281.5);
      }
    }                
            

    for ( k=1; k<5; k++) {
      sprintf(histo,"time_str_osc%d_mean",k);
      //      hosc_mean[k-1]=new TH1F (histo,name,282,-0.5,281.5);
      hosc_mean[k-1]=new TH1F (histo,name,564,-0.5,563.5);
    }
    sprintf(name,"RUN%d - Time Structure (mV) ",run_nr); 
    for ( k=1; k<5; k++) {
      sprintf(histo,"time_str_osc%d_mean_v",k);
      //      hosc_meanv[k-1]=new TH1F (histo,name,282,-0.5,281.5);
      hosc_meanv[k-1]=new TH1F (histo,name,564,-0.5,563.5);
    }
    sprintf(name,"RUN%d - Integral  ",run_nr);
    for (k=1;k<5;k++){
      sprintf(histo,"int_osc%d_mean_v",k);
      hosc_int_mean_v[k-1]=new TH1F (histo,name,10100,-200000,2000);
    }
 
    counto=0;
    num1=0;
    num2=0;
    num3=0;
    num4=0;

    for  (unsigned int i=0;i<1128;i++) {
      sum1[i]=0;
      sum2[i]=0;
      sum3[i]=0;
      sum4[i]=0;
      sum1v[i]=0;
      sum2v[i]=0;
      sum3v[i]=0;
      sum4v[i]=0;
    }
    for (unsigned int i=0;i<4;i++) {
      osc_int[i]=0;
    }

  }
  return 0;
}

static unsigned int nSingleHisto = 0;

int dreammon_event(unsigned int doSingle, unsigned int evt, unsigned int * buf,bool drs, int drs_setup){
  int rc;
  unsigned int j;
  unsigned int hits;
  unsigned int *addr;
  mySCA scaData;
  myADCN adcnData;
  myTH03 th03Data;
  myTEKOSC tekoscData;
  myTDC tdcData;
  /*myADC adcData;
    myFADC fadcData;
    myKTDC ktdcData;
  */

  int isFirstEvent ;
  int carry;
  unsigned int doThat;

  if(evt==0) isFirstEvent=1;
  if(evt!=0) isFirstEvent=0;

  unsigned int i;
  EventHeader * head=(EventHeader *) buf;

  if(head->evmark!=0xCAFECAFE){
    printf( "Cannot find the event marker. Something is wrong.\n" );
    return 0;
  }
  

  i=head->evhsiz/sizeof(unsigned int);

  Nevtda   = 0; 
  // Fill histo of run number and pedestal run number 
  hrn->Fill(Nrunnumber);
  hprn->Fill(ped_run_number);

  if (isFirstEvent) {
    BegTimeEvs = head->tsec;
    BegTimeEvu = head->tusec;
    isFirstEvent =0;
  }
  Nevtda   = head->evnum;
  TimeEvu  = head->tusec - BegTimeEvu;
  if (TimeEvu<0.) { double TimeEvu_t = 1e6 + head->tusec - BegTimeEvu; TimeEvu = (int) TimeEvu_t ;carry = 1; }
  else carry = 0;
  TimeEvs  = head->tsec - BegTimeEvs - carry; 

  // doThat is true if the event number is 10001,2001,....100001 (necessary for oscillospe histos)
  doThat = (doSingle) && (nSingleHisto < MAX_SINGLE_HISTO);
  if (doThat) {
    nSingleHisto ++;
  } 

  /* Used for FLASH ADC (TB 2006) 
     if (doThat) {
     usigned int k,h_n1a,h_n2a; 
     h_n1a=200+10+Nevtda*1000;
     h_n2a=500+10+Nevtda*1000;
     for ( k=1; k<9; k++) {
     sprintf(histo,"h%d",h_n1a+k);
     sprintf(histop,"h%d",h_n2a+k);
     //     hfadcsetup1[k-1] = new TH2F (histo," Time Structure SETUP1",60,-4.4,70.6,2000,-2000.,2000.);
     //     hfadcsetup2[k-1] = new TH2F (histo," Time Structure SETUP2",30,-3.8,71.2,2000,-2000.,2000.);
     }
     }
  */

  // Decode and Fill Histo for the Modules 
  //   FILL SCALER 260
  hits = 0;
  rc=0;
  unsigned int sz = (head->evsiz - head->evhsiz )/4;
  addr = SubEventSeek2(0x200003,&buf[i],sz);
  rc += DecodeV260(addr, &scaData);
  for (j=0; j<scaData.index; j++) {
    CHSCA[hits]  = scaData.channel[j];
    COUNTSCA[hits] = scaData.counts[j];
    hits++;
  }
  NSCA = hits;

  // Decode and FILL Histo for temperature TH03 Module

  hits = 0;
  addr =  SubEventSeek2(0x0000ffff, &buf[i],sz);
  rc += DecodeTH03(addr, &th03Data);
  //   std::cout<<" temp index "<<th03Data.index<<std::endl;
  for (j=0; j<th03Data.index; j++) {
    // std::cout<<"  dreammon chan "<<th03Data.channel[j]<<std::endl;  
    CHTH03[hits]  = th03Data.channel[j];
    DATATH03[hits] = th03Data.data[j];
    unsigned int ch=CHTH03[hits];
    htemp[ch]->Fill(DATATH03[hits]);
    if((evt%100)==0)
      htemptrend[ch]->Fill(evt,DATATH03[hits]);
    hits++;
  }
  NhitTH03 = hits;
                       
  // Decode and  fill Histo for  ADC  MODULE 0 V792AC (The new ADC=ADCN)
  float s_tot=0;
  float q_tot=0;
  float s_tot_p=0;
  float q_tot_p=0;
 
// ADC05 connected to S signal
  if ((!drs)||(drs)&&(drs_setup==2)) {
    hits = 0;
    addr =  SubEventSeek2(0x05000005, &buf[i],sz);
    rc += DecodeV792AC(addr, &adcnData);
    for (j=0; j<adcnData.index; j++) {
      CHADCN0[hits]  = adcnData.channel[j];
      CHARGEADCN0[hits] = adcnData.data[j];
      hadc0n[CHADCN0[hits]]->Fill(CHARGEADCN0[hits]);
      hadc0np[CHADCN0[hits]]->Fill(CHARGEADCN0[hits]- pmean[CHADCN0[hits]]);
      //    hadc0ovmap->Fill(CHADCN0[hits],adcnData.ov[j]);
      
      //2009 mapping 
      //if(((CHADCN0[hits]<19)&&(CHADCN0[hits]!=13))&&(CHADCN0[hits]!=1)||(CHADCN0[hits]==22)||(CHADCN0[hits]==20)){
      // 2010 mapping 
      if(CHADCN0[hits]>0 && CHADCN0[hits]<22 && CHADCN0[hits]!=16){
	hadc0ovmap->Fill(CHADCN0[hits],adcnData.ov[j]);
	s_tot=s_tot+CHARGEADCN0[hits];
	s_tot_p=s_tot_p+CHARGEADCN0[hits]-pmean[CHADCN0[hits]];
      }

      hits++;
    }
    NhitADCN0 = hits;

    // Decode and FILL  Histo for ADC MODULE 1 V792AC (The new ADC=ADCN)
    // ADC04 connected to Q signal
    hits = 0;
    addr =  SubEventSeek2(0x04000005, &buf[i],sz);
    rc += DecodeV792AC(addr, &adcnData);
    for (j=0; j<adcnData.index; j++) {
      CHADCN1[hits]  = adcnData.channel[j];
      CHARGEADCN1[hits] = adcnData.data[j];
      hadc1n[CHADCN1[hits]]->Fill(CHARGEADCN1[hits]);
      hadc1np[CHADCN1[hits]]->Fill(CHARGEADCN1[hits]-pmean[CHADCN1[hits]+32]);
      //    hadc1ovmap->Fill(CHADCN1[hits],adcnData.ov[j]);
      //2009 mapping 
      //if (((CHADCN1[hits]<19)&&(CHADCN1[hits]!=16)&&(CHADCN1[hits]!=17))||(CHADCN1[hits]==20)||(CHADCN1[hits]==22)) {
      
      // 2010 mapping 
      if(CHADCN0[hits]>0 && CHADCN0[hits]<22 && CHADCN0[hits]!=16){
	hadc1ovmap->Fill(CHADCN1[hits],adcnData.ov[j]); 
	q_tot=q_tot+CHARGEADCN1[hits];
	q_tot_p=q_tot_p+CHARGEADCN1[hits]-pmean[CHADCN1[hits]+32];
      }
      hits++;
    }
    NhitADCN1 = hits;


    hs->Fill(s_tot);
    hq->Fill(q_tot);
    hsp->Fill(s_tot_p);
    hqp->Fill(q_tot_p);
  }  

  //debug histogram with all channels
  for(int i_dreamadc=0; i_dreamadc<2; i_dreamadc++){
    int adc_id = 0x04000005+0x01000000*i_dreamadc;
    addr =  SubEventSeek2(adc_id, &buf[i],sz);
    rc += DecodeV792AC(addr, &adcnData);
    for (j=0; j<adcnData.index; j++) {
      int ch = adcnData.channel[j];
      int data = adcnData.data[j];
      hadc_dream[i_dreamadc][ch]->Fill(data);
    }
  }

  //Muon ADC histograms
  int adc_id = 0x40000025;
  addr =  SubEventSeek2(adc_id, &buf[i],sz);
  rc += DecodeV862(addr, &adcnData);
  for (j=0; j<adcnData.index; j++) {
    int ch = adcnData.channel[j];
    int data = adcnData.data[j];
    hadc_muons[ch]->Fill(data);
  }
  //Decode and FILL  Histo for Hodoscope ADCs
  if(evt<100){
    char *name = Form("Hodoscope_Fiber_ev%d",evt);
    char *title = Form("Hodoscope fiber signal (ev%d)",evt);
    hadc_hod_1ev[evt] = new TH1F(name,title, 192, -0.5, 191.5);
  }

  int hodo_x=-1,hodo_y=-1;
  float hodo_x_max = -1,hodo_y_max = -1; 

  for(int i_hodadc=0; i_hodadc<6; i_hodadc++){
    int adc_id = 0x06000005+0x01000000*i_hodadc;
    addr =  SubEventSeek2(adc_id, &buf[i],sz);
    rc += DecodeV792AC(addr, &adcnData);
    for (j=0; j<adcnData.index; j++) {
      int ch = adcnData.channel[j];
      int data = adcnData.data[j];
      hadc_hod[i_hodadc][ch]->Fill(data);
      //fill single event hists
      if(evt<100){
	hadc_hod_1ev[evt]->Fill(ch+i_hodadc*32,data);
      }
      //search for maximum signal
      if(i_hodadc<3){ 
	//y cohordinate
	if(hodo_y_max<data){
	  hodo_y = ch+i_hodadc*32;
	  hodo_y_max = data;
	}
      }else{
      	//x cohordinate
	if(hodo_x_max<data){
	  hodo_x = ch+i_hodadc*32 - 96;
	  hodo_x_max = data;
	}
      }
    }
  }
  h_hodo_x->Fill(hodo_x);
  h_hodo_y->Fill(hodo_y);
  h_hodo_xy->Fill(hodo_x,hodo_y);

  // Decode and FILL  Histo for ADC MODULE 2 V792AC (The new ADC=ADCN)
  hits = 0;
  addr =  SubEventSeek2(0x08000005, &buf[i],sz);
  rc += DecodeV792AC(addr, &adcnData);
  for (j=0; j<adcnData.index; j++) {
    CHADCN2[hits]  = adcnData.channel[j];
    CHARGEADCN2[hits] = adcnData.data[j];
    hadc2n[CHADCN2[hits]]->Fill(CHARGEADCN2[hits]);
    hadc2np[CHADCN2[hits]]->Fill(CHARGEADCN2[hits]-pmean[CHADCN2[hits]+64]);
    if ((!drs)||(drs&&drs_setup==2)) {
      if (CHADCN2[hits]<8)  {hadc2ovmap->Fill(CHADCN2[hits],adcnData.ov[j]);}}
    if (drs&&drs_setup==1) {
      if  ((CHADCN2[hits]==6)||(CHADCN2[hits]==7)) {hadc2ovmap->Fill(CHADCN2[hits],adcnData.ov[j]);} }

    hits++;
  }
  NhitADCN2 = hits;

                                                                           
  // Decode and FILL Oscillospe TEKOSC

  if ((!drs)||(drs&&(drs_setup==1))) {
    hits = 0;
    addr =  SubEventSeek2(0x0000fafa, &buf[i],sz);
    rc += DecodeTEKOSC(addr, &tekoscData);
    unsigned int kp;
    float underflowed[4] = {0, 0, 0, 0};
    for (unsigned int i=0;i<4;i++) {
      osc_int[i]=0;
    }

    double bl[4]={0,0,0,0};
    double nbl[4]={0,0,0,0};
    double npt[4]={0,0,0,0};
    for (j=0; j<tekoscData.index; j++) {
      unsigned int nh = NUMOSC[hits] = tekoscData.num[j];
      unsigned int chan = CHOSC[hits] = tekoscData.channel[j];
      const int oscvalue = tekoscData.data[j]/256;
      double dv = (double(oscvalue)/25.6-(double(tekoscData.position[chan])/1000.)+5.)*double(tekoscData.scale[chan]);
      if (oscvalue <= -127)
        underflowed[chan] = 1.f;
      hosc[chan]->Fill(oscvalue);
      osc_int[chan] += dv;
      if (nh < 50) {
        bl[chan] += dv;
        nbl[chan] += 1;
      }
      npt[chan] += 1;
      if (chan==0) {
	dato1[nh]=(float)(oscvalue);
	sum1[nh ]+=dato1[nh];
	num1=nh;
      }
      if (chan==1) {
	dato2[nh]=(float)(oscvalue);
	sum2[nh ]+=dato2[nh];
	num2=nh;
      }
      if (chan==2) {
	dato3[nh]=(float)(oscvalue);
	sum3[nh ]+=dato3[nh];
	num3=nh;
      }
      if (chan==3) {
	dato4[nh]=(float)(oscvalue);
	sum4[nh ]+=dato4[nh];
	num4=nh;
      }

      if (doThat) {
	hosc2[nSingleHisto-1][chan]->Fill(nh,oscvalue);
      }
      hits++;
    }

    for (kp=0;kp<4;kp++) {
      hunder[kp]->Fill(underflowed[kp]);
    }

    NhitOSC = hits;
    if (NhitOSC>0) {counto++;}

    for (kp=0;kp<4;kp++) {
      if (nbl[kp]) osc_int[kp] -= bl[kp]*npt[kp]/nbl[kp];
      SCALEOSC[kp]=tekoscData.scale[kp];
      POSOSC[kp]=tekoscData.position[kp];
      TDOSC[kp]=tekoscData.tdiff[kp];
      CHFOSC[kp]=tekoscData.chfla[kp];
      hosc_int_mean_v[kp]->Fill(osc_int[kp]);
    }

  }
  //Decode and FILL ADC LECROY L1182 MODULE 0 

  /*
    hits = 0;
    addr = SubEventSeek2(0x10002,&buf[i],sz);
    rc += DecodeL1182(addr, &adcData);
    for (j=0; j<adcData.index; j++) {
    CHADC0[hits]  = adcData.channel[j];
    CHARGEADC0[hits]  = adcData.charge[j];
    if(CHADC0[hits]<2) {
    hadc[CHADC0[hits]]->Fill(CHARGEADC0[hits]);
    hadcp[CHADC0[hits]]->Fill(CHARGEADC0[hits]-pmean[CHADC0[hits]+64]);
    }
    hits++;
    //k++;
    }
    NhitADC0 = hits;
  */
  // DRS 2008  }

  /*  In TB 2006 there was other ADC LECROY 1182 Modules

  //FILL ADC LECROY L1182 MODULE 1
  hits = 0;
  addr = SubEventSeek2(0x50002, &buf[i],sz);
  rc += DecodeL1182(addr, &adcData);
  for (j=0; j<adcData.index; j++) {
  CHADC1[hits]  = adcData.channel[j];
  CHARGEADC1[hits]  = adcData.charge[j];
  hits++;
  k++;
  }
  NhitADC1 = hits;
  //FILL ADC LECROY L1182 MODULE 2
  hits = 0;
  addr = SubEventSeek2(0x40002, &buf[i],sz);
  rc += DecodeL1182(addr, &adcData);
  for (j=0; j<adcData.index; j++) {
  CHADC2[hits]  = adcData.channel[j];
  CHARGEADC2[hits]  = adcData.charge[j];
  hits++;
  k++;
  }
  NhitADC2 = hits;
  //FILL ADC LECROY L1182 MODULE 3
  hits = 0;
  addr = SubEventSeek2(0x30002, &buf[i],sz);
  rc += DecodeL1182(addr, &adcData);
  for (j=0; j<adcData.index; j++) {
  CHADC3[hits]  = adcData.channel[j];
  CHARGEADC3[hits]  = adcData.charge[j];

  hits++;
  k++;
  }
  NhitADC3 = hits;
  //FILL ADC LECROY L1182  MODULE 4
  hits = 0;
  addr = SubEventSeek2(0x20002, &buf[i],sz);
  rc += DecodeL1182(addr, &adcData);
  for (j=0; j<adcData.index; j++) {
  CHADC4[hits]  = adcData.channel[j];
  CHARGEADC4[hits]  = adcData.charge[j];

  hits++;
  k++;
  }
  NhitADC4 = hits;
 
  //FILL ADC LECROY L1182 MODULE 5
  hits = 0;
  addr = SubEventSeek2(0x10002, &buf[i],sz);
  rc += DecodeL1182(addr, &adcData);
  for (j=0; j<adcData.index; j++) {
  CHADC5[hits]  = adcData.channel[j];
  CHARGEADC5[hits]  = adcData.charge[j];
  hits++;
  k++;
  }
  NhitADC5 = hits;

  */

  /*  FILL FADC SIS3320 present in TB 2006
      float off2=0;
      hits = 0;
      addr = SubEventSeek2(0x20000013, &buf[i],sz);
      rc += DecodeSIS3320(addr, &fadcData);
      for (j=0; j<fadcData.index; j++) {
      CHFADC[hits]  = fadcData.channel[j];
      VALUEFADC[hits]  = fadcData.value[j];
      NUMFADC[hits]  = fadcData.num[j];
      hfadc[CHFADC[hits]-1]->Fill(VALUEFADC[hits]);
      if (doThat) { 
      // SETUP 1
      if ((CHFADC[hits]==1)||(CHFADC[hits]==5)) {
      off2=3.75;
      }
      if ((CHFADC[hits]==2)||(CHFADC[hits]==6)) {
      off2=2.5;
      }
      if ((CHFADC[hits]==3)||(CHFADC[hits]==7)) {
      off2=1.25;
      }
      if ((CHFADC[hits]==4)||(CHFADC[hits]==8)) {
      off2=0.;
      }      
      float x=5.*NUMFADC[hits]+off2; // nsec
      float y=VALUEFADC[hits];
      //      hfadcsetup1([CHFADC[hits]-1])->Fill(x,y);
      
      // SETUP 2
      if((CHFADC[hits]==1)||(CHFADC[hits]==3)||(CHFADC[hits]==5)||(CHFADC[hits]==7)) {
      off2=2.5;
      }
      if((CHFADC[hits]==2)||(CHFADC[hits]==4)||(CHFADC[hits]==6)||(CHFADC[hits]==8)) {
      off2=0.;
      }
   
      x=5.*NUMFADC[hits]+off2;
      y=VALUEFADC[hits];
      //      hfadcsetup2([CHFADC[hits]-1])->Fill(x,y);
      } // if Nevtda

      hits++;
      }
      NhitFADC = hits;
  */

  //Decode and  Fill TDC V775 debug histos
  addr = SubEventSeek2(0x20000024, &buf[i],sz);
  rc += DecodeV775(addr, &tdcData);
  for (j=0; j<tdcData.index; j++) { 
    int ch = tdcData.channel[j];
    int data = tdcData.data[j];
    htdc_debug[ch]->Fill(data);
  }

  //Decode and  Fill TDC L1176 (hacked to work with V775)
  hits = 0;
  for (int i=0;i<64;i++) {
    CHTDC[i] = 100;
  }

  int corr=0;
  int tdcdwleft=-1;
  int tdcdwright=-1;
  int tdcdwup=-1;
  int tdcdwdown=-1;
  int tdcupleft=-1;
  int tdcupright=-1;
  int tdcupup=-1;
  int tdcupdown=-1;
  bool foundbchtdc[8]={false,false,false,false,false,false,false,false};
  addr = SubEventSeek2(0x20000024, &buf[i],sz);
  
  rc += DecodeV775(addr, &tdcData);
  // j=tdcData.index; while (j--) {
  for (j=0; j<tdcData.index; j++) { 
    COUNTTDC[hits]  = tdcData.data[j];
    EDGETDC[hits]  = tdcData.edge[j];
    int tdcchan = CHTDC[hits]  = tdcData.channel[j];
    VALIDTDC[hits]  = tdcData.valid[j];
    //TB 2007    if((tdcchan>0)&&(tdcchan<9)){
    if((tdcchan>=0)&&(tdcchan<8)){
      //TB 2007      if ((VALIDTDC[hits]==0) && (!foundbchtdc[tdcchan-1])) {
      //in 2010 is inverted 
      if ((VALIDTDC[hits]==1) && (!foundbchtdc[tdcchan])) {
	htdc[tdcchan]->Fill(COUNTTDC[hits]);
	//TB 2007        foundbchtdc[tdcchan-1] = true;
        foundbchtdc[tdcchan] = true;
      }else{
	corr++;
      }
    }
    //TB 2007 

    /*    if((tdcchan==1) && (tdcupleft == -1))
	  tdcupleft=COUNTTDC[hits];
	  if((tdcchan==2) && (tdcupright == -1))
	  tdcupright=COUNTTDC[hits];
	  if((tdcchan==3) && (tdcupup == -1))
	  tdcupup=COUNTTDC[hits];
	  if((tdcchan==4) && (tdcupdown == -1))
	  tdcupdown=COUNTTDC[hits];
	  if((tdcchan==5) && (tdcdwleft == -1))
	  tdcdwleft=COUNTTDC[hits];
	  if((tdcchan==6) && (tdcdwright == -1))
	  tdcdwright=COUNTTDC[hits];
	  if((tdcchan==7) && (tdcdwup == -1))
	  tdcdwup=COUNTTDC[hits];
	  if((tdcchan==8) && (tdcdwdown == -1))
	  tdcdwdown=COUNTTDC[hits];
    */
    if((tdcchan==0) && (tdcupleft == -1))
      tdcupleft=COUNTTDC[hits];
    if((tdcchan==1) && (tdcupright == -1))
      tdcupright=COUNTTDC[hits];
    if((tdcchan==2) && (tdcupup == -1))
      tdcupup=COUNTTDC[hits];
    if((tdcchan==3) && (tdcupdown == -1))
      tdcupdown=COUNTTDC[hits];
    if((tdcchan==4) && (tdcdwleft == -1))
      tdcdwleft=COUNTTDC[hits];
    if((tdcchan==5) && (tdcdwright == -1))
      tdcdwright=COUNTTDC[hits];
    if((tdcchan==6) && (tdcdwup == -1))
      tdcdwup=COUNTTDC[hits];
    if((tdcchan==7) && (tdcdwdown == -1))
      tdcdwdown=COUNTTDC[hits];

    hits++;
  }
  NhitTDC = hits; 
  htdccorr->Fill(corr); 

  if(tdcupleft>=0 && tdcupright>=0)
    htdcuplr->Fill(tdcupleft-tdcupright);
  if(tdcupup>=0 && tdcupdown>=0)
    htdcupud->Fill(tdcupup-tdcupdown);
  if(tdcdwleft>=0 && tdcdwright>=0)
    htdcdwlr->Fill(tdcdwleft-tdcdwright);
  if(tdcdwup>=0 && tdcdwdown>=0)
    htdcdwud->Fill(tdcdwup-tdcdwdown);
  //2008 Calibration also in the online
  // Convert time information of TDC L1176 in space coordinate
  unsigned int x,y,c[8];
  float count[8][4];
  /* 2008 Calibrations
     float hSlope_up=0.183;
     float vSlope_up=0.183;
     float hOffset_up=0.06;
     float vOffset_up=0.79;

     float hSlope_dw=0.185;
     float vSlope_dw=0.182;
     float hOffset_dw=0.92;
     float vOffset_dw=0.54;
  */
  /*
    float hSlope_up=0.18;
    float vSlope_up=0.18;
    float hOffset_up=0;
    float vOffset_up=0;

    float hSlope_dw=0.1854;
    float vSlope_dw=-0.1818;
    float hOffset_dw=2.1384;
    float vOffset_dw=-7.1877;
  */

  float hSlope_up=-0.183;
  float vSlope_up=-0.183;
  float hOffset_up=0.06;
  float vOffset_up=0.79;

  float hSlope_dw=-0.1818;
  float vSlope_dw=-0.1854;
  float hOffset_dw=-7.1877;
  float vOffset_dw=2.1384;


  for (j=0;j<8;j++) c[j]=0;
  for (j=0; j<NhitTDC; j++) {
    if (VALIDTDC[j]==1) {
      for (unsigned int i=0;i<8;i++) {
	if (CHTDC[j]==i) {
	  count[i][c[i]]=COUNTTDC[j];
	  c[i]++;
	}
      }
    }
  }
  x=0;

  for (unsigned int i1=0;i1<c[0] && x<=16;i1++)  {
    for (unsigned int i2=0;i2<c[1] && x<=16;i2++) {
      X_UP[x]=((count[1][i2]-count[0][i1])*hSlope_up)+hOffset_up;
      x++;
    }
  }
  N_X_UP=x;
  if (N_X_UP!=0) h_x_up->Fill(X_UP[0]);
  y=0;
  for (unsigned int i3=0;i3<c[2] && y<=16;i3++)  {
    for (unsigned int i4=0;i4<c[3] && y<=16;i4++) {
      Y_UP[y]=((count[3][i4]-count[2][i3])*vSlope_up)+vOffset_up;
      y++;
    }
  }
  N_Y_UP=y;
  if (N_Y_UP!=0) h_y_up->Fill(Y_UP[0]);
  if ((N_X_UP!=0)&&(N_Y_UP!=0)) h_xy_up->Fill(X_UP[0],Y_UP[0]);
  x=0;
  for (unsigned int i5=0;i5<c[4] && x<=16;i5++)  {
    for (unsigned int i6=0;i6<c[5] && x<=16;i6++) {

      X_DW[x]=((count[5][i6]-count[4][i5])*hSlope_dw)+hOffset_dw;
      x++;
    }
  }
  N_X_DW=x;
  if (N_X_DW!=0) h_x_dw->Fill(X_DW[0]);
  y=0;
  for (unsigned int i7=0;i7<c[6] && y<=16;i7++)  {
    for (unsigned int i8=0;i8<c[7] && y<=16;i8++) {
      Y_DW[y]=((count[7][i8]-count[6][i7])*vSlope_dw)+vOffset_dw;
      y++;
    }
  }
  N_Y_DW=y;
  if (N_Y_DW!=0) h_y_dw->Fill(Y_DW[0]);

  if ((N_X_DW!=0)&&(N_Y_DW!=0)) h_xy_dw->Fill(X_DW[0],Y_DW[0]); 

  /*
  //Decode for KLOE TDC -
  hits = 0;
  addr = SubEventSeek2(0x7C000012, &buf[i],sz);
  rc += DecodeKLOETDC(addr, &ktdcData);
  for (j=0; j<ktdcData.index; j++) {
  COUNTKTDC[hits]  = ktdcData.data[j];
  EDGEKTDC[hits]  = ktdcData.edge[j];
  CHKTDC[hits]  = ktdcData.channel[j];
  OVERKTDC[hits]  = ktdcData.over[j];
  HFILL(400+CHKTDC[hits],COUNTKTDC[hits],0.,1.);
  hits++;
  }  
  NhitKTDC = hits;
  */
  return 0;
}


int dreammon_exit(unsigned int i, bool drs,int drs_setup){
  TFile hfile(ntfilename,"RECREATE");
  if (hfile.IsZombie()) {
    fprintf(stderr, "Cannot open  file %s\n", ntfilename);
    return -1;
  }

  printf("%s",ntfilename);

  //debug plots in the Debug directory

  hfile.mkdir("Debug");
  hfile.cd("Debug");

  //Save debug histos for the 2 Dream ADCs
  for(int i_dreamadc=0; i_dreamadc<2; i_dreamadc++){
    for(int ch=0;ch<32;ch++){
      hadc_dream[i_dreamadc][ch]->Write();
    }
  }

  //Save Histo for Muon ADCs
  for(int ch=0;ch<32;ch++){
    hadc_muons[ch]->Write();
  }

  //Save Histo for Hodoscope ADCs
  for(int i_hodadc=0; i_hodadc<6; i_hodadc++){
    for(int ch=0;ch<32;ch++){
      hadc_hod[i_hodadc][ch]->Write();
    }
  }
  for(int i_hod_event=0; i_hod_event<100; i_hod_event++){
    if(hadc_hod_1ev[i_hod_event]!=0){
      hadc_hod_1ev[i_hod_event]->Write();
    }  
  }

  // debug histos for TDC
  for ( int i_tdc_ch=0;i_tdc_ch<16;i_tdc_ch++) {
    htdc_debug[i_tdc_ch]->Write();
  }

  // new plots are divided in directories
  hfile.cd("");
  hfile.mkdir("Hodoscope");
  hfile.cd("Hodoscope");

  h_hodo_x->Write();
  h_hodo_y->Write();
  h_hodo_xy->Write();

  hfile.cd("");

  //one day all of this stuff will goa in the Old directory
/*   hfile.mkdir("Old"); */
/*   hfile.cd("Old"); */

  // Write the istogramms
  hrn->Write();
  hprn->Write();

   
  unsigned int k=0;

  if ((!drs)||(drs&&drs_setup==2)) {
    for (k=1;k<22;k++){
      if(hadc0n[k]!=0)hadc0n[k]->Write();
      if(hadc0np[k]!=0)hadc0np[k]->Write();
    }

    hadc0ovmap->Write();
  }

  if ((!drs)||(drs&&drs_setup==2)) {
    for (k=1;k<22;k++) {
      if(hadc1n[k]!=0) hadc1n[k]->Write();
      if(hadc1np[k]!=0) hadc1np[k]->Write();
    }
    hadc1ovmap->Write();
  }
  if ((!drs)||(drs&&drs_setup==2)) {
    for (k=0;k<8;k++) {
      hadc2n[k]->Write();
      hadc2np[k]->Write();
    }
    hadc2ovmap->Write();
  }

  if (drs&&drs_setup==1){
    for (k=6;k<8;k++) {
      hadc2n[k]->Write();
      hadc2np[k]->Write();
    }
    hadc2ovmap->Write();

  }


  hs->Write();
  hq->Write();
  hsp->Write();
  hqp->Write();

  //DRS 2008  } 

  //2007  for (k=1;k<9;k++){
  for (k=0;k<16;k++){
    if(htdc[k]!=0)htdc[k]->Write();
  }
  htdccorr->Write();
  htdcuplr->Write();
  htdcupud->Write();
  htdcdwlr->Write();
  htdcdwud->Write();
  h_x_dw->Write();
  h_y_dw->Write();
  h_xy_dw->Write();
  h_x_up->Write();
  h_y_up->Write();
  h_xy_up->Write();
  k=0;
  for (k=0;k<3;k++){
    htemp[k]->Write();
    htemptrend[k]->Write();
  }
  if ((!drs)||(drs&&(drs_setup==1))){
    k=0;
    unsigned int l=0;
    for (k=0;k<4;k++) {
      hosc[k]->Write();
      hunder[k]->Write();
      hosc_int_mean_v[k]->Write();
      for(l=0;l<100;l++) {
	hosc2[l][k]->Write();
      }
    }
     
    if (i==0) {
      if (num1!=0){ 
	for (unsigned int k=0; k<(num1+1); k++)
	  {
	    sum1[k]=1.0*sum1[k]/(float)counto;
	    hosc_mean[0]->Fill(k,sum1[k]);
	    sum1v[k]=((sum1[k]/25.6)-((float)POSOSC[0]/1000.)+5.)*(float)SCALEOSC[0];
	    hosc_meanv[0]->Fill(k,sum1v[k]);
	  }
      }
      if (num2!=0){
	for (unsigned int k=0; k<(num2+1); k++)
	  {
	    sum2[k]=1.0*sum2[k]/(float)counto;
	    hosc_mean[1]->Fill(k,sum2[k]);
	    sum2v[k]=((sum2[k]/25.6)-((float)POSOSC[1]/1000.)+5.)*(float)SCALEOSC[1];
	    hosc_meanv[1]->Fill(k,sum2v[k]);
	    
	  }
      }
      if (num3!=0){
	for (unsigned int k=0; k<(num3+1); k++)
	  {
	    sum3[k]=1.0*sum3[k]/(float)counto;
	    hosc_mean[2]->Fill(k,sum3[k]);
	    sum3v[k]=((sum3[k]/25.6)-((float)POSOSC[2]/1000.)+5.)*(float)SCALEOSC[2];
	    hosc_meanv[2]->Fill(k,sum3v[k]);
	  }
      }
      if (num4!=0) {
	for (unsigned int k=0; k<(num4+1); k++)
	  {
	    sum4[k]=1.0*sum4[k]/(float)counto;
	    hosc_mean[3]->Fill(k,sum4[k]);
	    sum4v[k]=((sum4[k]/25.6)-((float)POSOSC[3]/1000.)+5.)*(float)SCALEOSC[3];
	    hosc_meanv[3]->Fill(k,sum4v[k]);
	    
	  }
      }
      for (unsigned int k=0; k<4; k++) { 
	hosc_mean[k]->Write();
	hosc_meanv[k]->Write();
	//	hosc_int_mean_v[k]->Write();
      }
    }
  }

  for (unsigned int k=0; k<4; k++)
    if (hosc_int_mean_v[k]->GetRMS() > 0)
      std::cout << "- channel " << k << " mean " << hosc_int_mean_v[k]->GetMean() << " ";
  std::cout << std::endl;

  hfile.Close(); 
  return 0;
}
/***************************************************/
