
#include <avr/interrupt.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
//Functionality - Sets bit on a PORTx
//Parameter: Takes in a uChar for a PORTx, the pin number and the binary value
//Returns: The new value of the PORTx
unsigned char SetBit(unsigned char pin, unsigned char number, unsigned char bin_value)
{
	return (bin_value ? pin | (0x01 << number) : pin & ~(0x01 << number));
}

////////////////////////////////////////////////////////////////////////////////
//Functionality - Gets bit from a PINx
//Parameter: Takes in a uChar for a PINx and the pin number
//Returns: The value of the PINx
unsigned char GetBit(unsigned char port, unsigned char number)
{
	return ( port & (0x01 << number) );
}
volatile unsigned char TimerFlag = 0; // TimerISR() sets this to 1. C programmer should clear to 0.

// Internal variables for mapping AVR's ISR to our cleaner TimerISR model.
unsigned long _avr_timer_M = 1; // Start count from here, down to 0. Default 1ms
unsigned long _avr_timer_cntcurr = 0; // Current internal count of 1ms ticks

// Set TimerISR() to tick every M ms
void TimerSet(unsigned long M) {
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}

void TimerOn() {
	// AVR timer/counter controller register TCCR1
	TCCR1B 	= 0x0B;	// bit3 = 1: CTC mode (clear timer on compare)
	// bit2bit1bit0=011: prescaler /64
	// 00001011: 0x0B
	// SO, 8 MHz clock or 8,000,000 /64 = 125,000 ticks/s
	// Thus, TCNT1 register will count at 125,000 ticks/s

	// AVR output compare register OCR1A.
	OCR1A 	= 125;	// Timer interrupt will be generated when TCNT1==OCR1A
	// We want a 1 ms tick. 0.001 s * 125,000 ticks/s = 125
	// So when TCNT1 register equals 125,
	// 1 ms has passed. Thus, we compare to 125.
	// AVR timer interrupt mask register

	TIMSK1 	= 0x02; // bit1: OCIE1A -- enables compare match interrupt

	//Initialize avr counter
	TCNT1 = 0;

	// TimerISR will be called every _avr_timer_cntcurr milliseconds
	_avr_timer_cntcurr = _avr_timer_M;

	//Enable global interrupts
	SREG |= 0x80;	// 0x80: 1000000
}

void TimerOff() {
	TCCR1B 	= 0x00; // bit3bit2bit1bit0=0000: timer off
}

void TimerISR() {
	TimerFlag = 1;
}

// In our approach, the C programmer does not touch this ISR, but rather TimerISR()
ISR(TIMER1_COMPA_vect)
{
	// CPU automatically calls when TCNT0 == OCR0 (every 1 ms per TimerOn settings)
	_avr_timer_cntcurr--; 			// Count down to 0 rather than up to TOP
	if (_avr_timer_cntcurr == 0) { 	// results in a more efficient compare
		TimerISR(); 				// Call the ISR that the user uses
		_avr_timer_cntcurr = _avr_timer_M;
	}
}

//--------Find GCD function --------------------------------------------------
unsigned long int findGCD(unsigned long int a, unsigned long int b)
{
	unsigned long int c;
	while(1){
		c = a%b;
		if(c==0){return b;}
		a = b;
b = c;
	}
	return 0;
}
//--------End find GCD function ----------------------------------------------

//--------Task scheduler data structure---------------------------------------
// Struct for Tasks represent a running process in our simple real-time operating system.
typedef struct _task {
	/*Tasks should have members that include: state, period,
		a measurement of elapsed time, and a function pointer.*/
	signed char state; //Task's current state
	unsigned long int period; //Task period
	unsigned long int elapsedTime; //Time elapsed since last task tick
	int (*TickFct)(int); //Task tick function
} task;

//--------End Task scheduler data structure-----------------------------------

//--------Shared Variables----------------------------------------------------
unsigned char PlayerPaddlePosition = 0x00;
unsigned char EnemyPaddlePosition = 0x00;
unsigned char BallXPosition = 0x00;
unsigned char BallYPosition = 0x00;


//--------End Shared Variables------------------------------------------------

//--------User defined FSMs---------------------------------------------------
//Enumeration of states.
enum Display_States { Disp_init, Disp_start, Disp_startSequence, PlayerOutput, BallOutput, EnemyOutput };

