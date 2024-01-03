/*! @file   uart_loopback_24mhz_brclk.c
    @brief   UART Loopback at 24MHz using 
    
    @mainpage
    @section intro Introduction
    This example shows how to configure the UART module as loopback mode.
    The example receives a character from the UART trasnmitter and echoes it
    back to the same UART transmitter.
    The example uses the following peripherals and I/O signals.  You must
    review these and change as needed for your own board:
    -# UART peripheral
    -# GPIO Port peripheral (for UART pins)
    -# UART_TXD
    -# UART_RXD
    @section hw HW and SW Configuration
    @subsection hwconfig Hardware Configuration
    This example can be run on any MSP432 board. It is not necessary to
    change any hardware settings to run this example.
    @subsection swconfig Software Configuration
    This example uses the following peripherals and I/O signals.  You must
    review these and change as needed for your own board:
    -# Configure UART pins for UART
    -# Configure UART peripheral
    -# Configure GPIOs for LEDs
    @subsection resources Resources
    -# <a href="http://www.ti.com/litv/pdf/swru367">MSP432 Family User's Guide</a> - Describes the UART module
    -# <a href="http://www.ti.com/litv/pdf/slau356">MSP432P4xx Technical Reference Manual</a> - Describes the UART module
    @section exampledescription Description of the example
    The example receives a character from the UART trasnmitter and echoes it
    back to the same UART transmitter.
    @section APIs APIs Used
    -# UART_initModule()
    -# UART_enableModule()
    -# UART_enableInterrupt()
    -# UART_transmitData()
    -# UART_receiveData()
    -# Interrupt_enableInterrupt()
    -# Interrupt_enableSleepOnIsrExit()
    -# Interrupt_disableSleepOnIsrExit()
    -# PCM_gotoLPM0InterruptSafe()
    -# GPIO_setAsOutputPin()
    -# GPIO_setOutputLowOnPin()
    -# GPIO_setOutputHighOnPin()
    -# GPIO_setAsPeripheralModuleFunctionInputPin()
    @section files Files Used
    -# @ref uart_loopback_24mhz_brclk.c
*/



/* DriverLib Includes */
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>
#include <ti/devices/msp432p4xx/driverlib/dma.h>
#include <ti/devices/msp432p4xx/inc/msp.h>

/* Standard Includes */
#include <stdint.h>
#include <stdbool.h>

#define RX_BUFFER_SIZE 256              //! Serial RX buffer size

uint8_t TXData = 1;                     //! UART internal loopback data
uint8_t RXData = 0;                     //! UART internal loopback data     
uint_fast8_t data[RX_BUFFER_SIZE];      //! Serial RX buffer
volatile uint_fast16_t dataCount = 0;   //! Serial RX buffer counter
volatile bool stringEnd = false;        //! Serial RX buffer end flag

/*! @details    Configuration Parameter. These are the configuration parameters to
                make the eUSCI A UART module to operate with a 9600 baud rate. These
                values were calculated using the online calculator that TI provides
                at:
                http://software-dl.ti.com/msp430/msp430_public_sw/mcu/msp430/MSP430BaudRateConverter/index.html
 */
const eUSCI_UART_ConfigV1 uartConfig =
{
        EUSCI_A_UART_CLOCKSOURCE_SMCLK,             // SMCLK Clock Source
        156,                                        // BRDIV = 13
        4,                                          // UCxBRF = 0
        0,                                          // UCxBRS = 37
        EUSCI_A_UART_NO_PARITY,                     // No Parity
        EUSCI_A_UART_LSB_FIRST,                     // LSB First
        EUSCI_A_UART_ONE_STOP_BIT,                  // One stop bit
        EUSCI_A_UART_MODE,                          // UART mode
        EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION,  // Oversampling
        EUSCI_A_UART_8_BIT_LEN                      // 8 bit data length
};

const eUSCI_UART_ConfigV1 pcUartConfig =
{
        EUSCI_A_UART_CLOCKSOURCE_SMCLK,             // SMCLK Clock Source
        13,                                         // BRDIV = 13
        0,                                          // UCxBRF = 0
        37,                                         // UCxBRS = 37
        EUSCI_A_UART_NO_PARITY,                     // No Parity
        EUSCI_A_UART_LSB_FIRST,                     // LSB First
        EUSCI_A_UART_ONE_STOP_BIT,                  // One stop bit
        EUSCI_A_UART_MODE,                          // UART mode
        EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION,  // Oversampling
        EUSCI_A_UART_8_BIT_LEN                      // 8 bit data length
};



