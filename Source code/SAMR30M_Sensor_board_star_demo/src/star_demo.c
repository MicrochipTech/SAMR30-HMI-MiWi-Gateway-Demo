/**
* \file  star_demo.c
*
* \brief Demo Application for MiWi Star Implementation
*
* Copyright (c) 2018 - 2019 Microchip Technology Inc. and its subsidiaries.
*
* \asf_license_start
*
* \page License
*
* Subject to your compliance with these terms, you may use Microchip
* software and any derivatives exclusively with Microchip products.
* It is your responsibility to comply with third party license terms applicable
* to your use of third party software (including open source software) that
* may accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS".  NO WARRANTIES,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
* INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
* AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE
* LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
* LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE
* SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE
* POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT
* ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
* RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*
* \asf_license_stop
*
*/
/*
* Support and FAQ: visit <a href="https://www.microchip.com/support/">Microchip Support</a>
*/

/************************ HEADERS ****************************************/
#include "string.h"
#include "math.h"
#include "miwi_api.h"
#include "miwi_p2p_star.h"
#include "task.h"
#include "star_demo.h"
#include "mimem.h"
#include "asf.h"
//diffin #if defined(ENABLE_SLEEP_FEATURE)
#include "sleep_mgr.h"
//diffin #endif
#if defined (ENABLE_CONSOLE)
#include "sio2host.h"
#endif
#if defined(ENABLE_NETWORK_FREEZER)
#include "pdsDataServer.h"
#include "wlPdsTaskManager.h"
#endif
#include "mimac_at86rf.h"

extern bool sleepMgr_sleepDirectly( void );	//diffin
#define TEMP_DATA_SEND_INTERVAL 10	//5 WSGA-13076 optimization
#define CONNECTION_RETRY_IN_APP 20	//2 WSGA-13104, increase app retry interval from 2s to 20s.
#if defined(PROTOCOL_STAR)
/************************ LOCAL VARIABLES ****************************************/
uint8_t i;
uint8_t TxSynCount = 0;    // Maintain the Count on TX's Done
//uint8_t TxSynCount2 = 0; // Maintain the Count on TX's Done
uint8_t TxNum = 0;         // Maintain the Count on TX's Done
uint8_t RxNum = 0;         // Maintain the Count on RX's Done
/* check for selections made by USER */
bool chk_sel_status;
uint8_t NumOfActiveScanResponse;
bool update_ed;
uint8_t select_ed = 0;
uint8_t msghandledemo = 0;
extern uint8_t myChannel;
/* Connection Table Memory */
extern CONNECTION_ENTRY connectionTable[CONNECTION_SIZE];
bool display_connections;
bool sleep_request;	//diffin
bool data_loopback_request;	//diffin
uint8_t data_loopback[128];	//diffin
uint8_t data_loopback_size;	//diffin
MIWI_TICK t1a , t2a;			//diffin
bool temp_report_disabled;	//diffin
MIWI_TICK t1b, t2b;		//diffin
bool join_retry = false;	//diffin
extern  void Connection_Confirm(miwi_status_t status);
/************************ FUNCTION DEFINITIONS ****************************************/
/*********************************************************************
* Function: static void dataConfcb(uint8_t handle, miwi_status_t status)
*
* Overview: Confirmation Callback for MiApp_SendData
*
* Parameters:  handle - message handle, miwi_status_t status of data send
****************************************************************************/
static void dataConfcb(uint8_t handle, miwi_status_t status, uint8_t* msgPointer)
{
    if (SUCCESS == status)
    {
        /* Update the TX NUM and Display it on the LCD */
        DemoOutput_UpdateTxRx(++TxNum, RxNum);
        /* Delay for Display */
        //delay_ms(100);
    }
    /* After Displaying TX and RX Counts , Switch back to showing Demo Instructions */
    STAR_DEMO_OPTIONS_MESSAGE (role);
}

