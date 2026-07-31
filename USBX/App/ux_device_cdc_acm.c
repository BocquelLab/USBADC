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
#include "app_usbx_device.h"
#include "stm32u0xx_hal_gpio.h"
#include "stm32u0xx_ll_adc.h"
#include "tx_api.h"
#include "ux_api.h"
#include "ux_device_class_cdc_acm.h"
#include <limits.h>
#include <stdatomic.h>
#include "protocol.h"
#include "byte_vector.h"

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

uint32_t power_meter_val;
uint32_t usb_sense_val;
uint32_t temperature_val;
uint32_t pin_a3_val;

VOID usbx_cdc_acm_write_thread_entry(ULONG thread_input)
{
  UX_PARAMETER_NOT_USED(thread_input);

  while (1)
  {
    ULONG ignore;

    struct String string;
    if (tx_queue_receive(&ux_cdc_write_queue, (void *) &string, TX_WAIT_FOREVER) != TX_SUCCESS) continue;
    if (cdc_acm == UX_NULL) {
      free(string.ptr);
      continue;
    };

    // if (bytes == 0) HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    if (ux_device_class_cdc_acm_write(cdc_acm, (UCHAR *) string.ptr, string.len, &ignore) != UX_SUCCESS) {
      free(string.ptr);
      continue;
    }

    free(string.ptr);
  }
}

#define log_current_position {                                                        \
      sleep_ms(100);                                                                  \
      struct String string = {                                                        \
        .ptr = malloc(sizeof(char) * 128),                                            \
        .len = 128,                                                                   \
      };                                                                              \
                                                                                      \
      if (string.ptr != NULL) {                                                       \
        string.len = sprintf(string.ptr, "%s: %d\r\n", __func__, __LINE__);           \
                                                                                      \
        if (tx_queue_send(&ux_cdc_write_queue, &string, TX_NO_WAIT) != TX_SUCCESS) {  \
          free(string.ptr);                                                           \
        }                                                                             \
      }                                                                               \
      sleep_ms(100);                                                                  \
}                                                                                     \

static void handle_received_packet(struct USBADC_PROTOCOL_PACKET packet) {
  log_current_position
  struct USBADC_PROTOCOL_PACKET out_packet = {
    .id = packet.id,
  };

  char *out_bytes = NULL;
  uint32_t out_len = 0;
  switch (packet.id) {
    case USBADC_PROTOCOL_REQUEST_PING:
      log_current_position
      {
        struct USBADC_PROTOCOL_REQUEST_PING in_data = {0};

        for (uint8_t i = 0 ; i < 4 ; i++) {
          in_data.bytes <<= 8;
          in_data.bytes += packet.data[i];
        }

        log_current_position
        struct USBADC_PROTOCOL_RESPONSE_PONG out_data = {0};
        out_data.bytes = in_data.bytes;

        log_current_position
        out_packet.type = USBADC_PROTOCOL_RESPONSE_PONG;
        out_packet.data = (char *) &out_data;
        out_packet.length = sizeof(out_data);

        log_current_position
        out_len = usbadc_protocol_encode_packet(out_packet, &out_bytes);
        log_current_position
      }
      break;
    case USBADC_PROTOCOL_REQUEST_READ_ADC:
      log_current_position
      break;
    case USBADC_PROTOCOL_REQUEST_WRITE_PIN:
      log_current_position
      break;
    case USBADC_PROTOCOL_REQUEST_REBOOT:
      log_current_position
      break;

    default:
      log_current_position
      // Unreachable
      break;
  }


  log_current_position
  struct String string = {
    .ptr = malloc(sizeof(char) * out_len),
    .len = out_len,
  };

  if (string.ptr != NULL) {
    memcpy(string.ptr, out_bytes, out_len);

    if (tx_queue_send(&ux_cdc_write_queue, &string, TX_NO_WAIT) != TX_SUCCESS) {
      free(string.ptr);
    }
  }

  log_current_position
}

