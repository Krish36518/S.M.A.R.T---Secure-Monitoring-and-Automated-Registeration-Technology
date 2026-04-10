#include "stm32f4xx_hal.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include "ssd_1306.h"
#include "esp32_at.h"
#include "application_config.h"
#include "mqtt_helper.h"
#include "hc_sr04.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

void SystemClock_Config(void);
void Display_OLED_Message(char* line1, char* line2);
void Error_handler(void);
void Error_Handler(void);

void UART2_Init(void);
void UART1_Init(void);
void UART6_Init(void);
void Empty_Data(void);
void MX_SPI1_Init(void);
void OLED_GPIO_Init(void);

void Img_to_char1(void);
void Img_to_char2(void);
void combine_to_template(void);
void store_atpg(void); //checking by storing in page no 1
void uploadtostm(void);
void GPIO_Analog_Init(void);
void Check_Total_Templates(void);
void GPIO_Button_Init(void);
uint8_t Search_the_id(void);
void Scan_finger_store(void);
void Scan_finger_search(void);
void check(void);
void Store_Data_for_People(void);
void fetch_details(void);

QueueHandle_t mqtt_tx_queue;
QueueHandle_t mqtt_rx_queue;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart6;
SPI_HandleTypeDef hspi1;
TIM_HandleTypeDef htim5;


char* msg = "Please place your finger on the sensor \r\n";
char msg1[200];

uint8_t Store_Copy[15];
uint8_t no = 10;
uint8_t finger_count = 1;
uint8_t id_no;
uint8_t total_votes = 0;