int SMDisplay(int state) {

	// Local Variables
unsigned char startSequenceDone = 0x00;

	//State machine transitions
switch(state){
		case Disp_init:
			state = Disp_start;
		break;
		
		case Disp_start:
		state = Disp_startSequence;
		break;
		
		case Disp_startSequence:
			state = PlayerOutput;
			
		/*	if((startSequenceDone) == 0x00){
			state = Disp_startSequence;	
			}
			else{
				state = Game;
			}
			*/
		break;
		
		case PlayerOutput:
			state = BallOutput;
		break;
		
		case BallOutput:
			state = EnemyOutput;
		break;

		case EnemyOutput:
			state = PlayerOutput;
		break;
}

	
//-----------------------------------------------------------------------------------
	//State machine actions
	switch(state){
		case Disp_init:
			PORTA = 0xFF;
		break;
		
		case Disp_start:
			PORTB = 0x0F;
		break;
		
		case Disp_startSequence:
			PORTB = 0xF0;
		break;
		
		case PlayerOutput:
		//PORTB stays the same, since player cannot move paddle up
			PORTB = 0xFE;
		//PORTA changes everytime button is pressed
			PORTA = 0x38;
		break;
		
		case BallOutput:
		//ball position will only depend from the Up-down motion 
		PORTB = ~BallYPosition;
		PORTA = BallXPosition;
		break;
		
		case EnemyOutput:
		PORTB = 0x7F;
		PORTA = 0x1C;
		break;
		
		default:
		
		break;
	break;
}
	
	return state;
}

//Enumeration of states.
enum SMBall_States { Ball_init, Ball_start,idle, Ball_Moving,Ball_Bounce};
	unsigned char ball_xMove_right =0x00;
	unsigned char ball_xMove_left =0x00;
	unsigned char ball_yMove_up =0x01;
	unsigned char ball_yMove_down =0x00;
	unsigned char indexXpos = 0x08;
// If paused: Do NOT toggle LED connected to PB0
// If unpaused: toggle LED connected to PB0
int SMBall(int state) {

	//State machine transitions
	switch (state) {
		case Ball_init:
			state = Ball_start;
		break;
		
		case Ball_start:
			state = Ball_Moving;
		break;
		case idle:
		if(~PORTC&0x04 == 0x04){
			state =  Ball_Moving;
		}
		else{
			state = idle;
		}
		break;
		case Ball_Moving:
// 		//still needs a case where it touches the corner
// 			state = Ball_Moving;
// 			
// 			if(BallYPosition == 0x80){
// 				state = Ball_Bounce;
// 			}
// 			else if(BallYPosition == 0x01){
// 				state = Ball_Bounce;
// 			}
// 			if(BallXPosition == 0x80){
// 				state = Ball_Bounce;
// 			}
// 			else if(BallXPosition == 0x01){
// 				state = Ball_Bounce;
// 			}
state = Ball_Moving;
		break;
		
		case Ball_Bounce:
			state = Ball_Moving;
		break;
		
	default:		
	break;
	}
	//B 
	// 7  -  -  -  -  -  -  -
	// 6  -  -  -  -  -  -  -
	// 5  -  -  -  -  -  -  -
	// 4  -  -  -  -  -  -  -
	// 3  -  -  -  -  -  -  -
	// 2  -  -  -  -  -  -  -
	// 1  -  -  -  -  -  -  -
	// 0  1  2  3  4  5  6  7
	
	//State machine actions
	switch(state) {
		case Ball_init:
		
		break;
		
		case Ball_start:
			BallYPosition = 0x02;
			BallXPosition = 0x08;
			ball_xMove_left = 0x01;
			ball_xMove_right = 0x00;
			indexXpos = BallXPosition;
		break;
		case idle:
			
		break;
		case Ball_Moving:
		
			//X-coordinate movement
			if((ball_xMove_left == 0x01)){
				if(BallXPosition != 0x80){
				BallXPosition = BallXPosition <<1;
				}
				else{
					
					ball_xMove_left = 0x00;
					ball_xMove_right = 0x01;
				}
			}
			if(ball_xMove_right == 0x01){
				if(BallXPosition != 0x01){
					BallXPosition = BallXPosition >>1;
				}
				else{
					BallXPosition = BallXPosition <<1;
					ball_xMove_left = 0x01;
					ball_xMove_right = 0x00;
				}
			}
	
	
			//Y-coordinate movement
			if((ball_yMove_up == 0x01)){
				if(BallYPosition != 0x40){
				BallYPosition = BallYPosition <<1;
				}
				else{
					ball_yMove_down = 0x01;
					ball_yMove_up = 0x00;
				}
			}
			if(ball_yMove_down == 0x01){
				if(BallYPosition != 0x02){
					BallYPosition = BallYPosition >>1;
				}
				else{
					BallYPosition = BallYPosition <<1;
					ball_yMove_down = 0x00;
					ball_yMove_up = 0x01;
				}
			}
			
		break;
		
		case Ball_Bounce:
	break;
		
	}
	return state;
}

//Enumeration of states.
enum Paddle_States {Paddle_init, Paddle_start, Paddle_idle, Paddle_press, Paddle_release };

