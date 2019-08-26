#include <stdio.h>

#include "register_addr.h"

#include <SPIv1.h>

#include <stdlib.h>

#include <time.h>

#include <signal.h>

#include <sys/time.h>

#include <unistd.h>

#include <string.h>

/*Änderungen: gettc
	-setRX/TX check if IDLE before setting in RX/TX mode

*/
typedef struct {
    int addr;
    int val;
}
registerSetting_t;

#include "set.h"

int freq0, freq1, freq2, val;
// long long freq;
int rssi;
int ref = 0;
int j = 0;
int PaketNr = 0;
int adr;
int daten[6];
int sec, msec; //sekunden und millisekunden
int reg1, reg2, reg3;
int abgelaufen;
int secOffset;
int msecOffset;
int poll;
int flag = 0;

//timer var
struct sigaction sa;
struct itimerval timer; //timer

void change_register() {
    int i = 0;
    int size = sizeof(preferredSettings) / sizeof(registerSetting_t);
    for (i = 0; i < size; i++) {
        cc1200_reg_write(preferredSettings[i].addr, preferredSettings[i].val);
    }
}

void getTimeinMs(int * r1, int * r2, int * r3) {

    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, & spec);

    sec = spec.tv_sec;
    msec = spec.tv_nsec / 1.0e6; // Convert nanoseconds to milliseconds

    * r1 = sec & 0xFF;
    * r2 = (msec >> 8) & 0xFF;
    * r3 = msec & 0xFF;
}

void getTime() {
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, & spec);

    sec = spec.tv_sec;
    msec = spec.tv_nsec / 1.0e6;
}

void printTime() {
    getTimeinMs( & reg1, & reg2, & reg3);
    printf("Time: %d.%d\n", sec, msec);
}

int getSec(int hex) {

}

int getDaten(int index) {
    return daten[index];
}

int getAbgelaufen() {
    return abgelaufen;
}

void resetAbgelaufen() {
    abgelaufen = 0;
}

//timer handler
void timer_handler(int signum) {
    if (flag == 0) {
        flag = 1;
        return;
    }
    abgelaufen++;
}

void init_timer() {
    //install timer
    memset( & sa, 0, sizeof(sa));
    sa.sa_handler = & timer_handler;
    sigaction(SIGALRM, & sa, NULL);
    //first alarm
    struct timeval curr;
    gettimeofday( & curr, NULL);
    int usec = 200000 - (int) curr.tv_usec % 200000;

    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = usec;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 200000;
    setitimer(ITIMER_REAL, & timer, NULL);
}

//zeit bis zum nächsten timer Ablaufen in usec
//evtl noch zu verändern flag etc
void synchTimer(int ti) {
    timer.it_value.tv_usec = ti;
    timer.it_value.tv_sec = 0;
    setitimer(ITIMER_REAL, & timer, NULL);

}

void overwriteTimer(int interval) {
    timer.it_interval.tv_usec = interval;
    setitimer(ITIMER_REAL, & timer, NULL);
}

int fifo_stat() {
    int val;
    cc1200_reg_read(NUM_RXBYTES, & val);
    return (val > 0);
}

void setIDLE() {
    cc1200_cmd(SIDLE);
    cc1200_cmd(SNOP);
    while (get_status_cc1200() != IDLE) {
        cc1200_cmd(SNOP);

        if (get_status_cc1200() == IDLE) {
            printf("Jetzt im IDLE Status...\n");
        }
    }
}

void setRX() {

    cc1200_cmd(SNOP);
    if (get_status_cc1200() != IDLE) setIDLE();
    while (get_status_cc1200() != RX) {
        cc1200_cmd(SNOP);
        cc1200_cmd(SRX);
    }
}

void setTX() {
    cc1200_cmd(SNOP);
    if (get_status_cc1200() != IDLE) setIDLE();
    cc1200_cmd(STX);
    cc1200_cmd(SNOP);
    while (get_status_cc1200() != TX) {
        cc1200_cmd(SNOP);
    }
}

void RSSIvalid() {
    int val = 0;
    while ((val & 0x1) == 0) {

        cc1200_reg_read(RSSI0, & val);

    }
}

void initialisieren() {
    if (spi_init()) {
        printf("ERROR: Initialization failed\n");
        exit(-1);
    }
    cc1200_cmd(SRES);
    change_register();

}

void naechsteFreq(int * f_klein) {

    * f_klein += 1;
    int f = * f_klein * 1000000;
    long long freq = (long long) f * 4 * 65536 / (40000000);
    freq2 = (freq >> 16);
    freq1 = (freq >> 8) & 0xFF;
    freq0 = freq & 0xFF;

    cc1200_reg_write(FREQ2, freq2);
    cc1200_reg_write(FREQ1, freq1);
    cc1200_reg_write(FREQ0, freq0);

}

