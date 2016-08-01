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
InterruptIn irqOut(IRQ_OUT); //interrupt from CR95HF

DigitalOut myled(PA_8);

Serial pc(SERIAL_TX, SERIAL_RX);


uint8_t bufferLength=0;
uint8_t bufferCode;
enum bufferCodes{IDN_BUFFER=0,SETPROTOCOL_BUFFER,SENDRECEIVE_BUFFER};
uint8_t IDNBuff[256],setProtocolBuff[256],sendReceiveBuff[256];
uint8_t *buffer[]={IDNBuff, setProtocolBuff,sendReceiveBuff};



void enterReadyState(){
    irqIn=1;
    wait_us(200);  //100 min
    irqIn=0;
    wait_us(20);   //10 min
    irqIn=1;
    wait_ms(20);   //ready in <10
}

void writeIDN(){
    cs=0;               //select chip (active low)
    spi.write(0x00);    //control byte -> write
    spi.write(0x01);    //idn command
    spi.write(0x00);    //length of data sent
    cs=1;               //deselect (needed by datasheet)
    bufferCode = IDN_BUFFER;
}

void setProtocol(){
    cs=0;
    spi.write(0x00);    //control byte -> write
    spi.write(0x02);    //set protocol command
    spi.write(0x02);    //length of data sent
    spi.write(0x02);    //set ISO 14443-A
    spi.write(0x00);    //set  106 Kbps transmission and reception rates
    cs=1;
    bufferCode = SETPROTOCOL_BUFFER;
}

void sendReceive(){
    cs=0;
    spi.write(0x00);    //control byte -> write
    spi.write(0x04);    //set SendReceive command
    spi.write(0x02);    //length of data sent
    spi.write(0x26);    //REQA
    spi.write(0x07);    //REQA
    cs=1;
    bufferCode = SENDRECEIVE_BUFFER;
}

void readNFC(){
    cs=0;
    spi.write(0x02);                                    //control byte -> read
    buffer[bufferCode][0] = spi.write(0x00);            //read result code
    if(buffer[bufferCode][0] != 0x55) {                 //result code not echo command
        bufferLength = buffer[bufferCode][1] = spi.write(0x00);  //read length of data
        for(int i = 0+2; i < bufferLength+2; i++) {
            buffer[bufferCode][i] = spi.write(0x00);    //read <length> bytes of data
        }
        bufferLength += 2;                              //length received + resultCode and length bytes
    } else {
        bufferLength = 1;
    }
    cs=1;
    myled = !myled;
}

void printResults(){
    switch (bufferCode) {
        case IDN_BUFFER:
            pc.printf("\nResponse: 0x");
            for(int i = 0; i <bufferLength; i++)     pc.printf("%02X",buffer[bufferCode][i]);
            pc.printf("\nResult code : 0x%02X",buffer[bufferCode][0]);
            pc.printf("\nLength of:  Data: 0x%02X (%d)    Total: 0x%02X (%d) \n",buffer[bufferCode][1],buffer[bufferCode][1],bufferLength,bufferLength);
            pc.printf("Device ID: 0x");
            for(int i = 2; i <bufferLength-2; i++)   pc.printf("%02X",buffer[bufferCode][i]);
            pc.printf("\nCRC of ROM: 0x");
            for(int i = bufferLength-2; i <bufferLength; i++) pc.printf("%02X",buffer[bufferCode][i]);
            break;

        default:
            pc.printf("\nResponse: 0X");
            for(int i = 0; i <bufferLength; i++)     pc.printf("%02X",buffer[bufferCode][i]);
            break;

    }
}






int main() {
    pc.baud(9600);
    spi.format(8,3);        // Setup the spi for 8 bit data, high steady state clock,
    spi.frequency(1000000); // second edge capture, with a 1MHz clock rate
    cs=1;
    myled=0;

    enterReadyState();      //Enter NFC ready state

    irqOut.fall(&readNFC);  //read when interrupted

    pc.printf("\n\nWriteIDN");
    writeIDN();
    printResults();

    pc.printf("\n\nSetProtocol");
    setProtocol();
    printResults();

    // pc.printf("\n\nSendReceive");
    // sendReceive();
    // printResults();

}
