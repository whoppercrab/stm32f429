#include "tm_stm32f4_rtc.h"

uint32_t TM_RTC_Status = 0;
uint8_t TM_RTC_noResetFlag = 0;
uint16_t uwSynchPrediv = 0xFF, uwAsynchPrediv = 0x7F;

uint32_t TM_RTC_Init(TM_RTC_ClockSource_t source) {
	uint32_t status;
	RTC_InitTypeDef RTC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);

	// Allow access to BKP Domain
	PWR_BackupAccessCmd(ENABLE);

	// Clear Wakeup flag
	PWR_ClearFlag(PWR_FLAG_WU);
	
	status = RTC_ReadBackupRegister(RTC_STATUS_REG);
	
	if (status == RTC_STATUS_TIME_OK) {
		TM_RTC_Status = RTC_STATUS_TIME_OK;
		
		// Config RTC
		TM_RTC_Config(source);
		
		// Wait for RTC APB registers synchronisation (needed after start-up from Reset)
		RTC_WaitForSynchro();
		
		//Clear interrupt flags
		RTC_ClearITPendingBit(RTC_IT_WUT);
		EXTI_ClearITPendingBit(EXTI_Line22);
	} else if (status == RTC_STATUS_INIT_OK) {
		TM_RTC_Status = RTC_STATUS_INIT_OK;
		
		// Config RTC
		TM_RTC_Config(source);
		
		// Wait for RTC APB registers synchronisation (needed after start-up from Reset)
		RTC_WaitForSynchro();
		
		//Clear interrupt flags
		RTC_ClearITPendingBit(RTC_IT_WUT);
		EXTI_ClearITPendingBit(EXTI_Line22);
	} else {
		TM_RTC_Status = 0;
		
		// Config RTC
		TM_RTC_Config(source);

		// Wait for RTC APB registers synchronisation (needed after start-up from Reset)
		RTC_WaitForSynchro();
		
		//Init OK
		RTC_WriteBackupRegister(RTC_STATUS_REG, RTC_STATUS_INIT_OK);

		// Set the RTC time base to 1s
		RTC_InitStructure.RTC_HourFormat = RTC_HourFormat_24;
		RTC_InitStructure.RTC_AsynchPrediv = uwAsynchPrediv;
		RTC_InitStructure.RTC_SynchPrediv = uwSynchPrediv;

		if (RTC_Init(&RTC_InitStructure) == ERROR) {
			return 0;
		}
		
		TM_RTC_Status = RTC_STATUS_INIT_OK;
	}
	
	// Clear RTC Alarm Flag
	RTC_ClearFlag(RTC_FLAG_ALRAF);
	
	return TM_RTC_Status;
}

void TM_RTC_SetDateTime(TM_RTC_Time_t* data, TM_RTC_Format_t format) {
	RTC_InitTypeDef RTC_InitStruct;
	RTC_TimeTypeDef RTC_TimeStruct;
	RTC_DateTypeDef RTC_DateStruct;
	
	RTC_TimeStruct.RTC_Hours = data->hours;
	RTC_TimeStruct.RTC_Minutes = data->minutes;
	RTC_TimeStruct.RTC_Seconds = data->seconds;
	
	if (format == TM_RTC_Format_BCD) {
		RTC_SetTime(RTC_Format_BCD, &RTC_TimeStruct);
	} else {
		RTC_SetTime(RTC_Format_BIN, &RTC_TimeStruct);
	}

	RTC_DateStruct.RTC_Date = data->date;
	RTC_DateStruct.RTC_Month = data->month;
	RTC_DateStruct.RTC_Year = data->year;
	RTC_DateStruct.RTC_WeekDay = data->day;
	
	if (format == TM_RTC_Format_BCD) {
		RTC_SetDate(RTC_Format_BCD, &RTC_DateStruct);
	} else {
		RTC_SetDate(RTC_Format_BIN, &RTC_DateStruct);
	}

	if (TM_RTC_Status != 0) {
		RTC_WriteBackupRegister(RTC_STATUS_REG, RTC_STATUS_TIME_OK);
	}
	
	// Set the RTC time base to 1s
	RTC_InitStruct.RTC_HourFormat = RTC_HourFormat_24;
	RTC_InitStruct.RTC_AsynchPrediv = uwAsynchPrediv;
	RTC_InitStruct.RTC_SynchPrediv = uwSynchPrediv;

	RTC_Init(&RTC_InitStruct);
}

