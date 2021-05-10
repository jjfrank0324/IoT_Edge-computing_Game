/**************************************************************************//**
* @file      ControlThread.c
* @brief     Thread code for the ESE516 Online game control thread
* @author    Jiahong Ji
* @date      2021-0509

******************************************************************************/


/******************************************************************************
* Includes
******************************************************************************/
#include "ControlThread/ControlThread.h"
#include "WifiHandlerThread/WifiHandler.h"
#include "UiHandlerThread/UiHandlerThread.h"
#include "SeesawDriver/Seesaw.h"
#include "thumbstick/thumbstick.h"
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "asf.h"
#include "main.h"
#include "stdio_serial.h"
#include "SerialConsole.h"
#include "shtc3.h"
/******************************************************************************
* Defines
******************************************************************************/

/******************************************************************************
* Variables
******************************************************************************/
QueueHandle_t xQueueGameBufferIn = NULL; ///<Queue to send the next play to the UI
QueueHandle_t xQueueRgbColorBuffer = NULL; ///<Queue to receive an LED Color packet

controlStateMachine_state controlState; ///<Holds the current state of the control thread
char buffer[64];///<buffer for print function
uint16_t raw_value; ///< raw ADC value
int steps[6];		///< the steps player one has goes through
/******************************************************************************
* Forward Declarations
******************************************************************************/

/******************************************************************************
* Callback Functions
******************************************************************************/


/******************************************************************************
* Task Functions
******************************************************************************/


/**************************************************************************//**
* @fn		void vUiHandlerTask( void *pvParameters )
* @brief	STUDENT TO FILL THIS
* @details 	student to fill this
                				
* @param[in]	Parameters passed when task is initialized. In this case we can ignore them!
* @return		Should not return! This is a task defining function.
* @note         
*****************************************************************************/
void vControlHandlerTask( void *pvParameters )
{
	SerialConsoleWriteString("ESE516 - Control Init Code\r\n");

	//generate random seeds, Need a better was of get the initial random number
	srand(50);

	//Initialize Queues
	xQueueGameBufferIn = xQueueCreate( 2, sizeof( struct GameDataPacket ) );
	xQueueRgbColorBuffer = xQueueCreate( 2, sizeof( struct RgbColorPacket ) );

	if(xQueueGameBufferIn == NULL || xQueueRgbColorBuffer == NULL){
		SerialConsoleWriteString("ERROR Initializing Control Data queues!\r\n");
	}
	controlState = CONTROL_WAIT_FOR_GAME; //Initial state
	int i = 0;
	while(1)
	{
		switch(controlState)
		{
		case (CONTROL_WAIT_FOR_GAME):
		{	
		
			// when it is the first time enter here, clear the previous LED pad
			if(i == 0)
			{	
				SerialConsoleWriteString("Now are at the place of waiting for game start\r\n");
				i++;

				for(int j = 0; j < 15; j++)
				{
					SeesawSetLed(j,0,0,0); //Turn button 15 off
					SeesawOrderLedUpdate();
				}
			}
		
		break;
	}

	case (CONTROL_PLAYING_MOVE):
	{
		//get the random initial position from 0~16
		int location = (rand() % 16) + 1;
		int prev_led = 15;
		snprintf(buffer,63, "Starting location is -> %d\r\n", location);
		SerialConsoleWriteString(buffer);
		int step_move = 1;
		steps[0] = location;
		
		//update the first led
		SeesawSetLed(location-1, 90,0,200);
		SeesawOrderLedUpdate();
		SendRealTimeUserGameInput(1,location,1);
		prev_led = location;
		
		//player one can move one step in total
		for(;step_move < 6;step_move++)
		{	
			
			//1.first step: read until joystick have a signal
			raw_value = ts_read_x();
			while(raw_value < TS_X_THRESHOLD_RIGHT && raw_value > TS_X_THRESHOLD_LEFT)
			{
				vTaskDelay(40);
				raw_value = ts_read_x();
			}
			
			//2.get the next location
			if(raw_value > TS_X_THRESHOLD_RIGHT)
			{
				location += 1;
			}
			
			if(raw_value < TS_X_THRESHOLD_LEFT)
			{
				location -= 1;
			}
			
			if(location < 1)
				location = 1;
			if(location > 16)
				location = 16;
			snprintf(buffer,63, "%d th move is at %d\r\n", step_move, location);
			SerialConsoleWriteString(buffer);
			
			//add an short delay to prevent the Joystick boucing back on the way and cause it read twice
			vTaskDelay(400);
			
			//3.wait tail the joystick reach neutral position
			raw_value = ts_read_x();
			while(raw_value > TS_X_THRESHOLD_RIGHT || raw_value < TS_X_THRESHOLD_LEFT)
			{	
				vTaskDelay(40);
				raw_value = ts_read_x();
			}
			
			
			//4.update led, send the real time signal

			SeesawSetLed(prev_led-1,0,0,0);
			SeesawSetLed(location-1, 90,0,200);
			SeesawOrderLedUpdate();
			prev_led = location;
			vTaskDelay(40);
			SendRealTimeUserGameInput(1,location,1);
			
			
			steps[step_move] = location;
			i=0;
			
					
			
		}
		
		//print the answer key
		snprintf(buffer,63, "the answer key is %d |%d |%d |%d |%d |%d | \r\n", steps[0], steps[1],steps[2],steps[3],steps[4],steps[5]);
		SerialConsoleWriteString(buffer);
		//send the answer key to the cloud
		SendAnswerKey(steps);
		
		controlState = CONTROL_WAIT_FOR_GAME;
		
		break;
	}

	case (CONTROL_END_GAME):
	{	

		break;
	}


	default:
		controlState = CONTROL_WAIT_FOR_GAME;
	break;



	}
vTaskDelay(40);

}


}



/**************************************************************************//**
int ControlAddGameData(struct GameDataPacket *gameIn);
* @brief	Adds an game data received from the internet to the local control for play
* @param[out] 
                				
* @return		Returns pdTrue if data can be added to queue, 0 if queue is full
* @note         

*****************************************************************************/
int ControlAddGameData(struct GameDataPacket *gameIn)
{
	int error = xQueueSend(xQueueGameBufferIn , gameIn, ( TickType_t ) 10);
	return error;
}


/**************************************************************************//**
void StartJXGame(void)
* @brief	The helper function used by wifi thread. Change the state of the state machine
* @note     Call by Wifi handlerThread

*****************************************************************************/
void StartJXGame(void)
{	
	SerialConsoleWriteString("Received Game on instruction! \r\n");
	controlState = CONTROL_PLAYING_MOVE;
	
}