#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_ALIGN(MSP_EXP432P401RLP_DMAControlTable, 1024)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma data_alignment=1024
#elif defined(__GNUC__)
__attribute__ ((aligned (1024)))
#elif defined(__CC_ARM)
__align(1024)
#endif
static DMA_ControlTable MSP_EXP432P401RLP_DMAControlTable[32];  //! DMA Control Table for 32 DMA channels
                                                                //!< DMA Control Table for 32 DMA channels aligned to 1024 byte boundary
/*!
*  @brief  This function initializes all the peripherals used in this example
*          and enables the global interrupt.
*
*  @param  none
*
*  @return none
*/
 
int main(void){
    /* Halting WDT  */
    MAP_WDT_A_holdTimer();

    /* P1.0 as output (LED) */
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0);
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0);
    //Set RGB led pins as output
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN0);
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN1);
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN2);

    /* Setting DCO to 24MHz (upping Vcore) */
    FlashCtl_setWaitState(FLASH_BANK0, 1);
    FlashCtl_setWaitState(FLASH_BANK1, 1);
    MAP_PCM_setCoreVoltageLevel(PCM_VCORE1);
    CS_setDCOCenteredFrequency(CS_DCO_FREQUENCY_24);


    /* Configuring UART Modules */
    //Setting pins
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P3,
                 GPIO_PIN2 | GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION);  //GPS
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P1,
                GPIO_PIN2 | GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION);   //PC
    //Configure module
    MAP_UART_initModule(EUSCI_A2_BASE, &uartConfig);                    //GPS
    MAP_UART_initModule(EUSCI_A0_BASE, &pcUartConfig);                  //PC
    // Enable UART module
    MAP_UART_enableModule(EUSCI_A2_BASE);                               //GPS
    MAP_UART_enableModule(EUSCI_A0_BASE);                               //PC
    // Enabling interrupts
    MAP_UART_enableInterrupt(EUSCI_A2_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT);
    MAP_Interrupt_enableInterrupt(INT_EUSCIA2);
//    MAP_UART_enableInterrupt(EUSCI_A0_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT);
//    MAP_Interrupt_enableInterrupt(INT_EUSCIA0);

    //DMA
    MAP_DMA_enableModule();
    MAP_DMA_setControlBase(MSP_EXP432P401RLP_DMAControlTable);
    MAP_DMA_assignChannel(DMA_CH5_EUSCIA2RX);

    MAP_Interrupt_enableSleepOnIsrExit();
    while(1){
        if(stringEnd){
            for(int i = 0; i < dataCount; ++i){
                MAP_UART_transmitData(EUSCI_A0_BASE, data[i]);
            }
        }

        MAP_Interrupt_enableSleepOnIsrExit();
        MAP_PCM_gotoLPM0InterruptSafe();
    }
}

/*!
*  @brief   This function handles the UART RX interrupt.
*  @details It reads the data from RX register until character "\n\r" is received and moves into the RX buffer on the memory.
*           It also sets the stringEnd flag to indicate the end of the string.
*  @param   none
*  @return  none
*/

void EUSCIA2_IRQHandler(void){
    uint32_t status = MAP_UART_getEnabledInterruptStatus(EUSCI_A2_BASE);

    if(status & EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG){
        RXData = MAP_UART_receiveData(EUSCI_A2_BASE);
        if(dataCount < RX_BUFFER_SIZE){
            data[dataCount++] = RXData;
        }else if(dataCount > RX_BUFFER_SIZE - 1 || (RXData == '\r' && data[dataCount-1] == '\n')){
            stringEnd = true;
            MAP_Interrupt_disableSleepOnIsrExit();
        }
    }
}

//void EUSCIA0_IRQHandler(void){
//    uint32_t status = MAP_UART_getEnabledInterruptStatus(EUSCI_A0_BASE);
//
//
//    if(status & EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG){
//        uint_fast8_t data = MAP_UART_receiveData(EUSCI_A0_BASE);
//        MAP_UART_transmitData(EUSCI_A0_BASE, data);
//    }
//
//}
