#include "dataLink.h"
#include <stdbool.h>
#include<errno.h>

struct termios oldtio,newtio;

applicationLayer app;
linkLayer ll;
int count = 0;
bool alarmFlag = FALSE;

void alarmHandler()
{
    printf("Got an alarm\n");
    count++;
    alarmFlag = TRUE;
    alarm(ll.timeout);
    siginterrupt(SIGALRM, 1);
    if(count > ll.numTransmissions)
    {
        printf("Max tries reached, aborting.\n");
        exit(1);
    }
}

int stuffing(const unsigned char *info, size_t size, unsigned char *stuffed_info)
{
    int i, j;
    for (i = 0, j = 0; i < size; i++, j++)
    {
        switch (info[i])
        {
        case FLAG:
            stuffed_info[j] = ESC;
            stuffed_info[j + 1] = MASKED_FLAG;
            j++;
            break;
        case ESC:
            stuffed_info[j] = ESC;
            stuffed_info[j + 1] = MASKED_ESC;
            j++;
            break;
        default:
            stuffed_info[j] = info[i];
            break;
        }
    }

    if (info == NULL || stuffed_info == NULL)
        return -1;

    return j;
}

int destuffing(const unsigned char *stuffed_info, size_t size, unsigned char *info)
{
    int i, j;
    for (i = 0, j = 0; j < size; i++, j++)
    {
        if (stuffed_info[j] == ESC) {
            switch (stuffed_info[j + 1])
            {
                case MASKED_ESC:
                    info[i] = ESC;
                    j++;
                    break;
                case MASKED_FLAG:
                    info[i] = FLAG;
                    j++;
                    break;
            }
        }
        else
            info[i] = stuffed_info[j];
    }
    if (stuffed_info == NULL || info == NULL)
        return -1;

    return i;
}

int openFile(char *buffer, char *fileName)
{
    FILE *fp;
    long lSize; 
    char *c;

    fp = fopen (fileName , "r" );
    if( !fp ) perror(fileName),exit(1);

    fseek( fp , 0L , SEEK_END);
    lSize = ftell( fp );
    rewind( fp );

    for (int i = 0; i < lSize; i++)
    {
        buffer[i] = fgetc(fp);
    }

    fclose(fp);

    return lSize;
}

int llwrite(int fd, char *buffer, int length)
{
    printf("\nStarting llwrite!\n");
    unsigned char result[65536];
    int res;
    int rej = 0;
    unsigned char splitBuffer[ll.frameSize];
    int frameDataSize = ll.frameSize;
    int bytesNeeded = sizeof(length);
    //CONTROL PACKET BEFORE TRANSMISSION
    unsigned char controlPacket[5+strlen(ll.fileName)+bytesNeeded];
    //indicate start
    controlPacket[0] = 2;
    //indicate its filesize
    controlPacket[1] = 0;
    //nr of bytes to show fileSize
    controlPacket[2] = bytesNeeded;
    //actual fileSize
    for (int i = 0; i < bytesNeeded; i++)
    {
        controlPacket[3 + i] = (length >> (8 * i))& 0xff;
    }
    
    //indicate its its fileName
    controlPacket[3+bytesNeeded] = 1;
    //nr of bytes to show fileName
    controlPacket[4+bytesNeeded] = strlen(ll.fileName);
    //actual fileName
    for (int i = 0; i < strlen(ll.fileName); i++)
    {
        controlPacket[5+bytesNeeded+i] = ll.fileName[i];
    }

    write(app.fd, controlPacket, 5 + strlen(ll.fileName)+bytesNeeded);

    // Get the file name
    char * filename = ll.fileName;

    int file_fd;
    struct stat file_stat;

    // Reads file info using stat
    if (stat(filename, &file_stat)<0){
        perror("Error getting file information.");
        return -1;
    }

    // Opens file to transmit
    if ((file_fd = open(filename, O_RDONLY)) < 0){
        perror("Error opening file.");
        return -1;
    }

    // Sends DATA packets
    unsigned char buf[65536];
    unsigned char conf;
    unsigned bytes_to_send;
    unsigned int bytesSent = 0;
    unsigned int bytesAddedInStuffing = 0;
    unsigned progress = 0;
    while ((bytes_to_send = read(file_fd, buf, frameDataSize)) > 0) {
        bytesSent += bytes_to_send;

        unsigned char dataPacket[65536];
        dataPacket[0] = 1;
        dataPacket[1] = ll.sequenceNumber % 255;
        dataPacket[2] = (bytes_to_send / 256);
        dataPacket[3] = (bytes_to_send % 256);
        memcpy(&dataPacket[4], buf, bytes_to_send);

        bytesAddedInStuffing = create_frame(dataPacket, bytes_to_send) - bytes_to_send;
        bytes_to_send += bytesAddedInStuffing;

        write(fd, ll.frame, ll.frameSize+bytesAddedInStuffing);
        //printf("I've sent %d bytes so far!\n", bytesSent);
//        printf("This trama is %d bytes long!\n", bytes_to_send);
//        printf("I've written %d bytes this time\n", bytes_to_send-bytesAddedInStuffing);
//        printf("I've written %d bytes so far\n", bytesSent);
        bool leaveLoop = FALSE;
        //RECEIVE CONFIRMATION
        states state = START;
        while(!leaveLoop && state != STOP)
        {
             alarm(ll.timeout);

             read(app.fd, &conf, 1);
             supervision_state(conf, &state);

             if(state == 3)
             {
                 if (conf == C_RCV)
                 {
                     //printf("Gonna send the next one\n");
                     ll.sequenceNumber++;
                     leaveLoop = TRUE;
                 }
                 else
                 {
                     printf("Gonna resend this one\n");
                     write(fd, ll.frame, ll.frameSize);
                     leaveLoop = TRUE;
                 }
             }
            if(alarmFlag)
            {
                write(fd, ll.frame, ll.frameSize);
                alarmFlag = FALSE;
                leaveLoop = TRUE;
            }
            alarm(0);
        }
    }

    //RESEND CONTROL PACKET
    controlPacket[0] = 3;
    write(app.fd, controlPacket, 5 + strlen(ll.fileName+bytesNeeded));

    return 0;
}

