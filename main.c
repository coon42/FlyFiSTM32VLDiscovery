#include "stm32f10x.h"


void init_USART1(int baudrate)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	USART_ClockInitTypeDef USART_ClockInitStructure;

	//enable bus clocks
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);

	//Set USART1 Tx (PA.09) as AF push-pull
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	//Set USART1 Rx (PA.10) as input floating
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	USART_ClockStructInit(&USART_ClockInitStructure);
	USART_ClockInit(USART1, &USART_ClockInitStructure);

	USART_InitStructure.USART_BaudRate = baudrate;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;

	//Write USART1 parameters
	USART_Init(USART1, &USART_InitStructure);

	//Enable USART1
	USART_Cmd(USART1, ENABLE);
}


/*
  Achtung!!
  HSE_VALUE = 8000000 als globales Symbol setzen (Quarzfrequenz 8MHz)
  Defaultwert ist 25MHz, sonst stimmt die PLL nicht.)
  Systemtakt wird dann automatisch auf 168MHz gesetzt.

  Alternativ: HSE_VALUE in stm32f4xx.h auf 8000000 setzen

  und:

  in system_stm32f4xx.c müssen folgende Einstellungen für die PLL von 8Mhz auf 168MHz stehen:

  #define PLL_M      8
  #define PLL_N      336

*/



uint8_t firstRun = 1; // Used for one-run-only stuffs;

//First pin being used for floppies, and the last pin.  Used for looping over all pins.
const uint8_t FIRST_STEP_PIN = 0;
const uint8_t FIRST_DIRECTION_PIN = 5;
const uint8_t FLOPPY_COUNT = 8;

#define RESOLUTION 40 //Microsecond resolution for notes
#define LOW 0
#define HIGH 1
#define FORWARD LOW
#define BACKWARD HIGH



///
#define MAX_STRLEN 32 // this is the maximum string length of our string in characters
volatile char received_string[MAX_STRLEN+1]; // this will hold the recieved string
///


/*An array of maximum track positions for each step-control pin. 3.5" Floppies have
 80 tracks, 5.25" have 50.  These should be doubled, because each tick is now
 half a position (use 158 and 98).
 When using all possible tracks, sometimes the head will make a clicking noise, when reaching the end.
 To avoiding damage of the head, there should be some space
 */
uint8_t MAX_POSITION[] = {
  120,120,120,120,120,120,120,120};

// Array to track the current position of each floppy head.
uint8_t currentPosition[] = {
  0,0,0,0,0,0,0,0};

// Array to keep track of state of the step pins
uint8_t currentDirection[] = {
	FORWARD, FORWARD, FORWARD, FORWARD, FORWARD, FORWARD, FORWARD, FORWARD
};

// Array to keep track of the state of the direction pins
uint8_t currentStepPinState[] = {
	LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW
};

//Current period assigned to each pin.  0 = off.  Each period is of the length specified by the RESOLUTION
//variable above.  i.e. A period of 10 is (RESOLUTION x 10) microseconds long.
uint16_t currentPeriod[] = {
  0,0,0,0,0,0,0,0
};

//Current tick
uint16_t currentTick[] = {
  0,0,0,0,0,0,0,0
};

void Delay(__IO uint32_t nCount) {
  while(nCount--) {
  }
}


void togglePin(uint8_t step_pin, uint8_t direction_pin) {

	// calculate index for arrays once
	uint8_t step_index = step_pin - FIRST_STEP_PIN;
	uint8_t direction_index = direction_pin - FIRST_DIRECTION_PIN;

	//Switch directions if end has been reached
	if (currentPosition[step_index] >= MAX_POSITION[step_index]) {
	currentDirection[direction_index] = BACKWARD;
	GPIO_SetBits(GPIOB, 1 << direction_pin);
	}
	else if (currentPosition[step_index] <= 0) {
	  currentDirection[direction_index] = FORWARD;
	  GPIO_ResetBits(GPIOB, 1 << direction_pin);
	}

	//Update currentPosition
	if (currentDirection[direction_index] == FORWARD){
		currentPosition[step_index]++;
	}
	else {
		currentPosition[step_index]--;
	}

	//Pulse the control pin
	if(currentStepPinState[step_index])
	  GPIO_SetBits(GPIOA, 1 << step_pin);
	else
	  GPIO_ResetBits(GPIOA, 1 << step_pin);

	currentStepPinState[step_index] = ~currentStepPinState[step_index];
}


