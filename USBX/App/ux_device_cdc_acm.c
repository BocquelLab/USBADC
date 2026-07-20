/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ux_device_cdc_acm.c
  * @author  MCD Application Team
  * @brief   USBX Device applicative file
  ******************************************************************************
    * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "ux_device_cdc_acm.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include "stm32u073xx.h"
#include "stm32u0xx_hal_adc.h"
#include "tx_api.h"
#include "ux_api.h"
#include "ux_device_class_cdc_acm.h"
#include <limits.h>
#include <stdatomic.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
UX_SLAVE_CLASS_CDC_ACM *cdc_acm;

UX_SLAVE_CLASS_CDC_ACM_LINE_CODING_PARAMETER CDC_VCP_LineCoding =
{
  .ux_slave_class_cdc_acm_parameter_baudrate = 115200,
  .ux_slave_class_cdc_acm_parameter_stop_bit = 0x00,
  .ux_slave_class_cdc_acm_parameter_parity = 0x00,
  .ux_slave_class_cdc_acm_parameter_data_bit = 0x08,
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  USBD_CDC_ACM_Activate
  *         This function is called when insertion of a CDC ACM device.
  * @param  cdc_acm_instance: Pointer to the cdc acm class instance.
  * @retval none
  */
VOID USBD_CDC_ACM_Activate(VOID *cdc_acm_instance)
{
  /* USER CODE BEGIN USBD_CDC_ACM_Activate */
  cdc_acm = (UX_SLAVE_CLASS_CDC_ACM *) cdc_acm_instance;

  if (ux_device_class_cdc_acm_ioctl(cdc_acm, UX_SLAVE_CLASS_CDC_ACM_IOCTL_SET_LINE_CODING, &CDC_VCP_LineCoding) != UX_SUCCESS)
  {
    Error_Handler();
  }
  /* USER CODE END USBD_CDC_ACM_Activate */

  return;
}

/**
  * @brief  USBD_CDC_ACM_Deactivate
  *         This function is called when extraction of a CDC ACM device.
  * @param  cdc_acm_instance: Pointer to the cdc acm class instance.
  * @retval none
  */
VOID USBD_CDC_ACM_Deactivate(VOID *cdc_acm_instance)
{
  /* USER CODE BEGIN USBD_CDC_ACM_Deactivate */
  UX_PARAMETER_NOT_USED(cdc_acm_instance);
  cdc_acm = UX_NULL;
  /* USER CODE END USBD_CDC_ACM_Deactivate */

  return;
}

/**
  * @brief  USBD_CDC_ACM_ParameterChange
  *         This function is invoked to manage the CDC ACM class requests.
  * @param  cdc_acm_instance: Pointer to the cdc acm class instance.
  * @retval none
  */
VOID USBD_CDC_ACM_ParameterChange(VOID *cdc_acm_instance)
{
  /* USER CODE BEGIN USBD_CDC_ACM_ParameterChange */
  UX_PARAMETER_NOT_USED(cdc_acm_instance);

  ULONG request;
  UX_SLAVE_TRANSFER *transfer_request;
  UX_SLAVE_DEVICE *device;

  device = &_ux_system_slave->ux_system_slave_device;
  transfer_request = &device->ux_slave_device_control_endpoint.ux_slave_endpoint_transfer_request;
  request = *(transfer_request->ux_slave_transfer_request_setup + UX_SETUP_REQUEST);

  switch (request) {
    case UX_SLAVE_CLASS_CDC_ACM_IOCTL_SET_LINE_CODING:
      if (ux_device_class_cdc_acm_ioctl(cdc_acm, UX_SLAVE_CLASS_CDC_ACM_IOCTL_GET_LINE_CODING, &CDC_VCP_LineCoding) != UX_SUCCESS)
      {
        Error_Handler();
      }
      break;
    case UX_SLAVE_CLASS_CDC_ACM_IOCTL_GET_LINE_CODING:
      if (ux_device_class_cdc_acm_ioctl(cdc_acm, UX_SLAVE_CLASS_CDC_ACM_IOCTL_SET_LINE_CODING, &CDC_VCP_LineCoding) != UX_SUCCESS)
      {
        Error_Handler();
      }
      break;
    case UX_SLAVE_CLASS_CDC_ACM_SET_CONTROL_LINE_STATE:
    default:
      break;

  }

  /* USER CODE END USBD_CDC_ACM_ParameterChange */

  return;
}

/* USER CODE BEGIN 1 */

static void sleep_ms(ULONG amount_milliseconds)
{
  const ULONG number_of_ticks = (amount_milliseconds * (ULONG) TX_TIMER_TICKS_PER_SECOND) / (ULONG) 1000UL;
  tx_thread_sleep(number_of_ticks);
}

atomic_uint_fast32_t power_meter_val;
atomic_uint_fast32_t usb_sense_val;
atomic_uint_fast32_t temperature_val;
atomic_uint_fast32_t pin_a3_val;

VOID usbx_cdc_acm_write_thread_entry(ULONG thread_input)
{
  UX_PARAMETER_NOT_USED(thread_input);
  char buffer[64];
  while (1)
  {
    sleep_ms(200);
    if (cdc_acm == UX_NULL) continue;
    ULONG len = sprintf((char *) &buffer, "Power meter: %d mV\r\nUSBsense: %d mV\r\ntemperature: %d mV\r\n\r\n", power_meter_val, usb_sense_val, temperature_val);
    ULONG length_written;

    ux_device_class_cdc_acm_write(cdc_acm, (UCHAR *) buffer, len, &length_written);
  }
}

VOID usbx_cdc_acm_read_thread_entry(ULONG thread_input)
{
  UX_PARAMETER_NOT_USED(thread_input);
  UCHAR buffer[64];
  while (1)
  {
    sleep_ms(10);
    if (cdc_acm == UX_NULL) continue;

    ULONG length_read;
    if (ux_device_class_cdc_acm_read(cdc_acm, buffer, sizeof(buffer), &length_read) != UX_SUCCESS) Error_Handler();
    if (length_read > 0 && buffer[length_read - 1] == 'A') {
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    }

    ux_device_class_cdc_acm_write(cdc_acm, buffer, length_read, &length_read);
  }
}


VOID sample_adc_thread_entry(ULONG thread_input)
{
  #define power_meter_channel ADC_CHANNEL_4
  #define usb_sense_channel ADC_CHANNEL_5
  #define temperature_channel ADC_CHANNEL_6
  #define pin_a3_channel ADC_CHANNEL_7

  UX_PARAMETER_NOT_USED(thread_input);
  ADC_ChannelConfTypeDef sConfig = {0};
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;

  // TODO: Make this non-blocking
  while (1)
  {
    sleep_ms(100);

    sConfig.Channel = power_meter_channel;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) continue;
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 1000);
    power_meter_val = HAL_ADC_GetValue(&hadc1);

    sConfig.Channel = usb_sense_channel;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) continue;
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 1000);
    usb_sense_val = HAL_ADC_GetValue(&hadc1);

    sConfig.Channel = temperature_channel;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) continue;
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 1000);
    temperature_val = HAL_ADC_GetValue(&hadc1);

    sConfig.Channel = pin_a3_channel;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) continue;
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 1000);
    pin_a3_val = HAL_ADC_GetValue(&hadc1);
  }
}

// TODO: Merge this into the ADC sampling thread to reduce complexity?
VOID set_fan_pwm_thread_entry(ULONG thread_input)
{
  UX_PARAMETER_NOT_USED(thread_input);

  LPTIM_OC_ConfigTypeDef oc_config = {
    .OCPolarity = LPTIM_OCPOLARITY_HIGH,
  };

  // TODO: Make a lookup table from temperature to PWM ratio?
  while (1)
  {
    sleep_ms(200);

    // Linear map from [0..4096[ to [0..640[
    oc_config.Pulse = temperature_val * 639 / 4096;

    HAL_LPTIM_OC_ConfigChannel(&hlptim2, &oc_config, 1);
  }
}

/* USER CODE END 1 */
