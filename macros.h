#ifndef MACROS_H_INCLUDED
#define MACROS_H_INCLUDED

//enum states {START, FLAG_OK, A_OK, C_OK, BCC_OK, STOP};

#define TRANSMITER  0
#define RECEIVER    1
#define MAX_SIZE    65536

#define HOME_PORT_0 10
#define HOME_PORT_1 11
#define LAB_PORT_0  0
#define LAB_PORT_4  4

#define DATA_SIZE       3
#define FRAME_SIZE      5

#define FLAG            0x7E

#define A_SND           0x03                    // A(campo de endereço) Comandos enviados pelo Emissor / Respostas enviadas pelo Receptor
#define A_RCV           0x01                    // A(campo de endereço) Comandos enviados pelo Receptor / Respostas enviadas pelo Emissor

#define C_SND           0x03
#define C_RCV           0x07
#define C_DISC          0x0B
#define C_RR(r)         ((0x05) ^ (r) << (7))
#define C_REJ(r)        ((0x01) ^ (r) << (7))
#define C_I(r)          ((0x40) ^ (r) << (6))
#define BCC(a, c)       (a ^ c)
#define MAX_ATTEMPTS
#define TIMEOUT

#define ESC             0x7D
#define MASK            0x20
#define MASKED_FLAG     0x5E
#define MASKED_ESC      0x5D

typedef struct
{
    int fd; /*Descritor correspondente à porta série*/
    int status;         /*TRANSMITTER | RECEIVER*/
} applicationLayer;

typedef struct {
    char port[20];
    char fileName[65536];
    int baudRate;
    int fileSize;
    unsigned int sequenceNumber;
    unsigned int timeout;
    unsigned int numTransmissions;
    unsigned int frameSize;
    unsigned char frame[MAX_SIZE];
} linkLayer;

#endif
