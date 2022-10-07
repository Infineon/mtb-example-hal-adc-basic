/******************************************************************************
* File Name:   main.c
*
* Description: In this code example, ADC HAL (Hardware Abstraction Layer) driver
*              is configured to sample input voltage and the sampled input
*              voltage is displayed on the UART.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2020-2022, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

/*******************************************************************************
* Macros
*******************************************************************************/
/* Macro for ADC Channel configuration*/
#define SINGLE_CHANNEL 1
#define MULTI_CHANNEL  2

/*
 * Macro to choose between single channel and multiple channel configuration of
 * ADC. Single channel configuration uses channel 0 in single ended mode.
 * Multiple channel configuration uses two channels, channel 0 in single ended
 * mode and channel 1 in differential mode.
 *
 * The default configuration is set to use single channel.
 * To use multiple channel configuration set ADC_EXAMPLE_MODE macro to MULTI_CHANNEL.
 *
 */
#define ADC_EXAMPLE_MODE SINGLE_CHANNEL

#if defined(CY_DEVICE_PSOC6A512K)  /* if the target is CY8CPROTO-062S3-4343W */
/* Channel 0 input pin */
#define VPLUS_CHANNEL_0  (P10_3)
#else 
#define VPLUS_CHANNEL_0  (P10_0)
#endif

#if ADC_EXAMPLE_MODE == MULTI_CHANNEL

/* Channel 1 VPLUS input pin */
#define VPLUS_CHANNEL_1  (P10_4)

/* Channel 1 VREF input pin */
#define VREF_CHANNEL_1   (P10_5)

/* Number of scans every time ADC read is initiated */
#define NUM_SCAN                    (1)

#endif /* ADC_EXAMPLE_MODE == MULTI_CHANNEL */

/* Conversion factor */
#define MICRO_TO_MILLI_CONV_RATIO        (1000u)

/* Acquistion time in nanosecond */
#define ACQUISITION_TIME_NS              (1000u)

/* ADC Scan delay in millisecond */
#define ADC_SCAN_DELAY_MS                (200u)

/*******************************************************************************
*       Enumerated Types
*******************************************************************************/
/* ADC Channel constants*/
enum ADC_CHANNELS
{
  CHANNEL_0 = 0,
  CHANNEL_1,
  NUM_CHANNELS
} adc_channel;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
#if ADC_EXAMPLE_MODE == MULTI_CHANNEL

/* Multichannel initialization function */
void adc_multi_channel_init(void);

/* Function to read input voltage from multiple channels */
void adc_multi_channel_process(void);

/* ADC Event Handler */
static void adc_event_handler(void* arg, cyhal_adc_event_t event);

#else /* ADC_EXAMPLE_MODE == SINGLE_CHANNEL */

/* Single channel initialization function*/
void adc_single_channel_init(void);

/* Function to read input voltage from channel 0 */
void adc_single_channel_process(void);

#endif /* ADC_EXAMPLE_MODE == MULTI_CHANNEL */

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* ADC Object */
cyhal_adc_t adc_obj;

/* ADC Channel 0 Object */
cyhal_adc_channel_t adc_chan_0_obj;

/* Default ADC configuration */
const cyhal_adc_config_t adc_config = {
        .continuous_scanning=false, // Continuous Scanning is disabled
        .average_count=1,           // Average count disabled
        .vref=CYHAL_ADC_REF_VDDA,   // VREF for Single ended channel set to VDDA
        .vneg=CYHAL_ADC_VNEG_VSSA,  // VNEG for Single ended channel set to VSSA
        .resolution = 12u,          // 12-bit resolution
        .ext_vref = NC,             // No connection
        .bypass_pin = NC };       // No connection

#if ADC_EXAMPLE_MODE == MULTI_CHANNEL

/* Asynchronous read complete flag, used in Event Handler */
static bool async_read_complete = false;

/* ADC Channel 1 Object */
cyhal_adc_channel_t adc_chan_1_obj;

/* Variable to store results from multiple channels during asynchronous read*/
int32_t result_arr[NUM_CHANNELS * NUM_SCAN] = {0};

#endif /* ADC_EXAMPLE_MODE == MULTI_CHANNEL */

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for CM4 CPU. It does...
*    1. Configure and initialize ADC.
*    2. Every 200ms read the input voltage and display input voltage on UART.
*
* Parameters:
*  none
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    /* Variable to capture return value of functions */
    cy_rslt_t result;

#if defined(CY_DEVICE_SECURE)
    cyhal_wdt_t wdt_obj;
    /* Clear watchdog timer so that it doesn't trigger a reset */
    result = cyhal_wdt_init(&wdt_obj, cyhal_wdt_get_max_timeout_ms());
    CY_ASSERT(CY_RSLT_SUCCESS == result);
    cyhal_wdt_free(&wdt_obj);