void vote_cast_task(void *parameters)
{
    while(1)
    {
        // 1. Wait for finger and search the database
        // ONLY proceed if the search actually found a registered voter!
        if (Search_the_id() == 1)
        {
            // 2. Fetch details and update the voting status
            fetch_details();

            // 3. Calculate the index
            uint16_t index = id_no - 1;

            // 4. Setup the MQTT queue item
            mqtt_queue_item_t item =
            {
                .operation = MQTT_OPERATION_PUBLISH,
                .topic = SENSOR_DATA_TOPIC,
                .topic_length = strlen(SENSOR_DATA_TOPIC),
            };

            LogDebug(("Voter ID: %d, Voter Name: %s", electoral_list[index].id, electoral_list[index].name));

            size_t payload_len = snprintf((char *)item.payload, sizeof(item.payload),
                 "{\"voter_id\": %d, \"voter_name\": \"%s\"}",
                 electoral_list[index].id, electoral_list[index].name);

            item.payload_length = payload_len;

            /* 5. Send the payload to the MQTT transmission queue! */
            if(xQueueSend(mqtt_tx_queue, &item, portMAX_DELAY) == pdPASS)
            {
                LogInfo(("Queued MQTT Publish = %s", SENSOR_DATA_TOPIC));
            }
            else
            {
                LogError(("Failed to queue MQTT Publish, topic = %s", SENSOR_DATA_TOPIC));
            }
        }

        // Delay to prevent bouncing/double scanning immediately
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
void at_cmd_handle_task(void *parameters)
{
	(void)parameters;

	mqtt_queue_item_t item,new_message;
	mqtt_receive_t mqtt_data;

	static char topic_buffer[MAX_MQTT_TOPIC_SIZE];
	static char payload_buffer[MAX_MQTT_PAYLOAD_SIZE];

	mqtt_data.p_payload = payload_buffer;
	mqtt_data.p_topic = topic_buffer;
	mqtt_data.payload_length = sizeof(payload_buffer);
	mqtt_data.topic_length = sizeof(topic_buffer);

	while(1)
	{
		// 1. De-Queue MQTT Tx queue
		if(xQueueReceive(mqtt_tx_queue,&item,0) == pdPASS)
		{
			if(item.operation == MQTT_OPERATION_PUBLISH)
			{
				mqtt_status_t status = mqtt_publish(item.topic,item.topic_length,item.payload,item.payload_length);

				if(status == MQTT_SUCCESS)
				{
					LogInfo(("MQTT PUBLISH SUCCESSFUL, TOPIC: '%.*s' Payload: '%.*s'",(int)item.topic_length,item.topic,
							(int)item.payload_length,(char*)item.payload));
				}
				else
				{
					LogError(("MQTT PUBLISH FAILED TOPIC: '%.*s'",(int)item.topic_length,item.topic));
				}
			}
			// 2. Send MQTT Subscribe AT Command
			else if(item.operation == MQTT_OPERATION_SUBSCRIBE)
			{
				mqtt_status_t status = mqtt_subscribe(item.topic, item.topic_length);
				if(status == MQTT_SUCCESS)
				{
					LogInfo(("MQTT SUBSCRIBE SUCCESSFUL, TOPIC: '%.*s'",(int)item.topic_length,item.topic));
				}
				else
				{
					LogError(("MQTT SUBSCRIBE FAILURE, TOPIC: '%.*s'",(int)item.topic_length,item.topic));
				}
			}
		}

		// 3. Check for incoming MQTT DATA
		esp32_status_t rx_status = esp32_recv_mqtt_data(&mqtt_data);

		if(rx_status != ESP32_ERROR && mqtt_data.payload_length > 0)
		{
			new_message.operation = MQTT_OPERATION_RECEIVE;
			new_message.payload_length = mqtt_data.payload_length;
			new_message.topic_length = mqtt_data.topic_length;
			memcpy(new_message.payload,mqtt_data.p_payload,mqtt_data.payload_length);
			memcpy(new_message.topic,mqtt_data.p_topic,mqtt_data.topic_length);

			// ---> MOVED THIS INSIDE THE IF STATEMENT <---
			if(xQueueSend(mqtt_rx_queue,&new_message,0) == pdPASS)
			{
				LogInfo(("Queued MQTT RECEIVE : Topic = %.*s",(int)new_message.topic_length,new_message.topic));
			}
		}

		vTaskDelay(pdMS_TO_TICKS(100));
	}
}



int main(void)
{
	HAL_Init();

	SystemClock_Config();

	UART2_Init(); // for debugging

	UART1_Init(); //for sensor

	UART6_Init(); //for esp so has it

	GPIO_Button_Init();

	MX_SPI1_Init();
	OLED_GPIO_Init();

	    // Now you can safely initialize the screen!
	SSD1306_Init();
	Display_OLED_Message("System Booting...", "Please Wait");
	HAL_Delay(1000);

//	for(uint8_t i = 0; i< 15;i++)
//	{
//		Store_Copy[i] = Store[i];
//	}
	//Empty the whole template data
	//Empty_Data();

	//First we will enroll  fingerprints and store it
	check();

	//Store_Data_for_People();
	LogInfo(("Initialising ESP32\n"));
	if(esp32_init() != ESP32_OK)
	{
		  LogError(("Filed to Initialise ESP32 module\n"));
		  Error_Handler();
	}
	LogInfo(("ESP32 Module Initialised successfully \n"));

	HAL_Delay(500);

	//Connect to wifi network
	LogInfo(("Joining Access Point  %s .. \n",WIFI_SSID));
	while(esp32_join_ap((uint8_t*)WIFI_SSID,(uint8_t*)WIFI_PASSWORD) != ESP32_OK)
	{
		  LogInfo(("Retrying to join access point %s\n",WIFI_SSID));
	}
	LogInfo(("Successfully joined access point %s \n",WIFI_SSID));

	HAL_Delay(500);

	//3.Configure SNTP for time synchronization
	if(esp32_config_sntp(UTC_OFFSET) != ESP32_OK)
	{
		  LogError(("Failed to configure SNTP \n"));
		  Error_Handler();
	}
	LogInfo(("Configured SNTP successfully in ESP32"));

	HAL_Delay(1000);

	//Retriving current time from ESP32
	sntp_time_t sntp_time;
	if(esp32_get_sntp_time(&sntp_time) != ESP32_OK)
	{
		  LogError(("Failed to retrieve SNTP time"));
		  Error_Handler();
	}
	LogInfo(("Retrieved SNTP time successfully - %s %02d %s %02d %02d %02d %02d",sntp_time.day,sntp_time.date,sntp_time.month,sntp_time.year,sntp_time.hour,sntp_time.min,sntp_time.sec));

	HAL_Delay(500);

	//4. AND 5 Configure MQtt client and connect it
	LogInfo(("Connecting to MQTT sefver at %s:%d",MQTT_BROKER,MQTT_PORT));
	if(mqtt_connect(CLIENT_ID, MQTT_BROKER, MQTT_PORT) != MQTT_SUCCESS)
	{
		  LogError(("MQTT CONNECTION FAILED \n"));
		  Error_Handler();
	}
	LogInfo(("Successfully connected to MQTT broker %s",MQTT_BROKER));

	HAL_Delay(500);

	LogInfo(("Subscribing to topic %s ",SENSOR_DATA_TOPIC));
	if(mqtt_subscribe(SENSOR_DATA_TOPIC,strlen(SENSOR_DATA_TOPIC)) != MQTT_SUCCESS)
	{
		LogError(("MQTT SUBSCRIBE FOR TOPIC %s Failed ",SENSOR_DATA_TOPIC));
		Error_Handler();
	}
	LogInfo(("MQTT SUBSCRIBE FOR TOPIC %s Successfull ",SENSOR_DATA_TOPIC));

	HAL_Delay(500);

	mqtt_tx_queue = xQueueCreate(5,sizeof(mqtt_queue_item_t));
	mqtt_rx_queue = xQueueCreate(5,sizeof(mqtt_queue_item_t));

	BaseType_t status = xTaskCreate(vote_cast_task,"Publish Sensor Data", 2048,NULL,2,NULL);

	configASSERT(status == pdPASS);

	BaseType_t mqtt_status = xTaskCreate(at_cmd_handle_task, "AT Cmd Task", 4096, NULL, 3, NULL);
	configASSERT(mqtt_status == pdPASS);

	vTaskStartScheduler();

	while(1);

	return(0);
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    *
    * HSE = 25 MHz (Blackpill), PLL to 84 MHz SYSCLK
    * VCO = (HSE / M) * N = (25 / 25) * 336 = 336 MHz
    * SYSCLK = VCO / P = 336 / 4 = 84 MHz
    * USB/48 MHz = VCO / Q = 336 / 7 = 48 MHz
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK  |
                                RCC_CLOCKTYPE_SYSCLK|
                                RCC_CLOCKTYPE_PCLK1 |
                                RCC_CLOCKTYPE_PCLK2;

  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;  // 84 MHz
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;         // HCLK  = 84 MHz
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;          // APB1 = 42 MHz
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;          // APB2 = 84 MHz

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_handler();
  }

  /* Optional: make sure SysTick uses HCLK and has proper priority for FreeRTOS */
 /* HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
  HAL_NVIC_SetPriority(SysTick_IRQn, 15, 0);*/
}
void UART2_Init(void)
{

	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1 ;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	if((HAL_UART_Init(&huart2)) != HAL_OK)
	{
		Error_handler();
	}
}

void UART1_Init(void)
{

	huart1.Instance = USART1;
	huart1.Init.BaudRate = 57600;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1 ;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	if((HAL_UART_Init(&huart1)) != HAL_OK)
	{
		Error_handler();
	}
}
// 1. Configures the SPI Settings (Baud rate, 8-bit, MSB first)
void MX_SPI1_Init(void)
{
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES; // Standard 2-line mode
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;               // Software CS pin!
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; // 84MHz / 8 = 10.5 MHz (Very Stable)
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;

  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_handler();
  }
}

