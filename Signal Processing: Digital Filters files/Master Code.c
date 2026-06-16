/*
 * MASTER CODE1.c
 *
 * Created: 2025/11/11 12:01:44
 * Author : PTA-ENG-LAB
 */ 

/*
 * New Master.c
 *
 * Created: 2025/11/10 12:51:07
 * Author : PTA-ENG-LAB
 */
 #define F_CPU 1000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdio.h>

// LCD PORTC connections
#define LCD_PORT PORTC
#define LCD_DDR  DDRC
#define RS PC5
#define EN PC4
#define D7 PC3
#define D6 PC2
#define D5 PC1
#define D4 PC0

// Signal selector pins on PORTB
#define SEL_A PB2
#define SEL_B PB4
#define SEL_C PB6
#define SEL_D PB7

// PWM output pins
#define PWM1 PB1
#define PWM2 PB3
#define PWM3 PB5

// Inline functions for signal selection on PORTB
static inline void sel_none(void){ PORTB &= ~((1<<SEL_A)|(1<<SEL_B)|(1<<SEL_C)|(1<<SEL_D)); }
static inline void sel_A(void){ sel_none(); PORTB |= (1<<SEL_A); }
static inline void sel_B(void){ sel_none(); PORTB |= (1<<SEL_B); }
static inline void sel_C(void){ sel_none(); PORTB |= (1<<SEL_C); }
static inline void sel_D(void){ sel_none(); PORTB |= (1<<SEL_D); }

// Interface variables
volatile unsigned char Keyrec;          // Last key pressed
volatile unsigned char All_Flags = 0;   // Flags for input events
#define KeyPressedFlag 0
#define ResetFlag 1

volatile char student_number[10] = {0};  // 9-digit student number plus null terminator
volatile unsigned char digit_index = 0;
volatile unsigned char last_three[3] = {0};   // Last three non-zero digits for frequency
volatile unsigned char signal_type = 0;       // Signal type 0=Square,1=Sine
volatile unsigned char freq_selector = 1;     // Frequency option from 1 to 3
volatile unsigned char stage = 0;              // Current stage of user interface

// LCD control routines (4-bit mode on PORTC)
void lcd_cmd(unsigned char cmd) {
	LCD_PORT = (LCD_PORT & 0xF0) | (cmd >> 4);
	LCD_PORT &= ~(1 << RS);
	LCD_PORT |= (1 << EN);
	_delay_us(2);
	LCD_PORT &= ~(1 << EN);
	_delay_us(100);

	LCD_PORT = (LCD_PORT & 0xF0) | (cmd & 0x0F);
	LCD_PORT |= (1 << EN);
	_delay_us(2);
	LCD_PORT &= ~(1 << EN);
	_delay_us(100);
}
void lcd_data(unsigned char data) {
	LCD_PORT = (LCD_PORT & 0xF0) | (data >> 4);
	LCD_PORT |= (1 << RS);
	LCD_PORT |= (1 << EN);
	_delay_us(2);
	LCD_PORT &= ~(1 << EN);
	_delay_us(100);

	LCD_PORT = (LCD_PORT & 0xF0) | (data & 0x0F);
	LCD_PORT |= (1 << EN);
	_delay_us(2);
	LCD_PORT &= ~(1 << EN);
	_delay_us(100);
}
void lcd_init(void) {
	LCD_DDR = 0xFF;
	LCD_PORT &= ~((1<<RS)|(1<<EN)|(1<<D7)|(1<<D6)|(1<<D5)|(1<<D4));
	_delay_ms(20);
	lcd_cmd(0x33);
	lcd_cmd(0x32);
	lcd_cmd(0x28);
	lcd_cmd(0x0C);
	lcd_cmd(0x06);
	lcd_cmd(0x01);
	_delay_ms(2);
}
void lcd_gotoxy(unsigned char x, unsigned char y) {
	unsigned char line_addr[] = {0x80, 0xC0};
	lcd_cmd(line_addr[y-1] + x-1);
	_delay_us(100);
}
void lcd_print(const char *str) {
	while (*str) lcd_data(*str++);
}
void lcd_clear(void) {
	lcd_cmd(0x01);
	_delay_ms(2);
}

// Extract last 3 non-zero digits (from entered student number)
static void last3_nonzero(const char *id_str, uint8_t *d1, uint8_t *d2, uint8_t *d3) {
	uint8_t buf[10], n=0;
	for(int i=8;i>=0;i--){
		uint8_t d = id_str[i]-'0';
		if(d != 0) buf[n++]=d;
		if(n==3) break;
	}
	*d1 = (n>=1) ? buf[0] : 5;
	*d2 = (n>=2) ? buf[1] : 5;
	*d3 = (n>=3) ? buf[2] : 5;
}

