#ifndef WRITER_H
#define WRITER_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h> //for exit
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include "stateMachine.h"
#include "dataLink.h"
#include "macros.h"
#include "dataLink.h"
#include <time.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

/*-------------------------FUNCTIONS-------------------------*/
int main();

#endif