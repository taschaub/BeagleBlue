#include <stdio.h>

#include "register_addr.h"

#include <SPIv1.h>

#include <stdlib.h>

#include <time.h>

#include <signal.h>

#include <sys/time.h>

#include <unistd.h>

#include <string.h>

typedef struct {
    int addr;
    int val;
}
registerSetting_t;

#include "set.h"

int freq0, freq1, freq2, val;
int rssi;
int ref = -3;
int j = 0;
int PaketNr = 0;
int adr;
unsigned char daten[6];
int sec, msec; //sekunden und millisekunden
unsigned char reg1, reg2, reg3;
int seqno = 0;
int seq;
int abgelaufen;
int secOffset;
int msecOffset;
int poll;
int flag = 0;
int x;
int mode;
void clockSynch();
int breakVal;
//timer var
struct sigaction sa;
struct itimerval timer; //timer

//global
int clockMatched;

void change_register() {

    int i = 0;
    int size = sizeof(preferredSettings) / sizeof(registerSetting_t);
    for (i = 0; i < size; i++) {
        cc1200_reg_write(preferredSettings[i].addr, preferredSettings[i].val);
    }
}

void getTimeinMs(unsigned char * r1, unsigned char * r2, unsigned char * r3) {
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, & spec);

    sec = spec.tv_sec;
    msec = spec.tv_nsec / 1.0e6; // Convert nanoseconds to milliseconds

    * r1 = sec & 0xFF;
    * r2 = (msec >> 8) & 0xFF;
    * r3 = msec & 0xFF;
}

void printTimeReg(int * s, int * m) {
    struct timespec spec;

    unsigned char r1, r2, r3;

    clock_gettime(CLOCK_REALTIME, & spec);

    sec = spec.tv_sec;
    msec = spec.tv_nsec / 1.0e6; // Convert nanoseconds to milliseconds

    r1 = sec & 0xFF;
    r2 = (msec >> 8) & 0xFF;
    r3 = msec & 0xFF;

    int sek = r1;
    int mi = (r2 << 8) + r3;

    printf("Sek: %d   Millisek: %d\n", sek, mi);

    * s = sek;
    * m = mi;
}

void getTime() {
    struct timeval curr;
    gettimeofday( & curr, NULL);
    msec = curr.tv_usec / 1000;
    sec = curr.tv_sec;
}

void printTime() {
    //getTimeinMs(&reg1, &reg2, &reg3);
    getTime();
    printf("Time: %d,  ms %d\n", sec, msec);
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
    if (seq) {
        static int flip = 0;
        if (flip) {
            seqno = (seqno + 1) % 10;
            flip = 0;
        } else {
            flip = 1;
        }
    }

    abgelaufen++;
    // seqno = (seqno + 1) % 100;
}

void init_timer() {
    //install timer
    memset( & sa, 0, sizeof(sa));
    sa.sa_handler = & timer_handler;
    sigaction(SIGALRM, & sa, NULL);
    //first alarm
    struct timeval curr;
    gettimeofday( & curr, NULL);
    int usec = 100000 - (int) curr.tv_usec % 100000;

    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = usec;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 100000;
    setitimer(ITIMER_REAL, & timer, NULL);
}
//zeit bis zum nächsten timer Ablaufen in usec
//evtl noch zu verändern flag etc
void synchTimer(int ti) {
    timer.it_value.tv_usec = ti;
    timer.it_value.tv_sec = 0;
    timer.it_interval.tv_usec = 200000;
    timer.it_interval.tv_sec = 0;
    setitimer(ITIMER_REAL, & timer, NULL);

}

void overwriteTimer(int interval) {

    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = interval;
    timer.it_interval.tv_sec = 0;
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
    if (get_status_cc1200() != IDLE && get_status_cc1200() != RX) setIDLE();
    while (get_status_cc1200() != RX) {
        cc1200_cmd(SNOP);
        cc1200_cmd(SRX);
    }
}