// 2. This is a special background function. When HAL_SPI_Init() runs,

// 3. Configures PB0, PB1, and PB2 for the OLED control lines
void OLED_GPIO_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // PB0 = CS, PB1 = DC, PB2 = RES
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 ;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;        // Standard Output
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH; // Fast pins to keep up with SPI
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);



    // Set them HIGH by default so the OLED stays quiet until we are ready
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2, GPIO_PIN_SET);
}



void GPIO_Analog_Init(void)
{
	GPIO_InitTypeDef gpioAnalog;

	uint32_t pins =  GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 |
			GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
			GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_15 ;
	gpioAnalog.Pin = pins;
	gpioAnalog.Mode = GPIO_MODE_ANALOG;

	HAL_GPIO_Init(GPIOA,&gpioAnalog);

	uint32_t pinss =  GPIO_PIN_0 |GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 |
			GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
			GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_15 ;

	gpioAnalog.Pin = pinss;
	HAL_GPIO_Init(GPIOC,&gpioAnalog);
}

void GPIO_Button_Init(void)
{
	GPIO_InitTypeDef buttongpio;
	__HAL_RCC_GPIOC_CLK_ENABLE();

	__HAL_RCC_GPIOA_CLK_SLEEP_DISABLE();
	__HAL_RCC_GPIOC_CLK_SLEEP_DISABLE();

	buttongpio.Pin = GPIO_PIN_0;
	buttongpio.Mode = GPIO_MODE_IT_FALLING;
	buttongpio.Pull = GPIO_PULLUP;

	HAL_GPIO_Init(GPIOA,&buttongpio);

	HAL_NVIC_EnableIRQ(EXTI0_IRQn);
	HAL_NVIC_SetPriority(EXTI0_IRQn,15,0);

}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
	if(HAL_UART_Transmit(&huart2, (uint8_t*)msg,strlen(msg),100) != HAL_OK)
	{
		Error_handler();
	}

}