void setFreq(int f) {
    f *= 1000000;
    int freq0, freq1, freq2;
    long long freq = (long long) f * 4 * 65536 / (40000000);
    freq2 = (freq >> 16);
    freq1 = (freq >> 8) & 0xFF;
    freq0 = freq & 0xFF;

    cc1200_reg_write(FREQ2, freq2);
    cc1200_reg_write(FREQ1, freq1);
    cc1200_reg_write(FREQ0, freq0);

}

void RSSIauslesen(int * rssi) {
    static int val = 0;
    while ((val & 0x1) == 0) {
        cc1200_reg_read(RSSI0, & val);
    }

    cc1200_reg_read(RSSI1, rssi);
}

void DatenSenden(int f) {

    setIDLE();

    int i = 0;
    printf("Sende: \n");
    for (i = 0; i < 5; i++) {

        cc1200_reg_write(0x3F, f);
        printf("%d\n", f);

    }

    setTX();
    val = 1;
    while (val != 0 || get_status_cc1200() == TX) { //warten bis daten gesendet sind

        cc1200_reg_read(NUM_TXBYTES, & val);
        cc1200_cmd(SNOP);
    }

}

void inqPack(int id, int addr) {
    cc1200_reg_write(0x3F, id); //id senden

    cc1200_reg_write(0x3F, addr);
    //adresse senden
    //clock senden
    int reg1, reg2, reg3, s, ms;
    getTimeinMs( & reg1, & reg2, & reg3);

    cc1200_reg_write(0x3F, reg1);
    cc1200_reg_write(0x3F, reg2);
    cc1200_reg_write(0x3F, reg3);
    printf("clock: %d.%d.%d\n", reg1, reg2, reg3);
}

void sendInquiry(int id, int adresse) {

    setIDLE();
    inqPack(id, adresse);
    setTX();
    val = 1;
    int count = 0;
    while (val != 0 || get_status_cc1200() == TX) { //warten bis daten gesendet sind

        cc1200_reg_read(NUM_TXBYTES, & val);

        cc1200_cmd(SNOP);

        count++;

        //Fehler, falls noch nicht alle bytes eines Pakets gesendet wurden,
        //TX Modus trotzdem beendet!
        if ((count > 50) && (get_status_cc1200() != TX) && (val != 0)) {

            cc1200_reg_read(NUM_TXBYTES, & val);
            printf("\n\n\n FEHLER Status: %s   val=%d\n\n\n", get_status_cc1200_str(), val);
            setTX();
            cc1200_cmd(SNOP);
            count = 0;
        }

    }

}

void InquiryScan(int freq, int * breakVal) {

    setRX();
    // RSSI valid?
    RSSIvalid();
    //Rssi ueberpruefen:
    cc1200_reg_read(RSSI1, & rssi);

    while ((signed char) rssi < ref && getAbgelaufen() == 0) cc1200_reg_read(RSSI1, & rssi);
    if (getAbgelaufen != 0) {
        resetAbgelaufen();
        return;
    }
    if ((signed char) rssi > ref) {
        printf("Rssi gut auf Channel: %d\n", freq);
        printTime();

        int len;
        int size = 0;
        cc1200_reg_read(PKT_LEN, & len);
        cc1200_reg_read(NUM_RXBYTES, & size);

        while (size < len + 2) {

            cc1200_reg_read(NUM_RXBYTES, & size);

            if (getAbgelaufen() > 1) {
                printf("%d\n", getAbgelaufen());
                printf("Keine Pakete erhalten.\n");
                * breakVal = 1;
                return;

                break;
            }
        }

        while (size > 0) {

            setRX();

            if (fifo_stat()) {
                cc1200_reg_read(NUM_RXBYTES, & size);
                while (size > 0) {
                    cc1200_reg_read(NUM_RXBYTES, & size);
                    cc1200_reg_read(0x3F, & val);
                    printf("%d\n", val);
                    daten[j] = val;
                    size = size - 1;
                    j++;
                    if (j == 7) {
                        cc1200_cmd(SNOP);
                        printf("Status:%d  ", get_status_cc1200());
                        j = 0;
                        PaketNr++;
                        printf("PaketNr:%d\n", PaketNr);
                        printTime();
                        resetAbgelaufen();
                        * breakVal = 2;
                    }

                }

            }

        }

    }
    // in IDLE wechseln
    setIDLE();

}

//beide Funktionen müssen noch angepasst werden
int calcfreqSlave() {

    getTime();
    int milli = msec % 1000;
    return milli / 100;
}

int calcfreqMaster() {

    getTime();
    int milli = (msec + msecOffset) % 1000;
    if (milli < 0) milli = milli + 1000;
    return milli / 100;

}

void pollAbfrage(int adresse) {
    char option;
    printf("Daten von Beaglebone mit Adresse %x pollen? (Y/N)\n", adresse);

    scanf(" %c", & option);
    if (option == 'y' || option == 'Y') {
        printf("\n- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n");
        poll = 1;
    } else if (option == 'n' || option == 'N') {
        poll = 0;
    } else {
        printf("Invalid selection.\n");
        pollAbfrage(adresse);
    }
}