/* This function is used to transmit a string of characters via
 * the USART specified in USARTx.
 *
 * It takes two arguments: USARTx --> can be any of the USARTs e.g. USART1, USART2 etc.
 * 						   (volatile) char *s is the string you want to send
 *
 * Note: The string has to be passed to the function as a pointer because
 * 		 the compiler doesn't know the 'string' data type. In standard
 * 		 C a string is just an array of characters
 *
 * Note 2: At the moment it takes a volatile char because the received_string variable
 * 		   declared as volatile char --> otherwise the compiler will spit out warnings
 * */
	void USART_puts(USART_TypeDef* USARTx, volatile char *s){

		while(*s){
		// wait until data register is empty
		while( !(USARTx->SR & 0x00000040) );
		USART_SendData(USARTx, *s);
		*s++;
	}
}

//***********************************************************************************************
// TImer 2 handler Wiederholrate: 40ms
//
//***********************************************************************************************

void TIM2_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
	{
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

		// If there is a period set for control pin 2, count the number of
		// ticks that pass, and toggle the pin if the current period is reached.
		for(uint8_t i = 0; i < FLOPPY_COUNT; i++){
			if (currentPeriod[i] > 0){
				currentTick[i]++;

				if (currentTick[i] >= currentPeriod[i]){
					togglePin(i + FIRST_STEP_PIN, i + FIRST_DIRECTION_PIN);
					currentTick[i]=0;
				}
			}
		}
	}
}
//***********************************************************************************************
// System-Einstellungen
//***********************************************************************************************
void SetupSystem (void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;

	// The Timer clocks with 24Mhz
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	// Time Base initialisieren

	TIM_TimeBaseInitStructure.TIM_Period = RESOLUTION - 1; // 1 MHz down to 1 KHz (1 ms)
	TIM_TimeBaseInitStructure.TIM_Prescaler = 24 - 1; // 24 MHz Clock down to 1 MHz (adjust per your clock)
	TIM_TimeBaseInitStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);

	// Interrupt enable for update
	TIM_ITConfig(TIM2,TIM_IT_Update,ENABLE);

	// start the timer
	TIM_Cmd(TIM2,ENABLE);

	NVIC_InitTypeDef NVIC_InitStructure;

	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0; // The priority --> smaller number = higher priority
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;

	USART_puts(USART1, "initialisiere NVIC für Timer2\n");
	NVIC_Init(&NVIC_InitStructure);
	USART_puts(USART1, "Timer2 initialisiert\n");
}

//***********************************************************************************************
// Floppy Pin-Einstellungen
//***********************************************************************************************


void SetupFloppyPins (void) {
	GPIO_InitTypeDef  GPIO_InitStructure;

	/////////////////////////////////////////
	// Enable GPIOA clock (direction pins) //
	/////////////////////////////////////////

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	// define GPIO pins as output
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 |
								  GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);


	/////////////////////////////////////
	// Enable GPIOB clock (clock pins) //
	/////////////////////////////////////

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	// define GPIO pins as output
	GPIO_InitStructure.GPIO_Pin =   GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 |
									GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);


	///////////////////////////////////////////////
	// // Enable GPIOC clock (drive select pins) //
	///////////////////////////////////////////////

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

	// define GPIO pins as output
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 |
								  GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
}


