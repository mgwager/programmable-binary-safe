/*
Michael Wager
CSE321 - Project 2
10/24/2018
*/
#ifndef F_CPU
#define F_CPU 1000000UL
#endif
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/eeprom.h>

#define SLOW 0xFF

//Stores lock code globally to carry from unlockedState to lockedState
volatile uint8_t lockCode;
volatile uint8_t unlockCode;
volatile uint8_t recentlyReleased = 0;
volatile uint8_t timer = 0;
volatile uint8_t globalTimer = 0;
volatile uint8_t count = 0;
volatile uint8_t currentState;
volatile uint8_t powerSave = 0;
//Constant press and release codes after polling
const uint8_t PRESSED = 0b11111111;
const uint8_t RELEASED = 0b00000000;

//Utility functions for simplification
void unlockedState(), lockedState(), redLED(), blueLED(), greenLED(), 
	yellowLED(), yellowStrobe(), noLED(), yellowStrobeNoRed();

//Interrupt subroutine
ISR(TIMER0_COMPA_vect) {
	//Check if key was recently released, if so increment timer
	if(recentlyReleased){
		timer = timer + 1;
	}
	//Check if recently released and timer has reached 3 seconds (converted to 16 SLOW units of time)
	if(recentlyReleased && timer >=16){
		//Timeout feedback
		yellowStrobeNoRed();
		//Reset timer variables
		timer = 0;
		recentlyReleased = 0;
		count = 0;
		//Check which states code to reset
		if(currentState){
			unlockCode = 0b11;
		}else{
			lockCode = 0b11;
		}
	}
  	// Add our interval to whatever the last value was
  	OCR0A += SLOW;
}

//Main function. Determine if device was locked when power was disconnected. If so, 
//go to lockedState(). Also determines if secretKey was pressed, if so, proceed to
//unlockedState().
int main() {
	OCR0A = SLOW;
  	TCCR0B = 0b101;
  	TIMSK = 1 << OCIE0A;
  	sei();
	//Checks designated storage space in EEPROM (register 23) if code was saved to it
	lockCode = eeprom_read_byte((uint8_t*)23);
	//Check for secretKeyPress from key0 or key1
	if((!(PINB & 0b010000)) || (!(PINB & 0b001000))){
		//Hisotry variable for polling
		uint8_t history = 0;
		//While loop required for polling, will exit once user releases secretKey
		while(1){
			history = history <<1;
			if((!(PINB & 0b010000)) || (!(PINB & 0b001000))){
				history |= 1;
			}
			if(history == PRESSED){
				blueLED();
			}
			if(history == RELEASED){
				unlockedState();
			}
		}
	}
	else if (lockCode == 0b11000000){
		//lockedState() simply returns after completion, so a fresh call to unlockedState is required after
		lockedState();
		unlockedState();
	}
	else{
		//Profram starts normally in unlockedState()
		unlockedState();
	}
}

//Unlocked state waits for 6 debounced key pressed then locks and transitions to lockedState
void unlockedState(){
	currentState = 0;
	count = 0;
	timer = 0;
	//Clear anything in our designated EEPROM register
	eeprom_write_byte((uint8_t*)23,0);
	//Local Variables
	uint8_t isPressed = 0;
	uint8_t keyPressed = 0;
	uint8_t history = 0;
	
	//Reset lock code to 0b11. The two 1's are required as placeholders in case user enters key0 for
	//first entries to bitshift properly
	lockCode = 0b00000011;
	//While loop for polling
	while (1) {
		//Green and yellow LEDs on via funtions
		greenLED();
		yellowLED();
		//Bitshift polling history
		history = history <<1;
		//Check if key1 or key0 pressed, poll for debouncing
		if((!(PINB & 0b010000)) || (!(PINB & 0b001000))){
			history |= 1;
		}
		//If key is pressed after debouncing, turn on blue LED and update isPressed to 1
		if(history == PRESSED){
			blueLED();
			isPressed = 1;
			//Determine which key is being pressed to store in our lockCode
			if(!(PINB & 0b001000)){
				keyPressed = 1;
			}
		}
		//If key is released after debouncing
		if(history == RELEASED){
			//History will typically always == RELEASED so check if flagged earlier as isPressed
			if(isPressed){
				//Update new lockCode bit, reset flags isPressed and keyPressed, and increment count
				lockCode = lockCode << 1;
				lockCode |= keyPressed;
				isPressed = 0;
				keyPressed = 0;
				count +=1;
				timer = 0;
				recentlyReleased = 1;
				
			}
		}
		//If we count reaches 6, we have enough key entries for a lockCode and can transition to lockedState()
		if(count == 6){
			//upload lockCode to register 23 in case we lose power,
			//transition to lockedState(), and once returned from lockedState(), reset lockCode
			eeprom_write_byte((uint8_t*)23, lockCode);
			recentlyReleased = 0;
			
			lockedState();
			lockCode = 0b00000011;
		}
	}
}

