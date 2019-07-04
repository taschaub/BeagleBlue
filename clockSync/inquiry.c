#include <stdio.h>

#include "register_addr.h"

#include <SPIv1.h>

#include <stdlib.h>

#include <time.h>

int train = 1; //train=0 ist train a train =1 ist train b
int count = 0;
int freq = 880;
int breakVal = 0;
int mode;
void masterINQ() {

    if (count == 25) {
        if (train == 0) {
            train = 1;
            freq = 880;
            setFreq(freq);
        } else {
            train = 0;
            freq = 890;
            setFreq(freq);
        }
        count = 0;
    }

    if (train == 1) {
        freq = 880;
        setFreq(freq);
    } else {
        freq = 890;
        setFreq(freq);
    }

    resetAbgelaufen();

    int f = 0;
    for (f = 0; f < 5 && mode == 0; f++) { //den gesamten Train durchlaufen. abwechselnd senden/empfangen

        setFreq(freq);
        int tempfreq = freq;

        //Auf zwei freq senden
        int c;
        //timer start
        for (c = 0; c < 2 && mode == 0; c++) {
            int d = f * 2 + c;
            printf("TXFreq: %d \n", tempfreq);

            sendInquiry(0x1, 0xaa);
            naechsteFreq( & tempfreq);

            printf("Timer nach senden %d mal abgelaufen\n", getAbgelaufen());
            while (getAbgelaufen() == 0);
            printTime();

            resetAbgelaufen();
        }

        //Auf zwei Freq empfangen
        int breakVal = 0;
        tempfreq = freq;
        setFreq(tempfreq);
        setRX();
        while (getAbgelaufen() == 0);
        resetAbgelaufen();

        for (c = 0; c < 2 && mode == 0; c++) {
            printf("Auf %d empfangen\n", tempfreq);

            InquiryScan();

            if (getDaten(0) == 2) {

                mode = 1;

                printf("\n\n\n\n\n\n SUCCESS \n \n\n\n\n\n");

                return;

            }

            printTime();
            resetAbgelaufen();
            //auf nächste Frequenz springen
            naechsteFreq( & tempfreq);

        }

        freq = freq + 2;

    }

    count++;

}

void slaveINQ() {

    printf("Auf %d suchen:\n", freq);
    int i;

    for (i = 0; i < 10; i++) {

        // früher Inquiryscan
        fastScan();

        if (getDaten(0) == 1) { //ID stimmt
            resetAbgelaufen();

            //offsetberechnen
            calcOffset();

            while (getAbgelaufen() < 2);
            printTime();
            sendtimeMaster(0x2, 0xbb);

            resetAbgelaufen();
            mode = 1;
            return;

        }

        if (breakVal) {
            breakVal = 0;
            i = 10;
        }

    }

    //auf nächste Frequenz springen

    naechsteFreq( & freq);

    if (freq > 899) {
        freq = 880;
        setFreq(freq);
    }

}