void Scan_finger_store(void)
{
    uint8_t data[12];

    while(1)
    {
        HAL_UART_Transmit(&huart1,GenImg,sizeof(GenImg),HAL_MAX_DELAY);

        __HAL_UART_CLEAR_OREFLAG(&huart1);
		__HAL_UART_CLEAR_NEFLAG(&huart1);
		__HAL_UART_CLEAR_FEFLAG(&huart1);

        if(HAL_UART_Receive(&huart1, data, 12,2000) == HAL_OK)
        {
            if(data[9] == 0x00)
             {
                 sprintf(msg1,"Finger no %d]] detected\r\n",finger_count);
                 HAL_UART_Transmit(&huart2,(uint8_t*)msg1,strlen(msg1),HAL_MAX_DELAY);
                 break;
             }

             HAL_Delay(500);
        }

    }
}

void Img_to_char1(void)
{
	uint8_t data[12];
	HAL_UART_Transmit(&huart1,Genchar1,sizeof(Genchar1),HAL_MAX_DELAY);
	HAL_UART_Receive(&huart1, data, 12,2000);
	if(data[9] == 0x00)
	{
		msg = " 1st Converted to Character file \r\n";
		HAL_UART_Transmit(&huart2,(uint8_t*)msg,strlen(msg),HAL_MAX_DELAY);
	}
}
void Img_to_char2(void)
{
	uint8_t data[12];
	HAL_UART_Transmit(&huart1,Genchar2,sizeof(Genchar2),HAL_MAX_DELAY);
	HAL_UART_Receive(&huart1, data, 12,2000);
	if(data[9] == 0x00)
	{
		msg = " 2 Converted to Character file \r\n";
		HAL_UART_Transmit(&huart2,(uint8_t*)msg,strlen(msg),HAL_MAX_DELAY);
	}
}
void combine_to_template(void)
{
	uint8_t data[12];
	HAL_UART_Transmit(&huart1,RegModel,sizeof(RegModel),HAL_MAX_DELAY);
	HAL_UART_Receive(&huart1, data, 12,2000);
	if(data[9] == 0x00)
	{
		msg = " Converted to template succesfully \r\n";
		HAL_UART_Transmit(&huart2,(uint8_t*)msg,strlen(msg),HAL_MAX_DELAY);
	}
}
void store_atpg(void)
{

	uint8_t data[12];
	HAL_UART_Transmit(&huart1,Store_Copy,sizeof(Store_Copy),HAL_MAX_DELAY);
	HAL_UART_Receive(&huart1, data, 12,2000);
	if(data[9] == 0x00)
	{
		sprintf(msg1,"The fingerprint template for finger number %d is stored succesfully at page no %02d \r\n",finger_count,Store_Copy[12]);
		HAL_UART_Transmit(&huart2,(uint8_t*)msg1,strlen(msg1),HAL_MAX_DELAY);
	}
	Store_Copy[12] += 1;
	Store_Copy[14] += 1;

	msg = "Succesfully incrimented values at store for next data to be able to store at next page \r\n";
	HAL_UART_Transmit(&huart2,(uint8_t*)msg,strlen(msg),HAL_MAX_DELAY);


}
void uploadtostm(void)
{

	uint8_t data[12];
	uint8_t rcv_buf[556];
	HAL_UART_Transmit(&huart1,UpChar,sizeof(UpChar),HAL_MAX_DELAY);
	HAL_UART_Receive(&huart1, data, 12,2000);
	if(data[9] == 0x00)
	{
		msg = " Initiating upload to STM\r\n";
		HAL_UART_Transmit(&huart2,(uint8_t*)msg,strlen(msg),HAL_MAX_DELAY);
		__HAL_UART_CLEAR_OREFLAG(&huart1);


		if(HAL_UART_Receive(&huart1,rcv_buf,556,3000) == HAL_OK)
		{
			msg = "STM Received the data now sending to print it\r\n";
			HAL_UART_Transmit(&huart2,(uint8_t*)msg,strlen(msg),HAL_MAX_DELAY);
			msg = "\r\n--- FINGERPRINT HEX DATA ---\r\n";
			HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 1000);
			char hex_str[6]; // Buffer to hold the converted text (e.g., "FF ")

			for (int i = 0; i < 556; i++)
			{
			    // Convert the raw byte into a 2-character Hex string followed by a space
			    sprintf(hex_str, "%02X ", rcv_buf[i]);
			    HAL_UART_Transmit(&huart2, (uint8_t*)hex_str, strlen(hex_str), 10);

			    // Every 16 bytes, print a "New Line" so it formats nicely on your screen
			    if ((i + 1) % 16 == 0)
			    {
			        HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, 10);
			    }
			}

			msg = "\r\n--- END OF DATA ---\r\n";
			HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 1000);
		}
		else
		{
			msg  = "Data nahi mila mujhe bhai mai kya karu re \r\n";
			HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 1000);
		}

		// NEW CODE: Convert to readable Hex and print neatly



	}
}

