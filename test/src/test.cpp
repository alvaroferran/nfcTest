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

int main() {
    double a=0.05;

    //Enter ready mode
    irqIn=1;
    wait_us(200);  //100 min
    irqIn=0;
    wait_us(20);   //10 min
    irqIn=1;


    cs=1;//Deselect cs;



    // Setup the spi for 8 bit data, high steady state clock,
    // second edge capture, with a 1MHz clock rate
    spi.format(8,3);
    spi.frequency(1000000);


    uint8_t rxBuffer[256];
    memset(rxBuffer, 0 , sizeof(rxBuffer)); //Fill with 0

    //write
    cs=0;               //select chip (active low)
    spi.write(0x00);    //control byte -> write
    spi.write(0x01);    //idn command
    spi.write(0x00);    //size of data sent
    cs=1;               //deselect (needed by datasheet)

    //wait until ready to read
    do{
        pc.printf("Ready to read\n");

        //read
        cs=0;
        spi.write(0x02);    //control byte -> read
        rxBuffer[0] = spi.write(0x00);  //result code
        int len = 0;
        if(rxBuffer[0] != 0x55) {
            len = rxBuffer[1] = spi.write(0x00);
            for(int i = 0; i < len && i < 256; i++) {
                rxBuffer[i+2] = spi.write(0x00);
            }
            len += 2;
        } else {
            len = 1;
        }
        cs=1;

        pc.printf("Reponse: 0x");
        for(int i = 0; i <len && i< 256; i++)
            pc.printf("%02x",rxBuffer[i]);
        pc.printf("\nResult code : 0x%02x",rxBuffer[0]);
        pc.printf("\nLength : 0x%02x (%d) \n",rxBuffer[1],len);
        pc.printf("Device ID: 0x");
        for(int i = 2; i <len-2; i++)
            pc.printf("%02x",rxBuffer[i]);
        pc.printf("\nCRC of ROM: 0x");
        for(int i = len-2; i <len; i++)
            pc.printf("%02x",rxBuffer[i]);

    }while(irqOut.read()!=0);




    while(1) {

    }
}
