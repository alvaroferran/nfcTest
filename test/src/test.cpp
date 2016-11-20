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
DigitalOut led1(PB_10),led2(PB_4);
DigitalIn b1(PC_13);

Serial pc(SERIAL_TX, SERIAL_RX);

//SENDRECEIVE COMMANDS (http://www.gorferay.com/initialization-and-anticollision-iso-iec-14433-3/)
uint8_t REQA[]             = {0x00,0x04,0x02,0x26,0x07};
uint8_t ANTICOL_CL1_LVL1[] = {0x00,0x04,0x03,0x93,0x20,0x08};
uint8_t ANTICOL_CL1_LVL2[] = {0x00,0x04,0x03,0x95,0x20,0x08};
uint8_t ANTICOL_CL1_LVL3[] = {0x00,0x04,0x03,0x97,0x20,0x08};
uint8_t SELECT_CL1_LVL1[]  = {0x00,0x04,0x08,0x93,0x70,0,0,0,0,0,0x28};
uint8_t SELECT_CL1_LVL2[]  = {0x00,0x04,0x08,0x95,0x70,0,0,0,0,0,0x28};
uint8_t SELECT_CL1_LVL3[]  = {0x00,0x04,0x08,0x97,0x70,0,0,0,0,0,0x28};
uint8_t READMEM1[]         = {0x00,0x04,0x03,0x30,0x04,0x28};
uint8_t READMEM2[]         = {0x00,0x04,0x03,0x30,0x08,0x28};
uint8_t READMEM3[]         = {0x00,0x04,0x03,0x30,0x0C,0x28};
uint8_t READMEM4[]         = {0x00,0x04,0x03,0x30,0x0F,0x28};
uint8_t UID[10]={0};
uint8_t TAG_DETECT[]       = {0x00,0x07,0x0E,0x02,0x21,0x00,0x79,0x01,0x18,0x00,0x01,0x60,0x60,0x7F,0xFE,0x3F,0x01};
uint8_t bufferLength=0;
uint8_t bufferCode;
enum bufferCodes{IDN_BUFF=0,SETPROTOCOL_BUFF,SENDRECEIVE_BUFF,IDLE_BUFF};
uint8_t IDNBuff[256],setProtocolBuff[256],sendReceiveBuff[256],idleBuff[256];
uint8_t *buffer[]={IDNBuff,setProtocolBuff,sendReceiveBuff,idleBuff};
uint8_t msg[48];
uint8_t tagContent[48]={0};
bool tagDetected=false, irqInLow=false;
uint8_t wakeUpEvent=0;


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

void setIdle(){
    sendReceive(TAG_DETECT);
    bufferCode = IDLE_BUFF;
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
    // pc.printf("\n\nUID is: ");  for(int i=0; i<uidSize; i++) pc.printf("%02X ",UID[i]);
}


void readTag(){
    memset(msg,0, sizeof(msg[48]));
    uint8_t msgSize=16; //as per MIFARE ULTRALIGHT definition
    uint8_t lineOffset=9, msgOffset=2;
    sendReceive(READMEM1);
    for(int i =0; i <msgSize-lineOffset; i++) msg[i]=buffer[bufferCode][i+msgOffset+lineOffset];    //Copy first portion of memory
    sendReceive(READMEM2);
    for(int i =0; i <msgSize; i++) msg[i+msgSize-lineOffset]=buffer[bufferCode][i+msgOffset];       //Copy second portion of memory
    sendReceive(READMEM3);
    for(int i =0; i <msgSize; i++) msg[i+msgSize*2-lineOffset]=buffer[bufferCode][i+msgOffset];     //Copy third portion of memory
    sendReceive(READMEM4);
    for(int i =0; i <msgSize; i++) msg[i+msgSize*3-lineOffset]=buffer[bufferCode][i+msgOffset];     //Copy fourth portion of memory
    uint8_t indexEOF=48;
    for(int i = sizeof(msg)/sizeof(uint8_t); i >0; i--) if(msg[i]==0xFE) indexEOF=i;                //Find first end of msg
    for(int i = 0; i <indexEOF; i++) tagContent[i]=msg[i];                                        //Print tag message
    // for(int i = 0; i <indexEOF; i++) pc.printf(" %c",msg[i]);                                        //Print tag message
    // pc.printf("\n");

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
    if(buffer[bufferCode][0]==0x00 && bufferCode==IDLE_BUFF){
        if(buffer[bufferCode][2]==0x01){
            wakeUpEvent=1;
            // pc.printf("\nTimeout");
        }
        if(buffer[bufferCode][2]==0x02){
            wakeUpEvent=2;
            // pc.printf("\nTag detected");
            // tagDetected=true;
        }
        if(buffer[bufferCode][2]==0x08){
            wakeUpEvent=3;
            // pc.printf("\nLow pulse on IRQ_IN pin");
            // irqInLow=true;
        }
        if(buffer[bufferCode][2]==0x10){
            wakeUpEvent=4;
            // pc.printf("\nLow pulse on SPI_SS pin");
        }
    }
    // else{
    //     wakeUpEvent=0;
    // }
    // myled = !myled;
}