int llread(int fd, char *buffer)
{
    //RECEIVE CONTROL BYTE
    ctrl_states ctrlStates = START_CTRL;
    unsigned char bufferCntrl;
    unsigned char bufferTemp;
    unsigned char messsage[5];
    unsigned char bufferInfo[512];
    unsigned int fileSize = 0;
    int bytesForFileName;
    int bytesForSize;
    FILE *file = fopen("pinguim-novo.gif", "wb+");

    while(ctrlStates != STOP_CTRL)
    {
        read(fd, &bufferCntrl, 1);
        switch(ctrlStates)
        {
            case(START_CTRL):
                if(bufferCntrl == 2)
                    ctrlStates = T;
                break;
            case(T):
                if(bufferCntrl == 0)
                {
                    ctrlStates = L;
                }
                break;
            case(L):
                bytesForSize = bufferCntrl;
                for (int i = 0; i < bytesForSize; i++)
                {
                    read(fd, &bufferCntrl, 1);
                    fileSize += (bufferCntrl << (8 * i));
                }
                    ctrlStates = T2;
                break;
            case(T2):
                if(bufferCntrl == 1)
                    ctrlStates = L2;
                break;
            case(L2):
                bytesForFileName = bufferCntrl;
                for (int i = 0; i < bytesForFileName; i++)
                {
                    read(fd, &bufferCntrl, 1);
                    ll.fileName[i] = bufferCntrl;
                }
                ctrlStates = STOP_CTRL;
                break;
        }
    }

    unsigned char *finalFile = (char *)malloc(sizeof(char) * fileSize);
    ll.fileSize = fileSize;
    int bytesWritten = 0;

    printf("\nStarting llread!\n");
    unsigned char result[65536];
    int stuffed_size = 0;
    unsigned char message[5];
    states state = START;
    unsigned char bcc;
    int temp = 0;
    int currentTramaDataSize = 99999;
    while(bytesWritten < ll.fileSize)
    {
        //alarm(ll.timeout);
        int bytes_read = 0;
        //receive F,A,C,BCC from trama I
        while(state != BCC_OK)
        {
            read(app.fd,&bufferTemp, 1);
            info_state(bufferTemp, &state);
            if(alarmFlag)
            {
                message[0] = FLAG;
                message[1] = A_RCV;
                message[2] = C_REJ(ll.sequenceNumber);
                message[3] = BCC(A_RCV, C_REJ(ll.sequenceNumber));
                message[4] = FLAG;
                write(app.fd, message, 5);
                break;
                //message resend
            }
        }
        if(alarmFlag)
        {
            continue;
        }
        alarm(0);
        alarm(ll.timeout);

        data_packet_states dp_state = START_DP;
        //receive actual data packet
        while(read(app.fd, &bufferTemp, 1) > 0 && bytes_read < currentTramaDataSize)
        {
            switch(dp_state)
            {
                case START_DP:
                    if(bufferTemp == 1)
                        dp_state = N_DP;
                    break;
                case N_DP:
                    if((int)bufferTemp == ll.sequenceNumber%255)
                    {
                        dp_state = L2_DP;
                    }
                    break;
                case L2_DP:
                    temp = 256*(int)bufferTemp;
                    dp_state = L1_DP;
                    break;
                case L1_DP:
                    currentTramaDataSize = temp + (int)bufferTemp;
                    //printf("This trama is %d bytes long\n", currentTramaDataSize);
                    dp_state=P_DP;
                    break;
                case P_DP:
                    bufferInfo[bytes_read] = bufferTemp;
                    bytes_read++;
                    break;
            }
            if(alarmFlag)
            {
                message[0] = FLAG;
                message[1] = A_RCV;
                message[2] = C_REJ(ll.sequenceNumber);
                message[3] = BCC(A_RCV, C_REJ(ll.sequenceNumber));
                message[4] = FLAG;
                write(app.fd, message, 5);
                break;
            }
        }


        dp_state = START_DP;
        int info_size = destuffing(bufferInfo, bytes_read, result);

        while(state != STOP)
        {
            read(app.fd,&bufferTemp, 1);
            info_state(bufferTemp, &state);
            if(alarmFlag)
            {
                message[0] = FLAG;
                message[1] = A_RCV;
                message[2] = C_REJ(ll.sequenceNumber);
                message[3] = BCC(A_RCV, C_REJ(ll.sequenceNumber));
                message[4] = FLAG;
                write(app.fd, message, 5);
                alarmFlag = FALSE;
                break;
            }
        }
        alarm(0);

        state = START;

        for (int i = 5; i < info_size - 2; i++)
        {
            bcc ^= result[i];
        }
        message[0] = FLAG;
        message[1] = A_RCV;
        message[2] = C_RCV;
        message[3] = BCC(A_RCV, C_RCV);
        message[4] = FLAG;
        write(app.fd, message, 5);

        fwrite(result,info_size, 1, file);
        //printf("Wrote to file\n");
        bytesWritten += info_size;
        ll.sequenceNumber++;

//        printf("I've written %d bytes this time!\n", info_size);
//        printf("I've written %d bytes so far!\n", bytesWritten);

    }
    fclose(file);
    return 0;
}

