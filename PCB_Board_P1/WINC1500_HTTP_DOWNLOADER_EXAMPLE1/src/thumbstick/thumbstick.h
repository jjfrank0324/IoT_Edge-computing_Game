/**************************************************************************//**
* @file      thumbstick.h
* @brief	 ADC library for thumb stick
* @author    Jiahong Ji
* @date      2021-04-16

******************************************************************************/

#define TS_X_THRESHOLD_LEFT 200 //!< therhold value for read an X_left action
#define TS_X_THRESHOLD_RIGHT 1100//!< therhold value for read an X_right action

void initialize_thumbstick(void);
uint16_t ts_read_x(void);
uint16_t tx_read_y(void);