void Empty_Data(void)
{
	uint8_t data[12];
	HAL_UART_Transmit(&huart1,Empty,sizeof(Empty),HAL_MAX_DELAY);
	HAL_UART_Receive(&huart1, data, 12,2000);
	if(data[9] == 0x00)
	{
		msg = " All templates in the board are emptied BINGO!!\r\n";
		HAL_UART_Transmit(&huart2,(uint8_t*)msg,strlen(msg),HAL_MAX_DELAY);
	}
}

void Store_Data_for_People(void)
{
	for(uint8_t j = 0;j < no;j++)
	{
		msg = "Please place your finger on the sensor \r\n";

		if(HAL_UART_Transmit(&huart2,(uint8_t*)msg,strlen(msg),HAL_MAX_DELAY) != HAL_OK)
		{
			Error_handler();
		}

		Scan_finger_store();

		Img_to_char1();

		HAL_Delay(2500);

		msg = "Please place finger again \r\n";

		if(HAL_UART_Transmit(&huart2,(uint8_t*)msg,strlen(msg),HAL_MAX_DELAY) != HAL_OK)
		{
			Error_handler();
		}

		HAL_Delay(1500);

		Scan_finger_store();

		Img_to_char2();

		combine_to_template();

		store_atpg();

		uploadtostm();

		finger_count++;

		HAL_Delay(4000);
	}

	msg = "Captured the data for given people succesfully";
	HAL_UART_Transmit(&huart2, (uint8_t*)msg,strlen(msg),2000);
}