int set_termios()
{
    printf("setting up termios...\n");

    if ( tcgetattr(app.fd, &oldtio) == -1) { /* save current port settings */
      perror("tcgetattr");
      exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    //default timer, should be 0 since we implemented our own
    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    //minimum of chars read, must be at least 1
    newtio.c_cc[VMIN]     = 1;   

    /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) pr�ximo(s) caracter(es)
    */

    //tcflush(app.fd, TCIOFLUSH);

    if (tcsetattr(app.fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }
}


int llopen(int port, int status)
{
    printf("\nStarting llopen!\n");
    if (status != TRANSMITER && status != RECEIVER)
        return -1;

    app.status = status;

    ll.sequenceNumber = 0;

    switch (port)
    {
    case HOME_PORT_0:
        strcpy(ll.port, "/dev/ttyS10");
        break;
    case HOME_PORT_1:
        strcpy(ll.port, "/dev/ttyS11");
        break;
    case LAB_PORT_0:
        strcpy(ll.port, "/dev/ttyS0");
        break;
    case LAB_PORT_1:
        strcpy(ll.port, "/dev/ttyS1");
        break;
    case LAB_PORT_4:
        strcpy(ll.port, "/dev/ttyS4");
        break;
    default:
        return -1;
    }

    app.fd = open(ll.port, O_RDWR | O_NOCTTY);
    if(app.fd < 0)
    {
        perror(ll.port);
        exit(-1);
    }

    set_termios();
    signal(SIGALRM, alarmHandler);
    alarm(ll.timeout);

    if(app.status == TRANSMITER)
    {

        // TRANSMITER

        // Send SET
        int res;
        unsigned char set[5];
        create_set(set);

        printf("Sending SET ...\n");
        res = write(app.fd, set, 5);

        // Receive UA
        states state = START;
        unsigned char result;
        for (int i = 0; state != STOP; ++i)
        {
            read(app.fd, &result, 1);
            ua_state(result, &state);

            if(alarmFlag && count > ll.numTransmissions)
            {
                printf("Max attempts reached, disconnecting\n");
                return 1;
            }
            if(alarmFlag)
            {
                printf("Didn't receive UA, resending SET\n");
                res = write(app.fd, set, 5);
                alarmFlag = FALSE;
            }
        }
    }
    else
    {
        // READER

        // Receive SET
        int res;
        states state = START;
        unsigned char result;
        for (int i = 0; state != STOP; ++i)
        {
            res = read(app.fd, &result, 1);
            set_state(result, &state);
            if (alarmFlag && count > ll.numTransmissions)
            {
                printf("Max attempts reached, disconnecting\n");
                return 1;
            }
            if(alarmFlag)
            {
                alarmFlag = FALSE;
            }
        }
        printf("Received SET\n");

        // Send UA
        unsigned char ua[5];
        create_ua(ua);
        printf("Sending UA ...\n");
        res = write(app.fd, ua, 5);
    }
    alarm(0);
    printf("llopen executed correctly\n");

    return 0;
}


int llclose(int fd)
{
    printf("\nStarting llclose...\n");
    int res;
    
    states state;

    if (app.status != TRANSMITER && app.status != RECEIVER) return -1;

    if(app.status == TRANSMITER) //case transmiter
    {   
        // // SEND DISC
        printf("Creating DISCONNECT!\n");
        unsigned char disc_snd[5];
        create_disc(disc_snd);
        printf("Sending DISC ...\n");
        res = write(app.fd, disc_snd, 5);

        // //RECEIVE DISC
        unsigned char disc_rcv;
        state = START;
        alarm(ll.timeout);
        for (int i = 0; state != STOP; i++)
        {
            if(alarmFlag && count > ll.numTransmissions)
            {
                printf("Max attempts reached, disconnecting\n");
                return 1;
            }
            if(alarmFlag)
            {
                printf("Didn't receive DISC, resending DISC\n");
                res = write(app.fd, disc_snd, 5);
                alarmFlag = FALSE;
            }
 
            read(app.fd, &disc_rcv, 1);
            disc_state(disc_rcv, &state);
        }
        alarm(0);


        // SEND UA
        unsigned char ua[5];
        create_ua(ua);
        printf("Sent UA\n");
        res = write(app.fd, ua, 5);
    }
    else //case receiver
    {   
        // //RECEIVE DISC
        unsigned char disc_rcv;
        state = START;
        alarm(ll.timeout);
        for (int i = 0; state != STOP; i++)
        {
            read(app.fd, &disc_rcv, 1);
            disc_state(disc_rcv, &state);
        }
        alarm(0);


        // sSEND DISC
        printf("Creating DISCONNECT!\n");
        unsigned char disc_snd[5];
        create_disc(disc_snd);
        printf("Sending DISC ...\n");
        res = write(app.fd, disc_snd, 5);
        res = write(app.fd, disc_snd, 5);

        // RECEIVE UA
        alarm(ll.timeout);
        state = START;
        unsigned char ua;
        printf("Gonna receive ua\n");
        state = START;
        for (int i = 0; state != STOP; i++)
        {
            if(alarmFlag && count > ll.numTransmissions)
            {
                printf("Max attempts reached, disconnecting\n");
                return 1;
            }
            if(alarmFlag)
            {
                //printf("Didn't receive UA, resending DISC\n");
                res = write(app.fd, disc_snd, 5);
                alarmFlag = FALSE;
            }

            read(fd, &ua, 1);
            ua_state(ua, &state);
        }
        alarm(0);
        printf("Received ua\n");
        return close(fd);
    }

    return 0;
}