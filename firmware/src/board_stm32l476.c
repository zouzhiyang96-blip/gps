#include "stm32l4xx_hal.h"
#include "config.h"
#include "port.h"
#include "modem.h"
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef modem_uart;
#if APP_DEBUG_UART
static UART_HandleTypeDef debug_uart;
#endif
static RTC_HandleTypeDef rtc;
static uint8_t rx_byte;
static void (*rx_handler)(uint8_t);
static volatile uint32_t wake_flags;
static bool modem_on;

void HAL_MspInit(void){__HAL_RCC_SYSCFG_CLK_ENABLE();__HAL_RCC_PWR_CLK_ENABLE();}
static void pin(GPIO_TypeDef *bank,uint32_t pins,uint32_t mode,uint32_t pull,uint32_t alternate) {
    GPIO_InitTypeDef g={0};g.Pin=pins;g.Mode=mode;g.Pull=pull;
    g.Speed=GPIO_SPEED_FREQ_LOW;g.Alternate=alternate;HAL_GPIO_Init(bank,&g);
}
static bool clocks(void) {
    RCC_OscInitTypeDef osc={0};RCC_ClkInitTypeDef clk={0};
    if(HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE2)!=HAL_OK)return false;
    osc.OscillatorType=RCC_OSCILLATORTYPE_MSI;osc.MSIState=RCC_MSI_ON;
    osc.MSICalibrationValue=RCC_MSICALIBRATION_DEFAULT;osc.MSIClockRange=RCC_MSIRANGE_6;
    osc.PLL.PLLState=RCC_PLL_NONE;if(HAL_RCC_OscConfig(&osc)!=HAL_OK)return false;
    clk.ClockType=RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource=RCC_SYSCLKSOURCE_MSI;clk.AHBCLKDivider=RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider=RCC_HCLK_DIV1;clk.APB2CLKDivider=RCC_HCLK_DIV1;
    if(HAL_RCC_ClockConfig(&clk,FLASH_LATENCY_0)!=HAL_OK)return false;
    __HAL_RCC_WAKEUPSTOP_CLK_CONFIG(RCC_STOP_WAKEUPCLOCK_MSI);
    return true;
}
static bool rtc_init(void) {
    RCC_OscInitTypeDef osc={0};RCC_PeriphCLKInitTypeDef pc={0};bool lse=true;
    HAL_PWR_EnableBkUpAccess();
    /* No UTC calendar in this prototype. A cold reset deliberately restarts RTC. */
    __HAL_RCC_BACKUPRESET_FORCE();__HAL_RCC_BACKUPRESET_RELEASE();
    osc.OscillatorType=RCC_OSCILLATORTYPE_LSE;osc.LSEState=RCC_LSE_ON;
    if(HAL_RCC_OscConfig(&osc)!=HAL_OK) {
        lse=false;osc.OscillatorType=RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_LSI;
        osc.LSEState=RCC_LSE_OFF;osc.LSIState=RCC_LSI_ON;
        if(HAL_RCC_OscConfig(&osc)!=HAL_OK)return false;
    }
    pc.PeriphClockSelection=RCC_PERIPHCLK_RTC;
    pc.RTCClockSelection=lse?RCC_RTCCLKSOURCE_LSE:RCC_RTCCLKSOURCE_LSI;
    if(HAL_RCCEx_PeriphCLKConfig(&pc)!=HAL_OK)return false;
    __HAL_RCC_RTC_ENABLE();rtc.Instance=RTC;rtc.Init.HourFormat=RTC_HOURFORMAT_24;
    rtc.Init.AsynchPrediv=127;rtc.Init.SynchPrediv=lse?255u:249u;
    rtc.Init.OutPut=RTC_OUTPUT_DISABLE;rtc.Init.OutPutPolarity=RTC_OUTPUT_POLARITY_HIGH;
    rtc.Init.OutPutType=RTC_OUTPUT_TYPE_OPENDRAIN;
    if(HAL_RTC_Init(&rtc)!=HAL_OK)return false;
    HAL_NVIC_SetPriority(RTC_WKUP_IRQn,2,0);HAL_NVIC_EnableIRQ(RTC_WKUP_IRQn);
    return true;
}
#if APP_DEBUG_UART
static bool debug_init(void) {
    __HAL_RCC_USART2_CLK_ENABLE();
    pin(GPIOA,GPIO_PIN_2|GPIO_PIN_3,GPIO_MODE_AF_PP,GPIO_NOPULL,GPIO_AF7_USART2);
    debug_uart.Instance=USART2;debug_uart.Init.BaudRate=115200;
    debug_uart.Init.WordLength=UART_WORDLENGTH_8B;debug_uart.Init.StopBits=UART_STOPBITS_1;
    debug_uart.Init.Parity=UART_PARITY_NONE;debug_uart.Init.Mode=UART_MODE_TX_RX;
    debug_uart.Init.HwFlowCtl=UART_HWCONTROL_NONE;debug_uart.Init.OverSampling=UART_OVERSAMPLING_16;
    return HAL_UART_Init(&debug_uart)==HAL_OK;
}
#endif
bool port_init(void) {
    HAL_Init();if(!clocks())return false;
    __HAL_RCC_GPIOA_CLK_ENABLE();__HAL_RCC_GPIOB_CLK_ENABLE();__HAL_RCC_GPIOC_CLK_ENABLE();
    pin(GPIOA,GPIO_PIN_All&~(GPIO_PIN_13|GPIO_PIN_14),GPIO_MODE_ANALOG,GPIO_NOPULL,0);
    pin(GPIOB,GPIO_PIN_All,GPIO_MODE_ANALOG,GPIO_NOPULL,0);
    pin(GPIOC,GPIO_PIN_All&~(GPIO_PIN_14|GPIO_PIN_15),GPIO_MODE_ANALOG,GPIO_NOPULL,0);
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2,GPIO_PIN_RESET);
    pin(GPIOB,GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2,GPIO_MODE_OUTPUT_PP,GPIO_NOPULL,0);
    HAL_GPIO_WritePin(GPIOC,GPIO_PIN_0,GPIO_PIN_RESET);
    pin(GPIOC,GPIO_PIN_0,GPIO_MODE_OUTPUT_PP,GPIO_NOPULL,0);
    HAL_GPIO_WritePin(GPIOA,GPIO_PIN_5,GPIO_PIN_RESET);
    pin(GPIOA,GPIO_PIN_5,GPIO_MODE_OUTPUT_PP,GPIO_NOPULL,0);
    pin(GPIOB,GPIO_PIN_12,GPIO_MODE_IT_RISING,GPIO_PULLDOWN,0);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn,2,0);HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
    /* The journal erase geometry below is ONLY valid in unswapped dual-bank mode. */
    if(*(const uint16_t *)FLASHSIZE_BASE!=1024u || !(FLASH->OPTR&FLASH_OPTR_DUALBANK) ||
        (SYSCFG->MEMRMP&SYSCFG_MEMRMP_FB_MODE))return false;
    HAL_DBGMCU_DisableDBGStopMode();HAL_DBGMCU_DisableDBGStandbyMode();
    if(!rtc_init())return false;
