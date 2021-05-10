/**************************************************************************//**
* @file      thumbstick.c
* @brief	 ADC library for thumb stick
* @author    Jiahong Ji
* @date      2021-04-16

******************************************************************************/


/******************************************************************************
* Includes
******************************************************************************/
#include <asf.h>
#include <system.h>
#include "thumbstick.h"


/******************************************************************************
* Define
******************************************************************************/
struct adc_module adc_instance;
struct adc_module adc_instance2;
/**************************************************************************//**
void initialize_thumbstick(void)
* @brief:	Initialize the ADC drive for thumb stick
* @details:
	? 1V from internal bandgap reference
	? Div 4 clock prescaler
	? 12 bit resolution
	? Window monitor disabled
	? No gain
	? Positive input on ADC PIN 0
	? Negative input on ADC PIN 1
	? Averaging disabled
	? Oversampling disabled
	? Right adjust data
	? Single-ended mode
	? Free running disabled
	? All events (input and generation) disabled
	? Sleep operation disabled
	? No reference compensation
	? No gain/offset correction
	? No added sampling time
	? Pin scan mode disabled      				
* @note	change positive_input and negative_input for future work
*****************************************************************************/
void initialize_thumbstick(void)
{	

	struct adc_config config_adc;

	adc_get_config_defaults(&config_adc);
	
	config_adc.positive_input = ADC_POSITIVE_INPUT_PIN6;
	config_adc.negative_input = ADC_NEGATIVE_INPUT_GND;


	adc_init(&adc_instance, ADC, &config_adc);
	adc_enable(&adc_instance);
	
	struct adc_config config_adc2;

	adc_get_config_defaults(&config_adc2);
	
	config_adc2.positive_input = ADC_POSITIVE_INPUT_PIN6;
	config_adc2.negative_input = ADC_NEGATIVE_INPUT_GND;


	adc_init(&adc_instance2, ADC, &config_adc2);
	adc_enable(&adc_instance2);



}


/**************************************************************************//**
uint16_t ts_read_x(void)
* @brief	read the X axis ADC raw value from the thumb stick				
* @return	uint16_t Reading result 
* @note     Polling method of reading
*****************************************************************************/
uint16_t ts_read_x(void)
{
	uint16_t result;
	enum status_code stat; 
	 adc_start_conversion(&adc_instance);
	 do {
		 /* Wait for conversion to be done and read out result */
	 
		 stat = adc_read(&adc_instance, &result);
	 } while (stat == STATUS_BUSY);
	
	return result;
}

uint16_t tx_read_y(void)
{
	
	
}