#endif

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    
    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    result = cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX,
                                 CY_RETARGET_IO_BAUDRATE);

    /* retarget-io init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Print message */
    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");
    printf("-----------------------------------------------------------\r\n");
    printf("PSoC 6 MCU: ADC using HAL\r\n");
    printf("-----------------------------------------------------------\r\n\n");

#if ADC_EXAMPLE_MODE == MULTI_CHANNEL

    /* Initialize Channel 0 and Channel 1 */
    adc_multi_channel_init();

#else /* ADC_EXAMPLE_MODE == SINGLE_CHANNEL */

    /* Initialize Channel 0 */
    adc_single_channel_init();

#endif /* ADC_EXAMPLE_MODE == MULTI_CHANNEL */

    /* Update ADC configuration */
    result = cyhal_adc_configure(&adc_obj, &adc_config);
    if(result != CY_RSLT_SUCCESS)
    {
        printf("ADC configuration update failed. Error: %ld\n", (long unsigned int)result);
        CY_ASSERT(0);
    }

    for (;;)
    {

#if ADC_EXAMPLE_MODE == MULTI_CHANNEL

        /* Sample input voltage at channel 0 and channel 1*/
        adc_multi_channel_process();

#else  /* ADC_EXAMPLE_MODE == SINGLE_CHANNEL */

        /* Sample input voltage at channel 0 */
        adc_single_channel_process();

#endif /* ADC_EXAMPLE_MODE == MULTI_CHANNEL */

        /* 200ms delay between scans */
        cyhal_system_delay_ms(ADC_SCAN_DELAY_MS);
    }
}

#if ADC_EXAMPLE_MODE == MULTI_CHANNEL

/*******************************************************************************
 * Function Name: adc_multi_channel_init
 *******************************************************************************
 *
 * Summary:
 *  ADC Multichannel initilization. This function initializes and configures
 *  channel 0 and channel 1 of ADC.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 *******************************************************************************/
void adc_multi_channel_init(void)
{
    /* Variable to capture return value of functions */
    cy_rslt_t result;

    /* Initialize ADC. The ADC block which can connect to the channel 0 input pin is selected */
    result = cyhal_adc_init(&adc_obj, VPLUS_CHANNEL_0, NULL);
    if(result != CY_RSLT_SUCCESS)
    {
        printf("ADC initialization failed. Error: %ld\n", (long unsigned int)result);
        CY_ASSERT(0);
    }

    /* ADC channel configuration */
    const cyhal_adc_channel_config_t channel_config = {
            .enable_averaging = false,  // Disable averaging for channel
            .min_acquisition_ns = ACQUISITION_TIME_NS, // Minimum acquisition time set to 1us
            .enabled = true };          // Sample this channel when ADC performs a scan

    /* Initialize a channel 0 and configure it to scan the channel 0 input pin in single ended mode. */
    result  = cyhal_adc_channel_init_diff(&adc_chan_0_obj, &adc_obj, VPLUS_CHANNEL_0,
                                          CYHAL_ADC_VNEG, &channel_config);
    if(result != CY_RSLT_SUCCESS)
    {
        printf("ADC first channel initialization failed. Error: %ld\n", (long unsigned int)result);
        CY_ASSERT(0);
    }

    /*
     * For multichannel configuration use to same channel configuration structure
     * "channel_config" to configure the second channel.
     * For second channel to be set to differential mode, two inputs from Pins
     * channel 1 input pin and channel 1 voltage reference pin are configured to be inputs.
     *
     */

    /* Initialize channel 1 in differential mode with VPLUS and VMINUS input pins */
    result = cyhal_adc_channel_init_diff(&adc_chan_1_obj, &adc_obj, VPLUS_CHANNEL_1,
                                         VREF_CHANNEL_1, &channel_config);
    if(result != CY_RSLT_SUCCESS)
    {
        printf("ADC second channel initialization failed. Error: %ld\n", (long unsigned int)result);
        CY_ASSERT(0);
    }

    /* Register a callback to handle asynchronous read completion */
     cyhal_adc_register_callback(&adc_obj, &adc_event_handler, result_arr);

     /* Subscribe to the async read complete event to process the results */
     cyhal_adc_enable_event(&adc_obj, CYHAL_ADC_ASYNC_READ_COMPLETE, CYHAL_ISR_PRIORITY_DEFAULT, true);

     printf("ADC is configured in multichannel configuration.\r\n\n");
     printf("Channel 0 is configured in single ended mode, connected to the \r\n");
     printf("channel 0 input pin. Provide input voltage at the channel 0 input pin \r\n");
     printf("Channel 1 is configured in differential mode, connected to the \r\n");
     printf("channel 1 input pin and channel 1 voltage reference pin. Provide input voltage at the channel 1 input pin and reference \r\n");
     printf("voltage at the Channel 1 voltage reference pin \r\n\n");
}

