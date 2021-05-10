/**************************************************************************//**
* @file      BootMain.c
* @brief     Main file for the ESE516 bootloader. Handles updating the main application
* @details   Main file for the ESE516 bootloader. Handles updating the main application
* @author    Eduardo Garcia
* @date      2020-02-15
* @version   2.0
* @copyright Copyright University of Pennsylvania
******************************************************************************/


/******************************************************************************
* Includes
******************************************************************************/
#include <asf.h>
#include "conf_example.h"
#include <string.h>
#include "sd_mmc_spi.h"

#include "SD Card/SdCard.h"
#include "Systick/Systick.h"
#include "SerialConsole/SerialConsole.h"
#include "ASF/sam0/drivers/dsu/crc32/crc32.h"






/******************************************************************************
* Defines
******************************************************************************/
#define APP_START_ADDRESS  ((uint32_t)0x12000) ///<Start of main application. Must be address of start of main application
#define APP_START_RESET_VEC_ADDRESS (APP_START_ADDRESS+(uint32_t)0x04) ///< Main application reset vector address
//#define MEM_EXAMPLE 1 //COMMENT ME TO REMOVE THE MEMORY WRITE EXAMPLE BELOW

/******************************************************************************
* Structures and Enumerations
******************************************************************************/

struct usart_module cdc_uart_module; ///< Structure for UART module connected to EDBG (used for unit test output)

/******************************************************************************
* Local Function Declaration
******************************************************************************/
static void jumpToApplication(void);
static bool StartFilesystemAndTest(void);
static void configure_nvm(void);

int check_fw_update(void);
void install_firmware(char* firmware_filename);


/******************************************************************************
* Global Variables
******************************************************************************/
//INITIALIZE VARIABLES
char test_file_name[] = "0:sd_mmc_test.txt";	///<Test TEXT File name
char test_bin_file[] = "0:sd_binary.bin";	///<Test BINARY File name
Ctrl_status status; ///<Holds the status of a system initialization
FRESULT res; //Holds the result of the FATFS functions done on the SD CARD TEST
FATFS fs; //Holds the File System of the SD CARD
FIL file_object; //FILE OBJECT used on main for the SD Card Test
DIR			dir; // the root directory
FILINFO		fno;



/******************************************************************************
* Global Functions
******************************************************************************/

/**************************************************************************//**
* @fn		int main(void)
* @brief	Main function for ESE516 Bootloader Application

* @return	Unused (ANSI-C compatibility).
* @note		Bootloader code initiates here.
*****************************************************************************/

int main(void)
{

	/*1.) INIT SYSTEM PERIPHERALS INITIALIZATION*/
	system_init();
	delay_init();
	InitializeSerialConsole();
	system_interrupt_enable_global();
	/* Initialize SD MMC stack */
	sd_mmc_init();

	//Initialize the NVM driver
	configure_nvm();

	irq_initialize_vectors();
	cpu_irq_enable();

	//Configure CRC32
	dsu_crc32_init();

	SerialConsoleWriteString("ESE516 - ENTER BOOTLOADER");	//Order to add string to TX Buffer

	/*END SYSTEM PERIPHERALS INITIALIZATION*/


	/*2.) STARTS SIMPLE SD CARD MOUNTING AND TEST!*/

	//EXAMPLE CODE ON MOUNTING THE SD CARD AND WRITING TO A FILE
	//See function inside to see how to open a file
	SerialConsoleWriteString("\x0C\n\r-- SD/MMC Card Example on FatFs --\n\r");

	if(StartFilesystemAndTest() == false)
	{
		SerialConsoleWriteString("SD CARD failed! Check your connections. System will restart in 5 seconds...");
		delay_cycles_ms(5000);
		system_reset();
	}
	else
	{
		SerialConsoleWriteString("SD CARD mount success! Filesystem also mounted. \r\n");
	}

	/*END SIMPLE SD CARD MOUNTING AND TEST!*/


	/*3.) STARTS BOOTLOADER HERE!*/


	//We will ask the NVM driver for information on the MCU (SAMD21)
	struct nvm_parameters parameters;
	char helpStr[64]; //Used to help print values
	nvm_get_parameters (&parameters); //Get NVM parameters
	snprintf(helpStr, 63,"NVM Info: Number of Pages %d. Size of a page: %d bytes. \r\n", parameters.nvm_number_of_pages, parameters.page_size);
	SerialConsoleWriteString(helpStr);
	
	delay_cycles_ms(2000); //Delay to allow print


	FILINFO fno;

	check_fw_update();
	//char* fname = "UTAFO.bin";
	//install_firmware(fname);

	/*END BOOTLOADER HERE!*/

	//4.) DEINITIALIZE HW AND JUMP TO MAIN APPLICATION!
	SerialConsoleWriteString("ESE516 - EXIT BOOTLOADER");	//Order to add string to TX Buffer
	delay_cycles_ms(100); //Delay to allow print
		
	//Deinitialize HW - deinitialize started HW here!
	DeinitializeSerialConsole(); //Deinitializes UART
	sd_mmc_deinit(); //Deinitialize SD CARD


	//Jump to application
	jumpToApplication();

	//Should not reach here! The device should have jumped to the main FW.
	
}







