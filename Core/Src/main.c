/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Corpo principal do programa
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * Todos os direitos reservados.
  *
  * Este software é licenciado sob os termos encontrados no arquivo LICENSE
  * no diretório raiz deste componente de software.
  * Se nenhum arquivo LICENSE acompanha este software, ele é fornecido COMO ESTÁ.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
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
ADC_HandleTypeDef hadc1;

TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */
/* Variável global de setpoint de temperatura em graus Celsius (Item 5) */
float gSetpoint_oC = 45.0f;

/* Flag para indicar tecla pressionada via serial */
volatile char gTeclaRecebida = 0;

/* Constante do controlador proporcional (Item 11a) */
float Kp = 1.0f;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */
void ProcessarTeclaSerial(void);
void EnviarMensagemSerial(const char* mensagem);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* Função para enviar mensagem via serial USB CDC */
void EnviarMensagemSerial(const char* mensagem)
{
  CDC_Transmit_FS((uint8_t*)mensagem, strlen(mensagem));
}

/* Função para processar tecla recebida via serial (Item 8) */
void ProcessarTeclaSerial(void)
{
  if (gTeclaRecebida == '+')
  {
    /* Incrementa setpoint em 5°C (Item 8a) */
    gSetpoint_oC += 5.0f;
    gTeclaRecebida = 0;
  }
  else if (gTeclaRecebida == '-')
  {
    /* Decrementa setpoint em 5°C (Item 8b) */
    gSetpoint_oC -= 5.0f;
    gTeclaRecebida = 0;
  }
}

/* USER CODE END 0 */

/**
  * @brief  Ponto de entrada da aplicação.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  uint32_t ultimoTempoControle = 0;
  char bufferSerial[128];
  /* A primeira execução do controle ocorrerá após 1 segundo - 
     ultimoTempoControle inicializado em 0 é intencional para que
     o primeiro ciclo execute imediatamente após a inicialização */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  /* Inicia saídas PWM */
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);

  uint32_t periodoPwm = htim3.Init.Period;

  while (1)
  {
    /* Processa tecla recebida via serial (Item 8) */
    ProcessarTeclaSerial();

    /* Controle a cada 1 segundo (Item 9 e 11) */
    uint32_t tempoAtual = HAL_GetTick();
    if (tempoAtual - ultimoTempoControle >= 1000)
    {
      ultimoTempoControle = tempoAtual;

      /* Lê temperatura do ADC (PA0) - Item 4 */
      HAL_ADC_Start(&hadc1);
      if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
      {
        uint32_t valorAdc = HAL_ADC_GetValue(&hadc1);

        /* Converte ADC para temperatura usando NTC 10k (divisor 10k) e Beta=3950 */
        const float Vref = 3.3f;   // Tensão de referência do ADC
        const float Rpullup = 10000.0f; // Resistência de pull-up do divisor
        const float R0 = 10000.0f; // Resistência nominal do NTC a 25°C
        const float Beta = 3950.0f; // Coeficiente Beta do NTC para 10k
        float tensao = (valorAdc / 4095.0f) * Vref; // Converte valor ADC para tensão
        float Rntc = Rpullup * (tensao / (Vref - tensao + 1e-6f)); // Calcula resistência do NTC
        float temperaturaK = 1.0f / ( (1.0f/(25.0f+273.15f)) + (1.0f/Beta) * logf(Rntc / R0) );  //
        float temperaturaC = temperaturaK - 273.15f; // Converte Kelvin para Celsius

        /* Variáveis de controle */
        uint32_t dutyCyclePeltier = 0; // Duty cycle para o Peltier
        uint32_t dutyCycleFan = 0; // Duty cycle para o ventilador

        /* Calcula erro: ek = gSetpoint_oC - T (Item 11b) */
        float ek = gSetpoint_oC - temperaturaC;

        /* Item 9a: Se temperatura > setpoint + 2°C, PWM potência = 0%, fan = 60% */
        if (temperaturaC > gSetpoint_oC + 2.0f)
        {
          dutyCyclePeltier = 0;
          dutyCycleFan = (uint32_t)((60.0f / 100.0f) * periodoPwm);
        }
        /* Item 9b: Se temperatura < setpoint - 2°C, PWM potência = 100%, fan = 100% */
        else if (temperaturaC < gSetpoint_oC - 2.0f)
        {
          dutyCyclePeltier = periodoPwm;
          dutyCycleFan = periodoPwm;
        }
        /* Controlador P - dentro da faixa de +/- 2°C (Item 11) */
        else
        {
          /* Item 11c: Se erro positivo, duty cycle = Kp * ek */
          if (ek > 0.0f)
          {
            float saida = Kp * ek;
            
            /* Item 11d: Se Kp * ek > 100, limita a 100% */
            if (saida > 100.0f)
            {
              saida = 100.0f;
            }
            
            dutyCyclePeltier = (uint32_t)((saida / 100.0f) * periodoPwm);
            
            /* Fan acompanha potência do peltier, mínimo 60% */
            float fanPct = (saida < 60.0f) ? 60.0f : saida;
            dutyCycleFan = (uint32_t)((fanPct / 100.0f) * periodoPwm);
          }
          /* Item 11e: Se erro negativo, duty cycle = 0% */
          else
          {
            dutyCyclePeltier = 0;
            dutyCycleFan = (uint32_t)((60.0f / 100.0f) * periodoPwm);
          }
        }

        /* Aplica PWM ao Peltier (Canal 1) */
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, dutyCyclePeltier);

        /* Aplica PWM ao Fan (Canal 2) */
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, dutyCycleFan);

        /* Envia dados via serial para monitoramento (Item 10) */
        sprintf(bufferSerial, "T=%.2f oC | Setpoint=%.2f oC | Erro=%.2f | PWM_Peltier=%.0f%% | Kp=%.1f\r\n",
                temperaturaC, gSetpoint_oC, ek,
                (dutyCyclePeltier * 100.0f) / periodoPwm, Kp);
        EnviarMensagemSerial(bufferSerial);

        /* Alterna LED como indicador de funcionamento */
        HAL_GPIO_TogglePin(KIT_LED_GPIO_Port, KIT_LED_Pin);
      }
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(KIT_LED_GPIO_Port, KIT_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : KIT_LED_Pin */
  GPIO_InitStruct.Pin = KIT_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(KIT_LED_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
