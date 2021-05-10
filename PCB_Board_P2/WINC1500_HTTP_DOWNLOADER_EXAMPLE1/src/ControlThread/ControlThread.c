/**************************************************************************//**
* @file      ControlThread.c
* @brief     Thread code for the ESE516 Online game control thread
* @author    Jiahong Ji
* @date      2021-05-09

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
char buffer[64];
uint16_t raw_value;
int steps[6];
char usr2_ans[64];
int numEvent;
int ledNum, temp_act ;
int total_input;
int i;
uint8_t eventBuffer[8];	
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


srand(50);

//Initialize Queues
xQueueGameBufferIn = xQueueCreate( 2, sizeof( struct GameDataPacket ) );
xQueueRgbColorBuffer = xQueueCreate( 2, sizeof( struct RgbColorPacket ) );

if(xQueueGameBufferIn == NULL || xQueueRgbColorBuffer == NULL){
	SerialConsoleWriteString("ERROR Initializing Control Data queues!\r\n");
}
controlState = CONTROL_WAIT_FOR_GAME; //Initial state
i = 0;
while(1)
{
	switch(controlState)
	{
	case (CONTROL_WAIT_FOR_GAME):
	{	
		
		//If first time enter, reset LED, clear the events buffer for keypad
		if(i == 0)
		{	

			SerialConsoleWriteString("Now are at the place of waiting for game start\r\n");
			i++;
			

			for(int j = 0; j < 15; j++)
			{
				SeesawSetLed(j,0,0,0); //Turn button 15 off
				SeesawOrderLedUpdate();
			}
			
			//Clear the buffer
			numEvent = SeesawGetKeypadCount();
			SeesawReadKeypad(NULL, numEvent);
		}

		break;
	}

	case (CONTROL_PLAYING_MOVE):
	{	
		
		total_input= 0;
		//while loop for read six input from keypad
		while(total_input < 6)
		{
			numEvent = SeesawGetKeypadCount();
			
			if(numEvent != 0)
			{
				//load the events of keypad
				SeesawReadKeypad(&eventBuffer, numEvent);
				for(i = 0;i < numEvent; i++)
				{
					ledNum = NEO_TRELLIS_SEESAW_KEY(eventBuffer[i]>>2);
					//using mask to get the last two bits
					temp_act = (eventBuffer[i] & 0x03);
					
					
					//if detect action release button:
					if(temp_act == 2)
					{
						SeesawSetLed(ledNum, 0, 0, 0);
						SeesawOrderLedUpdate();
						
					}
					//if detect action button presseed
					else if(temp_act == 3)
					{	
						//loght up the led
						SeesawSetLed(ledNum, 50, 60, 170);
						SeesawOrderLedUpdate();
						
						vTaskDelay(40);
						snprintf(buffer,63, "current input is %d\r\n", ledNum+1);
						SerialConsoleWriteString(buffer);
						SendRealTimeUserGameInput(2, ledNum+1, 1);
						vTaskDelay(40);
						
						//mark down the steps 
						steps[total_input] = ledNum + 1;
						total_input += 1;
						
					}
					else continue;
				}
				
				
			}
			

		}
		//turn off the last LED that has been turned on
		SeesawSetLed(ledNum, 0, 0, 0);
		SeesawOrderLedUpdate();
		vTaskDelay(40);
		
		//print out the LED
		snprintf(buffer,63,"%d,%d,%d,%d,%d,%d", steps[0], steps[1],steps[2],steps[3],steps[4],steps[5]);
		SerialConsoleWriteString("The usr2 input is the following");
		SerialConsoleWriteString(buffer);
		vTaskDelay(40);
		
		//use the formed string for result comparison
		//send the game verdict to the cloud
		if(strncmp(buffer, usr2_ans, 63) == 0)
		SendGameResult(2);
		else
		SendGameResult(1);
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
void StartJXGameP2(void* msg, int msg_len)
* @brief	Stored the answer key, and starte the game
* @param[in]	 msg, list of integer that represent the answer key
				msg_len, the len of the message			
* @return		NULL
* @note         Called by wifi_handler. 

*****************************************************************************/
void StartJXGameP2(void* msg, int msg_len)
{	
	
	memcpy(usr2_ans, msg,msg_len);
	SerialConsoleWriteString("Received Game on instruction! \r\n");
	SerialConsoleWriteString(usr2_ans);
	controlState = CONTROL_PLAYING_MOVE;
	
}