/*********************************************************************
* Function: void led_indicate_sleep(void)
*
* Overview: add this function to make a LED pattern to indicate entering sleep mode
*
* Parameters: None
*********************************************************************/
void led_indicate_sleep(void)
{
	MIWI_TICK t1, t2;
	uint8_t i;
	
	for(i=0; i< 6; i++)
	{
		if(i==0 || i==2 || i==4)
			LED_Off(LED0);
		else
			LED_On(LED0);
		t1.Val = MiWi_TickGet();
		while(1)
		{
			t2.Val = MiWi_TickGet();
			if(MiWi_TickGetDiff(t2,t1) >= 200*MS)
				break;
		}
	}
}
/*********************************************************************
* Function: void run_star_demo(void)
*
* Overview: runs the demo based on input
*
* Parameters: None
*********************************************************************/
void run_star_demo(void)
{
	bool mac_ack_status;
	uint8_t PressedButton;
	uint16_t broadcastAddress = 0xFFFF;

        P2PTasks();
#if defined(ENABLE_NETWORK_FREEZER)
#if PDS_ENABLE_WEAR_LEVELING
        PDS_TaskHandler();
#endif
#endif

#ifdef ENABLE_MANUAL_SLEEP
		if(sleep_request)
		{
			sleep_request = false;
			#if defined (ENABLE_CONSOLE)
			/* Disable UART */
			sio2host_disable();
			#endif
			MiMAC_PowerState(POWER_STATE_DEEP_SLEEP);
			led_indicate_sleep();
			LED_Off(LED0);
			sleepMgr_sleepDirectly();
			LED_On(LED0);
			while(1);
			#if defined (ENABLE_CONSOLE)
			/* Enable UART */
			//...sio2host_enable();
			#endif
		}
#endif	//#ifdef ENABLE_MANUAL_SLEEP

	#if 0	////sensor board demo disabled
		if(data_loopback_request)
		{
			data_loopback_request = false;
			mac_ack_status = MiApp_SendData(LONG_ADDR_LEN, connectionTable[0].Address,
				data_loopback_size, (uint8_t*)&data_loopback[0], msghandledemo++, true, dataConfcb);
			if (mac_ack_status)
			{
				TxSynCount++;
			}
		}
	#endif
		
		if(Total_Connections() && !temp_report_disabled)
		{
			temperature_send();
		}
		
		if(join_retry)
		{
			t2b.Val = MiWi_TickGet();
			if(MiWi_TickGetDiff(t2b, t1b) > CONNECTION_RETRY_IN_APP*ONE_SECOND)
			{
				join_retry = false;
				MiApp_EstablishConnection(myChannel, 2, (uint8_t*)&broadcastAddress, 0, Connection_Confirm);
				t1b.Val = MiWi_TickGet();
			}
		}
            /*******************************************************************/
            // If no packet received, now we can check if we want to send out
            // any information.
            // Function ButtonPressed will return if any of the two buttons
            // has been pushed.
            /*******************************************************************/
            PressedButton = ButtonPressed();
            if ( PressedButton == 1 )
            {
	#if 0	//sensor board demo disabled				
                if(role != PAN_COORD)	//end device
                {
					//repeat to demo unicast to PAN, unicast to other nodes, broadcast to all
					if (select_ed == end_nodes)
					{
						//broadcast to all
						mac_ack_status = MiApp_SendData(SHORT_ADDR_LEN, (uint8_t *)&broadcastAddress,
							MIWI_TEXT_LEN, (uint8_t *)&MiWi_TEST_DATA[4][0], msghandledemo++, true, dataConfcb);
					}
					else if (select_ed == myConnectionIndex_in_PanCo)
					{
						//unicast to PAN if index is itself
						mac_ack_status = MiApp_SendData(LONG_ADDR_LEN, connectionTable[0].Address,
							MIWI_TEXT_LEN, (uint8_t*)&MiWi_TEST_DATA[3][0], msghandledemo++, true, dataConfcb);
					}
					else
					{
						//unicast to other nodes if else
						mac_ack_status = MiApp_SendData(3, END_DEVICES_Short_Address[select_ed].Address,
							MIWI_TEXT_LEN, (uint8_t*)&MiWi_TEST_DATA[3][0], msghandledemo++, true, dataConfcb);
					}

					if (mac_ack_status)
					{
						TxSynCount++;
					}
										
					if (select_ed >= end_nodes)  /* Reset Peer Device Info */
					{
						/* If end of Peer Device Info reset the count */
						select_ed = 0;
					}
					else
					{
						/* New device Information */
						select_ed = select_ed+1;
					}								
                }/* end of END_DEVICE send packet option */
	#else

				if(temp_report_disabled)
					temp_report_disabled = false;
				else
					temp_report_disabled = true;
	#endif				
            } /* end of options on button press */

}
#endif

/*********************************************************************
* Function: void ReceivedDataIndication (RECEIVED_MESSAGE *ind)
*
* Overview: Process a Received Message
*
* PreCondition: MiApp_ProtocolInit
*
* Input:  RECEIVED_MESSAGE *ind - Indication structure
********************************************************************/
void ReceivedDataIndication (RECEIVED_MESSAGE *ind)
{
#if defined(ENABLE_CONSOLE)
    /* Print the received information via Console */
    DemoOutput_HandleMessage();
#endif

    /* Update the TX AND RX Counts on the display */
    DemoOutput_UpdateTxRx(TxNum, ++RxNum);

    /* Delay for Showing the contents on the display before showing instructions */
    //delay_ms(500);

#if 0	////sensor board demo disabled
#if !defined(ENABLE_SLEEP_FEATURE)
    /* Toggle LED2 to indicate receiving a packet */
    LED_Toggle(LED0);
#endif
#endif

    /* Display the Source End Device Info on reception msg, Do not display if it is
       a PAN CO or if the message received was a broadcast packet */
    if (role == END_DEVICE)
    {
		handle_receive_msg(&rxMessage.Payload[0], rxMessage.PayloadSize);
#if 0	//sensor board demo disabled		
		if(!rxMessage.flags.bits.broadcast)
		{
			if(rxMessage.Payload[0] == 'C')	//this is unicast messag from PAN in auto test mode
			{
				data_loopback_request = true;
				data_loopback_size = rxMessage.PayloadSize;
				memcpy(&data_loopback[0], &rxMessage.Payload[0], data_loopback_size);
			}
		}
#endif		
    }
}

