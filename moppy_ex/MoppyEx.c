#include <stdio.h> // printf
#include "MoppyEx.h"
#include "MoppyEx_conf.h"
#include "midifile.h"

//#define printf(fmt, ...) (0) // this is a trick to ignore all printfs


void Init_TIM3(void)
{
  TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

  // Takt freigeben
  RCC_APB1PeriphClockCmd (RCC_APB1Periph_TIM3, ENABLE);

  // Time Base configuration
  TIM_TimeBaseStructInit (&TIM_TimeBaseStructure);
  TIM_TimeBaseStructure.TIM_Period = UINT16_MAX;
  TIM_TimeBaseStructure.TIM_Prescaler = (SystemCoreClock / 1000000) - 1; // resolution: every 1ms
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  TIM_TimeBaseInit (TIM3, &TIM_TimeBaseStructure);

  // Timer Interruptkonfiguration = Interrupt bei Überlauf
  TIM_ITConfig (TIM3, TIM_IT_Update, ENABLE);

  TIM_Cmd (TIM3, ENABLE);
}


static void delay_us(uint32_t microseconds)
{
	int i;
	volatile uint16_t cur = 0;
	uint32_t parts = microseconds / UINT16_MAX; // for values bigger than 16 bit ...
	uint32_t rest_us = microseconds % UINT16_MAX;

	for(i=0; i < 2 * parts; i++)
	{
		cur = 0;
		TIM3->CNT = 0;

		while( cur < UINT16_MAX / 2 )
		{
			cur = TIM3->CNT;
		}
	}

	cur = 0;
	TIM3->CNT = 0;
	while( cur < rest_us )
	{
		cur = TIM3->CNT;
	}
}

static void delay_ms(uint32_t miliseconds)
{
	uint32_t i;

	for(i = 0; i < miliseconds; i++)
		delay_us(1000);
}

static void delay_s(uint32_t seconds)
{
	uint32_t i;

	for(i = 0; i < seconds; i++)
		delay_ms(1000);
}



void floppy_play_midi_note(uint8_t channel, uint16_t midi_note)
{
	uint32_t real_period = microPeriods[midi_note] / ( 2 * MOPPYEX_RESOLUTION);
	currentPeriod[channel - 1] = real_period;

	if(currentPeriod[channel - 1])
		GPIO_ResetBits(GPIOC, 1 << channel - 1);
	else
		GPIO_SetBits(GPIOC, 1 << channel -1);
}


//***********************************************************************************************
// Floppy Pin-Einstellungen
//***********************************************************************************************


static void SetupFloppyPins (void) {
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



void SetupMoppyEx()
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;

	// The Timer clocks with 24Mhz
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	// Time Base initialisieren


	TIM_TimeBaseInitStructure.TIM_Period = MOPPYEX_RESOLUTION - 1; // 1 MHz down to 1 KHz (1 ms)
	TIM_TimeBaseInitStructure.TIM_Prescaler = (SystemCoreClock / 1000000) - 1; // resolution: every 1us
	TIM_TimeBaseInitStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);


	// Interrupt enable for update
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

	// start the timer
	TIM_Cmd(TIM2, ENABLE);

	NVIC_InitTypeDef NVIC_InitStructure;

	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0; // The priority --> smaller number = higher priority
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;

	printf("initialisiere NVIC für Timer2\r\n");
	NVIC_Init(&NVIC_InitStructure);
	printf("Timer2 initialisiert\r\n");

	SetupFloppyPins(); // Peripherie-Einstellungen
	Init_TIM3(); // for delay timing
	printf("Timer3 initialisiert\r\n");
	resetAll();

	delay_s(1); // wait 1 seconds after init.

	printf("Orgel initialisiert.\r\n");
}

static void togglePin(uint8_t step_pin, uint8_t direction_pin) {

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
    for (uint8_t p=FIRST_STEP_PIN; p < FLOPPY_COUNT; p++){
    	GPIO_SetBits(GPIOB, 1 << (p + FIRST_DIRECTION_PIN)); // Go in reverse
    	//GPIO_ResetBits(GPIOB, 1 << (p + FIRST_DIRECTION_PIN)); // Go forward
    	GPIO_SetBits(GPIOA, 1 << p); // step
    	delay_us(500);
    	GPIO_ResetBits(GPIOA, 1 << p); // step
    }
    delay_us(500);
  }

  // deselect all drives
  GPIO_SetBits(GPIOC, 0xFF);

  for (uint8_t p=FIRST_STEP_PIN;p<=FLOPPY_COUNT;p+=2){
    currentPosition[p] = 0; // We're reset.
    GPIO_ResetBits(GPIOB, 1 << (p+1));
    currentDirection[p+1] = 0; // Ready to go forward.
  }
}