/******************************************************************************
* Static Functions
******************************************************************************/



/**************************************************************************//**
* function      static void StartFilesystemAndTest()
* @brief        Starts the filesystem and tests it. Sets the filesystem to the global variable fs
* @details      Jumps to the main application. Please turn off ALL PERIPHERALS that were turned on by the bootloader
*				before performing the jump!
* @return       Returns true is SD card and file system test passed. False otherwise.
******************************************************************************/
static bool StartFilesystemAndTest(void)
{
	bool sdCardPass = true;
	uint8_t binbuff[256];

	//Before we begin - fill buffer for binary write test
	//Fill binbuff with values 0x00 - 0xFF
	for(int i = 0; i < 256; i++)
	{
		binbuff[i] = i;
	}

	//MOUNT SD CARD
	Ctrl_status sdStatus= SdCard_Initiate();
	if(sdStatus == CTRL_GOOD) //If the SD card is good we continue mounting the system!
	{
		SerialConsoleWriteString("SD Card initiated correctly!\n\r");

		//Attempt to mount a FAT file system on the SD Card using FATFS
		SerialConsoleWriteString("Mount disk (f_mount)...\r\n");
		memset(&fs, 0, sizeof(FATFS));
		res = f_mount(LUN_ID_SD_MMC_0_MEM, &fs); //Order FATFS Mount
		if (FR_INVALID_DRIVE == res)
		{
			LogMessage(LOG_INFO_LVL ,"[FAIL] res %d\r\n", res);
			sdCardPass = false;
			goto main_end_of_test;
		}
		SerialConsoleWriteString("[OK]\r\n");

		//Create and open a file
		SerialConsoleWriteString("Create a file (f_open)...\r\n");

		test_file_name[0] = LUN_ID_SD_MMC_0_MEM + '0';
		res = f_open(&file_object,
		(char const *)test_file_name,
		FA_CREATE_ALWAYS | FA_WRITE);
		
		if (res != FR_OK)
		{
			LogMessage(LOG_INFO_LVL ,"[FAIL] res %d\r\n", res);
			sdCardPass = false;
			goto main_end_of_test;
		}

		SerialConsoleWriteString("[OK]\r\n");

		//Write to a file
		SerialConsoleWriteString("Write to test file (f_puts)...\r\n");

		if (0 == f_puts("Test SD/MMC stack\n", &file_object))
		{
			f_close(&file_object);
			LogMessage(LOG_INFO_LVL ,"[FAIL]\r\n");
			sdCardPass = false;
			goto main_end_of_test;
		}

		SerialConsoleWriteString("[OK]\r\n");
		f_close(&file_object); //Close file
		SerialConsoleWriteString("Test is successful.\n\r");


		//Write binary file
		//Read SD Card File
		test_bin_file[0] = LUN_ID_SD_MMC_0_MEM + '0';
		res = f_open(&file_object, (char const *)test_bin_file, FA_WRITE | FA_CREATE_ALWAYS);
		
		if (res != FR_OK)
		{
			SerialConsoleWriteString("Could not open binary file!\r\n");
			LogMessage(LOG_INFO_LVL ,"[FAIL] res %d\r\n", res);
			sdCardPass = false;
			goto main_end_of_test;
		}

		//Write to a binaryfile
		SerialConsoleWriteString("Write to test file (f_write)...\r\n");
		uint32_t varWrite = 0;
		if (0 != f_write(&file_object, binbuff,256, &varWrite))
		{
			f_close(&file_object);
			LogMessage(LOG_INFO_LVL ,"[FAIL]\r\n");
			sdCardPass = false;
			goto main_end_of_test;
		}

		SerialConsoleWriteString("[OK]\r\n");
		f_close(&file_object); //Close file
		SerialConsoleWriteString("Test is successful.\n\r");
		
		main_end_of_test:
		SerialConsoleWriteString("End of Test.\n\r");

	}
	else
	{
		SerialConsoleWriteString("SD Card failed initiation! Check connections!\n\r");
		sdCardPass = false;
	}

	return sdCardPass;
}