void handle_receive_msg(uint8_t* payload, uint8_t payloadSize)
{
	uint8_t* p2;
	uint8_t i=0;
	p2 = payload;
	while(i < payloadSize)
	{
		if(*p2 == ' ')
		{
			*p2++ = 0;	//clear SPACE
			break;
		}
		else
			p2++;
	}
	if(i<payloadSize)
	{
		if((strcmp(payload, "LED1") == 0) || (strcmp(payload, "led1") == 0))
		{
			if(*p2 == '0')
				LED_Off(LED0);
			else
				LED_On(LED0);
		}
		else if((strcmp(payload, "GPIO1") == 0) || (strcmp(payload, "gpio1") == 0))
		{
			if(*p2 == '0')
				port_pin_set_output_level(GPIO_0, GPIO_LOW);
			else
				port_pin_set_output_level(GPIO_0, GPIO_HIGH);
		}
		else if((strcmp(payload, "GPIO2") == 0) || (strcmp(payload, "gpio2") == 0))
		{
			if(*p2 == '0')
				port_pin_set_output_level(GPIO_1, GPIO_LOW);
			else
				port_pin_set_output_level(GPIO_1, GPIO_HIGH);
		}
	}
}

void temperature_sensor_init(void)
{
	at30tse_init();
	
	/* Read thigh and tlow */
	//! [read_thigh]
	volatile uint16_t thigh = 0;
	thigh = at30tse_read_register(AT30TSE_THIGH_REG,
	AT30TSE_NON_VOLATILE_REG, AT30TSE_THIGH_REG_SIZE);
	//! [read_thigh]
	//! [read_tlow]
	volatile uint16_t tlow = 0;
	tlow = at30tse_read_register(AT30TSE_TLOW_REG,
	AT30TSE_NON_VOLATILE_REG, AT30TSE_TLOW_REG_SIZE);
	//! [read_tlow]
	
	/* Set 12-bit resolution mode. */
	//! [write_conf]
	at30tse_write_config_register(
	AT30TSE_CONFIG_RES(AT30TSE_CONFIG_RES_12_bit));
	//! [write_conf]
	
}

int32_t temperature_sensor_read(void)
{
	double temp_val = at30tse_read_temperature();
	temp_val *= 10;
	temp_val = round(temp_val);
	return (int32_t)temp_val;
}

void temperature_send_start(void)
{
	t1a.Val = MiWi_TickGet();
}

void temperature_send(void)
{
	uint8_t data[12];
	int32_t temp;
	uint32_t utemp1, utemp2;
	uint8_t len=0;
	bool mac_ack_status;
	t2a.Val = MiWi_TickGet();
	if(MiWi_TickGetDiff(t2a, t1a) > TEMP_DATA_SEND_INTERVAL*ONE_SECOND)
	{
		data[len++] = 'T';
		data[len++] = 'E';
		data[len++] = 'M';
		data[len++] = 'P';
		data[len++] = ' ';
		temp = temperature_sensor_read();
		if(temp < 0)
		{
			data[len++] = '-';
		}
		utemp1 = abs(temp);
		if(utemp1 >= 1000 )
		{
			utemp2 = utemp1/1000;
			data[len++] = (uint8_t)(utemp2+0x30);
			utemp1 -= utemp2*1000;
			if(utemp1 >= 100 )
			{
				utemp2 = utemp1/100;
				data[len++] = (uint8_t)(utemp2+0x30);
				utemp1 -= utemp2*100;
			}
			else
			{
				data[len++] = '0';
			}
		}
		else
		{
			if(utemp1 >= 100 )
			{
				utemp2 = utemp1/100;
				data[len++] = (uint8_t)(utemp2+0x30);
				utemp1 -= utemp2*100;
			}
		}
		
		if(utemp1 >= 10 )
		{
			utemp2 = utemp1/10;
			data[len++] = (uint8_t)(utemp2+0x30);
			utemp1 -= utemp2*10;
		}
		else
		{
			data[len++] = '0';
		}
		data[len++] = '.';
		data[len++] = (uint8_t)(utemp1+0x30);
		mac_ack_status = MiApp_SendData(LONG_ADDR_LEN, connectionTable[0].Address,
		len, data, msghandledemo++, true, dataConfcb);
		if (mac_ack_status)
		TxSynCount++;
		t1a.Val = MiWi_TickGet();
	}
}

void start_join_retry(void)
{
	join_retry = true;
	t1b.Val = MiWi_TickGet();
}

void stop_join_retry(void)
{
	join_retry = false;
}