/*******************************************************************************
 * Function Name: adc_multi_channel_process
 *******************************************************************************
 *
 * Summary:
 *  ADC single channel process function. This function reads the input voltage
 *  from channel 0 and channel 1. Prints the input voltage on UART.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 *******************************************************************************/
void adc_multi_channel_process(void)
{
    /* Variable to capture return value of functions */
    cy_rslt_t result;

    /* Variable to store ADC conversion result from channel 0 */
    int32_t adc_result_0 = 0;

    /* Variable to store ADC conversion result from channel 1 */
    int32_t adc_result_1 = 0;

    /* Initiate an asynchronous read operation. The event handler will be called
     * when it is complete. */
    result = cyhal_adc_read_async_uv(&adc_obj, NUM_SCAN, result_arr);
    if(result != CY_RSLT_SUCCESS)
    {
        printf("ADC async read failed. Error: %ld\n", (long unsigned int)result);
        CY_ASSERT(0);
    }

    /*
     * Read data from result list, input voltage in the result list is in
     * microvolts. Convert it millivolts and print input voltage
     *
     */
    adc_result_0 = result_arr[CHANNEL_0] / MICRO_TO_MILLI_CONV_RATIO;
    adc_result_1 = result_arr[CHANNEL_1] / MICRO_TO_MILLI_CONV_RATIO;
    printf("Channel 0 input: %4ldmV \t Channel 1 input: %4ldmV\r\n", (long int)adc_result_0, (long int)adc_result_1);

    /* Clear async read complete flag */
    async_read_complete = false;
}


/*******************************************************************************
 * Function Name: adc_event_handler
 *******************************************************************************
 *
 * Summary:
 *  ADC event handler. This function handles the asynchronous read complete event
 *  and sets the async_read_complete flag to true.
 *
 * Parameters:
 *  void *arg : pointer to result list
 *  cyhal_adc_event_t event : ADC event type
 *
 * Return:
 *  void
 *
 *******************************************************************************/
static void adc_event_handler(void* arg, cyhal_adc_event_t event)
{
    if(0u != (event & CYHAL_ADC_ASYNC_READ_COMPLETE))
    {
        /* Set async read complete flag to true */
        async_read_complete = true;
    }
}

#else

/*******************************************************************************
 * Function Name: adc_single_channel_init
 *******************************************************************************
 *
 * Summary:
 *  ADC single channel initialization function. This function initializes and
 *  configures channel 0 of ADC.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 *******************************************************************************/
void adc_single_channel_init(void)
{
    /* Variable to capture return value of functions */
    cy_rslt_t result;

    /* Initialize ADC. The ADC block which can connect to the channel 0 input pin is selected */
    result = cyhal_adc_init(&adc_obj, VPLUS_CHANNEL_0, NULL);
    if(result != CY_RSLT_SUCCESS)
    {
        printf("ADC initialization failed. Error: %ld\n", (long unsigned int)result);
        CY_ASSERT(0);
    }

    /* ADC channel configuration */
    const cyhal_adc_channel_config_t channel_config = {
            .enable_averaging = false,  // Disable averaging for channel
            .min_acquisition_ns = ACQUISITION_TIME_NS, // Minimum acquisition time set to 1us
            .enabled = true };          // Sample this channel when ADC performs a scan

    /* Initialize a channel 0 and configure it to scan the channel 0 input pin in single ended mode. */
    result  = cyhal_adc_channel_init_diff(&adc_chan_0_obj, &adc_obj, VPLUS_CHANNEL_0,
                                          CYHAL_ADC_VNEG, &channel_config);
    if(result != CY_RSLT_SUCCESS)
    {
        printf("ADC single ended channel initialization failed. Error: %ld\n", (long unsigned int)result);
        CY_ASSERT(0);
    }

    printf("ADC is configured in single channel configuration\r\n\n");
    printf("Provide input voltage at the channel 0 input pin. \r\n\n");
}

/*******************************************************************************
 * Function Name: adc_single_channel_process
 *******************************************************************************
 *
 * Summary:
 *  ADC single channel process function. This function reads the input voltage
 *  and prints the input voltage on UART.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 *******************************************************************************/
void adc_single_channel_process(void)
{
    /* Variable to store ADC conversion result from channel 0 */
    int32_t adc_result_0 = 0;

    /* Read input voltage, convert it to millivolts and print input voltage */
    adc_result_0 = cyhal_adc_read_uv(&adc_chan_0_obj) / MICRO_TO_MILLI_CONV_RATIO;
    printf("Channel 0 input: %4ldmV\r\n", (long int)adc_result_0);
}

#endif /* ADC_EXAMPLE_MODE == MULTI_CHANNEL */

/* [] END OF FILE */