/**************************************************************************//**
* function      static void jumpToApplication(void)
* @brief        Jumps to main application
* @details      Jumps to the main application. Please turn off ALL PERIPHERALS that were turned on by the bootloader
*				before performing the jump!
* @return       
******************************************************************************/
static void jumpToApplication(void)
{
// Function pointer to application section
void (*applicationCodeEntry)(void);

// Rebase stack pointer
__set_MSP(*(uint32_t *) APP_START_ADDRESS);

// Rebase vector table
SCB->VTOR = ((uint32_t) APP_START_ADDRESS & SCB_VTOR_TBLOFF_Msk);

// Set pointer to application section
applicationCodeEntry =
(void (*)(void))(unsigned *)(*(unsigned *)(APP_START_RESET_VEC_ADDRESS));

// Jump to application. By calling applicationCodeEntry() as a function we move the PC to the point in memory pointed by applicationCodeEntry, 
//which should be the start of the main FW.
applicationCodeEntry();
}



/**************************************************************************//**
* function      static void configure_nvm(void)
* @brief        Configures the NVM driver
* @details      
* @return       
******************************************************************************/
static void configure_nvm(void)
{
    struct nvm_config config_nvm;
    nvm_get_config_defaults(&config_nvm);
    config_nvm.manual_page_write = false;
    nvm_set_config(&config_nvm);
}




/**************************************************************************//**
* function      enum firmware_num check_fw_update(void)
* @brief        check which firmware should install
* @details      Read though the sd card, printing the content inisde the sd_card along the way,
*				determine the fireware we need to install according to the flag we used
* @return       (enum firmware_num)Return the firmware enumerator for firmware A or B
******************************************************************************/
int check_fw_update(void)
{	
	char* test_flag_A = "FLAGA.TXT";
	char* test_flag_B = "FLAGB.TXT";
	
	
	int fw_num = 0;

	res = f_opendir(&dir, "/");
	if(res == FR_OK)
	SerialConsoleWriteString("Current flashdisk have the following contents: \r\n");

	//read though the directory to find the corresponding file
	while(1)
	{
	res = f_readdir(&dir, &fno);
		if(res != FR_OK)
		{
		break;
		}
		//if finish reading
		if(fno.fname[0] == '\0')
		{
		break;
		}

		//see weather the file is an dirctory or file, print accordingly
		if(fno.fattrib & AM_DIR)
		{
		SerialConsoleWriteString("	DIRTORY:  ");
		SerialConsoleWriteString(fno.fname);
		SerialConsoleWriteString("\r\n");
		}

		//else it is a file
		else
		{
		if(!strcmp(test_flag_A,fno.fname))
		{
		fw_num = 1;
		SerialConsoleWriteString("	File:  ");
		SerialConsoleWriteString(fno.fname);
		SerialConsoleWriteString(" ->UPDATE FIRMWARE A\r\n");
		SerialConsoleWriteString("\r\n");
		f_unlink("/FlagA.txt");  //if it is found, need to earse it for the next time use

		}
		else if(!strcmp(test_flag_B,fno.fname))
		{
		fw_num = 2;
		SerialConsoleWriteString("	File:  ");
		SerialConsoleWriteString(fno.fname);
		SerialConsoleWriteString(" ->UPDATE FIRMWARE B\r\n");
		SerialConsoleWriteString("\r\n");
		f_unlink("/FlagB.txt");//if it is found, need to erase it for the next time use
		}
		else
		{
		SerialConsoleWriteString("	File:  ");
		SerialConsoleWriteString(fno.fname);
		SerialConsoleWriteString("\r\n");
		}
		}

		}

	return fw_num;

}




