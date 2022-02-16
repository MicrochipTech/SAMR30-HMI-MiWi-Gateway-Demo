/**
 * @file sleep_mgr.c
 *
 * @brief
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
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES,
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
 *
 */

/*
 * Copyright (c) 2014-2018 Microchip Technology Inc. and its subsidiaries.
 *
 * Licensed under Atmel's Limited License Agreement --> EULA.txt
 */

#include "sleep_mgr.h"
#include "rtc_count.h"
#include "system.h"
#include "rtc_count_interrupt.h"
#include "asf.h"
#include "trx_access.h"
#include "sysTimer.h"

/* Minimum sleep interval in milliseconds */
#define MIN_SLEEP_INTERVAL     (1000)

struct rtc_module rtc_instance;

/**
 * @brief Configuring RTC Callback Function on Overflow
 *
 * @param void
 */
static void configure_rtc_callbacks(void);

/**
 * @brief Callback Function indicating RTC Overflow
 *
 * @param void
 */
static void rtc_overflow_callback(void);

/**
 * @brief Sleep Preparation procedures
 *
 * @param void
 */
static void sleepPreparation(void);

/**
 * @brief Sleep Exit procedures
 *
 * @param sleepTime
 */
static void sleepExit(uint32_t sleepTime);

#ifdef ENABLE_32K_FOR_SLEEP
static bool sleep_clock_select(const enum system_clock_source clock_source);
#endif
/**
 * \brief This function Initializes the Sleep functions
 * Enable RTC Clock in conf_clocks.h
 */
void sleepMgr_init(void)
{
    struct rtc_count_config config_rtc_count;

    rtc_count_get_config_defaults(&config_rtc_count);
    config_rtc_count.prescaler           = RTC_COUNT_PRESCALER_DIV_1;
    config_rtc_count.mode                = RTC_COUNT_MODE_32BIT;

#ifdef FEATURE_RTC_CONTINUOUSLY_UPDATED
    /** Continuously update the counter value so no synchronization is
     *  needed for reading. */
    config_rtc_count.continuously_update = true;
#endif

    /* Clear the timer on match to generate the overflow interrupt*/
    config_rtc_count.clear_on_match = true;

    /* Initialize RTC Counter */
    rtc_count_init(&rtc_instance, RTC, &config_rtc_count);
    configure_rtc_callbacks();
}

/**
 * @brief Sleep Preparation procedures
 *
 * @param void
 */
static void sleepPreparation(void)
{
    /* Disable Transceiver SPI */
    trx_spi_disable();
}

/**
 * @brief Sleep Exit procedures
 *
 * @param sleepTime
 */
static void sleepExit(uint32_t sleepTime)
{
    /* Enable Transceiver SPI */
    trx_spi_enable();

#ifdef ENABLE_32K_FOR_SLEEP
	// Turn the 16MHz oscillator back on
	sleep_clock_select(SYSTEM_CLOCK_SOURCE_OSC16M);
#endif

    /* Synchronize Timers */
    SYS_TimerAdjust_SleptTime(sleepTime);
}

/**
 * \brief This function puts the transceiver and device to sleep
 * \Parameter interval - the time to sleep in milliseconds
 */
bool sleepMgr_sleep(uint32_t interval)
{
    if (interval < MIN_SLEEP_INTERVAL)
    {
        return false;
    }

    /*Set the timeout for compare mode and enable the RTC*/
    rtc_count_set_compare(&rtc_instance, interval, RTC_COUNT_COMPARE_0);

    /* Configure RTC Callbacks */
    configure_rtc_callbacks();

    /* Enable RTC */
    rtc_count_enable(&rtc_instance);

#ifdef ENABLE_32K_FOR_SLEEP
	struct system_standby_config config;
	
	system_standby_get_config_defaults(&config);
	config.enable_dpgpd0       = false;
	config.enable_dpgpd1       = false;
	config.power_domain        = SYSTEM_POWER_DOMAIN_DEFAULT;
	config.vregs_mode          = SYSTEM_SYSTEM_VREG_SWITCH_AUTO;
	config.linked_power_domain = SYSTEM_LINKED_POWER_DOMAIN_DEFAULT;
	config.hmcramchs_back_bias = SYSTEM_RAM_BACK_BIAS_STANDBY_OFF;
	config.hmcramclp_back_bias = SYSTEM_RAM_BACK_BIAS_STANDBY_OFF;

	system_standby_set_config(&config);
#endif
    /* Preparing to go for sleep */
    sleepPreparation();

#ifdef ENABLE_32K_FOR_SLEEP
	// Shut off 16MHz oscillator and run off the ultra low power 32kHz oscillator
	sleep_clock_select(SYSTEM_CLOCK_SOURCE_ULP32K);
#endif

    /*put the MCU in standby mode with RTC as wakeup source*/
    system_set_sleepmode(SYSTEM_SLEEPMODE_STANDBY);
    system_sleep();

    /* Exit procedure after wakeup */
    sleepExit(interval);
    return true;
}