uint8_t Search_the_id(void)
{
	char oled_buffer[50];

	msg = "\r\n--- AUTHENTICATION MODE ---\r\nPlease place your finger to search...\r\n";
	HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 1000);

	// 1. We only need ONE scan to perform a search!
	Scan_finger_search();
	Img_to_char1();

	HAL_Delay(200);

    // The Search response packet is 16 bytes long
	uint8_t data[16] = {0}; // Initialize with zeros to be safe

	msg = "Searching database...\r\n";
	HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 1000);


	// 2. Send the Search command
	HAL_UART_Transmit(&huart1, Search, sizeof(Search), 1000);

    // 3. Receive the full 16-byte packet ALL AT ONCE
	if(HAL_UART_Receive(&huart1, data, 16, 10000) == HAL_OK)
	{
		if(data[9] == 0x00) // 0x00 means a Match was Found!
		{
			msg = "Lets start the match!\r\n";
			HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 1000);

			// Extract the ID from byte 10 (High) and byte 11 (Low)
			id_no = (data[10] << 8) | data[11];

            // Print the success message using msg1
			sprintf(msg1, "SUCCESS: The following data found at id no %d\r\n", id_no);
			HAL_UART_Transmit(&huart2, (uint8_t*)msg1, strlen(msg1), 2000);
			return 1;
		}
		else if (data[9] == 0x09)
		{
			// 0x09 means the sensor searched, but this finger isn't enrolled
			msg = "Fingerprint not found in the database, means person is not in this voting list.\r\n";

			HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 2000);
	        sprintf(oled_buffer, "Invalid Voter");
	        Display_OLED_Message(oled_buffer, "Sorry");
			return 0;

		}
		else
		{
			sprintf(msg1, "Search Error. Sensor Code: 0x%02X\r\n", data[9]);
			HAL_UART_Transmit(&huart2, (uint8_t*)msg1, strlen(msg1), 2000);
		}
	}
	else
	{
		// If the UART times out completely
		msg = "Bhai receive nahi ho rahi id mai kya kru re (UART Timeout!)\r\n";
		HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 2000);

        // Print whatever partial data it DID receive for debugging
        sprintf(msg1, "Raw Data caught: %02X %02X %02X %02X %02X\r\n", data[0], data[1], data[2], data[8], data[9]);
        HAL_UART_Transmit(&huart2, (uint8_t*)msg1, strlen(msg1), 2000);

		Error_handler();
	}
	return 0;
}
void Scan_finger_search(void)
{
    uint8_t data[12];

    while(1)
    {
        HAL_UART_Transmit(&huart1,GenImg,sizeof(GenImg),HAL_MAX_DELAY);

        __HAL_UART_CLEAR_OREFLAG(&huart1);
		__HAL_UART_CLEAR_NEFLAG(&huart1);
		__HAL_UART_CLEAR_FEFLAG(&huart1);

        if(HAL_UART_Receive(&huart1, data, 12,2000) == HAL_OK)
        {
            if(data[9] == 0x00)
             {
                 sprintf(msg1,"Finger  detected\r\n");
                 HAL_UART_Transmit(&huart2,(uint8_t*)msg1,strlen(msg1),HAL_MAX_DELAY);
                 break;
             }

             HAL_Delay(500);
        }

    }
}
void check(void)
{
	Check_Total_Templates();
}
void Check_Total_Templates(void)
{
    uint8_t data[12]; // The response packet is 12 bytes

    msg = "\r\n--- CHECKING DATABASE SIZE ---\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 1000);

    // 1. Send the command to the sensor
    HAL_UART_Transmit(&huart1, ReadTemplateNum, sizeof(ReadTemplateNum), 1000);
    __HAL_UART_CLEAR_OREFLAG(&huart1);

    // 2. Read the response
    if(HAL_UART_Receive(&huart1, data, 12, 2000) == HAL_OK)
    {
        if(data[9] == 0x00) // 0x00 means Success!
        {
            // The total count is split across byte 10 (High) and byte 11 (Low)
            uint16_t total_stored = (data[10] << 8) | data[11];

            if (total_stored == 0)
            {
                msg = "The database is COMPLETELY EMPTY (0 templates).\r\n";
                HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 1000);
            }
            else
            {
                sprintf(msg1, "SUCCESS: There are %d fingerprints saved in the sensor!\r\n", total_stored);
                HAL_UART_Transmit(&huart2, (uint8_t*)msg1, strlen(msg1), 2000);
            }
        }
        else
        {
            sprintf(msg1, "Error checking templates. Sensor Code: 0x%02X\r\n", data[9]);
            HAL_UART_Transmit(&huart2, (uint8_t*)msg1, strlen(msg1), 2000);
        }
    }
    else
    {
        msg = "UART Timeout while trying to check templates.\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 1000);
    }
}

