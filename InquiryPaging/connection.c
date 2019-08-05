#include <stdio.h>
#include "register_addr.h"
#include <SPIv1.h>
#include <stdlib.h>


int hopSeq[10];
int mode;
int poll = 0;

void masterCON(){


pollAbfrage(0xbb);


// poll
if(poll){
int hopfreq = 0;//calcfreqMaster();
setFreq(hopSeq[hopfreq]);

sendInquiry(0x6, 0xaa);
}


}

void slaveCON(){

  printf("Auf request warten\n");

  int noRequest=1;
  resetAbgelaufen();

  while(noRequest){

      int hopfreq = calcfreqSlave();
      setFreq(hopSeq[hopfreq]);

      // früher Inquiryscan
      int breakVal;
      InquiryScan(hopSeq[hopfreq], &breakVal);

      if (getDaten(0)==6 && getDaten(1)==0xaa){ //ID stimmt

        if(getDaten(2)==1){ //falls ==1 dann poll, else nur connection alive
          noRequest = 0;
          // request = 1;
        }
        resetAbgelaufen();
        break;
      }

      if(breakVal){
        breakVal = 0;
        break;
      }

     if(getAbgelaufen() > 50) { // break connection
        mode = 0;
        break;
     }


  }

  // if (request){
  //
  // }

}