//Locked state waits for 6 debounced key presses and determines if they match the global
//lockCode set from unlockedState(). If so we will transition back to unlockedState();
void lockedState(){
	currentState = 1;
	unlockCode = 0b00000011;
	timer = 0;
	count = 0;
	//Local variables
	uint8_t keyPressed = 0;
	uint8_t isPressed = 0;
	uint8_t history = 0;
	//While loop for polling
	while(1){
		//Turn on red LED via funtions
		redLED();
		//Bitshift polling history
		history = history <<1;
		//Check if key1 or key0 pressed, poll for debouncing
		if((!(PINB & 0b010000)) || (!(PINB & 0b001000))){
			history |= 1;
		}
		//If key is pressed after debouncing, turn on blue LED and update isPressed to 1
		if(history == PRESSED){
			blueLED();
			isPressed = 1;
			//Determine which key is being pressed to store in our unlockCode
			if(!(PINB & 0b001000)){
				keyPressed = 1;
			}
		}
		//If key is released after debouncing
		if(history == RELEASED){
			//History will typically always == RELEASED so check if flagged earlier as isPressed
			if(isPressed == 1){
				//Update new unlockCode bit, reset flags isPressed and keyPressed, and increment count
				unlockCode = unlockCode << 1;
				unlockCode |= keyPressed;
				keyPressed = 0;
				isPressed = 0;
				count +=1;
				timer = 0;
				recentlyReleased = 1;
				
			}
		}
		//If we count reaches 6, we have enough key entries for a unlockCode and can check if
		//unlock and lockCode match.
		if(count == 6){
			//Reset count
			count = 0;
			recentlyReleased = 0;
			//Determine if we started in lockedState (indicated by the existence of data in
			//deginated EEPROM register 23)
			lockCode = eeprom_read_byte((uint8_t*)23);
			//Check if correct unlockCode was entered, if so clear EEPROM data and return to unlockedState()
			if(unlockCode == lockCode){
				eeprom_write_byte((uint8_t*)23,0);
				return;
			}else{ //Else, activate yellowStrobe() which flashes yellow light several times while
					//keeping red lit to indicate error. Reset unlockCode.
				yellowStrobe();
				unlockCode = 0b00000011;
			}
		}
	}
}

//UTILITY FUNCTIONS
//Lights red LED
void redLED(){
	DDRB = 0b0110;
	PORTB = 0b100;
	_delay_ms(1);
}
//Lights blue LED
void blueLED(){
	DDRB = 0b0110;
	PORTB = 0b010;
	_delay_ms(1);
}
//Lights green LED
void greenLED(){
	DDRB = 0b00011;
	PORTB = 0b0001;
    _delay_ms(1);
}
//Lights yellow LED
void yellowLED(){
	DDRB = 0b00011;
	PORTB = 0b0010;
	_delay_ms(1);
}
//Turns of all LEDs
void noLED(){
	DDRB = 0;
	PORTB = 0;
	_delay_ms(1);
}
//Flashes yellow LED for incorrect code while keeping red LED lit
void yellowStrobe(){
	for (uint8_t i = 0; i < 4;i++){
		DDRB = 0b101;
		PORTB = 0b110;
		_delay_ms(300);
		DDRB = 0b110;
		PORTB = 0b100;
		_delay_ms(300);
	}
}
//Flashes yellow LED for code inpput timeout without red LED
void yellowStrobeNoRed(){
	for (uint8_t i = 0; i < 4;i++){
		yellowLED();
		_delay_ms(300);
		noLED();
		_delay_ms(300);
	}
}