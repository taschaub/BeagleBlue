#include <stdio.h>
#include "register_addr.h"
#include <SPIv1.h>
#include <stdlib.h>

int mode = 0;
int flag;

int main(int argc, char const *argv[]) {
  initialisieren();
  init_timer();

  while (1) {
    if(mode==0)slaveINQ();
    if(mode==1){
      slaveCLK();
      return;
    }
    if(mode==2){
      printf("connection\n");
      break;
    }

  }

  return 0;
}
