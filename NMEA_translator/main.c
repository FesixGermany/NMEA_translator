/*
 * Created: 09.04.2020
 * Last update: 01.05.2020
 * Author : Fesix (ham radio callsign DF3LIX)
 *
 * This firmware collects the $GPRMC frame from GPS receiver EM-406a and translates it for a Kenwood TH-D7 handheld two way radio for using APRS.
 * GPS receiver puts out some form of NMEA standard protocol with 4800 baud but the TH-D7 wants it in a slightly different format.
 *
 * Original data acquiring and checksum calculation by ham radio operator TA1MD, same idea but firmware written for PIC12F675 and different format on input.
 *
 * This was written for use with ATtiny45 but program went too big and failed to run.
 * Works fine on ATtiny85.
 *
 * I do not call myself a programmer, writing this code was a long nerve wracking journey for me, I'm sure there are ways to write the code shorter and better but works for me.
 *
 * Data input: PB2
 * Data output: PB0 or PB1
 */ 


#define F_CPU 8000000UL						// use external 8MHz crystal

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>

#define DataOutPort	PB1						// choose between PB0 and PB1 for tiny85

// variables
enum {idle, RX, TX};
volatile uint8_t state;

volatile uint16_t bitcounter = 0;
volatile uint8_t rxtemp = 0;
volatile uint8_t ByteReceived = 0;
uint16_t frame = 0;

char utc[6], status, lat[8] = {"0000000N"}, lon[9] = {"00000000E"}, speed[6] = {"000000"}, course[3] = {"000"}, date[6], checksum[2];
char temp;
uint8_t i;
uint8_t counter;
char OutputString[67];

// declarations of functions
void SendCharacter(char data);
char ReceiveCharacter(void);
void calc_cs(void);

/*** main program ***/
int main(void)
{
	// define outputs
	DDRB |= (1 << DataOutPort);
	
	// set up timer
	TCCR0A |= (1 << WGM01);					// CTC mode
	TCCR0B |= (1 << CS01);					// prescaler 8
	
	// set up interrupts
	MCUCR |= (1 << ISC01);					// INT0 (PB2) falling edge
	
	PORTB |= (1 << DataOutPort);			// idle state of TX channel high
	
	while((PINB & (1 << PB2)) == 0)			// wait until RX line is high
	{
	}
	
	state = idle;							// initial state
	
	sei();									// activate interrupts
	
	// main loop
	while(1)
	{
		START:								// start point
		
		// wait until $GPRSM frame otherwise go to start
		while(ReceiveCharacter() != '$')
		{
		}
		if(ReceiveCharacter() != 'G')
			goto START;
		if(ReceiveCharacter() != 'P')
			goto START;
		if(ReceiveCharacter() != 'R')
			goto START;
		if(ReceiveCharacter() != 'M')
			goto START;
		if(ReceiveCharacter() != 'C')
			goto START;
		
		ReceiveCharacter();					// ,
		for(i=0; i<6; i++)					// receive utc time hhmmss
			utc[i] = ReceiveCharacter();
		while(ReceiveCharacter() != ',')	// ignore .sss
		{
		}
		status = ReceiveCharacter();		// receive status (data valid/invalid)
		ReceiveCharacter();					// ,
		lat[0] = ReceiveCharacter();		// receive latitude
		lat[1] = ReceiveCharacter();
		lat[2] = ReceiveCharacter();
		lat[3] = ReceiveCharacter();
		ReceiveCharacter();					// .
		lat[4] = ReceiveCharacter();
		
		if(lat[0] == ',')					// if no position and movement data available
		{
			for(i = 0; i < 5; i++)			// clear received characters
				lat[i] = '0';
			
			goto SKIPACQ;					// skip to acquisition of date
		}
		
		lat[5] = ReceiveCharacter();
		lat[6] = ReceiveCharacter();
		ReceiveCharacter();					// ignore 4th decimal place
		ReceiveCharacter();					// ,
		lat[7] = ReceiveCharacter();		// N/S
		ReceiveCharacter();					// ,
		lon[0] = ReceiveCharacter();		// receive longitude
		lon[1] = ReceiveCharacter();
		lon[2] = ReceiveCharacter();
		lon[3] = ReceiveCharacter();
		lon[4] = ReceiveCharacter();
		ReceiveCharacter();					// .
		lon[5] = ReceiveCharacter();
		lon[6] = ReceiveCharacter();
		lon[7] = ReceiveCharacter();
		ReceiveCharacter();					// ignore 4th decimal place
		ReceiveCharacter();					// ,
		lon[8] = ReceiveCharacter();		// E/W
		ReceiveCharacter();					// ,
		
		// receive speed (knots) 1 to 4 digits
		for(i = 0; i < 6; i ++)				// reset variables with 0
			speed[i] = '0';
		counter = 0;
		i = 0;
		while(ReceiveCharacter() != '.')	// receive digits and count them
		{
			speed[i] = ByteReceived;
			counter ++;
			i ++;
			
			if(speed[0] == ',')				// if no movement data received
			{
				speed[0] = '0';				// reset value
				ReceiveCharacter();			// skip second ','
				goto SKIPACQ;				// skip to acquisition of date
			}
		}
		speed[4] = ReceiveCharacter();		// first decimal place
		speed[5] = ReceiveCharacter();		// second decimal place
		ReceiveCharacter();					// ,
		for(i = 0; i < 4; i ++)				// convert according to number of received digits
			speed[3 - i] = speed[0 - i + counter - 1];
		
		if(counter != 4)					// fill space left from digits with 0
		{
			for(i = 0; i < 4 - counter; i ++)
				speed[i] = '0';
		}
		
		// receive course (degrees) 1 to 3 digits
		for(i = 0; i < 3; i ++)				// reset variables
			course[i] = '0';
		counter = 0;
		i = 0;
		while(ReceiveCharacter() != '.')	// receive digits and count them
		{
			course[i] = ByteReceived;
			counter ++;
			i ++;
		}
		while(ReceiveCharacter() != ',')	// ignore decimal places
		{
		}
		for(i = 0; i < 3; i ++)				// convert according to number of received digits
			course[2 - i] = course[0 - i + counter - 1];
		
		if (counter != 4)					// fill space left from digits with 0
		{
			for(i = 0; i < 3 - counter; i ++)
				course[i] = '0';
		}
		
		SKIPACQ:							// jump here if no position and/or movement data available
		
		date[0] = ReceiveCharacter();		// receive date ddmmyy
		date[1] = ReceiveCharacter();
		date[2] = ReceiveCharacter();
		date[3] = ReceiveCharacter();
		date[4] = ReceiveCharacter();
		date[5] = ReceiveCharacter();
		
		calc_cs();							// calculate checksum of new frame
		
		// create the new GPRMC frame and transmit
		sprintf(OutputString, "$GPRMC,%c%c%c%c%c%c,%c,%c%c%c%c.%c%c%c,%c,%c%c%c%c%c.%c%c%c,%c,%c%c%c%c.%c%c,%c%c%c.00,%c%c%c%c%c%c,,,N*%c%c", utc[0], utc[1], utc[2], utc[3], utc[4], utc[5], status, lat[0], lat[1], lat[2], lat[3], lat[4], lat[5], lat[6], lat[7], lon[0], lon[1], lon[2], lon[3], lon[4], lon[5], lon[6], lon[7], lon[8], speed[0], speed[1], speed[2], speed[3], speed[4], speed [5], course[0], course[1], course[2], date[0], date[1], date[2], date[3], date[4], date[5], checksum[0], checksum[1]);
		for(i = 0; i < 67; i++)
			SendCharacter(OutputString[i]);
			
		SendCharacter(0x0D);				// carriage return
		SendCharacter(0x0A);				// line feed
	}
}

