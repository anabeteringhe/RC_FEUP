#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FLAG 0x7E
#define A_SENDER 0x03
#define A_RECEIVER 0x01
#define C_SET 0x03
#define C_UA 0x07

int fd;

#define BUF_SIZE 1024

int state = 0;
unsigned char RECIEVED[5] = {0};

void verifyState(int *state, unsigned char REC) {
    switch (*state) {
        case 0:
            printf("START\n");
            if (REC == FLAG) {
                *state = 1;
            } else {
                *state = 0;
            }
            break;
        case 1:
            printf("FLAG RCV\n");
            if (REC == A_SENDER || REC == A_RECEIVER) {
                *state = 2;
            } else if (REC == FLAG) {
                *state = 1;
            } else {
                *state = 0;
            }
            break;
        case 2:
            printf("A RACV\n");
            if (REC == C_SET || REC == C_UA) {
                *state = 3;
            } else if (REC == FLAG) {
                *state = 1;
            } else {
                *state = 0;
            }
            break;
        case 3:
            printf("C REV\n");
            if (REC == (A_SENDER ^ C_SET) || REC == (A_RECEIVER ^ C_UA)) {
                *state = 4;
            } else if (REC == FLAG) {
                *state = 1;
            } else {
                *state = 0;
            }
            break;
        case 4:
            printf("BCC OK\n");
            if (REC == FLAG) {
                *state = 5;
            } else {
                *state = 0;
            }
            break;
        default:
            printf("Invalid state\n");
            *state = 0;
            break;
    }
}

void recievedFrame(unsigned char REC) {
    verifyState(&state, REC);
    if (state >= 0 && state < 5) {  // Ensure the index is within the bounds of the array
        RECIEVED[state] = REC;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    const char *serialPortName = argv[1];

    fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    if (tcgetattr(fd, &oldtio) == -1) {
        perror("tcgetattr");
        exit(-1);
    }

    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 1;

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Read data and process the frame
    unsigned char buf[BUF_SIZE] = {0};
    int bytes;
    for (int i = 0; i < 1024 && state != 5; i++) {
        bytes = read(fd, buf, 1);
        if (bytes > 0) {
            recievedFrame(buf[0]);
        }
    }

    sleep(1);

    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
