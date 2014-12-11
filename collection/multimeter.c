#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/fcntl.h>


/*
 * Serial communications protocol for the TekPower TP4000ZC digital
 * multimeter.
 *
 * Written by Mark Mason and Robin Garner.  Feel free to use and copy,
 * with credit given to us.  mason at porklips dot org.
 *
 * Data is transferred at 2400 baud, 8 data bits, one stop bit, no
 * parity.
 *
 * Instead of sending ASCII text showing what's on the display, the
 * meter sends data representing the segments of the LCD display,
 * which requires some translation to turn into a usable value.
 *
 * "Packets" representing single samples are sent at regular intervals
 * when the meter is in RS232 mode.  It is not necessary to poll for
 * samples.  The samples are sent at approximately 1 second intervals
 * on the volts, amps, and ohms scale.  Capacitance will take longer,
 * depending on the capacitance - up 10 15 seconds for 100uf.  In
 * addition, a single zero byte is sent when the meter is turned on.
 *
 * A packet is 13 or 14 bytes.  The upper 4 bits contains a value
 * between 1 and 0xE (14).  This shows the position of that byte
 * within the packet.  They are sent in sequence, but the first byte
 * is not sent in some cases.
 *
 * The lower four bits of each byte represent either the segments of
 * the digits on the LCD display or attributes teling what mode the
 * meter is in, such as mV, kohms, hz, etc.  The 2nd through 9th bytes
 * represent LCD segments, the 1st and 10th through 14th bytes give
 * attribute information.
 *
 * For example, if the display says: "04.71 k ohms RS232 AUTO", the
 * following bytes would be sent:
 *
 *   27 3D 42 57 69 75 80 95 A2 B0 C4 D0 E8
 *
 * In this case, the first byte (1x) is not sent.  '2x' through '9x'
 * represent the LCD segments displaying "04.71", and Ax through Ex
 * indicate that the meter is in "k ohms RS232 AUTO" mode.
 *
 * To decode this, discard the first four bits of each byte, since
 * they are just for packet framing.  This leaves us with 13 four bit
 * values:
 *
 *   7 D 2 7 9 5 0 5 2 0 4 0 8
 *
 * There are eight bits per digit, so four digits would be grouped
 * like this:
 * 
 *   7D 27 95 05
 *
 * These numbers represent segments of the LCD display.  The segments
 * are:
 *
 *    -        A
 *   | |     F   B
 *    -        G
 *   | |     E   C
 * .  _        D
 *
 * The segments correspond to the following bits:
 *
 *   B = 1
 *   G = 2
 *   C = 3
 *   D = 4
 *   A = 5
 *   F = 6
 *   E = 7
 *   Decimal = 8 (or the negative sign on the first digit)
*
* In the example above, where "7D 27 95 05" represents "04.71", the
* first digit is hex 7D.  This is, in binary,
	*
	*   0111 1101
	*
	* Bits 1, 3, 4, 5, 6, and 7 are set, resresenting LCD segments B, C,
	* D, A, F, and E, or:
	*
	*    A       -
	*  F   B    | |
	*              
	*  E   C    | |
	*    D       -
	*
	* Which is 0.
	*
	* Simple enough?
	*
	* The remaining bits, in bytes 1x (when present) and Ax through Ex
	* (that is, the bytes starting with 1, and A through E) give the
	* meter mode.  More than one bit can be set.  The bits are as
	* follows:
	*
	* 11 - Unknown
	* 12 - AUTO
	* 14 - DC
	* 18 - AC
	* A1 - Diode test
	* A2 - Kilo (k)
	* A4 - Nano (n)
	* A8 - Micro (u)
	* B1 - Audible Alert (sound waves)
* B2 - Mega (M)
	* B4 - %
* B8 - Milli (m)
	* C1 - Hold
	* C2 - Rel Delta (triangle)
	* C4 - Ohms (omega)
* C8 - Farads (F)
	* D1 - Unknown
	* D2 - Hertz (Hz)
	* D4 - Volts (V)
* D8 - Amps (A)
	* E1 - Unknown
	* E2 - Unknown
	* E4 - Degrees celcius
	* E8 - Unknown
	*  
	* In the 4.71 k ohms example above, the attributes are A2, C4, and
	* E8, which indicates that the mode is kilo ohms, with the unknown
	* E8.
	*/ 

	int