void playMidiFile(const char *pFilename)
{
	_MIDI_FILE pMF;
	BOOL open_success;
	char str[128];
	int ev;
	int i = 0;


	midiFileOpen(&pMF, pFilename, &open_success);

	if (open_success)
	{
		static MIDI_MSG msg[MAX_MIDI_TRACKS];
		int i, iNum;
		unsigned int j;
		int any_track_had_data = 1;
		uint32_t current_midi_tick = 0;
		uint32_t bpm = 120;
		uint32_t us_per_tick = 60000000 / (bpm * pMF.Header.PPQN);
		uint32_t ticks_to_wait = 0;
		uint8_t channel_of_track[MAX_MIDI_TRACKS] = {0};

		iNum = midiReadGetNumTracks(&pMF);

		for(i=0; i< iNum; i++)
		{
			midiReadInitMessage(&msg[i]);
			midiReadGetNextMessage(&pMF, i, &msg[i]);
		}


		printf("start playing...\r\n");

		while(any_track_had_data)
		{
			any_track_had_data = 0;
			ticks_to_wait = UINT32_MAX; // For getting the biggest possible integer value

			for(i=0; i < iNum; i++)
			{
				if(pMF.Track[i].ptr2 != pMF.Track[i].pEnd2)
				{
					any_track_had_data = 1;

					while(current_midi_tick == pMF.Track[i].pos && pMF.Track[i].ptr2 != pMF.Track[i].pEnd2)
					{
						printf("[Track: %d]", i);

						if (msg[i].bImpliedMsg)
						{ ev = msg[i].iImpliedMsg; }
						else
						{ ev = msg[i].iType; }

						printf(" %06d ", msg[i].dwAbsPos);


						if (muGetMIDIMsgName(str, ev))
							printf("%s  ", str);

						switch(ev)
						{
						case	msgNoteOff:
							muGetNameFromNote(str, msg[i].MsgData.NoteOff.iNote);
							printf("(%d) %s", channel_of_track[i] + 1, str);
							floppy_play_midi_note(channel_of_track[i], 0);

							break;
						case	msgNoteOn:
							muGetNameFromNote(str, msg[i].MsgData.NoteOn.iNote);
							printf("  (%d) %s %d", channel_of_track[i] + 1, str, msg[i].MsgData.NoteOn.iVolume);

							if(msg[i].MsgData.NoteOn.iVolume)
								floppy_play_midi_note(channel_of_track[i], msg[i].MsgData.NoteOn.iNote);
							else
								floppy_play_midi_note(channel_of_track[i], 0);

							break;
						case	msgNoteKeyPressure:
							muGetNameFromNote(str, msg[i].MsgData.NoteKeyPressure.iNote);
							printf("(%d) %s %d", msg[i].MsgData.NoteKeyPressure.iChannel,
								str,
								msg[i].MsgData.NoteKeyPressure.iPressure);
							break;
						case	msgSetParameter:
							muGetControlName(str, msg[i].MsgData.NoteParameter.iControl);
							printf("(%d) %s -> %d", msg[i].MsgData.NoteParameter.iChannel,
								str, msg[i].MsgData.NoteParameter.iParam);
							break;
						case	msgSetProgram:
							muGetInstrumentName(str, msg[i].MsgData.ChangeProgram.iProgram);
							channel_of_track[i] = msg[i].MsgData.ChangeProgram.iChannel;

							printf("(%d) %s", msg[i].MsgData.ChangeProgram.iChannel, str);
							break;
						case	msgChangePressure:
							muGetControlName(str, msg[i].MsgData.ChangePressure.iPressure);
							printf("(%d) %s", msg[i].MsgData.ChangePressure.iChannel, str);
							break;
						case	msgSetPitchWheel:
							printf("(%d) %d", msg[i].MsgData.PitchWheel.iChannel,
							msg[i].MsgData.PitchWheel.iPitch);
							break;

						case	msgMetaEvent:
							printf("---- ");
							switch(msg[i].MsgData.MetaEvent.iType)
							{
							case	metaMIDIPort:
								printf("MIDI Port = %d", msg[i].MsgData.MetaEvent.Data.iMIDIPort);
								break;

							case	metaSequenceNumber:
								printf("Sequence Number = %d",msg[i].MsgData.MetaEvent.Data.iSequenceNumber);
								break;

							case	metaTextEvent:
								printf("Text = '%s'",msg[i].MsgData.MetaEvent.Data.Text.pData);
								break;
							case	metaCopyright:
								printf("Copyright = '%s'",msg[i].MsgData.MetaEvent.Data.Text.pData);
								break;
							case	metaTrackName:
								printf("Track name = '%s'",msg[i].MsgData.MetaEvent.Data.Text.pData);
								break;
							case	metaInstrument:
								printf("Instrument = '%s'",msg[i].MsgData.MetaEvent.Data.Text.pData);
								break;
							case	metaLyric:
								printf("Lyric = '%s'",msg[i].MsgData.MetaEvent.Data.Text.pData);
								break;
							case	metaMarker:
								printf("Marker = '%s'",msg[i].MsgData.MetaEvent.Data.Text.pData);
								break;
							case	metaCuePoint:
								printf("Cue point = '%s'",msg[i].MsgData.MetaEvent.Data.Text.pData);
								break;
							case	metaEndSequence:
								printf("End Sequence");
								break;
							case	metaSetTempo:
								bpm = msg[i].MsgData.MetaEvent.Data.Tempo.iBPM;
								us_per_tick = 60000000 / (bpm * pMF.Header.PPQN);
								printf("Tempo = %d", msg[i].MsgData.MetaEvent.Data.Tempo.iBPM);
								break;
							case	metaSMPTEOffset:
								printf("SMPTE offset = %d:%d:%d.%d %d",
									msg[i].MsgData.MetaEvent.Data.SMPTE.iHours,
									msg[i].MsgData.MetaEvent.Data.SMPTE.iMins,
									msg[i].MsgData.MetaEvent.Data.SMPTE.iSecs,
									msg[i].MsgData.MetaEvent.Data.SMPTE.iFrames,
									msg[i].MsgData.MetaEvent.Data.SMPTE.iFF
									);
								break;
							case	metaTimeSig:
								printf("Time sig = %d/%d",msg[i].MsgData.MetaEvent.Data.TimeSig.iNom,
									msg[i].MsgData.MetaEvent.Data.TimeSig.iDenom/MIDI_NOTE_CROCHET);
								break;
							case	metaKeySig:
								if (muGetKeySigName(str, msg[i].MsgData.MetaEvent.Data.KeySig.iKey))
									printf("Key sig = %s", str);
								break;

							case	metaSequencerSpecific:
								printf("Sequencer specific = ");
								HexList(msg[i].MsgData.MetaEvent.Data.Sequencer.pData, sizeof(msg[i].MsgData.MetaEvent.Data.Sequencer.pData)); // ok
								printf("\r\n");
								break;
							}
							break;

						case	msgSysEx1:
						case	msgSysEx2:
							printf("Sysex = ");
							HexList(msg[i].MsgData.SysEx.pData, msg[i].MsgData.SysEx.iSize); // ok
							break;
						}

						if (ev == msgSysEx1 || ev == msgSysEx1 || (ev==msgMetaEvent && msg[i].MsgData.MetaEvent.iType==metaSequencerSpecific))
						{
							// Already done a hex dump
						}
						else
						{
							printf("  [");
							if (msg[i].bImpliedMsg) printf("%X!", msg[i].iImpliedMsg);
							for(j=0;j<msg[i].iMsgSize;j++)
								printf("%X ", msg[i].data[j]);
							printf("]\r\n");
						}

						midiReadGetNextMessage(&pMF, i, &msg[i]);
					}
				}

				if(pMF.Track[i].pos > current_midi_tick )
					ticks_to_wait = (pMF.Track[i].pos - current_midi_tick > 0 && ticks_to_wait > pMF.Track[i].pos - current_midi_tick) ? pMF.Track[i].pos - current_midi_tick : ticks_to_wait;
			}

			if(ticks_to_wait == UINT32_MAX)
				ticks_to_wait = 0;

			delay_us( us_per_tick * ticks_to_wait );

			current_midi_tick += ticks_to_wait;
		}

		midiReadFreeMessage(&msg);
		midiFileClose(&pMF);


		printf("done.\r\n");
	}
	else
	{
		printf("Open Failed!\nInvalid MIDI-File Header!\n");

	}
}



//***********************************************************************************************
// Timer 2 handler. This will control the Motors of the floppy drives depending
// on the resolution. Default is 40ms. (See MoppyEx_conf.h)
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