void setTX() {
    cc1200_cmd(SNOP);
    if (get_status_cc1200() != IDLE && get_status_cc1200() != TX) setIDLE();
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
    if ( * f_klein > 899) * f_klein = 880;
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
    cc1200_reg_write(0x3F, (unsigned char) id); //id senden

    cc1200_reg_write(0x3F, (unsigned char) addr); //adresse senden

    //clock senden
    unsigned char reg1, reg2, reg3, s, ms;
    getTimeinMs( & reg1, & reg2, & reg3);
    cc1200_reg_write(0x3F, reg1);
    cc1200_reg_write(0x3F, reg2);
    cc1200_reg_write(0x3F, reg3);
    printf("clock: %d.%d.%d\n", reg1, reg2, reg3);
}
////WICHTIG!!!
//nach send immer auf nächsten time-slot warten
void sendInquiry(int id, int adresse) {
    inqPack(id, adresse);
    setTX();
    val = 1;
    int count = 0;
    while ((val != 0 || get_status_cc1200() == TX) && getAbgelaufen() == 0) { //warten bis daten gesendet sind

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

void sendData(int id, int addr, int data) {
    cc1200_reg_write(0x3F, (unsigned char) id); //id senden

    cc1200_reg_write(0x3F, (unsigned char) addr); //adresse senden
    cc1200_reg_write(0x3F, (unsigned char) data);
    cc1200_reg_write(0x3F, (unsigned char) data);
    cc1200_reg_write(0x3F, (unsigned char) data);

    setTX();
    val = 1;
    int count = 0;
    while ((val != 0 || get_status_cc1200() == TX) && getAbgelaufen() == 0) { //warten bis daten gesendet sind

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

//retuns 1 if packet was received
int InquiryScan() {
    int received = 0;
    setRX();
    // RSSI valid?
    RSSIvalid();

    //Rssi ueberpruefen:
    cc1200_reg_read(RSSI1, & rssi);
    cc1200_cmd(SNOP);

    while ((signed char) rssi < ref && getAbgelaufen() < 1) {
        cc1200_reg_read(RSSI1, & rssi);
    }
    cc1200_cmd(SNOP);

    if (getAbgelaufen() != 0) {
        resetAbgelaufen();
        return 0;
    }
    if ((signed char) rssi > ref) {
        //RX auf diesem Channel
        int len;
        int size = 0;
        cc1200_reg_read(PKT_LEN, & len);
        cc1200_reg_read(NUM_RXBYTES, & size);
        while (size < len + 2) {

            cc1200_reg_read(NUM_RXBYTES, & size);

            if (getAbgelaufen() > 0) {
                printf("Keine Pakete erhalten.\n");

                resetAbgelaufen();
                return 0;
            }
        }
        while (size > 0 && fifo_stat()) {
            setRX();
            cc1200_reg_read(NUM_RXBYTES, & size);
            cc1200_reg_read(NUM_RXBYTES, & size);
            cc1200_reg_read(0x3F, & val);
            printf("%d\n", (unsigned char) val);
            daten[j] = val;
            size = size - 1;
            j++;
            if (j == 7) {
                cc1200_cmd(SNOP);
                printf("Status:%d  ", get_status_cc1200());
                j = 0;
                PaketNr++;
                printf("PaketNr:%d\n", PaketNr);
                //clock anpassen, nur slave
                received = 1;
                printTime();
            }
        }
        setTX();
        while (getAbgelaufen() == 0);
        resetAbgelaufen();
        return received; //bis hier
    }
    // in IDLE wechseln
    setIDLE();
}

void fastScan() {
    setRX();
    // RSSI valid?
    RSSIvalid();
    //Rssi ueberpruefen:
    cc1200_reg_read(RSSI1, & rssi);

    cc1200_reg_read(RSSI1, & rssi);

    cc1200_cmd(SNOP);

    int size = 0;
    cc1200_reg_read(NUM_RXBYTES, & size);
    int len;
    cc1200_reg_read(PKT_LEN, & len);

    if ((signed char) rssi > ref || size > len) {

        while (size < len + 2) {

            cc1200_reg_read(NUM_RXBYTES, & size);

            if (getAbgelaufen() > 1) {
                printf("Keine Pakete erhalten.\n");
                breakVal = 1;

                resetAbgelaufen();
                return;
            }
        }
        while (size > 0 && fifo_stat()) {
            setRX();
            cc1200_reg_read(NUM_RXBYTES, & size);
            cc1200_reg_read(NUM_RXBYTES, & size);
            cc1200_reg_read(0x3F, & val);
            printf("%d\n", (unsigned char) val);
            daten[j] = val;
            size = size - 1;
            j++;
            if (j == 7) {
                cc1200_cmd(SNOP);

                printf("Status:%d  ", get_status_cc1200());
                j = 0;
                PaketNr++;
                printf("PaketNr:%d\n", PaketNr);
                breakVal = 1;
                printTime();

            }
        }

    }

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
//geändert
void clockSynch() {

    getTime();
    int secSlave = (unsigned int) getDaten(2);
    int msecSlave = getDaten(3) * 100 + getDaten(4); //((unsigned int)getDaten(3)<<8)+getDaten(4);
    // Offsets berechen:
    //timer intervalle  anpassen

    synchTimer((300 - ((msecSlave + 98) % 300)) * 1000);
    secOffset = (-1) * (secSlave - sec);
    msecOffset = msec - msecSlave - 98;
    printf("clock msec= %d   offset= %d  ", msec, msecOffset);
    clockMatched = 1;
}

void clockMatch() {
    unsigned char secReg, msecReg1, msecReg2;
    getTimeinMs( & secReg, & msecReg1, & msecReg2);

    int secSlave = getDaten(2);
    int msecSlave = (getDaten(3) << 8) + getDaten(4);

    // Offsets berechen:
    secOffset = secSlave - secReg;
    msecOffset = msecSlave - (msecReg1 << 8) + msecReg2;

    clockMatched = 1;
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

void flushDaten() {
    int i;
    for (i = 0; i < 6; i++) daten[i] = 0;
}

void calcOffset() {
    unsigned char secReg, msecReg1, msecReg2;
    getTimeinMs( & secReg, & msecReg1, & msecReg2);

    int secMaster = getDaten(2);
    int msecMaster = (getDaten(3) << 8) + getDaten(4);

    // Offsets berechen:
    secOffset = secMaster - secReg;
    msecOffset = msecMaster - (msecReg1 << 8) - msecReg2;

}

void calcTimeRegMaster(unsigned char * reg1, unsigned char * reg2, unsigned char * reg3) {
    unsigned char secReg, msecReg1, msecReg2;
    getTimeinMs( & secReg, & msecReg1, & msecReg2);

    static int i = 1;
    int off;
    if (i) {
        off = (getDaten(3) << 8) + getDaten(4) + 800;
        printf("Offset ist:%d\n", off);
        i = 0;
    }

    int secMaster = secReg; //+ secOffset;
    int msecMaster = ((msecReg1 << 8) + msecReg2 + off);

    if (msecMaster > 999) {
        secMaster++;
        msecMaster = msecMaster - 1000;
    }

    * reg1 = secMaster & 0xFF;
    * reg2 = (msecMaster >> 8) & 0xFF;
    * reg3 = msecMaster & 0xFF;

}

void sendtimeMaster(int id, int addr) {
    cc1200_reg_write(0x3F, (unsigned char) id); //id senden

    cc1200_reg_write(0x3F, (unsigned char) addr); //adresse senden

    //clock senden
    unsigned char reg1, reg2, reg3, s, ms;
    calcTimeRegMaster( & reg1, & reg2, & reg3);
    //printTime();
    cc1200_reg_write(0x3F, reg1);
    cc1200_reg_write(0x3F, reg2);
    cc1200_reg_write(0x3F, reg3);
    printf("Masterclock: %d.%d.%d\n", reg1, reg2, reg3);

    printf("Meine Zeit: \n");
    printTime();

    setTX();
    val = 1;
    int count = 0;
    while ((val != 0 || get_status_cc1200() == TX)) { //warten bis daten gesendet sind

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

void sendReg(int id, int addr, unsigned char reg1, unsigned char reg2, unsigned char reg3) {
    cc1200_reg_write(0x3F, (unsigned char) id); //id senden

    cc1200_reg_write(0x3F, (unsigned char) addr); //adresse senden

    cc1200_reg_write(0x3F, reg1);
    cc1200_reg_write(0x3F, reg2);
    cc1200_reg_write(0x3F, reg3);
    printf("Masterclock: %d.%d.%d\n", reg1, reg2, reg3);

    printf("Meine Zeit: \n");
    printTime();

    setTX();
    val = 1;
    int count = 0;
    while ((val != 0 || get_status_cc1200() == TX)) { //warten bis daten gesendet sind

        cc1200_reg_read(NUM_TXBYTES, & val);
        cc1200_cmd(SNOP);
        count++;

        //Fehler, falls noch nicht alle bytes eines Pakets gesendet wurden,
        //TX Modus trotzdem beendet!
        if ((count > 50) && (get_status_cc1200() != TX) && (val != 0)) {
            // printf("%d\n", val );
            // printf("%s\n", get_status_cc1200_str());
            cc1200_reg_read(NUM_TXBYTES, & val);
            printf("\n\n\n FEHLER Status: %s   val=%d\n\n\n", get_status_cc1200_str(), val);
            setTX();
            cc1200_cmd(SNOP);
            count = 0;
        }
    }

}

void IdiotenScan() {
    setRX();
    // RSSI valid?
    RSSIvalid();
    //Rssi ueberpruefen:
    cc1200_reg_read(RSSI1, & rssi);

    cc1200_reg_read(RSSI1, & rssi);

    cc1200_cmd(SNOP);

    int size = 0;
    cc1200_reg_read(NUM_RXBYTES, & size);
    int len;
    cc1200_reg_read(PKT_LEN, & len);

    if ((signed char) rssi > ref || size > len) {

        while (size < len + 2) {

            cc1200_reg_read(NUM_RXBYTES, & size);

            if (getAbgelaufen() > 1) {
                printf("Keine Pakete erhalten.\n");
                breakVal = 1;

                resetAbgelaufen();
                return;
            }
        }
        while (size > 0 && fifo_stat()) {
            setRX();
            cc1200_reg_read(NUM_RXBYTES, & size);
            cc1200_reg_read(NUM_RXBYTES, & size);
            cc1200_reg_read(0x3F, & val);
            printf("%d\n", (unsigned char) val);
            daten[j] = val;
            size = size - 1;
            j++;
            if (j == 7) {
                cc1200_cmd(SNOP);

                printf("Status:%d  ", get_status_cc1200());
                j = 0;
                PaketNr++;
                printf("PaketNr:%d\n", PaketNr);
                breakVal = 1;
                printTime();

            }
        }

    }

    setIDLE();
}