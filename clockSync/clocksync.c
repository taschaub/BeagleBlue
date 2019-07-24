#include <stdio.h>

#include "register_addr.h"

#include <SPIv1.h>

#include <stdlib.h>

#include <time.h>


int freq;
int breakVal;
int mode;

void masterCLK() {

    printf("Meine time:\n");
    printTime();

    printf("Meine clock register:\n");
    int s, m;
    printTimeReg( & s, & m);

    // masterCLK vom slave anzeigen
    printf("Slave denkt meine clock ist hier:\n");

    int sek = getDaten(2);
    int milli = (getDaten(3) << 8) + getDaten(4);

    printf("Sek: %d Millisek: %d\n", sek, milli);

    // berechne offset für slave:

    int slaveSekOffset = s - sek;
    int slaveMsekOffset = (m - milli + 1000) % 1000;

    if (slaveSekOffset == 0) {
        slaveMsekOffset = slaveMsekOffset / 2;
    } else if (slaveSekOffset == 1) {
        slaveMsekOffset = (slaveMsekOffset / 2) + 500;
    } else {
        printf("Error!!!%d\n");

        return;
    }

    //offset reg berechnen
    unsigned char off1 = (slaveMsekOffset >> 8) & 0xFF;
    unsigned char off2 = slaveMsekOffset & 0xFF;

    resetAbgelaufen();

    while (getAbgelaufen() < 2);

    sendReg(0x11, 0xaa, 0x0, off1, off2);

    overwriteTimer(0);

    char c;
    while (c != 'a') {
        printf("Für clock ausgabe beliebige taste drücken: ");
        c = getchar();

        printTimeReg( & s, & m);

    }

}

void slaveCLK() {

    printf("Warten auf Offset vom Master\n", freq);

    resetAbgelaufen();

    int t = 0;
    while (t < 50 && (get_status_cc1200() == TX)) {
        while (getAbgelaufen() == 0);
        resetAbgelaufen();
        cc1200_cmd(SNOP);
        t++;
    }

    while (getAbgelaufen() < 10) {
        IdiotenScan();

        if (getDaten(0) == 0x11) {

            printf("Offset gefunden!!!\n");

            break;
        }
    }
    if (getDaten(0) != 0x11) {
        printf("FEHLER\n");
        return;
    }

    overwriteTimer(0);

    char c;
    while (c != 'a') {
        printf("Für clock ausgabe beliebige taste drücken:");
        c = getchar();

        unsigned char reg1, reg2, reg3;
        calcTimeRegMaster( & reg1, & reg2, & reg3);

        int sek = reg1;
        int mi = (reg2 << 8) + reg3;

        printf("Sek: %d   Millisek: %d\n", sek, mi);

    }

}