#include <stdio.h>
#include <stdint.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>


#define I2C_FILE_NAME "/dev/i2c-0"
#define USAGE_MESSAGE \
	"Usage:\n" \
"  %s r [addr] [register]   " \
"to read value from [register]\n" \
"  %s w [addr] [register] [value]   " \
"to write a value [value] to register [register]\n" \
""

static int set_i2c_register(int file,
		unsigned char addr,
		unsigned char reg,
		unsigned char value) {

	unsigned char outbuf[2];
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg messages[1];

	messages[0].addr  = addr;
	messages[0].flags = 0;
	messages[0].len   = sizeof(outbuf);
	messages[0].buf   = outbuf;

	/* The first byte indicates which register we'll write */
	outbuf[0] = reg;

	/* 
	 * The second byte indicates the value to write.  Note that for many
	 * devices, we can write multiple, sequential registers at once by
	 * simply making outbuf bigger.
	 */
	outbuf[1] = value;

	/* Transfer the i2c packets to the kernel and verify it worked */
	packets.msgs  = messages;
	packets.nmsgs = 1;
	if(ioctl(file, I2C_RDWR, &packets) < 0) {
		perror("Unable to send data");
		return 1;
	}

	return 0;
}


static int get_i2c_register(int file,
		unsigned char addr,
		unsigned char reg,
		unsigned char *val)
{
	unsigned char inbuf, outbuf;
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg messages[2];

	/*
	 * In order to read a register, we first do a "dummy write" by writing
	 * 0 bytes to the register we want to read from.  This is similar to
	 * the packet in set_i2c_register, except it's 1 byte rather than 2.
	 */
	outbuf = reg;
	messages[0].addr  = addr;
	messages[0].flags = 0;
	messages[0].len   = sizeof(outbuf);
	messages[0].buf   = &outbuf;

	/* The data will get returned in this structure */
	messages[1].addr  = addr;
	messages[1].flags = I2C_M_RD/* | I2C_M_NOSTART*/;
	messages[1].len   = sizeof(inbuf);
	messages[1].buf   = &inbuf;

	/* Send the request to the kernel and get the result back */
	packets.msgs      = messages;
	packets.nmsgs     = 2;
	if(ioctl(file, I2C_RDWR, &packets) < 0) {
		perror("Unable to send data");
		return 1;
	}
	*val = inbuf;

	return 0;
}

char *MakeCRC(uint8_t *BitString){
	static char Res[9];
	char CRC[8];
	int  i;
	char DoInvert;

	for (i=0; i<8; ++i)  CRC[i] = 0;

	for (i=0; i < 2; ++i){
		DoInvert = ('1'==BitString[i]) ^ CRC[7];

		CRC[7] = CRC[6];
		CRC[6] = CRC[5];
		CRC[5] = CRC[4] ^ DoInvert;
		CRC[4] = CRC[3] ^ DoInvert;
		CRC[3] = CRC[2];
		CRC[2] = CRC[1];
		CRC[1] = CRC[0];
		CRC[0] = DoInvert;
	}

	for (i=0; i<8; ++i)  Res[7-i] = CRC[i] ? '1' : '0'; // Convert binary to ASCII
	Res[8] = 0;                                         // Set string terminator

	return(Res);
}

/* CRC f = x^8 + x^5 + x^4 + 1 */
static int read_value(int file,
		unsigned char addr,
		unsigned char reg,
		uint16_t *result)
{
	unsigned char outbuf;
	uint8_t inbuf[3];

	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg messages[2];

	/*
	 * In order to read a register, we first do a "dummy write" by writing
	 * 0 bytes to the register we want to read from.  This is similar to
	 * the packet in set_i2c_register, except it's 1 byte rather than 2.
	 */
	outbuf = reg;
	messages[0].addr  = addr;
	messages[0].flags = 0;
	messages[0].len   = sizeof(outbuf);
	messages[0].buf   = &outbuf;

	/* The data will get returned in this structure */
	messages[1].addr  = addr;
	messages[1].flags = I2C_M_RD/* | I2C_M_NOSTART*/;
	messages[1].len   = sizeof(inbuf);
	messages[1].buf   = inbuf;

	/* Send the request to the kernel and get the result back */
	packets.msgs      = messages;
	packets.nmsgs     = 2;
	if(ioctl(file, I2C_RDWR, &packets) < 0) {
		perror("Unable to send data");
		return 1;
	}

	fprintf(stderr, "%02hhx %02hhx %02hhx \n", inbuf[0], inbuf[1], inbuf[2]);
	uint16_t v = inbuf[0];
	v <<= 8;
	v |= inbuf[1];

	*result = v;

	return 0;
}

int raw_to_temp(uint16_t raw){
	int v = raw;
	v *= 17572;
	v /= 65536;
	v -= 4685;
	return v;
}

int raw_to_RH(uint16_t raw){
	int v = raw;
	v *= 125;
	v /= 65536;
	v -= 6;
	return v;
}


int main(int argc, char **argv) {
	int i2c_file;

	// Open a connection to the I2C userspace control file.
	if ((i2c_file = open(argv[1], O_RDWR)) < 0) {
		perror("Unable to open i2c control file");
		exit(1);
	}


	uint16_t temperature_raw;
	uint16_t humidity_raw;
	if(read_value(i2c_file, 0x40, 0xE3, &temperature_raw)){
		perror("read temp error");
		exit(1);
	}

	if(read_value(i2c_file, 0x40, 0xE5, &humidity_raw)){
		perror("read temp error");
		exit(1);
	}

	int temperature = raw_to_temp(temperature_raw);
	int humidity = raw_to_RH(humidity_raw);


	//fprintf(stderr, "Temperature %i.%02i C\n", temperature / 100, temperature %100);
	//fprintf(stderr, "Humidity    %i %%\n", humidity);

	struct timeval t;
	gettimeofday(&t, NULL);
	printf("%u.%06u %i.%02i %i\n", t.tv_sec, t.tv_usec, temperature / 100, temperature %100, humidity);

#if 0
		if(argc > 3 && !strcmp(argv[1], "r")) {
			int addr = strtol(argv[2], NULL, 0);
			int reg = strtol(argv[3], NULL, 0);
			unsigned char value;
			if(get_i2c_register(i2c_file, addr, reg, &value)) {
				printf("Unable to get register!\n");
			}
			else {
				printf("Register %d: %d (%x)\n", reg, (int)value, (int)value);
			}
		}
		else if(argc > 4 && !strcmp(argv[1], "w")) {
			int addr = strtol(argv[2], NULL, 0);
			int reg = strtol(argv[3], NULL, 0);
			int value = strtol(argv[4], NULL, 0);
			if(set_i2c_register(i2c_file, addr, reg, value)) {
				printf("Unable to get register!\n");
			}
			else {
				printf("Set register %x: %d (%x)\n", reg, value, value);
			}
		}
		else {
			fprintf(stderr, USAGE_MESSAGE, argv[0], argv[0]);
		}
#endif


		close(i2c_file);


		return 0;
	}