unsigned char PlayerPaddlePosition;
int SMTick3(int state) {
	//State machine transitions
	switch (state) {
		case Paddle_init:
			state = Paddle_start;
		break;
		
		case Paddle_start:
			state = Paddle_idle;
		break;
		
		case Paddle_idle:
			//move left
			if(~PINC&0x80 == 0x80){
				if(PlayerPaddlePosition == 0x40){
					//I LEFT OFFF HEREEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE
				}
				state = Paddle_press;
			}
			else if(~PINC&0x40 == 0x40){
				state = Paddle_press;
			}
			else{
				state = idle;
			}
		break;
		
		case Paddle_press:
			if(~PINC&0x80 == 0x80){
				state = Paddle_press;
			}
			else if(~PINC&0x40==0x40){
				state = Paddle_press;
			}
			else{
				state = idle;
			}	
		break;
		
		case Paddle_release:
		
		break;
	
	}

	//State machine actions
	switch(state) {

	}

	return state;
}


//Enumeration of states.
enum SM4_States { SM4_display };

// Combine blinking LED outputs from SM2 and SM3, and output on PORTB
int SMTick4(int state) {
	// Local Variables

	unsigned char output;

	//State machine transitions
	switch (state) {
	case SM4_display:	break;

	default:		state = SM4_display;
				break;
	}

	//State machine actions
	switch(state) {
	case SM4_display://	output = SM2_output | SM3_output; // write shared outputs
									// to local variables
break;

	default:		break;
	}

	//PORTB = ~PORTB;	// Write combined, shared output variables to PORTB

	return state;
}

// --------END User defined FSMs-----------------------------------------------

// Implement scheduler code from PES.
int main()
{
// Set Data Direction Registers
// Buttons PORTA[0-7], set AVR PORTA to pull down logic
DDRA = 0xFF; PORTA = 0x00;
DDRB = 0xFF; PORTB = 0x00;
// . . . etc
PORTA = 0xFF;

// Period for the tasks
unsigned long int SMDisplay_calc = 5;
unsigned long int SMBall_calc = 300;
unsigned long int SMTick3_calc = 1000;
unsigned long int SMTick4_calc = 100;

//Calculating GCD
unsigned long int tmpGCD = 1;
tmpGCD = findGCD(SMDisplay_calc, SMBall_calc);
tmpGCD = findGCD(tmpGCD, SMTick3_calc);
tmpGCD = findGCD(tmpGCD, SMTick4_calc);

//Greatest common divisor for all tasks or smallest time unit for tasks.
unsigned long int GCD = tmpGCD;

//Recalculate GCD periods for scheduler
unsigned long int SMDisplay_period = SMDisplay_calc/GCD;
unsigned long int SMBall_period = SMBall_calc/GCD;
unsigned long int SMTick3_period = SMTick3_calc/GCD;
unsigned long int SMTick4_period = SMTick4_calc/GCD;

//Declare an array of tasks 
static task task1, task2, task3, task4;
task *tasks[] = { &task1, &task2, &task3, &task4 };
const unsigned short numTasks = sizeof(tasks)/sizeof(task*);

// Task 1
task1.state = 0;//Task initial state.
task1.period = SMDisplay_period;//Task Period.
task1.elapsedTime = SMDisplay_period;//Task current elapsed time.
task1.TickFct = &SMDisplay;//Function pointer for the tick.

// Task 2
task2.state = 0;//Task initial state.
task2.period = SMBall_period;//Task Period.
task2.elapsedTime = SMBall_period;//Task current elapsed time.
task2.TickFct = &SMBall;//Function pointer for the tick.

// Task 3
task3.state = 0;//Task initial state.
task3.period = SMTick3_period;//Task Period.
task3.elapsedTime = SMTick3_period; // Task current elasped time.
task3.TickFct = &SMTick3; // Function pointer for the tick.

// Task 4
task4.state = 0;//Task initial state.
task4.period = SMTick4_period;//Task Period.
task4.elapsedTime = SMTick4_period; // Task current elasped time.
task4.TickFct = &SMTick4; // Function pointer for the tick.

// Set the timer and turn it on
TimerSet(GCD);
TimerOn();

unsigned short i; // Scheduler for-loop iterator
while(1) {
	// Scheduler code
	for ( i = 0; i < numTasks; i++ ) {
		// Task is ready to tick
		if ( tasks[i]->elapsedTime == tasks[i]->period ) {
			// Setting next state for task
			tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
			// Reset the elapsed time for next tick.
			tasks[i]->elapsedTime = 0;
		}
		tasks[i]->elapsedTime += 1;
	}
	while(!TimerFlag);
	TimerFlag = 0;
}

// Error: Program should not exit!
return 0;
}
