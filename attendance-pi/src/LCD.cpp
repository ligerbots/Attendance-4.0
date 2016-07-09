#include "LCD.h"
#include "ANSI.h"

#include <stdio.h>
#include <stdexcept>
#include <bcm2835.h>
#include <unistd.h>

#define I2C_SLAVE_ADDRESS	0x27

#define BACKLIGHT_BIT	0b00001000
#define ENABLE_BIT		0b00000100
#define RDWR_BIT		0b00000010
#define REGSELECT_BIT	0b00000001

/*!	@section mod_init Module Initialization
 *
 * 	The initialization process for the LCD module first attempts to initialize
 * 	the bcm2835's I2C interface, and if successful, proceeds to the
 * 	configuration of the LCD module itself.  If the I2C initialization fails,
 * 	the method throws an exception of type std::runtime_error().  The process
 * 	for initializing the LCD module involves shifting the device into 4-bit
 * 	mode, specifying the character mode, and specifying other miscellaneous
 * 	operating parameters.
 *
 * 	@image html lcd_flow.png
 *
 *	@section lcd_init LCD Initialization
 *
 *	The LCD must be properly initialized before it can be used. Initialization
 *	configures the device to use a 4-bit communication model instead of the
 *	standard 8-bit one. The initialization process also configures settings on
 *	the device such as disabling the blinking cursor, hiding the cursor
 *	altogether, and disabling scrolling.
 *
 *	@section lcd_hwdoc LCD Communication Protocol
 *
 *	Following initialization, the LCD is set to operate in 4-bit mode.  The
 *	4 bits used for communication correspond to the higher 4 bits of the raw
 *	i2c message, as shown in the below chart:
 *
 *	@image html lcd_cmdbyte.png
 *
 *	The <b>BL</b> bit is used to determine weather or not the display's LED
 *	backlight should be turned on or off.<br>
 *	The <b>EN</b> bit is used to "push" the state of D1-D4 into the memory of
 *	the LCD controller.  After data has been written to pins D1-D4, the EN pin
 *	is pulled high momentarily to instruct the controller to read the 4 bits
 *	available.<br>
 *	The <b>R/W</b> bit is used to change the device between read and write
 *	mode.<br>
 *	The <b>RS</b> bit selects the target register to write data to.  When high,
 *	data is interpreted as a command, such as clear or go home.  When low, data
 *	is interpreted as a character.
 *
 *	Because the LCD requires 8 bits of data to perform an action, the command
 *	is split in half, with the higher 8 bits being sent first, followed by the
 *	second immediately after.
 *
 *	More complete documentation of the commands and character codes used by the
 *	LCD module can be found on its datasheet, which can be found
 *	<a href="https://www.sparkfun.com/datasheets/LCD/GDM1602K-Extended.pdf">
 *	here</a>
 */
namespace LCD {

	///Weather or not the LCD backlight is on. true = on, false = off.
	bool backlight = false;
	///Register selector. true = character register, false = command register.
	bool regSelect = false;
	///Read/write register.  Currently unused.
	bool readWrite = false;

	/*!	LCD Initialization Method.
	 *
	 * 	This method initializes the bcm2835's I2C interface, then sends the
	 * 	proper messages to configure the LCD device.
	 */
	void init() {
		//Initialize the LCD
		printf("[" WHITE "----" RESET "] Initializing LCD...");

		//Initialize the I2C connection
		if(!bcm2835_i2c_begin()) {
			//Failed to initialize I2C
			printf("\r[" RED "FAIL\n" RESET);
			throw std::runtime_error("Failed to initialize I2C");
		}

		//Set the display slave address
		bcm2835_i2c_setSlaveAddress(I2C_SLAVE_ADDRESS);

		//Set display to command mode
		regSelect = false;
		//Send the initialization data
		write(0b0010);	usleep(5000);	//Set mode to 4-bit
		write(0b1000);	usleep(5000);	//Set to 2 line, 5x8 character mode

		write(0b0000);
		write(0b1000);	usleep(5000);	//Set display off, cursor off, and blink off

		write(0b0000);
		write(0b0001);	usleep(5000);	//Clear display and move cursor home

		write(0b0000);
		write(0b0110);	usleep(5000);	//Set cursor to increment right, don't shift screen

		write(0b0000);
		write(0b1100);	usleep(5000);	//Turn the display back on

		//Turn the backlight on
		backlight = true;

		//Test message
		writeMessage("TESTING TESTING",0);

		//Success
		printf("\r[" GREEN "OKAY\n" RESET);
	}