void fetch_details(void)
{
	uint16_t index = id_no - 1;
    char oled_buffer[50]; // Temporary buffer for screen formatting

	if(electoral_list[index].has_voted == not_voted)
	{
		electoral_list[index].has_voted = voted;
		sprintf(msg1, "The given person named %s  with id %d can sucessfully cast his vote here\r\n",electoral_list[index].name,electoral_list[index].id);
		if(HAL_UART_Transmit(&huart2, (uint8_t*)msg1,strlen(msg1),2000) != HAL_OK)
		{
			Error_handler();
		}
		total_votes += 1;

        // ---> SHOW SUCCESS ON OLED <---
        sprintf(oled_buffer, "Welcome %s", electoral_list[index].name);
        Display_OLED_Message(oled_buffer, "Please Vote Now");
        // Make a beep tone for a passive buzzer
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);

                // USE vTaskDelay INSTEAD OF HAL_Delay!
		vTaskDelay(pdMS_TO_TICKS(1000));

                // Turn Buzzer OFF
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
	}
	else
	{
		sprintf(msg1, "The given person named %s  with id %d has sucessfully casted his vote before\r\n",electoral_list[index].name,electoral_list[index].id);
		if(HAL_UART_Transmit(&huart2, (uint8_t*)msg1,strlen(msg1),2000) != HAL_OK)
		{
			Error_handler();
		}

        // ---> SHOW DENIED ON OLED <---
        sprintf(oled_buffer, "Sorry %s", electoral_list[index].name);
        Display_OLED_Message(oled_buffer, "ALREADY VOTED");
	}
}
// Helper function to easily write two lines of text to the OLED
void Display_OLED_Message(char* line1, char* line2)
{
    SSD1306_Clear();

    // Line 1: Top of the screen
    if (line1 != NULL) {
        SSD1306_SetCursor(0, 10);
        SSD1306_WriteString(line1, SSD1306_COLOR_WHITE);
    }

    // Line 2: Middle of the screen
    if (line2 != NULL) {
        SSD1306_SetCursor(0, 30);
        SSD1306_WriteString(line2, SSD1306_COLOR_WHITE);
    }

    SSD1306_UpdateScreen();
}
int _write(int file, char *ptr, int len) {
    /* Send the string out over UART2 (Change to huart1 if your PC is wired to that one!) */
    HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len,100);
    return len;
}
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    // Handle FreeRTOS HAL tick
    if (htim->Instance == TIM5)
    {
        HAL_IncTick();
    }
    // Handle your custom timer (assuming htimer2 based on your global variables)
    else if (htim->Instance == TIM2)
    {
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
    }
}
void UART6_Init(void)
{
    huart6.Instance = USART6;
    huart6.Init.BaudRate = 115200; // Check your ESP32 datasheet, it might be 115200 or 9600
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    if((HAL_UART_Init(&huart6)) != HAL_OK)
    {
        Error_handler();
    }
}
void Error_handler(void)
{
	HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_9);
}
void Error_Handler(void)
{
	HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_9);
}
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  RCC_ClkInitTypeDef    clkconfig;
  uint32_t              uwTimclock = 0;
  uint32_t              uwPrescalerValue = 0;
  uint32_t              pFLatency;

  /* 1. Enable TIM5 clock */
  __HAL_RCC_TIM5_CLK_ENABLE();

  /* 2. Get clock configuration */
  HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);
  uwTimclock = HAL_RCC_GetPCLK1Freq();

  if (clkconfig.APB1CLKDivider != RCC_HCLK_DIV1) {
      uwTimclock *= 2;
  }

  /* 3. Compute prescaler to get 1MHz clock */
  uwPrescalerValue = (uint32_t) ((uwTimclock / 1000000U) - 1U);

  /* 4. Initialize TIM5 */
  htim5.Instance = TIM5;
  htim5.Init.Period = (1000000U / 1000U) - 1U; // 1ms tick
  htim5.Init.Prescaler = uwPrescalerValue;
  htim5.Init.ClockDivision = 0;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;

  if(HAL_TIM_Base_Init(&htim5) == HAL_OK)
  {
    HAL_NVIC_EnableIRQ(TIM5_IRQn);
    HAL_NVIC_SetPriority(TIM5_IRQn, TickPriority, 0U);
    uwTickPrio = TickPriority;

    /* CRITICAL: Start the timer in interrupt mode */
    return HAL_TIM_Base_Start_IT(&htim5);
  }

  return HAL_ERROR;
}

// Required by HAL to prevent compilation errors
void HAL_SuspendTick(void) { __HAL_TIM_DISABLE_IT(&htim5, TIM_IT_UPDATE); }
void HAL_ResumeTick(void)  { __HAL_TIM_ENABLE_IT(&htim5, TIM_IT_UPDATE); }
