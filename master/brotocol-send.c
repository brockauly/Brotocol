/**
 * @file brotocol-send.c
 * @brief Master Brotocol implementation, for sending a message to a slave.
 * @author Brock Auly
 * @date Jul 30, 2025
 *
 * @note Compile on RPi with `gcc -Wall -pthread -lpigpio -lrt -o brotocol-send brotocol-send.c`
 */

// App includes
#include <stdio.h>  // printf
#include <unistd.h> // getopt
#include <stdlib.h> // strtol

// HAL includes
#include <pigpio.h> // gpio

/////////////////////////
///  BROTOCOL CONFIG  ///
#define ADDRESS_N       5
#define DATA_N          1
#define TIME_UNIT_MS   10
/////////////////////////

#define EXIT_OK         0x00
#define EXIT_ADDR_NACK  0x01
#define EXIT_DATA_NACK  0x02
#define EXIT_HAL        0x04
#define EXIT_ERROR      0x08

typedef enum {
	BUS_LOW  = 0,
	BUS_HIGH
} bus_state_t;

///////////////////////////
/////////   HAL   /////////
///////////////////////////

#define GPIO_PIN 4

/**
 * Configure bus, eg set GPIO pin mode.
 */
void config_bus(void)
{
	if (gpioInitialise() == PI_INIT_FAILED) {
		fprintf(stderr, "Error initialising GPIO\n");
		exit(EXIT_HAL);
	}
	if (gpioSetMode(GPIO_PIN, PI_INPUT) != 0) {
		fprintf(stderr, "Error setting GPIO in high-Z\n");
		exit(EXIT_HAL);
	}
	if (gpioSetPullUpDown(GPIO_PIN, PI_PUD_UP) != 0) {
		fprintf(stderr, "Error setting GPIO pull-up\n");
		exit(EXIT_HAL);
	}
}

/**
 * Read the bus state, either BUS_HIGH or BUS_LOW.
 */
bus_state_t get_bus_state(void)
{
	int read = gpioRead(GPIO_PIN);
	if (read == PI_BAD_GPIO) {
		fprintf(stderr, "Error reading bus state\n");
		exit(EXIT_HAL);
	}
	return read ? BUS_HIGH : BUS_LOW;
}

/**
 * Set the bus state, either BUS_HIGH or BUS_LOW.
 */
void set_bus_state(bus_state_t state)
{
	if (state == BUS_LOW) {
		if (gpioSetMode(GPIO_PIN, PI_OUTPUT) != 0) {
			fprintf(stderr, "Error setting GPIO pin mode\n");
			exit(EXIT_HAL);
		}
		if (gpioWrite(GPIO_PIN, 0) != 0) {
			fprintf(stderr, "Error setting GPIO low\n");
			exit(EXIT_HAL);
		}
	} else {
		if (gpioSetMode(GPIO_PIN, PI_INPUT) != 0) {
			fprintf(stderr, "Error setting GPIO in high-Z\n");
			exit(EXIT_HAL);
		}
	}
}

/**
 * Blocking delay function in milliseconds.
 */
void delay_ms(int duration_ms)
{
	gpioDelay((uint32_t) duration_ms * 1000L);
}

///////////////////////////
///////////////////////////

int is_quiet;

void usage(char *name)
{
	fprintf(stderr, "Usage: %s [-q] -a <addr> -d <data>\n", name);
	fprintf(stderr, "  <addr> is %u-bit address, in hex or decimal\n", ADDRESS_N);
	fprintf(stderr, "  <data> is %u-bit data, in hex or decimal\n", DATA_N);
}

void wait(int unit_times)
{
	for (int i = 0; i < unit_times; ++i) {
		delay_ms(TIME_UNIT_MS);
		if (! is_quiet) {
			printf("%c", get_bus_state() == BUS_LOW ? '_' : '-');
			fflush(stdout);
		}
	}
}

int main(int argc, char **argv)
{
	is_quiet = 0;

	int opt;
	int address, data;
	int is_address_set = 0;
	int is_data_set = 0;

	while ((opt = getopt(argc, argv, "a:d:q")) != -1) {
		switch (opt) {
			case 'a':
				address = (int) strtol(optarg, NULL, 0);
				is_address_set = 1;
				if (address >= 0x1 << ADDRESS_N) {
					fprintf(stderr, "Address out of range, %u bit(s) max (up to 0x%x)\n", ADDRESS_N, ( 0x1 << ADDRESS_N ) - 1);
					return EXIT_ERROR;
				}
				break;
			case 'd':
				data = (int) strtol(optarg, NULL, 0);
				is_data_set = 1;
				if (data >= 0x1 << DATA_N) {
					fprintf(stderr, "Data out of range, %u bit max(s) (up to 0x%x)\n", DATA_N, ( 0x1 << DATA_N ) - 1);
					return EXIT_ERROR;
				}
				break;
			case 'q':
				is_quiet = 1;
				break;
			default:
				usage(argv[0]);
				return EXIT_ERROR;
		}
	}

	if (is_address_set && is_data_set) {
		if (! is_quiet) {
			printf("Address 0x%x\n", address);
			printf("Data    0x%x\n", data);
		}
	} else {
		usage(argv[0]);
		return EXIT_ERROR;
	}

	config_bus();

	// print time scale to analyse bus
	if (! is_quiet) {
		printf("\n");
		for (int i = 0; i < 2 + ADDRESS_N + 1 + DATA_N + 1; ++i) {
			printf("% 9d|", i);
		}
		printf("\n");
	}

	// Send Start symbol
	set_bus_state(BUS_LOW);
	wait(18);
	set_bus_state(BUS_HIGH);
	wait(2);

	// Send Address symbols
	for (int i = 0; i < ADDRESS_N; ++i) {
		set_bus_state(BUS_LOW);
		if ((address >> i) & 0x1 ) { // check only nth bit, LSB first
			wait(8);
			set_bus_state(BUS_HIGH);
			wait(2);
		} else {
			wait(2);
			set_bus_state(BUS_HIGH);
			wait(8);
		}
	}

	// Send and read ACK symbol
	set_bus_state(BUS_LOW);
	wait(2);
	set_bus_state(BUS_HIGH);
	wait(3);
	if (get_bus_state() == BUS_HIGH) {
		return EXIT_ADDR_NACK;
	}
	wait(5);

	// Send Data symbols
	for (int i = 0; i < DATA_N; ++i) {
		set_bus_state(BUS_LOW);
		if ((data >> i) & 0x1 ) { // check only nth bit, LSB first
			wait(8);
			set_bus_state(BUS_HIGH);
			wait(2);
		} else {
			wait(2);
			set_bus_state(BUS_HIGH);
			wait(8);
		}
	}

	// Send and read ACK symbol
	set_bus_state(BUS_LOW);
	wait(2);
	set_bus_state(BUS_HIGH);
	wait(3);
	if (get_bus_state() == BUS_HIGH) {
		return EXIT_DATA_NACK;
	}
	wait(5);

	if (! is_quiet) {
		printf("\n\n");
	}

	return EXIT_OK;
}