read_packet(int fd, unsigned char* buf)
{
	int x;
	int n;
	int byte = 0;
	int idx;
	int bytes_read = 0;

	for (x = 0; x < 14;x++)
		buf[x] = 0;

	for (x = 0;x < 15;x++)
	{
		n = read(fd, &byte, 1);

		if (n <= 0)
		{
			printf("Read EOF\n");
			exit(0);
		}

		if (byte == 0)
		{
			printf("Meter ON.\n");
			return -1;
		}

		/* This is the byte number */
		idx = (byte >> 4) & 0xF;

		if ((idx == 0) || (idx == 0xF))
		{
			printf("Read invalid byte 0x%02X\n", byte);
			return -1;	/* Invalid byte */
		}

		/* IDX is 1-14, but buf is 0 based, so we use idx - 1. */
		buf[idx - 1] = byte & 0xF;
		bytes_read++;

		if (idx == 0xE)
		{
			/* This is the last byte of a packet. */

			/*
			 * We should have read at least 13 bytes - A packet is 14
			 * bytes, but the first byte is not always sent.
			 */

			if (bytes_read < 13)
			{
				printf("Only read %d bytes of packet.\n", bytes_read);
				return -1;
			}
			else
				return 0;	/* We're done. */
		}
	}

	/* There were too many bytes read without seeing the last byte */
	printf("Read too many bytes.\n");
	return -1;
}

/*
 ****************************************************************
 *
 * Decode a single digit.
 *
 ****************************************************************
 */
static const int lcd_segments[12] =
{
	0x7D,	/* 0 */
	0x05,
	0x5B,
	0x1F,
	0x27,
	0x3E,
	0x7E,
	0x15,
	0x7F,
	0x3F,	/* 9 */
	0x68,	/* L (out of range) */
	0x00	/* Blank */
};

/*
 * This takes two bytes of data from the meter and returns 0-12,
 * representing the digits 0-9, L, and Blank.
 */
	int
decode_digit(unsigned int byte1, unsigned int byte2)
{
	int value;
	int n;

	/*
	 * Concatenate the low four bits of each byte into one eight bit
	 * value and look it up in the LCD segment table.
	 */
	value = ((byte1 & 0x7) << 4) | byte2;
	for (n = 0; n < 12;n++)
	{
		if (lcd_segments[n] == value)
			return n;
	}

	/* Not table, invalid value. */
	return -1;
}

	int
print_display_number(const unsigned char *buf)
{
	int n;
	int val;

	/*
	 * There are four digits, contained in bytes 2 and 3, 4 and 5, 6
	 * and 7, and 8 and 9.
	 */
	for (n = 1;n < 8;n += 2)
	{
		/*
		 * The high bit is the decimal point, or the minus sign on the
		 * first digit.
		 */
		if (buf[n] & 0x8)
		{
			if (n == 1)
				printf("-");
			else
				printf(".");
		}
		val = decode_digit(buf[n], buf[n + 1]);
		if (val == -1)
		{
			printf("Unknown digit %X %X\n", buf[n], buf[n + 1]);
			return -1;
		}
		else
		{
			if (val < 10)
				printf("%d", val);
			else
			{
				if (val == 10)
					printf("L");
				if (val == 11)
					printf(" ");
				if (val > 11)
					printf("?");
			}
		}
	}

	return 0;
}

/*
 ****************************************************************
 *
 * Decode attributes.
 *
 ****************************************************************
 */

/*
 * The modes the meter can be in.
 *
 * The LCD display has 'hfe' on it, but the meter doesn't do hfe
 * (transistor test), so there's a good chance that one of the
 * unknowns is hfe.
 *
 * It also seems likely that one of the unknowns is degrees
 * fahrenheit, but the meter doesn't support that either.
 *
 * One of the unknowns might be low battery.
 *
 * E8 is always on, except when measuring temperature.
 */