/*** interrupts ***/
ISR (INT0_vect)
{
	GIMSK &= ~(1 << INT0);			// deactivate INT0
	
	state = RX;						// change state
	
	_delay_us(312);					// wait half the startbit plus one bit
	TIMSK |= (1 << OCIE0A);			// output compare match interrupt enable
	OCR0A = 205;					// 208us
	TCNT0 = 0;						// reset timer
	
	GIFR = (1 << INTF0);			// clear interrupt flag
}

ISR (TIMER0_COMPA_vect)
{
	// receiving
	if(state == RX)
	{
		if(bitcounter < 8)						// receive 8 bits
		{
			rxtemp = (PINB & (1 << PB2)) << 5;	// read bit and shift left
			ByteReceived = ByteReceived >> 1;	// shift right
			ByteReceived |= rxtemp;				// write bit
			
			bitcounter++;						// increase counter
		}
		
		if(bitcounter == 8)						// if 8 bits received
		{
			bitcounter = 0;						// reset counter
			TIMSK &= ~(1 << OCIE0A);			// stop timer
			_delay_us(208);						// wait until half bit idle
			state = idle;						// change state
		}
	}
	
	// transmitting
	if(state == TX)
	{
		if(bitcounter < 11)						// send 11 bits (1 start, 8 data, 1 stop, 1 idle)
		{
			if((frame & 1) == 1)				// if bit = 1 drive TX line high
				PORTB |= (1 << DataOutPort);
			else
				PORTB &= ~(1 << DataOutPort);	// if bit = 0 drive TX line low
			frame = frame >> 1;					// shift data in frame
			
			bitcounter++;						// increase counter
		}
		
		if(bitcounter == 11)					// if 11 bits sent
		{
			bitcounter = 0;						// reset counter
			frame = 0;							// clear frame
			state = idle;						// change state
			TIMSK &= ~(1 << OCIE0A);			// stop timer
		}
	}
}

/*** functions ***/
void SendCharacter(char data)
{
	state = TX;						// change state
	
	frame = data << 1;				// insert data into frame and shift one for startbit
	frame |= 0b1111111000000000;	// insert stopbit and idle
	
	TIMSK |= (1 << OCIE0A);			// output compare match interrupt enable
	OCR0A = 205;					// 208us
	TCNT0 = 0;						// reset timer
	
	while(state == TX)				// wait until transmission is complete
	{
	}
	
	GIFR = (1 << INTF0);			// clear interrupt flag (not necessary in theory but otherwise INT0 kept retriggering without actual event)
}

char ReceiveCharacter(void)
{
	GIMSK |= (1 << INT0);			// activate INT0 for incoming startbit
	
	while(state == idle)			// wait until startbit comes in
	{
	}
	
	while(state == RX)				// wait until reception complete
	{
	}
	
	GIFR = (1 << INTF0);			// clear interrupt flag (not necessary in theory but otherwise INT0 kept retriggering without actual event)
	
	return ByteReceived;
}

void calc_cs(void)					// this is directly taken from ham radio operator TA1MD, just minor changes
{
	int cs = 0x35;
	int i;
	
	for(i=0; i<6; i++)
		cs ^= utc[i];
	cs ^= status;
	for(i=0; i<8; i++)
		cs ^= lat[i];
	for(i=0; i<9; i++)
		cs ^= lon[i];
	for(i=0; i<4; i++)
		cs ^= speed[i];
	for(i=0; i<3; i++)
		cs ^= course[i];
	for(i=0; i<6; i++)
		cs ^= date[i];
	
	checksum[0] = (cs / 16) + 48;
	checksum[1] = (cs % 16) + 48;
	if(checksum[0] > 57)
		checksum[0] += 7;
	if(checksum[1] > 57)
		checksum[1] += 7;
}