void printResults(){
    switch (bufferCode) {
        case IDN_BUFF:
            pc.printf("\nResponse: 0x");
            for(int i = 0; i <bufferLength; i++)     pc.printf("%02X",buffer[bufferCode][i]);
            pc.printf("\nResult code : 0x%02X",buffer[bufferCode][0]);
            pc.printf("\nSize: Data: 0x%02X (%d) Total: 0x%02X (%d) \n",buffer[bufferCode][1],buffer[bufferCode][1],bufferLength,bufferLength);
            pc.printf("Device ID: 0x");
            for(int i = 2; i <bufferLength-2; i++)   pc.printf("%02X",buffer[bufferCode][i]);
            pc.printf("\nCRC of ROM: 0x");
            for(int i = bufferLength-2; i <bufferLength; i++) pc.printf("%02X",buffer[bufferCode][i]);
            break;

        default:
            pc.printf("\nResponse: 0X");
            for(int i = 0; i <bufferLength; i++)     pc.printf(" %02X",buffer[bufferCode][i]);
            break;
    }
}


void tagCalibration(){
    uint8_t TAG_CALIBRATION[] = {0x00,0x07,0x0E,0x03,0xA1,0x00,0xF8,0x01,0x18,0x00,0x01,0x60,0x60,0x00,0xFE,0x3F,0x01};
    bool calibrationDone=false;
    uint8_t DacDataRef=0;
    wakeUpEvent=0;

    pc.printf("\nStarting calibration" );
    // for(int i=0xFF; i>0x00; i--){pc.printf("%x\n",i );}
    for(int i=0xFE; i>0x00; i--){
        wait_ms(20);
        TAG_CALIBRATION[14]=i;
        //tagCalibration();
        sendReceive(TAG_CALIBRATION);
        bufferCode = IDLE_BUFF;
        while(1){   //wait until wakeup
            wait_ms(10);
            if(wakeUpEvent!=0){
                pc.printf("   %X\n",i );
                if(wakeUpEvent==2){ // if tag detected
                    calibrationDone=true;
                }
                break;
            }
        }
        if(calibrationDone){
            DacDataRef=i;
            pc.printf("\nDacDataL= %X, DacDataH= 0xFE\n", DacDataRef);
            break;
        }
    }

}


void processInfo(){
    pc.printf("%c\n",tagContent[0] );
    if(tagContent[0]==(uint8_t)'A'){
        led1=1;
    }
    if(tagContent[0]==(uint8_t)'B'){
        led2=1;
    }
}





int main() {
    pc.baud(115200);
    spi.format(8,3);        // Setup the spi for 8 bit data, high steady state clock,
    spi.frequency(1000000); // second edge capture, with a 1MHz clock rate
    cs=1;
    myled=0;

    irqOut.fall(&readNFC);  //get data from reader when interrupted

    enterReadyState();      //Enter NFC ready state

    // writeIDN();
    // printResults();


    // tagCalibration();



    setIdle();
    while(1){
        myled=0;
        led1=0;
        led2=0;
        if(wakeUpEvent==2){
            // myled=1;
            wakeUpEvent=0;
            setProtocol();          //Set tag's protocol
            activateTag();          //Activate tag
            readTag();              //Print contents of tag
            processInfo();
            setIdle();              //Wait for new tag detection
        }
        // wait_ms(100);
    }




}
