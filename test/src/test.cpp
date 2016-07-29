#include "mbed.h"

#define MOSI    PA_7
#define MISO    PA_6
#define SCK     PA_5
#define CS      PB_6
#define IRQ_IN  PA_9
#define IRQ_OUT PA_10

SPI spi(MOSI,MISO,SCK);
DigitalOut  cs(CS);
DigitalOut  irqIn(IRQ_IN);   //interrupt to CR95HF
DigitalIn   irqOut(IRQ_OUT); //interrupt from CR95HF

DigitalOut myled(PA_8);

Serial pc(SERIAL_TX, SERIAL_RX);

uint8_t rxBuffer[256];
int len = 0;

void writeIDN(){
    cs=0;               //select chip (active low)
    spi.write(0x00);    //control byte -> write
    spi.write(0x01);    //idn command
    spi.write(0x00);    //size of data sent
    cs=1;               //deselect (needed by datasheet)
}

void readNFC(){
    cs=0;
    spi.write(0x02);                            //control byte -> read
    rxBuffer[0] = spi.write(0x00);              //read result code
    if(rxBuffer[0] != 0x55) {                   //result code not echo command
        len = rxBuffer[1] = spi.write(0x00);    //read length of data
        for(int i = 0+2; i < len+2; i++) {        
            rxBuffer[i] = spi.write(0x00);      //read <length> bytes of data
        }
        len += 2;                               //length received + resultCode and length bytes
    } else {
        len = 1;
    }
    cs=1;
}


void printResults(){
    pc.printf("\nResponse: 0x");
    for(int i = 0; i <len; i++)     pc.printf("%02X",rxBuffer[i]);
    pc.printf("\nResult code : 0x%02X",rxBuffer[0]);
    pc.printf("\nLength of:  Data: 0x%02X (%d)    Total: 0x%02X (%d) \n",rxBuffer[1],rxBuffer[1],len,len);
    pc.printf("Device ID: 0x");
    for(int i = 2; i <len-2; i++)   pc.printf("%02X",rxBuffer[i]);
    pc.printf("\nCRC of ROM: 0x");
    for(int i = len-2; i <len; i++) pc.printf("%02X",rxBuffer[i]);
}

int main() {
    pc.baud(9600);
    spi.format(8,3);        // Setup the spi for 8 bit data, high steady state clock,
    spi.frequency(1000000); // second edge capture, with a 1MHz clock rate
    cs=1;//Deselect cs;

    memset(rxBuffer, 0 , sizeof(rxBuffer)); //Fill rxBuffer with 0


    //write
    writeIDN();

    //wait until ready to read
    while(1){
        if(irqOut.read()==0){
            readNFC();
            break;
        }
    }

    printResults();

    while(1) { }
}
