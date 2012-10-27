/******************************************************************************
            USB Custom Demo, Host

This file provides the main entry point to the Microchip USB Custom
Host demo.  This demo shows how a PIC24F system could be used to
act as the host, controlling a USB device running the Microchip Custom
Device demo.

******************************************************************************/

/******************************************************************************
* Filename:        main.c
* Dependancies:    USB Host Driver with Generic Client Driver
* Processor:       PIC24F256GB1xx
* Hardware:        Explorer 16 with USB PICtail Plus
* Compiler:        C30 v2.01/C32 v0.00.18
* Company:         Microchip Technology, Inc.

Software License Agreement

The software supplied herewith by Microchip Technology Incorporated
(the 鼎ompany・ for its PICmicroｮ Microcontroller is intended and
supplied to you, the Company痴 customer, for use solely and
exclusively on Microchip PICmicro Microcontroller products. The
software is owned by the Company and/or its supplier, and is
protected under applicable copyright laws. All rights are reserved.
Any use in violation of the foregoing restrictions may subject the
user to criminal sanctions under applicable laws, as well as to
civil liability for the breach of the terms and conditions of this
license.

THIS SOFTWARE IS PROVIDED IN AN 鄭S IS・CONDITION. NO WARRANTIES,
WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.


*******************************************************************************/

#include <stdlib.h>
#include "GenericTypeDefs.h"
#include "HardwareProfile.h"
#include "usb_config.h"
#include "USB/usb.h"
#include "USB/usb_host_generic.h"
#include "user.h"
#include "LCDBlocking.h"
#include "timer.h"

// *****************************************************************************
// *****************************************************************************
// Configuration Bits
// *****************************************************************************
// *****************************************************************************
    #pragma config UPLLEN   = ON            // USB PLL Enabled
    #pragma config FPLLMUL  = MUL_15        // PLL Multiplier
    #pragma config UPLLIDIV = DIV_2         // USB PLL Input Divider
    #pragma config FPLLIDIV = DIV_2         // PLL Input Divider
    #pragma config FPLLODIV = DIV_1         // PLL Output Divider
    #pragma config FPBDIV   = DIV_1         // Peripheral Clock divisor
    #pragma config FWDTEN   = OFF           // Watchdog Timer
    #pragma config WDTPS    = PS1           // Watchdog Timer Postscale
    #pragma config FCKSM    = CSDCMD        // Clock Switching & Fail Safe Clock Monitor
    #pragma config OSCIOFNC = OFF           // CLKO Enable
    #pragma config POSCMOD  = HS            // Primary Oscillator
    #pragma config IESO     = OFF           // Internal/External Switch-over
    #pragma config FSOSCEN  = OFF           // Secondary Oscillator Enable (KLO was off)
    #pragma config FNOSC    = PRIPLL        // Oscillator Selection
    #pragma config CP       = OFF           // Code Protect
    #pragma config BWP      = OFF           // Boot Flash Write Protect
    #pragma config PWP      = OFF           // Program Flash Write Protect
    #pragma config ICESEL   = ICS_PGx2      // ICE/ICD Comm Channel Select

// Application States
typedef enum
{
    DEMO_INITIALIZE = 0,                // Initialize the app when a device is attached
    DEMO_STATE_IDLE,                    // Inactive State
    DEMO_STATE_READ,                    // Inactive State
    DEMO_STATE_WAIT_READ,                    // Inactive State
    DEMO_STATE_ERROR                    // An error has occured

} DEMO_STATE;

// *****************************************************************************
// *****************************************************************************
// Global Variables
// *****************************************************************************
// *****************************************************************************

BYTE        deviceAddress;  // Address of the device on the USB
DATA_PACKET DataPacket;     // Data to send to the device
DEMO_STATE  DemoState;      // Current state of the demo application

BOOL InitializeSystem ( void )
{

    int  value;

    value = SYSTEMConfigWaitStatesAndPB( GetSystemClock() );

    // Enable the cache for the best performance
    CheKseg0CacheOn();

    INTEnableSystemMultiVectoredInt();

    value = OSCCON;
    while (!(value & 0x00000020))
    {
        value = OSCCON;    // Wait for PLL lock to stabilize
    }

    // Init UART
    UART2Init();
    // Set Default demo state
    DemoState = DEMO_INITIALIZE;

    return TRUE;
} // InitializeSystem


BOOL CheckForNewAttach ( void )
{
    // Try to get the device address, if we don't have one.
    if (deviceAddress == 0)
    {
        GENERIC_DEVICE_ID DevID;

        DevID.vid   = 0x046D;
        DevID.pid   = 0xC526;
        #ifdef USB_GENERIC_SUPPORT_SERIAL_NUMBERS
            DevID.serialNumberLength = 0;
            DevID.serialNumber = NULL;
        #endif

        if (USBHostGenericGetDeviceAddress(&DevID))
        {
            deviceAddress = DevID.deviceAddress;
            UART2PrintString( "Generic demo device attached - polled, deviceAddress=" );
            UART2PutDec( deviceAddress );
            UART2PrintString( "\r\n" );
            return TRUE;
        }
    }

    return FALSE;

} // CheckForNewAttach