//Resets all the pins
void resetAll(){
  //Stop all notes (don't want to be playing during/after reset)
  for (uint8_t p=FIRST_STEP_PIN;p<FLOPPY_COUNT;p++){
    currentPeriod[p] = 0; // Stop playing notes
  }

  // Select all drives
  GPIO_ResetBits(GPIOC, 0xFF);

  // New all-at-once reset
  for (uint8_t s=0;s<80;s++){ // For max drive's position
    for (uint8_t p=FIRST_STEP_PIN;p<FLOPPY_COUNT;p++){
    	GPIO_SetBits(GPIOB, 1 << (p + FIRST_DIRECTION_PIN)); // Go in reverse
    	GPIO_SetBits(GPIOA, 1 << p); // step
    	Delay(1000);
    	GPIO_ResetBits(GPIOA, 1 << p); // step
    }
    Delay(1000);
  }

  // deselect all drives
  GPIO_SetBits(GPIOC, 0xFF);

  for (uint8_t p=FIRST_STEP_PIN;p<=FLOPPY_COUNT;p+=2){
    currentPosition[p] = 0; // We're reset.
    GPIO_ResetBits(GPIOB, 1 << (p+1));
    currentDirection[p+1] = 0; // Ready to go forward.
  }
}



int main(void)
{
	  SystemCoreClockUpdate();

	  init_USART1(9600); // initialize USART1 @ 9600 baud
	  SetupSystem();        // System-Einstellungen
	  SetupFloppyPins();    // Peripherie-Einstellungen


	  USART_puts(USART1, "Orgel initialisiert.\n"); // just send a message to indicate that it works

	  while (1){
		  //The first loop, reset all the drives, and wait 2 seconds...
		  if (firstRun)
		  {
		    firstRun = 0;
		    resetAll();
		    Delay(50000);
		  }

		  if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET)
		  {
			  uint16_t t = USART_ReceiveData(USART1);

			  if(t == 'v') // debug: vor (TODO: not working anymore, fix!)
			  {
					USART_puts(USART1, "Floppy vor\n");

					//GPIO_ResetBits(GPIOC, 1 << (2 + 1));
					GPIO_ResetBits(GPIOB, 1 << 3); // Go forward
					GPIO_SetBits(GPIOB, 1 << 2);
					Delay(500);
					GPIO_ResetBits(GPIOB, 1 << 2);
					//GPIO_SetBits(GPIOC, 1 << (2 + 1));
			  }
			  else if(t == 'z') //debug: zurück (TODO: not working anymore, fix!)
			  {
					USART_puts(USART1, "Floppy rueck\n");

					//GPIO_ResetBits(GPIOC, 1 << (2 + 1));
					GPIO_SetBits(GPIOB, 1 << 3); // Go backward
					GPIO_SetBits(GPIOB, 1 << 2);
					Delay(500);
					GPIO_ResetBits(GPIOB, 1 << 2);
					//GPIO_SetBits(GPIOC, 1 << (2 + 1));
			  }
			  else if(t == 100)
			  {
				  resetAll();
				  USART_puts(USART1, "Floppy Reset\n"); // just send a message to indicate that it works

				  //Flush any remaining messages. (macht das hider überhaupt sinn, da interrupt!?)
				  // Flush Receive-Buffer (entfernen evtl. vorhandener ungültiger Werte)

				  //while(USART_GetITStatus(USART1, USART_IT_RXNE) == RESET) // Wait for Char
				//	  USART_ReceiveData(USART1);
			  }
			  else
			  {
				  uint8_t pin = t/2 - 1; // arduino version (moppy software) start with pin 2, stmboard starts with pin 0
				  uint8_t high;
				  uint8_t low;


				  while(USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET); // Wait for Char
				  high = USART_ReceiveData(USART1);

				  while(USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET); // Wait for Char
				  low = USART_ReceiveData(USART1);

				  currentPeriod[pin] = high << 8 | low;

				  if(currentPeriod[pin])
					  GPIO_ResetBits(GPIOC, 1 << pin);
				  else
					  GPIO_SetBits(GPIOC, 1 << pin);
			  }
		  }
	  }
}
