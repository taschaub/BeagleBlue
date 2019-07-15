#include <stdio.h>

#include "register_addr.h"

#include <SPIv1.h>

#include <stdlib.h>

int clockMatched = 0;
int secOffset;
int msecOffset;
int sec, msec;
int hopSeq[10] = {910,906,912,908,902,913,919,911,903,909};
int countTries = 0;
int mode;

void masterPAG() {

    //MasterClock offset fuer synchronisation mit dem slave berechnen
    if (clockMatched == 0) {

        int secReg, msecReg1, msecReg2;
        getTimeinMs( & secReg, & msecReg1, & msecReg2);

        int secSlave = getDaten(2);
        int msecSlave = (getDaten(3) << 8) + getDaten(4);

        // Offsets berechen:
        secOffset = secSlave - secReg;
        msecOffset = msecSlave - (msecReg1 << 8) + msecReg2;

        clockMatched = 1;

    }

    if (countTries > 10) {
        printf("\n\n\nOffset anpassen!!\n\n\n\n");
        msecOffset = msecOffset + 100;
        countTries = 0;
    }

    int freqnumber = calcfreqMaster();

    setFreq(hopSeq[freqnumber]);
    printf("Pagesend Freq: %d\n", hopSeq[freqnumber]);
    printTime();

    sendInquiry(0x3, 0xaa);

    while (getAbgelaufen() == 0);
    printTime();
    resetAbgelaufen();

    setRX();
    while (getAbgelaufen() == 0);
    resetAbgelaufen();

    freqnumber = calcfreqMaster();
    printf("Auf %d empfangen\n", hopSeq[freqnumber]);
    printTime();

    int breakVal = 0;
    while (getAbgelaufen() == 0) {
        InquiryScan(hopSeq[freqnumber], & breakVal);

        if (getDaten(0) == 4) {
            resetAbgelaufen();

            mode = 2;
            printf("\n\n\n\n\n\n Paging SUCESS \n \n\n\n\n\n");

            freqnumber = calcfreqMaster();

            setFreq(hopSeq[freqnumber]);
            printf("Pagesend-Ack Freq: %d\n", hopSeq[freqnumber]);

            while (getAbgelaufen() == 0);

            sendInquiry(0x5, 0xaa);

        }

        if (breakVal) {
            breakVal = 0;
            break;
        }
    }
    printTime();
    resetAbgelaufen();

    countTries++;
}

void slavePAG() {

    int freqnumber = calcfreqSlave();
    setFreq(hopSeq[freqnumber]);

    printf("Pagereceive Freq: %d\n", hopSeq[freqnumber]);

    while (getAbgelaufen() == 0) {
        int breakVal;
        InquiryScan(hopSeq[freqnumber], & breakVal);

        if (getDaten(0) == 3) { //ID stimmt

            while (getAbgelaufen() < 2);
            freqnumber = calcfreqSlave();
            setFreq(hopSeq[freqnumber]);
            sendInquiry(0x4, 0xbb);

            printf("Timer %d-mal abgelaufen", getAbgelaufen());
            while (getAbgelaufen() == 0);
            printTime();
            resetAbgelaufen();

            printf("setRX...\n");
            setRX();
            printf("Timer %d-mal abgelaufen\n", getAbgelaufen());
            while (getAbgelaufen() == 0);
            printTime();
            resetAbgelaufen();

            freqnumber = calcfreqSlave();
            printf("Auf %d empfangen\n", hopSeq[freqnumber]);

            while (getAbgelaufen() == 0) {
                InquiryScan(hopSeq[freqnumber], & breakVal);

                if (getDaten(0) == 4) {

                    mode = 2;
                    printf("\n\n\n\n\n\n Paging SUCESS \n \n\n\n\n\n");

                    break;
                }

            }

            break;
        }

        if (breakVal) {
            breakVal = 0;
            break;
        }

    }

}