// Keypad INT0 ISR; assume identical to previous code with keypad on PORTD upper bits
ISR(INT0_vect) {
	unsigned char KeyArray[] = {2,14,0,15,7,8,9,4,5,6,13,12,11,10,3,1};
	unsigned char Buffer = PIND >> 4;
	Keyrec = KeyArray[Buffer];
	if((Keyrec <= 9) || (Keyrec>=10 && Keyrec<=15)) {
		if(Keyrec == 15) All_Flags |= (1 << ResetFlag);
		else All_Flags |= (1 << KeyPressedFlag);
	}
}

// PWM timer setups for PB1 (T1), PB3 (T2), PB5 (timer0 software toggle)
static void t1_pb1_set_freq(uint32_t fout_hz) {
	static const uint16_t prescalers[5] = {1,8,64,256,1024};
	static const uint16_t cs_bits[5] = {(1<<CS10),(1<<CS11),(1<<CS11)|(1<<CS10),(1<<CS12),(1<<CS12)|(1<<CS10)};
	DDRB |= (1<<PWM1);
	TCCR1A = (1 << COM1A0);
	TCCR1B = (1 << WGM12);
	for(uint8_t i=0;i<5;i++) {
		uint32_t ocr = (F_CPU/(2UL*prescalers[i]*fout_hz))-1UL;
		if(ocr <= 65535UL) {
			OCR1A = (uint16_t)ocr;
			TCCR1B = (1 << WGM12) | cs_bits[i];
			return;
		}
	}
	OCR1A = 65535;
	TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10);
}
static void t2_pb3_set_freq(uint32_t fout_hz) {
	static const uint16_t prescalers[7] = {1,8,32,64,128,256,1024};
	static const uint16_t cs_bits[7] = {(1<<CS20),(1<<CS21),(1<<CS21)|(1<<CS20),(1<<CS22),(1<<CS22)|(1<<CS20),(1<<CS22)|(1<<CS21),(1<<CS22)|(1<<CS21)|(1<<CS20)};
	DDRB |= (1<<PWM2);
	TCCR2A = (1<<COM2A0)|(1<<WGM21);
	for(uint8_t i=0;i<7;i++) {
		uint32_t ocr = (F_CPU/(2UL*prescalers[i]*fout_hz)) - 1UL;
		if(ocr <= 255UL) {
			OCR2A = (uint8_t)ocr;
			TCCR2B = cs_bits[i];
			return;
		}
	}
	OCR2A = 255;
	TCCR2B = (1<<CS22)|(1<<CS21)|(1<<CS20);
}
static volatile uint16_t div_pb5 = 1, cnt_pb5 = 0;
ISR(TIMER0_COMPA_vect) {
	if(++cnt_pb5 >= div_pb5) {
		PINB = (1 << PWM3);
		cnt_pb5 = 0;
	}
}
static void t0_init_1kHz_and_set_pb5(uint32_t fout_hz) {
	DDRB |= (1<<PWM3);
	TCCR0A = (1<<WGM01);
	OCR0A = 124; // 1kHz base interrupt
	TIMSK0 = (1<<OCIE0A);
	TCCR0B = (1<<CS01)|(1<<CS00);
	if(fout_hz<1) fout_hz=1;
	uint16_t d = (uint16_t)(1000UL/(2UL*fout_hz));
	if(d == 0) d = 1;
	cnt_pb5 = 0; div_pb5 = d;
}

// Display final waveform info on LCD
void display_final_output(void) {
	uint8_t f1 = (last_three[0]==0) ? 1 : last_three[0];
	uint8_t f2 = (last_three[1]==0) ? f1 : last_three[1];
	uint8_t f3 = (last_three[2]==0) ? f2 : last_three[2];
	uint32_t curr_freq = 0;
	switch(freq_selector) {
		case 1: curr_freq = f1; break;
		case 2: curr_freq = f2*10; break;
		case 3: curr_freq = f3*100; break;
		default: curr_freq = f1;
	}
	lcd_clear();
	lcd_gotoxy(1,1);
	lcd_print(signal_type ? "Sine Wave Out" : "Square Wave Out");
	lcd_gotoxy(1,2);
	char buff[17];
	snprintf(buff, sizeof(buff), "Freq=%luHz D=50%%", curr_freq);
	lcd_print(buff);
}

