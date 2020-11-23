#ifndef DAKA_LINK_H
#define DATA_LINK_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include "writer.h"
#include "macros.h"
#include "stateMachine.h"
#include <signal.h>


#include <strings.h>
#include <string.h>
#include <stdlib.h> //for exit
#include <fcntl.h> // for open
#include <unistd.h> // for close

#define BAUDRATE        B38400
#define MODEMDEVICE     "/dev/ttyS1"
#define _POSIX_SOURCE   1 /* POSIX compliant source */
#define FALSE           0
#define TRUE            1

/*
argumentos
– fd: identificador da ligação de dados
– buffer: array de caracteres a transmitir
– length: comprimento do array de caracteres
*
retorno
– número de caracteres escritos
– valor negativo em caso de erro
*/
int llwrite(int fd, char *buffer, int length);

/*
argumentos
– fd: identificador da ligação de dados
– buffer: array de caracteres recebidos
*
retorno
– comprimento do array (número de caracteres lidos)
– valor negativo em caso de erro*/
int llread(int fd, char *buffer);

/*
argumentos
– porta: COM1, COM2, ...
– flag: TRANSMITTER / RECEIVER
*
retorno
– identificador da ligação de dados
– valor negativo em caso de erro
*/
int llopen(int porta, int flag);

/*
argumentos
– fd: identificador da ligação de dados
*
retorno
– valor positivo em caso de sucesso
– valor negativo em caso de erro*/
int llclose();

int openFile(char *buffer, char *fileName);

int stuffing(const unsigned char *info, size_t size, unsigned char *stuffed_info);

#endif