/**************************************************************************//**
* function      void install_firmware(char* firmware_filename)
* @brief        install the firmware from sd_card to NVM
* @paraIn       firmware_filename-> the name of the firmware binary file
* @details      Open the file in with the given name in sd Card, erase four page,
*				and write to NVM 4 pages per cycle
* @return       NULL
******************************************************************************/

void install_firmware(char* firmware_filename)
{
	//open the firmware file
	res = f_open(&file_object, (char const *)firmware_filename, FA_READ);
		
	//check if open successfully
	if (res != FR_OK)
	{
		SerialConsoleWriteString("Could not open test file!\r\n System will restart in 5 seconds...");
		delay_cycles_ms(5000);
		return;
		system_reset();
	}
	
	//get the total bytes need to read
	uint32_t numBytesLeft = f_size(&file_object);
	uint32_t numberBytesTotal = 0;
	uint32_t nvm_offset = 0;
		
	uint8_t readBuffer[256]; //Buffer the size of one row
	uint32_t numBytesRead = 0;
		
	char helper_string[64];
	snprintf(helper_string, 63, "the Size of the File is %d ! \r\n", numBytesLeft);
	SerialConsoleWriteString(helper_string);
	while(numBytesLeft  != 0) 
	{		
		//erase on NVM + current offset
		enum status_code nvmError = nvm_erase_row(APP_START_ADDRESS + nvm_offset);
		if(nvmError != STATUS_OK)
		{
			SerialConsoleWriteString("Erase error");
		}
	
		
		//in one cycle, read 256 bytes only
		res = f_read(&file_object, &readBuffer[0], 256, &numBytesRead); 
		numBytesLeft -= numBytesRead;
		numberBytesTotal += numBytesRead;
			
		//Write data to first row. Writes are per page, so we need four writes to write a complete row
		res = nvm_write_buffer (APP_START_ADDRESS + nvm_offset, &readBuffer[0], 64);
		res = nvm_write_buffer (APP_START_ADDRESS + nvm_offset + 64, &readBuffer[64], 64);
		res = nvm_write_buffer (APP_START_ADDRESS + nvm_offset + 128, &readBuffer[128], 64);
		res = nvm_write_buffer (APP_START_ADDRESS + nvm_offset + 192, &readBuffer[192], 64);
			
		if (res != FR_OK)
		{
			SerialConsoleWriteString("Test write to NVM failed!\r\n");
			SerialConsoleWriteString("System will restart in 5 seconds...");
			delay_cycles_ms(5000);
			system_reset();
		}

			
		//CRC of SD Card
		uint32_t resultCrcSd = 0;
			*((volatile unsigned int*) 0x41007058) &= ~0x30000UL;
		enum status_code crcres = dsu_crc32_cal	(readBuffer	,256, &resultCrcSd); //Instructor note: Was it the third parameter used for? Please check how you can use the third parameter to do the CRC of a long data stream in chunks - you will need it!
			
			//Errata Part 2 - To be done after RAM CRC
			*((volatile unsigned int*) 0x41007058) |= 0x20000UL;
			 
		//CRC of memory (NVM)
		uint32_t resultCrcNvm = 0;
		crcres |= dsu_crc32_cal	(APP_START_ADDRESS + nvm_offset,256, &resultCrcNvm);

		if (crcres != STATUS_OK)
		{
			SerialConsoleWriteString("Could not calculate CRC!!\r\n");
		}
		//if checksum failed, writing failed
		else if(resultCrcNvm != resultCrcSd)
		{
			SerialConsoleWriteString("CRC_error detected!\r\n");
			SerialConsoleWriteString("System will restart in 5 seconds...");
			delay_cycles_ms(5000);
			system_reset();
		}
			
		nvm_offset += 256;
			
			
		}
	
}