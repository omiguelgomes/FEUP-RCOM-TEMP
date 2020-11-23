#include "stateMachine.h"

applicationLayer app;
linkLayer ll;

//int main() { return 0; }

void create_set(char *set)
{
    set[0] = FLAG;
    set[1] = A_RCV;
    set[2] = C_RCV;
    set[3] = BCC(A_RCV, C_RCV);
    set[4] = FLAG;
}

void create_ua(char *ua)
{
    ua[0] = FLAG;
    ua[1] = A_SND;
    ua[2] = C_SND;
    ua[3] = BCC(A_SND, C_SND);
    ua[4] = FLAG;
}

void create_rr(char *rr)
{
    rr[0] = FLAG;
    rr[1] = A_RCV;
    rr[2] = C_RR(ll.sequenceNumber);
    rr[3] = BCC(A_RCV, C_RR(ll.sequenceNumber));
    rr[4] = FLAG;
}

void create_rej(char *rej)
{
    rej[0] = FLAG;
    rej[1] = A_RCV;
    rej[2] = C_REJ(ll.sequenceNumber);
    rej[3] = BCC(A_RCV, C_REJ(ll.sequenceNumber));
    rej[4] = FLAG;
}

void create_disc(char *disc)
{
    disc[0] = FLAG;
    disc[1] = (app.status == TRANSMITER) ? A_SND : A_RCV;
    disc[2] = C_DISC;
    disc[3] = BCC(disc[1], C_DISC);
    disc[4] = FLAG;
}

int create_bcc2(const unsigned char *info, size_t size, unsigned char *bcc2)
{
    bcc2[0] = info[0];

//    for (int i = 1; i < size; i++)
//    {
//        bcc2[0] ^= info[i];
//    }
    bcc2[0] = BCC(A_SND, C_SND);

    if (bcc2[0] == FLAG)
    {
        bcc2[0] = ESC;
        bcc2[1] = MASKED_FLAG;
        return 2;
    }
    else if (bcc2[0] == ESC)
    {
        bcc2[0] = ESC;
        bcc2[1] = MASKED_ESC;
        return 2;
    }
    else if (bcc2 == NULL)
        return 0;
    else
        return 1;
}

int create_frame(const unsigned char *info, size_t size)
{
    char temp[65536];
    memcpy(&temp, info, size);
    char stuffed_info[size * 2];
    int stuffed_size = stuffing(info+4, size, stuffed_info);


    char bcc2[2];
    int bcc2_i = 4 + stuffed_size;
    int bcc2_size = create_bcc2(info, size, bcc2);

    char frame[4 + stuffed_size + bcc2_size + 1];
    frame[0] = FLAG;
    frame[1] = A_SND;
    frame[2] = C_SND;
    frame[3] = BCC(A_SND, C_SND);


    frame[4] = info[0];
    frame[5] = info[1];

    frame[6] = (stuffed_size / 256);
    frame[7] = (stuffed_size % 256);

    for (int i = 0; i < stuffed_size; i++)
        frame[8 + i] = stuffed_info[i];

    frame[8 + stuffed_size] = bcc2[0];
    if (bcc2_size == 2)
        frame[8 + stuffed_size + 1] = bcc2[1];
    
    frame[8 + stuffed_size + bcc2_size] = FLAG;

    ll.frameSize = 8 + stuffed_size + bcc2_size + 1;
    memcpy(ll.frame, frame, ll.frameSize);
    return stuffed_size;
}

void set_state(char byte, states *state)
{
    switch (*state)
    {
        case START:
            if(byte == FLAG)
                *state = FLAG_OK;
            break;

        case FLAG_OK:
            if(byte == A_RCV)   
                *state = A_OK;
            else if(byte != FLAG){
                *state = START;
            }
            break;

        case A_OK:
            if(byte == C_RCV)
                *state = C_OK;
            else if(byte == FLAG)
                *state = FLAG_OK;
            else *state = START;
            break;

        case C_OK:
            if(byte == BCC(A_RCV, C_RCV))
                *state = BCC_OK;
            else if(byte == FLAG)
                *state = FLAG_OK;
            else *state = START;
            break;
        
        case BCC_OK:
            if (byte == FLAG)
                *state = STOP;
            else *state = START;
            break;
    }
}

void ua_state(char byte, states *state)
{
    switch (*state)
    {
        case START:
            if(byte == FLAG)
                *state = FLAG_OK;
            break;

        case FLAG_OK:
            if(byte == A_SND)
                *state = A_OK;
            else if(byte != FLAG){
                *state = START;
            }
            break;
        case A_OK:
            if(byte == C_SND)
                *state = C_OK;
            else if(byte == FLAG)
                *state = FLAG_OK;
            else *state = START;
            break;
        case C_OK:
            if(byte == BCC(A_SND, C_SND))
                *state = BCC_OK;
            else if(byte == FLAG)
                *state = FLAG_OK;
            else *state = START;
            break;
        
        case BCC_OK:
            if (byte == FLAG)
                *state = STOP;
            else *state = START;
            break;
    }
}

void disc_state(char byte, states *state)
{
  char a;
  //printf("Received byte: %#x, and am in state: %d\n", byte, *state);
  switch (*state)
  {
  case START:
    if (byte == FLAG)
      *state = FLAG_OK;
    break;
  case FLAG_OK:
    if (byte == A_SND || byte == A_RCV)
    {
      a = byte;
      *state = A_OK;
    }
    else if (byte != FLAG)
      *state = FLAG_OK;
    break;
  case A_OK:
    if (byte == C_DISC)
      *state = C_OK;
    else if (byte != a)
      *state = A_OK;
    break;
  case C_OK:
    if (byte == (BCC(a, C_DISC)))
      *state = BCC_OK;
    else if (byte != C_DISC)
      *state = C_OK;
    break;
  case BCC_OK:
    if (byte == FLAG)
      *state = STOP;
    else
      *state = BCC_OK;
    break;
  }
}


void supervision_state(char byte, states *state)
{
  unsigned char c;
  switch (*state)
  {
  case START:
    if (byte == FLAG)
      *state = FLAG_OK;
    break;
  case FLAG_OK:
    if (byte == A_RCV)
      *state = A_OK;
    else if (byte != FLAG)
      *state = START;
    break;
  case A_OK:
    if (byte == C_RCV)
    {
      *state = C_OK;
      c = byte;
    }
    else if (byte != A_RCV)
      *state = START;
    break;
  case C_OK:
    if (byte == (BCC(A_RCV, c)))
      *state = BCC_OK;
    else if (byte != c)
      *state = START;
    break;
  case BCC_OK:
    if (byte == FLAG)
      *state = STOP;
    else
      *state = START;
    break;
  }
}

void info_state(char byte, states *state)
{
  unsigned char a;
  unsigned char c;
  switch (*state)
  {
  case START:
    if (byte == FLAG)
      *state = FLAG_OK;
    break;
  case FLAG_OK:
    if (byte == A_SND)
      *state = A_OK;
    break;
  case A_OK:
    if (byte == C_SND)
    {
      *state = C_OK;
    }
    break;
  case C_OK:
    if (byte == BCC(A_SND, C_SND))
      *state = BCC_OK;
    break;
  case BCC_OK:
    if (byte == FLAG)
      *state = STOP;
    break;
  }

}