#if APP_DEBUG_UART
    if(!debug_init())return false;
#endif
    return true;
}
uint32_t port_millis(void){return HAL_GetTick();}
void port_idle(void){__WFI();}
void port_device_id(char out[25]) {
    snprintf(out,25,"%08lX%08lX%08lX",(unsigned long)HAL_GetUIDw0(),
        (unsigned long)HAL_GetUIDw1(),(unsigned long)HAL_GetUIDw2());
}
void port_log(const char *message) {
#if APP_DEBUG_UART
    (void)HAL_UART_Transmit(&debug_uart,(uint8_t *)message,(uint16_t)strlen(message),100);
    (void)HAL_UART_Transmit(&debug_uart,(uint8_t *)"\r\n",2,100);
#else
    (void)message;
#endif
}
bool port_battery_mv(uint32_t *mv) {
    ADC_HandleTypeDef adc={0};ADC_ChannelConfTypeDef channel={0};bool ok=false;uint32_t raw;
    __HAL_RCC_ADC_CLK_ENABLE();
    pin(GPIOA,GPIO_PIN_0,GPIO_MODE_ANALOG_ADC_CONTROL,GPIO_NOPULL,0);
    HAL_GPIO_WritePin(GPIOC,GPIO_PIN_0,GPIO_PIN_SET);HAL_Delay(5);
    adc.Instance=ADC1;adc.Init.ClockPrescaler=ADC_CLOCK_SYNC_PCLK_DIV4;
    adc.Init.Resolution=ADC_RESOLUTION_12B;adc.Init.DataAlign=ADC_DATAALIGN_RIGHT;
    adc.Init.ScanConvMode=ADC_SCAN_DISABLE;adc.Init.EOCSelection=ADC_EOC_SINGLE_CONV;
    adc.Init.NbrOfConversion=1;adc.Init.ExternalTrigConv=ADC_SOFTWARE_START;
    adc.Init.ExternalTrigConvEdge=ADC_EXTERNALTRIGCONVEDGE_NONE;
    adc.Init.Overrun=ADC_OVR_DATA_PRESERVED;adc.Init.OversamplingMode=DISABLE;
    channel.Channel=ADC_CHANNEL_5;channel.Rank=ADC_REGULAR_RANK_1;
    channel.SamplingTime=ADC_SAMPLETIME_640CYCLES_5;channel.SingleDiff=ADC_SINGLE_ENDED;
    channel.OffsetNumber=ADC_OFFSET_NONE;
    if(HAL_ADC_Init(&adc)==HAL_OK && HAL_ADCEx_Calibration_Start(&adc,ADC_SINGLE_ENDED)==HAL_OK &&
       HAL_ADC_ConfigChannel(&adc,&channel)==HAL_OK && HAL_ADC_Start(&adc)==HAL_OK &&
       HAL_ADC_PollForConversion(&adc,20)==HAL_OK) {
        raw=HAL_ADC_GetValue(&adc);
        /* Assumes regulated VDDA=3.300V, 200k/100k divider, 10nF filter. */
        if(raw>0u && raw<4095u){*mv=(raw*3300u*3u+2047u)/4095u;ok=true;}
    }
    (void)HAL_ADC_Stop(&adc);(void)HAL_ADC_DeInit(&adc);__HAL_RCC_ADC_CLK_DISABLE();
    HAL_GPIO_WritePin(GPIOC,GPIO_PIN_0,GPIO_PIN_RESET);
    pin(GPIOA,GPIO_PIN_0,GPIO_MODE_ANALOG,GPIO_NOPULL,0);return ok;
}
void port_modem_rx_handler(void (*receive)(uint8_t)){rx_handler=receive;}
bool port_modem_power(bool on) {
    if(!on){
        modem_on=false;HAL_NVIC_DisableIRQ(USART1_IRQn);
        if(modem_uart.Instance){(void)HAL_UART_AbortReceive(&modem_uart);(void)HAL_UART_DeInit(&modem_uart);}
        __HAL_RCC_USART1_CLK_DISABLE();pin(GPIOA,GPIO_PIN_9|GPIO_PIN_10,GPIO_MODE_ANALOG,GPIO_NOPULL,0);
        HAL_GPIO_WritePin(GPIOB,GPIO_PIN_1|GPIO_PIN_0,GPIO_PIN_RESET);return true;
    }
    __HAL_RCC_USART1_CLK_ENABLE();
    pin(GPIOA,GPIO_PIN_9|GPIO_PIN_10,GPIO_MODE_AF_PP,GPIO_NOPULL,GPIO_AF7_USART1);
    modem_uart.Instance=USART1;modem_uart.Init.BaudRate=115200;
    modem_uart.Init.WordLength=UART_WORDLENGTH_8B;modem_uart.Init.StopBits=UART_STOPBITS_1;
    modem_uart.Init.Parity=UART_PARITY_NONE;modem_uart.Init.Mode=UART_MODE_TX_RX;
    modem_uart.Init.HwFlowCtl=UART_HWCONTROL_NONE;modem_uart.Init.OverSampling=UART_OVERSAMPLING_16;
    if(HAL_UART_Init(&modem_uart)!=HAL_OK)return false;
    modem_on=true;HAL_NVIC_ClearPendingIRQ(USART1_IRQn);
    HAL_NVIC_SetPriority(USART1_IRQn,1,0);HAL_NVIC_EnableIRQ(USART1_IRQn);
    if(HAL_UART_Receive_IT(&modem_uart,&rx_byte,1)!=HAL_OK)return false;
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_0,GPIO_PIN_SET);return true;
}
void port_modem_key(bool asserted){HAL_GPIO_WritePin(GPIOB,GPIO_PIN_1,asserted?GPIO_PIN_SET:GPIO_PIN_RESET);}
bool port_modem_write(const uint8_t *data,size_t length) {
    return modem_on&&length<=65535u&&HAL_UART_Transmit(&modem_uart,(uint8_t *)data,(uint16_t)length,100)==HAL_OK;
}
void USART1_IRQHandler(void){HAL_UART_IRQHandler(&modem_uart);}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart) {
    if(uart->Instance==USART1 && modem_on){if(rx_handler)rx_handler(rx_byte);(void)HAL_UART_Receive_IT(uart,&rx_byte,1);}
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart) {
    if(uart->Instance==USART1 && modem_on){modem_rx_error_isr();__HAL_UART_CLEAR_OREFLAG(uart);(void)HAL_UART_Receive_IT(uart,&rx_byte,1);}
}
static uint32_t slot_address(unsigned slot){return slot==0u?APP_STATE_A_ADDRESS:APP_STATE_B_ADDRESS;}
bool port_storage_read(unsigned slot,uint8_t *data,size_t length) {
    if(slot>1u||length>APP_FLASH_SLOT_BYTES)return false;
    memcpy(data,(const void *)slot_address(slot),length);return true;
}
bool port_storage_erase(unsigned slot) {
    FLASH_EraseInitTypeDef erase={0};uint32_t error;HAL_StatusTypeDef result;
    if(slot>1u || HAL_FLASH_Unlock()!=HAL_OK)return false;
    erase.TypeErase=FLASH_TYPEERASE_PAGES;erase.Banks=FLASH_BANK_2;
    erase.Page=(slot_address(slot)-0x08080000u)/APP_FLASH_SLOT_BYTES;erase.NbPages=1;
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    result=HAL_FLASHEx_Erase(&erase,&error);(void)HAL_FLASH_Lock();return result==HAL_OK;
}
bool port_storage_write64(unsigned slot,unsigned offset,const uint8_t data[8]) {
    uint64_t word=0;unsigned i;HAL_StatusTypeDef result;
    if(slot>1u || offset%8u || offset>APP_FLASH_SLOT_BYTES-8u || HAL_FLASH_Unlock()!=HAL_OK)return false;
    for(i=0;i<8;i++)word|=(uint64_t)data[i]<<(8u*i);
    result=HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,slot_address(slot)+offset,word);
    (void)HAL_FLASH_Lock();return result==HAL_OK;
}
bool port_schedule_wakeup(uint32_t seconds) {
    if(seconds==0u||seconds>65535u)return false;
    if(HAL_RTCEx_DeactivateWakeUpTimer(&rtc)!=HAL_OK)return false;
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&rtc,RTC_FLAG_WUTF);
    __HAL_RTC_WAKEUPTIMER_EXTI_CLEAR_FLAG();HAL_NVIC_ClearPendingIRQ(RTC_WKUP_IRQn);
    return HAL_RTCEx_SetWakeUpTimer_IT(&rtc,seconds-1u,RTC_WAKEUPCLOCK_CK_SPRE_16BITS)==HAL_OK;
}
uint32_t port_deep_sleep(void) {
    uint32_t reason;
    (void)port_modem_power(false);
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_2,GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC,GPIO_PIN_0,GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA,GPIO_PIN_5,GPIO_PIN_RESET);
#if APP_DEBUG_UART
    (void)HAL_UART_DeInit(&debug_uart);__HAL_RCC_USART2_CLK_DISABLE();
    pin(GPIOA,GPIO_PIN_2|GPIO_PIN_3,GPIO_MODE_ANALOG,GPIO_NOPULL,0);
#endif
    for(;;){
        __disable_irq();
        if(wake_flags){reason=wake_flags;wake_flags=0;__enable_irq();break;}
        HAL_SuspendTick();SCB->ICSR=SCB_ICSR_PENDSTCLR_Msk;
        HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
        /* MSI 4MHz is also the configured STOP wake clock. No PLL restore needed. */
        SystemCoreClockUpdate();HAL_ResumeTick();__enable_irq();
    }
    (void)HAL_RTCEx_DeactivateWakeUpTimer(&rtc);
#if APP_DEBUG_UART
    (void)debug_init();
#endif
    return reason;
}
void RTC_WKUP_IRQHandler(void){HAL_RTCEx_WakeUpTimerIRQHandler(&rtc);}
void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *handle){(void)handle;wake_flags|=WAKE_RTC;}
void EXTI15_10_IRQHandler(void){HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_12);}
void HAL_GPIO_EXTI_Callback(uint16_t pin_number){if(pin_number==GPIO_PIN_12)wake_flags|=WAKE_SENSOR;}
void SysTick_Handler(void){HAL_IncTick();}
