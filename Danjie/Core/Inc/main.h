/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define HoolleInput_Pin GPIO_PIN_2
#define HoolleInput_GPIO_Port GPIOE
#define HoolleInput_EXTI_IRQn EXTI2_IRQn
#define HoolleOutput_1_Pin GPIO_PIN_3
#define HoolleOutput_1_GPIO_Port GPIOE
#define HoolleOutput_1_EXTI_IRQn EXTI3_IRQn
#define HoolleOutput_2_Pin GPIO_PIN_4
#define HoolleOutput_2_GPIO_Port GPIOE
#define HoolleOutput_2_EXTI_IRQn EXTI4_IRQn
#define CardFeedback_Pin GPIO_PIN_5
#define CardFeedback_GPIO_Port GPIOE
#define CardFeedback_EXTI_IRQn EXTI9_5_IRQn
#define CoinInput_Pin GPIO_PIN_6
#define CoinInput_GPIO_Port GPIOE
#define CoinInput_EXTI_IRQn EXTI9_5_IRQn
#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC
#define SettingButton_Pin GPIO_PIN_0
#define SettingButton_GPIO_Port GPIOC
#define SettingButton_EXTI_IRQn EXTI0_IRQn
#define Button1_Pin GPIO_PIN_1
#define Button1_GPIO_Port GPIOC
#define KeyLED_1_Pin GPIO_PIN_2
#define KeyLED_1_GPIO_Port GPIOC
#define KeyLED_2_Pin GPIO_PIN_3
#define KeyLED_2_GPIO_Port GPIOC
#define Servo_1_Pin GPIO_PIN_2
#define Servo_1_GPIO_Port GPIOA
#define SD_CS_Pin GPIO_PIN_4
#define SD_CS_GPIO_Port GPIOA
#define SD_CLK_Pin GPIO_PIN_5
#define SD_CLK_GPIO_Port GPIOA
#define SD_MISO_Pin GPIO_PIN_6
#define SD_MISO_GPIO_Port GPIOA
#define SD_MOSI_Pin GPIO_PIN_7
#define SD_MOSI_GPIO_Port GPIOA
#define CardOutput_Pin GPIO_PIN_4
#define CardOutput_GPIO_Port GPIOC
#define Hole_B8_Pin GPIO_PIN_12
#define Hole_B8_GPIO_Port GPIOB
#define Hole_B6_Pin GPIO_PIN_13
#define Hole_B6_GPIO_Port GPIOB
#define Hole_Y5_Pin GPIO_PIN_14
#define Hole_Y5_GPIO_Port GPIOB
#define Hole_Y6_Pin GPIO_PIN_15
#define Hole_Y6_GPIO_Port GPIOB
#define Hole_B4_Pin GPIO_PIN_8
#define Hole_B4_GPIO_Port GPIOD
#define Hole_Y7_Pin GPIO_PIN_9
#define Hole_Y7_GPIO_Port GPIOD
#define Hole_Y8_Pin GPIO_PIN_10
#define Hole_Y8_GPIO_Port GPIOD
#define Hole_B1_Pin GPIO_PIN_11
#define Hole_B1_GPIO_Port GPIOD
#define Hole_B2_Pin GPIO_PIN_12
#define Hole_B2_GPIO_Port GPIOD
#define Hole_Y1_Pin GPIO_PIN_13
#define Hole_Y1_GPIO_Port GPIOD
#define Hole_P1_Pin GPIO_PIN_14
#define Hole_P1_GPIO_Port GPIOD
#define Hole_P2_Pin GPIO_PIN_15
#define Hole_P2_GPIO_Port GPIOD
#define Hole_Y2_Pin GPIO_PIN_6
#define Hole_Y2_GPIO_Port GPIOC
#define Hole_B3_Pin GPIO_PIN_7
#define Hole_B3_GPIO_Port GPIOC
#define Hole_Y3_Pin GPIO_PIN_8
#define Hole_Y3_GPIO_Port GPIOC
#define Hole_B5_Pin GPIO_PIN_9
#define Hole_B5_GPIO_Port GPIOC
#define Hole_P3_Pin GPIO_PIN_8
#define Hole_P3_GPIO_Port GPIOA
#define Hole_Y4_Pin GPIO_PIN_11
#define Hole_Y4_GPIO_Port GPIOA
#define Hole_B7_Pin GPIO_PIN_12
#define Hole_B7_GPIO_Port GPIOA
#define Servo_2_Pin GPIO_PIN_15
#define Servo_2_GPIO_Port GPIOA
#define KeyBoard1_Pin GPIO_PIN_0
#define KeyBoard1_GPIO_Port GPIOD
#define Q7_Pin GPIO_PIN_3
#define Q7_GPIO_Port GPIOD
#define Q8_Pin GPIO_PIN_4
#define Q8_GPIO_Port GPIOD
#define Q9_Pin GPIO_PIN_5
#define Q9_GPIO_Port GPIOD
#define Q10_Pin GPIO_PIN_6
#define Q10_GPIO_Port GPIOD
#define Q11_Pin GPIO_PIN_7
#define Q11_GPIO_Port GPIOD
#define Servo_3_Pin GPIO_PIN_3
#define Servo_3_GPIO_Port GPIOB
#define KeyBoard3_Pin GPIO_PIN_6
#define KeyBoard3_GPIO_Port GPIOB
#define KeyBoard2_Pin GPIO_PIN_7
#define KeyBoard2_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
