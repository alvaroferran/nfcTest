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

//SENDRECEIVE COMMANDS (http://www.gorferay.com/initialization-and-anticollision-iso-iec-14433-3/)
uint8_t REQA[]             = {0x00,0x04,0x02,0x26,0x07};
uint8_t ANTICOL_CL1_LVL1[] = {0x00,0x04,0x03,0x93,0x20,0x08};
uint8_t ANTICOL_CL1_LVL2[] = {0x00,0x04,0x03,0x95,0x20,0x08};
uint8_t ANTICOL_CL1_LVL3[] = {0x00,0x04,0x03,0x97,0x20,0x08};
uint8_t SELECT_CL1_LVL1[]  = {0x00,0x04,0x08,0x93,0x70,0,0,0,0,0,0x28};
uint8_t SELECT_CL1_LVL2[]  = {0x00,0x04,0x08,0x95,0x70,0,0,0,0,0,0x28};
uint8_t SELECT_CL1_LVL3[]  = {0x00,0x04,0x08,0x97,0x70,0,0,0,0,0,0x28};
uint8_t UID[10]={0};
// uint8_t READ_A5[]          = {0x00,0x04,0x03,0x30,0x0C,0x28};

uint8_t bufferLength=0;
uint8_t bufferCode;
enum bufferCodes{IDN_BUFF=0,SETPROTOCOL_BUFF,SENDRECEIVE_BUFF,TAGID_BUFF};
uint8_t IDNBuff[256],setProtocolBuff[256],sendReceiveBuff[256];
uint8_t *buffer[]={IDNBuff, setProtocolBuff,sendReceiveBuff};



void enterReadyState(){
    irqIn=1;
    wait_us(200);  //100us min
    irqIn=0;
    wait_us(20);   //10us min
    irqIn=1;
    wait_ms(20);   //ready in <10ms
}


void writeIDN(){
    cs=0;               //select chip (active low)
    spi.write(0x00);    //control byte -> write
    spi.write(0x01);    //idn command
    spi.write(0x00);    //length of data sent
    cs=1;               //deselect (needed by datasheet)
    bufferCode = IDN_BUFF;
}

void setProtocol(){
    cs=0;
    spi.write(0x00);    //control byte -> write
    spi.write(0x02);    //set protocol command
    spi.write(0x02);    //length of data sent
    spi.write(0x02);    //set ISO 14443-A
    spi.write(0x00);    //set  106 Kbps transmission and reception rates
    cs=1;
    bufferCode = SETPROTOCOL_BUFF;
}

void sendReceive(uint8_t sendBuffer[]){ //68:6f:6c:61
    wait_ms(10);
    cs=0;
    for(int i=0; i<sendBuffer[2]+3; i++) spi.write(sendBuffer[i]);
    cs=1;
    bufferCode = SENDRECEIVE_BUFF;
    wait_ms(10);
}

void readNFC(){
    memset(buffer[bufferCode],0, sizeof(buffer[bufferCode])); //fill buffer with '0'
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
    // myled = !myled;
}

void printResults(){
    switch (bufferCode) {
        case IDN_BUFF:
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
            for(int i = 0; i <bufferLength; i++)     pc.printf("%02X ",buffer[bufferCode][i]);
            break;
    }
}



void activateTag(){
    sendReceive(REQA);
    sendReceive(ANTICOL_CL1_LVL1);
    uint8_t cascadeByte1=sendReceiveBuff[2];
    for(int i=2; i<7; i++) SELECT_CL1_LVL1[i+3]=sendReceiveBuff[i];
    uint8_t uidSize=4;
    for(int i=0; i<4; i++) UID[i]=sendReceiveBuff[i+2];
    sendReceive(SELECT_CL1_LVL1);
    if(cascadeByte1==0x88){
        sendReceive(ANTICOL_CL1_LVL2);
        uint8_t cascadeByte2=sendReceiveBuff[2];
        for(int i=2; i<7; i++) SELECT_CL1_LVL2[i+3]=sendReceiveBuff[i];
        uidSize=7;
        for(int i=0; i<4; i++) UID[i]=UID[i+1];
        for(int i=0; i<4; i++) UID[i+3]=sendReceiveBuff[i+2];
        sendReceive(SELECT_CL1_LVL2);
        if(cascadeByte2==0x88){
            sendReceive(ANTICOL_CL1_LVL3);
            for(int i=2; i<7; i++) SELECT_CL1_LVL3[i+3]=sendReceiveBuff[i];
            uidSize=10;
            for(int i=0; i<4; i++) UID[i+3]=UID[i+4];
            for(int i=0; i<4; i++) UID[i+6]=sendReceiveBuff[i+2];
            sendReceive(SELECT_CL1_LVL3);
        }
    }
    pc.printf("\n\nUID is: ");  for(int i=0; i<uidSize; i++) pc.printf("%02X ",UID[i]);
}




int main() {
    pc.baud(9600);
    spi.format(8,3);        // Setup the spi for 8 bit data, high steady state clock,
    spi.frequency(1000000); // second edge capture, with a 1MHz clock rate
    cs=1;
    myled=0;

    enterReadyState();      //Enter NFC ready state

    irqOut.fall(&readNFC);  //read when interrupted

    // pc.printf("\n\nWriteIDN");
    // writeIDN();
    // printResults();

    // pc.printf("\n\nSetProtocol");
    setProtocol();
    // printResults();

    activateTag();


}