#define ATTR_UNK_11	(1 << 0)	/* 11 - Unknown */
#define ATTR_AUTO	(1 << 1)
#define ATTR_DC		(1 << 2)
#define ATTR_AC		(1 << 3)
#define ATTR_DIODE	(1 << 4)	/* A1 */
#define ATTR_KILO	(1 << 5)
#define ATTR_NANO	(1 << 6)
#define ATTR_MICRO	(1 << 7)
#define ATTR_BEEP	(1 << 8)	/* B1 */
#define ATTR_MEGA	(1 << 9)
#define ATTR_PERCENT	(1 << 10)
#define ATTR_MILI	(1 << 11)
#define ATTR_HOLD	(1 << 12)	/* C1 */
#define ATTR_REL	(1 << 13)
#define ATTR_OHMS	(1 << 14)
#define ATTR_FARAD	(1 << 15)
#define ATTR_UNK_D1	(1 << 16)	/* D1 - Unknown */
#define ATTR_HERTZ	(1 << 17)
#define ATTR_VOLTS	(1 << 18)
#define ATTR_AMPS	(1 << 19)
#define ATTR_UNK_E1	(1 << 20)	/* E1 - Unknown */
#define ATTR_UNK_E2	(1 << 21)	/* E2 - Unknown */
#define ATTR_DEGC	(1 << 22)
#define ATTR_UNK_E8	(1 << 23)	/* E8 - Unknown */

const char* attribute_table[] =
{
	"(unknown 11)",
	"AUTO",
	"DC",
	"AC",
	"DIODE",
	"kilo",
	"nano",
	"micro",
	"beep",
	"mega",
	"Percent",
	"mili",
	"HOLD",
	"REL",
	"Ohms",
	"Farads",
	"(unknown 0xD1)",
	"Hertz",
	"Volts",
	"Amps",
	"(unknown E1)",
	"(unknown E2)",
	"DegreesC",
	"(unknown E8)",
	NULL
};

/*
 * Convert the attributes from the string of bytes passed in to a 32
 * bit value.
 */
	unsigned long
decode_attributes(const unsigned char* buf)
{
	unsigned long attributes = 0;
	int bit;
	int attr;
	int byte = 0;

	for (bit = 0;bit < 24;bit++)
	{
		if (bit < 4)
			byte = 0;
		else
			byte = (bit / 4) + 0x8;

		attr = bit % 4;

		if (buf[byte] & (1 << attr))
			attributes |= (1 << bit);
	}

	return attributes;
}

/*
 * Print out the attributes that are described by the 32 bit value
 * passed in.
 */
	void
print_attributes(unsigned long attributes)
{
	int n;

	for (n = 0;n < 24;n++)
	{
		if (attributes & (1 << n))
			printf("%s ", attribute_table[n]);
	}
}

/*
 ****************************************************************
 *
 * Main loop.
 *
 ****************************************************************
 */

/*
 * Configure the serial port to 2400 baut, 8 data bits, 1 stop bit,
 * and no parity.
 */
	int
configure_serial_port(const char* dev)
{
	char string[64];

	sprintf(string, "stty -F %s 2400 -parity -icanon -echo -echoe", dev);

	return system(string);
}


	int
main(int argc, char **argv)
{
	int fd;
	int n;
	unsigned char buf[15];
	unsigned long attributes;
	const char *port;

	if (argc > 1)
		port = argv[1];
	else
		port = "/dev/ttyS0";

	if (configure_serial_port(port))
		printf("Couldn't configure serial port \"%s\"\n", port);

	fd = open(port, O_RDONLY);

	if (fd < 0)
	{
		perror(port);
		exit(0);
	}

	while (1)
	{
		/* Read a packet. */
		n = read_packet(fd, buf);

		/* Ignore errors. */
		if (n)
			continue;

#if 0
		for (n = 0;n < 14;n++)
			printf("%1X=%02X ", n + 1, buf[n]);
		printf("\n");
#endif

		/* Print the number. */
		struct timeval t;
		gettimeofday(&t, NULL);
		printf("%u.%06u ", t.tv_sec, t.tv_usec);
		n = print_display_number(buf);
		if (n != 0)
			continue;
		/* If the nunber was valid then print the attributes. */
		printf(" ");
		attributes = decode_attributes(buf);
		print_attributes(attributes);
		printf("\n");
	}


	return 0;
}
