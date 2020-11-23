#ifndef STATE_MACHINE_H_INCLUDED
#define STATE_MACHINE_H_INCLUDED

#include "macros.h"
#include "dataLink.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

typedef enum
{
    START,
    FLAG_OK,
    A_OK,
    C_OK,
    BCC_OK,
    STOP
}states;

typedef enum
{
    START_CTRL,
    T,
    L,
    V,
    T2,
    L2, 
    V2,
    STOP_CTRL
}ctrl_states;

typedef enum
{
    START_DP,
    N_DP,
    L2_DP,
    L1_DP,
    P_DP,
    STOP_DP
}data_packet_states;

void create_set(char *set);

void create_ua(char *ua);

void create_rr(char *rr);

void create_rej(char *rej);

void create_disc(char *disc);

int create_bcc2(const unsigned char *info, size_t size, unsigned char *bcc2);

void set_state(char byte, states *state);

void ua_state(char byte, states *state);

void disc_state(char byte, states *state);

int create_frame(const unsigned char *info, size_t size);

void supervision_state(char byte, states *state);

void info_state(char byte, states *state);

#endif