// -------- Main function -----------
int main(void) {
	cli(); // Disable interrupts during setup

	lcd_init();

	// Configure PORTB outputs for signal selection and PWM outputs
	DDRB |= (1<<SEL_A) | (1<<SEL_B) | (1<<SEL_C) | (1<<SEL_D);
	sel_none();

	DDRB |= (1<<PWM1) | (1<<PWM2) | (1<<PWM3);

	// Setup keypad interrupt INT0 on PD2 with pullup
	DDRD &= ~(1<<2);
	PORTD |= (1<<2);
	DDRD &= 0x0F;
	PORTD |= 0xF0;
	EICRA = 0x02; // Falling edge motion for INT0
	EIMSK |= (1<<INT0);

	sei(); // Enable global interrupts

	lcd_clear();
	lcd_gotoxy(1,1);
	lcd_print("Enter Student No:");

	while(1) {
		// Reset input and variables if requested
		if (All_Flags & (1 << ResetFlag)) {
			All_Flags &= ~(1 << ResetFlag);
			digit_index = 0;
			freq_selector = 0;
			stage = 0;
			for(uint8_t i=0; i<9; i++) student_number[i] = 0;
			lcd_clear();
			lcd_gotoxy(1,1);
			lcd_print("Enter Student No:");
		}

		// Stage 0: Student number input
		if (stage == 0 && (All_Flags & (1 << KeyPressedFlag))) {
			All_Flags &= ~(1 << KeyPressedFlag);
			if (Keyrec == 14) { // Finish input with 'A' key
				if (digit_index == 9) {
					student_number[9] = '\0';
					last3_nonzero(student_number, last_three+2, last_three+1, last_three);

					lcd_clear();
					lcd_gotoxy(1,1); lcd_print("Student No:");
					lcd_gotoxy(1,2); lcd_print(student_number);
					_delay_ms(50);

					stage = 1;
					lcd_clear();
					lcd_gotoxy(1,1); lcd_print("A:Sine");
					lcd_gotoxy(1,2); lcd_print("B:Square");
					} else {
					lcd_clear(); lcd_gotoxy(1,1); lcd_print("Need 9 digits");
					_delay_ms(50);
					lcd_clear(); lcd_gotoxy(1,1); lcd_print("Enter 9-digit ID:");
					digit_index = 0;
				}
				} else {
				if (digit_index < 9 && Keyrec <= 9) {
					student_number[digit_index++] = Keyrec + '0';
					lcd_clear();
					lcd_gotoxy(1,1);
					lcd_print(student_number);
				}
			}
		}

		// Stage 1: Waveform selection
		else if (stage == 1 && (All_Flags & (1 << KeyPressedFlag))) {
			All_Flags &= ~(1 << KeyPressedFlag);
			if (Keyrec == 10) {   // 'A' select Sine
				signal_type = 1; stage = 2;
				lcd_clear(); lcd_gotoxy(1,1); lcd_print("Sine selected");
				_delay_ms(50);
				lcd_clear(); lcd_gotoxy(1,1); lcd_print("Select freq:");
				lcd_gotoxy(1,2); lcd_print("1:F1 2:F2 3:F3");
				} else if (Keyrec == 11) { // 'B' select Square
				signal_type = 0; stage = 2;
				lcd_clear(); lcd_gotoxy(1,1); lcd_print("Square selected");
				_delay_ms(50);
				lcd_clear(); lcd_gotoxy(1,1); lcd_print("Select freq:");
				lcd_gotoxy(1,2); lcd_print("1:F1 2:F2 3:F3");
			}
		}

		// Stage 2: Frequency selection
		else if (stage == 2 && (All_Flags & (1 << KeyPressedFlag))) {
			All_Flags &= ~(1 << KeyPressedFlag);
			if (Keyrec >= 1 && Keyrec <= 3) {
				freq_selector = Keyrec;

				// Frequency values derived from last_three digits
				uint8_t f1 = (last_three[0] == 0) ? 1 : last_three[0];
				uint8_t f2 = (last_three[1] == 0) ? f1 : last_three[1];
				uint8_t f3 = (last_three[2] == 0) ? f2 : last_three[2];

				uint32_t fout = 0;
				if (freq_selector == 1) fout = f1;
				else if (freq_selector == 2) fout = f2 * 10;
				else if (freq_selector == 3) fout = f3 * 100;

				// Setup outputs and selector pins accordingly
				if (signal_type == 0) {  // Square wave mode activates raw PWM signal D
					sel_D();
					t1_pb1_set_freq(fout);  // Raw signal on PB1
					} else {  // Sine wave mode acts on channel A/B/C selector and frequencies
					if (freq_selector == 1) {
						sel_A();
						t1_pb1_set_freq(fout);
						} else if (freq_selector == 2) {
						sel_B();
						t2_pb3_set_freq(fout);
						} else if (freq_selector == 3) {
						sel_C();
						t0_init_1kHz_and_set_pb5(fout);
					}
				}
				stage = 3;
			}
		}

		// Stage 3: Display final output once
		else if (stage == 3) {
			display_final_output();
			stage = 4; // Prevent repeated display refresh
		}
	}
}