void ManageDemoState ( void )
{
    BYTE RetVal;
	//接続されていなかったら初期化
    if (USBHostGenericDeviceDetached(deviceAddress) && deviceAddress != 0)
    {
        UART2PrintString( "Generic demo device detached - polled\r\n" );
        DemoState = DEMO_INITIALIZE;
        deviceAddress   = 0;
    }
    switch (DemoState)
    {
    case DEMO_INITIALIZE:
        DemoState = DEMO_STATE_IDLE;
        break;
    case DEMO_STATE_IDLE:
		//接続されたら、読み出し処理に移行
        if (CheckForNewAttach())
        {
			DemoState = DEMO_STATE_READ;
        }
        break;
    case DEMO_STATE_READ:
        if (!USBHostGenericRxIsBusy(deviceAddress))
        {
            if ( (RetVal=USBHostGenericRead(deviceAddress, &DataPacket, 64)) == USB_SUCCESS )
            {
                DemoState = DEMO_STATE_WAIT_READ;
            }
            else
            {
                UART2PrintString( "1 Device Read Error 0x" );
                UART2PutHex(RetVal);
                UART2PrintString( "\r\n" );
            }
        }
		break;
	case DEMO_STATE_WAIT_READ:
        if (!USBHostGenericRxIsBusy(deviceAddress)){
			int j;
			if(gc_DevData.rxLength == 0){//データ長が0だった場合はやり直し。
	            DemoState = DEMO_STATE_READ;
				break;
			}
			//内容を表示
           	UART2PrintString( "GET_DATA=" );
			for(j = 0;j < gc_DevData.rxLength;j++){
            	UART2PutHex(DataPacket._byte[j]);
			}
            UART2PrintString( "\r\n" );
	        DemoState = DEMO_STATE_READ;
    		//DelayMs(10);//表示が早すぎる場合は、delayを行う
		}
		break;
    case DEMO_STATE_ERROR:
        break;
    default:
        DemoState = DEMO_INITIALIZE;
        break;
    }
    //DelayMs(1); // 1ms delay
} // ManageDemoState

BOOL USB_ApplicationEventHandler ( BYTE address, USB_EVENT event, void *data, DWORD size )
{
    #ifdef USB_GENERIC_SUPPORT_SERIAL_NUMBERS
        BYTE i;
    #endif

    // Handle specific events.
    switch ( (INT)event )
    {
        case EVENT_GENERIC_ATTACH:
            return TRUE;
            break;

        case EVENT_GENERIC_DETACH:
            deviceAddress   = 0;
            DemoState = DEMO_INITIALIZE;
            UART2PrintString( "Generic demo device detached - event\r\n" );
            return TRUE;

        case EVENT_GENERIC_TX_DONE:           // The main state machine will poll the driver.
        case EVENT_GENERIC_RX_DONE:
            return TRUE;

        case EVENT_VBUS_REQUEST_POWER:
            // We'll let anything attach.
            return TRUE;

        case EVENT_VBUS_RELEASE_POWER:
            // We aren't keeping track of power.
            return TRUE;

        case EVENT_HUB_ATTACH:
            UART2PrintString( "\r\n***** USB Error - hubs are not supported *****\r\n" );
            return TRUE;
            break;

        case EVENT_UNSUPPORTED_DEVICE:
            UART2PrintString( "\r\n***** USB Error - device is not supported *****\r\n" );
            return TRUE;
            break;

        case EVENT_CANNOT_ENUMERATE:
            UART2PrintString( "\r\n***** USB Error - cannot enumerate device *****\r\n" );
            return TRUE;
            break;

        case EVENT_CLIENT_INIT_ERROR:
            UART2PrintString( "\r\n***** USB Error - client driver initialization error *****\r\n" );
            return TRUE;
            break;

        case EVENT_OUT_OF_MEMORY:
            UART2PrintString( "\r\n***** USB Error - out of heap memory *****\r\n" );
            return TRUE;
            break;

        case EVENT_UNSPECIFIED_ERROR:   // This should never be generated.
            UART2PrintString( "\r\n***** USB Error - unspecified *****\r\n" );
            return TRUE;
            break;

        case EVENT_SUSPEND:
        case EVENT_DETACH:
        case EVENT_RESUME:
        case EVENT_BUS_ERROR:
            return TRUE;
            break;

        default:
            break;
    }
    return FALSE;
}
int main ( void )
{
    if ( InitializeSystem() != TRUE )
    {
        UART2PrintString( "\r\n\r\nCould not initialize USB Custom Demo App - system.  Halting.\r\n\r\n" );
        while (1);
    }
    if ( USBHostInit(0) == TRUE )
    {
        UART2PrintString( "\r\n\r\n***** USB Custom Demo App Initialized *****\r\n\r\n" );
    }
    else
    {
        UART2PrintString( "\r\n\r\nCould not initialize USB Custom Demo App - USB.  Halting.\r\n\r\n" );
        while (1);
    }
    while (1)
    {
        USBHostTasks();
        ManageDemoState();
    }
    return 0;
} // main
/*************************************************************************
 * EOF main.c
 */