	/*!	LCD Destruction Method.
	 *
	 * 	This method terminates the bcm2835's I2C interface, contributing to a
	 * 	more graceful shutdown sequence.
	 */
	void destroy() {
		//Destroy the LCD
		printf("[" WHITE "----" RESET "] Destroying LCD...");

		bcm2835_i2c_end();

		//Success
		printf("\r[" GREEN "OKAY\n" RESET);
	}

	/*!	LCD write method.
	 *
	 * 	@warning This method should never be called directly, as doing so may
	 * 	result in i2c message synchronization issues.
	 *
	 * 	This method converts a 4-bit nybble destined for the LCD into a properly
	 * 	formatted 8-bit message which includes necessary bits for identifying
	 * 	the target register and weather or not the backlight should be enabled.
	 *
	 * 	@param byte	Byte to write to the LCD.  Note that the 4 higher order bits
	 * 		will be ignored.
	 */
	void write(char byte) {
		//Shift the byte
		byte = byte << 4;

		//Set the backlight bit
		if(backlight) { byte += BACKLIGHT_BIT; }
		//Set the register select bit
		if(regSelect) { byte += REGSELECT_BIT; }

		//Write low
		writeRaw(byte);
		usleep(1500);

		//Write high
		writeRaw(byte | ENABLE_BIT);
		usleep(1500);

		//Write low
		writeRaw(byte);
		usleep(1500);
	}

	/*!	Character write method.
	 *
	 * 	This method takes the given character, encodes it so that it can be
	 * 	understood by the LCD, specifies the proper register, and then sends it
	 * 	to the LCD.
	 *
	 *	@param c	Character to display.
	 */
	void writeChar(char c) {
		//Get the character code
		char ccode = encodeChar(c);
		//Split the character into two messages
		char m1 = (ccode & 0b11110000) >> 4;
		char m2 = (ccode & 0b00001111);
		//Set mode to character
		regSelect = true;
		//Write the two messages
		write(m1);
		write(m2);
	}

	/*!	Write a raw byte to the IO expander.
	 *
	 * 	This method writes an 8-bit message directly to the IO expander on the
	 * 	back of the LCD module.
	 *
	 * 	@param byte	The byte to write to the IO expander.
	 */
	int writeRaw(char byte) {
		//Convert to character array
		char message[] { byte };
		//Send message
		return bcm2835_i2c_write(message, 1);
	}

	/*! Write a message to the display.
	 *
	 * 	This method writes the null-terminated string @p message to the
	 * 	display.
	 *
	 * 	@param message	Null-terminated string to display
	 */
	void writeMessage(char* message) {
		//Encode the message
		int i = 0;
		while(true) {
			//Check if end of message
			if(message[i] == '\0') { return; }
			//Write encoded character
			writeChar(message[i]);
			//Increment counter
			i++;
		}
	}

	/*!	Clear the display.
	 *
	 *	This method does not clear the display. It doesn't. Nope. Not at all.
	 *	I don't know where you got that idea.
	 */
	void clear() {
		//Set mode to command
		regSelect = false;
		//Send the command
		write(0b0000);
		write(0b0001);
	}

	/*!	Return cursor to home.
	 *
	 * 	This method moves the cursor position back to zero.
	 */
	void home() {
		//Set mode to command
		regSelect = false;
		//Send the command
		write(0b0000);
		write(0b0010);
	}

