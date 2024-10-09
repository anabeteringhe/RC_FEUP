#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define FLAG 0x7E
#define A_SENDER 0x03
#define A_RECEIVER 0x01
#define C_SET 0x03
#define C_UA 0x07

int fd;
unsigned char UA[5];
unsigned char SET[5];

#define BUF_SIZE 1024

#define TIMEOUT 3
#define MAX_RETRANSMISSIONS 3

volatile int STOP = FALSE;
volatile int timeout = FALSE;
int retransmissions = 0;

void prepareUA() {
    UA[0] = FLAG;
    UA[1] = A_RECEIVER;
    UA[2] = C_UA;
    UA[3] = A_RECEIVER ^ C_UA;
    UA[4] = FLAG;
}

void prepareSET() {
    SET[0] = FLAG;
    SET[1] = A_SENDER;
    SET[2] = C_SET;
    SET[3] = A_SENDER ^ C_SET;
    SET[4] = FLAG;
}

void sendUA() {
    write(fd, UA, 5);
    printf("Sent UA frame [%x,%x,%x,%x,%x]\n", UA[0], UA[1], UA[2], UA[3], UA[4]);
}

void alarmHandler(int signo) {
    timeout = TRUE;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    const char *serialPortName = argv[1];

    fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    if (tcgetattr(fd, &oldtio) == -1)
    {
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

    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    signal(SIGALRM, alarmHandler);

    prepareUA();

    while (retransmissions < MAX_RETRANSMISSIONS && STOP == FALSE)
    {
        unsigned char buf[BUF_SIZE] = {0};
        int bytes;

        alarm(TIMEOUT); // Set alarm for 3 seconds

        while (timeout == FALSE && STOP == FALSE)
        {
            bytes = read(fd, buf, BUF_SIZE);
            if (bytes > 0)
            {
                printf("Received frame = [%x,%x,%x,%x,%x]\n", buf[0], buf[1], buf[2], buf[3], buf[4]);
                if (buf[0] == FLAG && buf[1] == A_SENDER && buf[2] == C_SET && buf[3] == (A_SENDER ^ C_SET) && buf[4] == FLAG)
                {
                    STOP = TRUE;
                    printf("Received SET successfully!\n");
                    sendUA();
                    alarm(0); // Reset alarm
                }
            }
        }

        if (timeout == TRUE)
        {
            timeout = FALSE;
            retransmissions++;
        }
    }

    if (STOP == FALSE)
    {
        printf("Failed to receive SET after %d retransmissions\n", MAX_RETRANSMISSIONS);
    }

    sleep(1);

    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
