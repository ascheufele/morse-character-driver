#include <stdlib.h>
#include <err.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define DEV_FILE "/dev/morse"
#define IOCTL_MORSE_RESET _IO(0x11, 0)
#define IOCTL_MORSE_SET_PLAIN _IO(0x11, 1)
#define IOCTL_MORSE_SET_MORSE _IO(0x11, 2)

int main()
{
	int fd;
    int pass = 0;
    int tests = 0;
	ssize_t ret;
	char message[3] = "SOS";
	char buffer[256];
    printf("***TESTING BEGIN***\n");
    tests += 1;
	fd = open(DEV_FILE, O_RDWR);
	if (fd < 0)
		err(1, "unable to open file");
    else {
        pass+=1;
        printf("Test %d passed.\n", tests);
    }

	//performing read on fd
    tests+=1;
	ret = read(fd, buffer, sizeof(buffer));
    printf("read returned.\n");
	if(0 > ret)
		err(1, "unable to read from device");
    else {
        pass+=1;
        printf("Test %d passed.\n", tests);
    }
    tests+=1;
	if(0 != ret)
		printf("initial read test failed, expected zero, got %zd\n", ret);
    else {
        pass+=1;
        printf("Test %d passed.\n", tests);
    }

	// Send a message to the driver
    tests+=1;
	ret = write(fd, message, sizeof(message));
	if (ret < 0) {
		perror("Failed to write to device");
		exit(-1);
	}
	if(3 != ret)
		printf("expected write to consume all 3 bytes, got %zd\n", ret);
    else {
        pass+=1;
        printf("Test %d passed.\n", tests);
    }

	// Switch to Morse code output mode
    tests+=1;
	ret = ioctl(fd, IOCTL_MORSE_SET_MORSE, NULL);
	if (ret < 0) {
		perror("Failed to toggle output mode to Morse");
		exit(-1);
	}
    else {
        pass+=1;
        printf("Test %d passed.\n", tests);
    }

	// Read the Morse code output from the driver
    tests+=1;
	ret = read(fd, buffer, sizeof(buffer));
	if (ret < 0) {
		perror("Failed to read from device");
		exit(-1);
	}
	if(11 != ret)
		printf("expected read to return 11 bytes, got %zd from fd\n", ret);

	if(0 != memcmp("... --- ...", buffer, 11))
			printf("expected to read translated morse code, got %.*s\n", (int)ret, buffer);
    else {
        pass+=1;
        printf("Test %d passed.\n", tests);
    }

	//Checking if lseek fails properly on SET MORSE MODE
    tests+=1;
	long pos = lseek(fd, 1, SEEK_SET);
    if(-1 == pos && errno == EINVAL){
            pass+=1;
        printf("Test %d passed.\n", tests);
    }
    else {
        printf("Expected -1 from seek and %d from errno, got %ld and %d", EINVAL, pos, errno);
        printf("    errno: %d\n", errno);
    }

	// Switch back to plaintext output mode
    tests+=1;
	ret = ioctl(fd, IOCTL_MORSE_SET_PLAIN, NULL);
	if (ret < 0) {
		perror("Failed to toggle output mode to plaintext");
		exit(-1);
	}
    else {
        pass+=1;
        printf("Test %d passed.\n", tests);
    }

	//performing lseek on fd
    tests+=1;
	pos = lseek(fd, 1, SEEK_SET);
	if(1 != pos) {
		printf("expected 1, got %ld\n", pos);
    }
    else {
        pass+=1;
        printf("Test %d passed.\n", tests);
    }
	char c;
    tests+=1;
	if(1 != (ret = read(fd, &c, 1)))
		printf("expected return of 1, got %ld\n", ret);
	if(c != 'O')
		printf("expected O, got %c\n", c);
    else {
        pass+=1;
        printf("Test %d passed.\n", tests);
    }

	// Reset the device to its initial state
    tests+=1;
	ret = ioctl(fd, IOCTL_MORSE_RESET, NULL);
	if (ret < 0) {
		perror("Failed to reset device");
		exit(-1);
	}
    else {
        pass+=1;
        printf("Test %d passed.\n", tests);
    }

	//performing read on fd
    tests+=1;
	ret = read(fd, buffer, sizeof(buffer));
	if(0 > ret)
		err(1, "unable to read from device");
	if(0 != ret)
		printf("IOCTL_MORSE_RESET test failed, expected zero from read, got %zd\n", ret);
    else {
        pass+=1;
        printf("Test %d passed.\n", tests);
    }


	//testing that the whole alphabet works
    tests+=1;
	int fd2;
	fd2 = open(DEV_FILE, O_RDWR);
        if (fd2 < 0)
                err(1, "unable to open file");

	// Send a message to the driver
	char alpha[] = "abcdefghijklmnopqrstuvwxyz";
        ret = write(fd2, alpha, 26);
        if (ret < 0) {
                perror("Failed to write to device");
                exit(-1);
        }
        if(26 != ret)
                printf("expected write to consume all 26 bytes, got %zd\n", ret);

    ret = ioctl(fd, IOCTL_MORSE_SET_MORSE, NULL);
	ret = read(fd2, buffer, sizeof(buffer));
    if(0 > ret)
            err(1, "unable to read from device");
    if(107 != ret)
            printf("Expected 107, got %zd\n", ret);
	if(0 != memcmp(".- -... -.-. -.. . ..-. --. .... .. .--- -.- .-.. -- -. --- .--. --.- .-. ... - ..- ...- .-- -..- -.-- --..", buffer, 107))
                        printf("expected to read translated morse language, got %.*s\n", (int)ret, buffer);
    else {
        pass+=1;
        printf("Test %d passed.\n", tests);
    }

	close(fd);
	close(fd2);

    printf("\n***TESTING END***\n\n");
    printf("%d of %d tests passed\n", pass, tests);
	return 0;
}