	/*!	Go to the specified position.
	 *
	 * 	This method instructs the LCD module to change the cursor position to
	 * 	the value of @p pos.
	 *
	 * 	@param pos	The position to move the LCD cursor to.
	 */
	void goTo(int pos) {

	}

	/*!	Character encoder.
	 *
	 * 	This method takes the input character c, and returns the corresponding
	 * 	LCD character code, if found.
	 *
	 * 	@param c	Input character
	 * 	@return		LCD character code, or "?" if the character was not
	 * 		recognized.
	 */
	char encodeChar(char c) {
		switch(c) {
			case ' ': return 0b00100000;
			case '!': return 0b00100001;
			case '"': return 0b00100010;
			case '#': return 0b00100011;
			case '$': return 0b00100100;
			case '%': return 0b00100101;
			case '&': return 0b00100110;
			case '\'':return 0b00100111;
			case '(': return 0b00101000;
			case ')': return 0b00101001;
			case '*': return 0b00101010;
			case '+': return 0b00101011;
			case ',': return 0b00101100;
			case '-': return 0b00101101;
			case '.': return 0b00101110;
			case '/': return 0b00101111;

			case '0': return 0b00110000;
			case '1': return 0b00110001;
			case '2': return 0b00110010;
			case '3': return 0b00110011;
			case '4': return 0b00110100;
			case '5': return 0b00110101;
			case '6': return 0b00110110;
			case '7': return 0b00110111;
			case '8': return 0b00111000;
			case '9': return 0b00111001;
			case ':': return 0b00111010;
			case ';': return 0b00111011;
			case '<': return 0b00111100;
			case '=': return 0b00111101;
			case '>': return 0b00111110;
			case '?': return 0b00111111;

			case '@': return 0b01000000;
			case 'A': return 0b01000001;
			case 'B': return 0b01000010;
			case 'C': return 0b01000011;
			case 'D': return 0b01000100;
			case 'E': return 0b01000101;
			case 'F': return 0b01000110;
			case 'G': return 0b01000111;
			case 'H': return 0b01001000;
			case 'I': return 0b01001001;
			case 'J': return 0b01001010;
			case 'K': return 0b01001011;
			case 'L': return 0b01001100;
			case 'M': return 0b01001101;
			case 'N': return 0b01001110;
			case 'O': return 0b01001111;

			case 'P': return 0b01010000;
			case 'Q': return 0b01010001;
			case 'R': return 0b01010010;
			case 'S': return 0b01010011;
			case 'T': return 0b01010100;
			case 'U': return 0b01010101;
			case 'V': return 0b01010110;
			case 'W': return 0b01010111;
			case 'X': return 0b01011000;
			case 'Y': return 0b01011001;
			case 'Z': return 0b01011010;
			case '[': return 0b01011011;
			case '�': return 0b01011100;
			case ']': return 0b01011101;
			case '^': return 0b01011110;
			case '_': return 0b01011111;

			case '`': return 0b01100000;
			case 'a': return 0b01100001;
			case 'b': return 0b01100010;
			case 'c': return 0b01100011;
			case 'd': return 0b01100100;
			case 'e': return 0b01100101;
			case 'f': return 0b01100110;
			case 'g': return 0b01100111;
			case 'h': return 0b01101000;
			case 'i': return 0b01101001;
			case 'j': return 0b01101010;
			case 'k': return 0b01101011;
			case 'l': return 0b01101100;
			case 'm': return 0b01101101;
			case 'n': return 0b01101110;
			case 'o': return 0b01101111;

			case 'p': return 0b01110000;
			case 'q': return 0b01110001;
			case 'r': return 0b01110010;
			case 's': return 0b01110011;
			case 't': return 0b01110100;
			case 'u': return 0b01110101;
			case 'v': return 0b01110110;
			case 'w': return 0b01110111;
			case 'x': return 0b01111000;
			case 'y': return 0b01111001;
			case 'z': return 0b01111010;
			case '{': return 0b01111011;
			case '|': return 0b01111100;
			case '}': return 0b01111101;
			//Further characters omitted

			default: return  0b00111111;	//Question mark
		}
	}

}