VOID usbx_cdc_acm_read_thread_entry(ULONG thread_input)
{
  UX_PARAMETER_NOT_USED(thread_input);
  static char buffer[2048];
  byte_vector vector = byte_vector_init(buffer, 2048);

  while (1)
  {
    sleep_ms(10);
    if (cdc_acm == UX_NULL) continue;

    ULONG length_read;
    char *free_space_ptr;
    uint32_t length_remaining = byte_vector_free_space_pointer(&vector, &free_space_ptr);
    if (ux_device_class_cdc_acm_read(cdc_acm, (UCHAR *) free_space_ptr, length_remaining, &length_read) != UX_SUCCESS) continue;
    vector.length += length_read;

    log_current_position;

    bool keep_decoding_packets = true;
    while (keep_decoding_packets && vector.length > 0) {
      log_current_position;
      struct USBADC_PROTOCOL_PACKET packet;
      int32_t decoded_length = usbadc_protocol_decode_packet(vector.data, vector.length, &packet);
      char buffer[128];
      int a = snprintf(buffer, 128, "%ld %d\r\n", decoded_length, vector.length);

      struct String string = {
        .ptr = malloc(sizeof(char) * a),
        .len = a,
      };

      if (string.ptr != NULL) {
        memcpy(string.ptr, buffer, a);

        if (tx_queue_send(&ux_cdc_write_queue, &string, TX_NO_WAIT) != TX_SUCCESS) {
          free(string.ptr);
        }
      }
      switch (decoded_length) {
        case USBADC_PROTOCOL_DECODE_DATA_LENGTH_TOO_SMALL:
          keep_decoding_packets = false;
          break;

        case USBADC_PROTOCOL_DECODE_VERSION_DOESNT_MATCH: // Signal
          // TODO: Send a packet that signals that the sent version is wrong.

        case USBADC_PROTOCOL_DECODE_WRONG_MAGIC_BYTES: // The data doesn't start with the packet's magic bytes
        case USBADC_PROTOCOL_DECODE_WRONG_CHECKSUM: // Packet is corrupted, discard it
          {
            const uint8_t magic_bytes[4] = {0xAA, 0x55, 0xAA, 0x55};
            char *next_magic_bytes_ptr = memmem(vector.data + 1, vector.length, magic_bytes, 4);
            if (next_magic_bytes_ptr == NULL) {
              byte_vector_delete_first_n_chars(&vector, vector.length);
              keep_decoding_packets = false;
            } else {
              byte_vector_delete_first_n_chars(&vector, next_magic_bytes_ptr - vector.data);
            }
            break;
          }

        default:
          log_current_position;
          handle_received_packet(packet);
          byte_vector_delete_first_n_chars(&vector, decoded_length);
          break;
      }
    }

    log_current_position;
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
    power_meter_val = HAL_ADC_GetValue(&hadc1) * VREFINT_CAL_VREF / (1 << 12);

    sConfig.Channel = usb_sense_channel;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) continue;
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 1000);
    usb_sense_val = HAL_ADC_GetValue(&hadc1) * VREFINT_CAL_VREF / (1 << 12);

    sConfig.Channel = temperature_channel;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) continue;
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 1000);
    temperature_val = HAL_ADC_GetValue(&hadc1) * VREFINT_CAL_VREF / (1 << 12);

    sConfig.Channel = pin_a3_channel;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) continue;
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 1000);
    pin_a3_val = HAL_ADC_GetValue(&hadc1) * VREFINT_CAL_VREF / (1 << 12);

    char buffer[128];
    ULONG len = sprintf((char *) &buffer, "Power meter: %ld mV\r\nUSBsense: %ld mV\r\ntemperature: %ld mV\r\n\r\n", power_meter_val, usb_sense_val, temperature_val);

    struct String string = {
      // .ptr = malloc(sizeof(char) * len),
      .ptr = NULL,
      .len = len,
    };

    if (string.ptr != NULL) {
      memcpy(string.ptr, buffer, len);

      if (tx_queue_send(&ux_cdc_write_queue, &string, TX_NO_WAIT) != TX_SUCCESS) {
        free(string.ptr);
        continue;
      }
    }
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
    // It might make more sense to have a lookup table of some sort
    // that maps temperature to PWM duty cycle
    oc_config.Pulse = temperature_val * 639 / 4095;

    HAL_LPTIM_OC_ConfigChannel(&hlptim2, &oc_config, 1);
  }
}

/* USER CODE END 1 */