#ifdef ENABLE_MANUAL_SLEEP
bool sleepMgr_sleepDirectly( void )	//diffin
{
#ifdef ENABLE_32K_FOR_SLEEP
	struct system_standby_config config;

	system_standby_get_config_defaults(&config);
	config.enable_dpgpd0       = false;
	config.enable_dpgpd1       = false;
	config.power_domain        = SYSTEM_POWER_DOMAIN_DEFAULT;
	config.vregs_mode          = SYSTEM_SYSTEM_VREG_SWITCH_AUTO;
	config.linked_power_domain = SYSTEM_LINKED_POWER_DOMAIN_DEFAULT;
	config.hmcramchs_back_bias = SYSTEM_RAM_BACK_BIAS_STANDBY_OFF;
	config.hmcramclp_back_bias = SYSTEM_RAM_BACK_BIAS_STANDBY_OFF;

	system_standby_set_config(&config);
#endif
	/* Preparing to go for sleep */
	/* Disable Transceiver SPI */
	trx_spi_disable();

#ifdef ENABLE_32K_FOR_SLEEP
	// Shut off 16MHz oscillator and run off the ultra low power 32kHz oscillator
	sleep_clock_select(SYSTEM_CLOCK_SOURCE_ULP32K);
#endif

	/*put the MCU in standby mode with RTC as wakeup source*/
	//system_set_sleepmode(SYSTEM_SLEEPMODE_STANDBY);		//9.76mA->0.20mA
	system_set_sleepmode(SYSTEM_SLEEPMODE_BACKUP);	//9.74mA->0.17mA->0.11mA
	//system_set_sleepmode(SYSTEM_SLEEPMODE_OFF);		//13mA->3.17mA
	system_sleep();
	//while(1);
	/* Exit procedure after wakeup */
	/* Enable Transceiver SPI */
	//...trx_spi_enable();
	return true;
}
#endif

static void configure_rtc_callbacks(void)
{
    /*Register rtc callback*/
    rtc_count_register_callback(
            &rtc_instance, rtc_overflow_callback,
            RTC_COUNT_CALLBACK_OVERFLOW);
    rtc_count_enable_callback(&rtc_instance, RTC_COUNT_CALLBACK_OVERFLOW);
}

static void rtc_overflow_callback(void)
{
    /* Disable RTC upon interrupt */
    rtc_count_disable(&rtc_instance);
}

#ifdef ENABLE_32K_FOR_SLEEP
/**
 * \brief Main clock source selection between ULP32K and OSC16M.
 */
static bool sleep_clock_select(const enum system_clock_source clock_source)
{
    struct system_gclk_gen_config gclk_conf;
    struct system_clock_source_osc16m_config osc16m_conf;

    switch(clock_source)
    {
        case SYSTEM_CLOCK_SOURCE_OSC16M:
            // Switch to new frequency selection and enable OSC16M
            system_clock_source_osc16m_get_config_defaults(&osc16m_conf);
            osc16m_conf.fsel = CONF_CLOCK_OSC16M_FREQ_SEL;
            osc16m_conf.on_demand = 0;
            osc16m_conf.run_in_standby = CONF_CLOCK_OSC16M_RUN_IN_STANDBY;
            system_clock_source_osc16m_set_config(&osc16m_conf);
            system_clock_source_enable(SYSTEM_CLOCK_SOURCE_OSC16M);
            while(!system_clock_source_is_ready(SYSTEM_CLOCK_SOURCE_OSC16M));

            // Select OSC16M as mainclock
            system_gclk_gen_get_config_defaults(&gclk_conf);
            gclk_conf.source_clock = SYSTEM_CLOCK_SOURCE_OSC16M;
            system_gclk_gen_set_config(GCLK_GENERATOR_0, &gclk_conf);
            if (CONF_CLOCK_OSC16M_ON_DEMAND)
            {
                OSCCTRL->OSC16MCTRL.reg |= OSCCTRL_OSC16MCTRL_ONDEMAND;
            }
            break;

        case SYSTEM_CLOCK_SOURCE_ULP32K:
            /* Select OSCULP32K as new clock source for mainclock temporarily */
            system_gclk_gen_get_config_defaults(&gclk_conf);
            gclk_conf.source_clock = SYSTEM_CLOCK_SOURCE_ULP32K;
            system_gclk_gen_set_config(GCLK_GENERATOR_0, &gclk_conf);

            /* Disable OSC16M clock*/
            system_clock_source_disable(SYSTEM_CLOCK_SOURCE_OSC16M);
            system_clock_source_disable(SYSTEM_CLOCK_SOURCE_OSC32K);
            break;

        default:
            return false;
    }
    return true;
}
#endif