void TM_RTC_GetDateTime(TM_RTC_Time_t* data, TM_RTC_Format_t format) {	
	RTC_DateTypeDef RTC_DateStruct;
	RTC_TimeTypeDef RTC_TimeStruct;
	
	RTC_GetDate(RTC_Format_BIN, &RTC_DateStruct);
	RTC_GetTime(RTC_Format_BIN, &RTC_TimeStruct);
	
	data->year = RTC_DateStruct.RTC_Year;
	data->month = RTC_DateStruct.RTC_Month;
	data->date = RTC_DateStruct.RTC_Date;
	data->day = RTC_DateStruct.RTC_WeekDay;
	
	data->hours = RTC_TimeStruct.RTC_Hours;
	data->minutes = RTC_TimeStruct.RTC_Minutes;
	data->seconds = RTC_TimeStruct.RTC_Seconds;
}

void TM_RTC_Config(TM_RTC_ClockSource_t source) {
	if (source == TM_RTC_ClockSource_Internal) {
		// Enable the LSI OSC 
		RCC_LSICmd(ENABLE);

		// Wait till LSI is ready 
		while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET);

		// Select the RTC Clock Source
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
	} else if (source == TM_RTC_ClockSource_External) {
		// Enable the LSE OSC
		RCC_LSEConfig(RCC_LSE_ON);

		// Wait till LSE is ready 
		while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET);

		// Select the RTC Clock Source
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
	}
	
	// Enable the RTC Clock
	RCC_RTCCLKCmd(ENABLE);
}

void TM_RTC_Interrupts(TM_RTC_Int_t int_value) {
	NVIC_InitTypeDef NVIC_InitStruct;
	EXTI_InitTypeDef EXTI_InitStruct;
	uint32_t int_val;

	// NVIC init for 
	NVIC_InitStruct.NVIC_IRQChannel = RTC_WKUP_IRQn;
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
	
	// RTC connected to EXTI_Line22
	EXTI_ClearITPendingBit(EXTI_Line22);
	EXTI_InitStruct.EXTI_Line = EXTI_Line22;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);
	
	if (int_value == TM_RTC_Int_Disable) {
		NVIC_InitStruct.NVIC_IRQChannelCmd = DISABLE;
		NVIC_Init(&NVIC_InitStruct); 
	} else {
		// Enable NVIC
		NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
		NVIC_Init(&NVIC_InitStruct); 
		//Enable EXT1 interrupt
		EXTI_InitStruct.EXTI_LineCmd = ENABLE;
		EXTI_Init(&EXTI_InitStruct);

		// First disable wake up command
		RTC_WakeUpCmd(DISABLE);

		// Clock divided by 8, 32768 / 8 = 4068
		// 4096 ticks for 1second interrupt
		RTC_WakeUpClockConfig(RTC_WakeUpClock_RTCCLK_Div8);

		if (int_value == TM_RTC_Int_60s) {
			int_val = 0x3BFFF; 		// 60 seconds = 60 * 4096 / 1 = 245760
		} else if (int_value == TM_RTC_Int_30s) {
			int_val = 0x1DFFF;		// 30 seconds
		} else if (int_value == TM_RTC_Int_15s) {
			int_val = 0xEFFF;		// 15 seconds
		} else if (int_value == TM_RTC_Int_10s) {
			int_val = 0x9FFF;		// 10 seconds
		} else if (int_value == TM_RTC_Int_5s) {
			int_val = 0x4FFF;		// 5 seconds
		} else if (int_value == TM_RTC_Int_2s) {
			int_val = 0x1FFF;		// 2 seconds
		} else if (int_value == TM_RTC_Int_1s) {
			int_val = 0x0FFF;		// 1 second
		} else if (int_value == TM_RTC_Int_500ms) {
			int_val = 0x7FF;		// 500 ms
		} else if (int_value == TM_RTC_Int_250ms) {
			int_val = 0x3FF;		// 250 ms
		} else if (int_value == TM_RTC_Int_125ms) {
			int_val = 0x1FF;		// 125 ms
		}		

		// Set RTC wakeup counter
		RTC_SetWakeUpCounter(int_val);
		// Enable wakeup interrupt
		RTC_ITConfig(RTC_IT_WUT, ENABLE);
		// Enable wakeup command
		RTC_WakeUpCmd(ENABLE);
	}
}

void RTC_WKUP_IRQHandler(void) {
	if (RTC_GetITStatus(RTC_IT_WUT) != RESET) {
		// Clear interrupt flags
		RTC_ClearITPendingBit(RTC_IT_WUT);
		EXTI_ClearITPendingBit(EXTI_Line22);
		
		// Call user function
		TM_RTC_RequestHandler();
	}
}

