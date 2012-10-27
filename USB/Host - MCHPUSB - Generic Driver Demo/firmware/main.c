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
    DEMO_STATE_IDLE,
    DEMO_STATE_GET_INFO,
    DEMO_STATE_WAIT_GET_INFO,
    DEMO_STATE_GET_DEF,
    DEMO_STATE_WAIT_GET_DEF,
    DEMO_STATE_GET_MIN,
    DEMO_STATE_WAIT_GET_MIN,
    DEMO_STATE_GET_MAX,
    DEMO_STATE_WAIT_GET_MAX,
    DEMO_STATE_GET_CUR,
    DEMO_STATE_WAIT_GET_CUR,
    DEMO_STATE_SET_CUR,
    DEMO_STATE_WAIT_SET_CUR,
    DEMO_STATE_GET_CUR2,
    DEMO_STATE_WAIT_GET_CUR2,
    DEMO_STATE_GET_INFO2,//Commit
    DEMO_STATE_WAIT_GET_INFO2,//Commit
    DEMO_STATE_GET_CUR3,//Commit
    DEMO_STATE_WAIT_GET_CUR3,//Commit
    DEMO_STATE_SET_CUR2,//Commit
    DEMO_STATE_WAIT_SET_CUR2,//Commit
    DEMO_STATE_SET_ISOCHRONOUS,
    DEMO_STATE_WAIT_SET_ISOCHRONOUS,

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
ISOCHRONOUS_DATA isocData;

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
	if(USBHostIsochronousBuffersCreate(&isocData,2,1024)){
    	UART2PrintString( "CreateIsochronousBuffers\r\n" );
	}else{
        UART2PrintString( "Fail:CreateIsochronousBuffers\r\n" );
	}

    return TRUE;
} // InitializeSystem


BOOL CheckForNewAttach ( void )
{
    // Try to get the device address, if we don't have one.
    if (deviceAddress == 0)
    {
        GENERIC_DEVICE_ID DevID;

        /*DevID.vid   = 0x046D;
        DevID.pid   = 0xC526;*/
		DevID.vid   = gc_DevData.ID.vid;
		DevID.pid   = gc_DevData.ID.pid;
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
	        UART2PrintString( "VID=" );
			UART2PutHexWord(DevID.vid);
	        UART2PrintString( "\r\n" );
	        UART2PrintString( "PID=" );
			UART2PutHexWord(DevID.pid);
	        UART2PrintString( "\r\n" );
            return TRUE;
        }
    }

    return FALSE;

} // CheckForNewAttach

#define  USB_DEVICE_TO_HOST         0x80
#define  USB_HOST_TO_DEVICE         0x00
#define  USB_REQUEST_TYPE_CLASS     0x20
#define  USB_RECIPIENT_INTERFACE    0x01

#define SET_CUR  0x01
#define GET_CUR  0x81
#define GET_MIN  0x82
#define GET_MAX  0x83
#define GET_RES  0x84
#define GET_LEN  0x85
#define GET_INFO 0x86
#define GET_DEF  0x87

#define VS_PROBE_CONTROL  0x01
#define VS_COMMIT_CONTROL 0x02

BYTE control(int req, int cs, int index, BYTE* buf, int size){
	BYTE bRequest = req;//GET_CUR,GET_MINなど
	WORD wValue = cs << 8;
	WORD wIndex = index;//Entity ID and Interface または End Point
	WORD wLength = size;//パラメータの長さ
	if(req == SET_CUR){
		BYTE bmRequestType = USB_HOST_TO_DEVICE | USB_REQUEST_TYPE_CLASS | USB_RECIPIENT_INTERFACE;
		return USBHostIssueDeviceRequest(deviceAddress,bmRequestType,bRequest,wValue,wIndex,wLength,buf,USB_DEVICE_REQUEST_SET,0);
	}else{
		BYTE bmRequestType = USB_DEVICE_TO_HOST | USB_REQUEST_TYPE_CLASS | USB_RECIPIENT_INTERFACE;
		return USBHostIssueDeviceRequest(deviceAddress,bmRequestType,bRequest,wValue,wIndex,wLength,buf,USB_DEVICE_REQUEST_GET,0);
	}
}
void packet_dump(BYTE* data,long size){
	long j;
    UART2PrintString( "DATA=" );
	for(j = 0;j < size;j++){
          	UART2PutHex(data[j]);
    		UART2PrintString( " " );
	}
    UART2PrintString( "\r\n" );
}
BYTE param[34];
BYTE temp[34];
//int param_len = 34;
int param_len = 26;
BYTE jpeg[40 * 1024];
void ManageDemoState ( void )
{
	int j;
	DWORD   byteCount;
	BYTE    errorCode;
    BYTE RetVal;
	//接続されていなかったら初期化
    if (USBHostGenericDeviceDetached(deviceAddress) && deviceAddress != 0)
    {
		long i;
		for(i = 0;i < sizeof(jpeg);i++){
			jpeg[i] = 0;
		}
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
			DemoState = DEMO_STATE_GET_INFO;
        	UART2PrintString( "USB_Ver=" );
			UART2PutDec(((USB_DEVICE_DESCRIPTOR *)pDeviceDescriptor)->bcdUSB >> 8);
        	UART2PrintString( "-" );
			UART2PutDec(((USB_DEVICE_DESCRIPTOR *)pDeviceDescriptor)->bcdUSB);
        	UART2PrintString( "\r\n" );
        }
        break;
	case DEMO_STATE_GET_INFO:
		if(control(GET_INFO, VS_PROBE_CONTROL, 1, param, 1) == USB_SUCCESS){
        	UART2PrintString( "GET_INFO, VS_PROBE_CONTROL\r\n" );
			DemoState = DEMO_STATE_WAIT_GET_INFO;
		}
		break;
	case DEMO_STATE_WAIT_GET_INFO:
		if (USBHostTransferIsComplete( deviceAddress, 0, &errorCode, &byteCount )){
        	UART2PrintString( "OK=GET_INFO, VS_PROBE_CONTROL\r\n" );
        	UART2PrintString( "Byte Count=" );
			UART2PutDec(byteCount);
        	UART2PrintString( "\r\n" );
			packet_dump(param,byteCount);
        	UART2PrintString( "\r\n" );
			DemoState = DEMO_STATE_GET_DEF;
		}
		break;
	case DEMO_STATE_GET_DEF:
		if(control(GET_DEF, VS_PROBE_CONTROL, 1, temp, param_len) == USB_SUCCESS){
        	UART2PrintString( "GET_DEF, VS_PROBE_CONTROL\r\n" );
			DemoState = DEMO_STATE_WAIT_GET_DEF;
		}
		break;
	case DEMO_STATE_WAIT_GET_DEF:
		if (USBHostTransferIsComplete( deviceAddress, 0, &errorCode, &byteCount )){
        	UART2PrintString( "OK=GET_DEF, VS_PROBE_CONTROL\r\n" );
        	UART2PrintString( "Byte Count=" );
			UART2PutDec(byteCount);
        	UART2PrintString( "\r\n" );
			packet_dump(temp,byteCount);
        	UART2PrintString( "\r\n" );
			DemoState = DEMO_STATE_GET_MIN;
		}
		break;
	case DEMO_STATE_GET_MIN:
		if(control(GET_MIN, VS_PROBE_CONTROL, 1, temp, param_len) == USB_SUCCESS){
        	UART2PrintString( "GET_MIN, VS_PROBE_CONTROL\r\n" );
			DemoState = DEMO_STATE_WAIT_GET_MIN;
		}
		break;
	case DEMO_STATE_WAIT_GET_MIN:
		if (USBHostTransferIsComplete( deviceAddress, 0, &errorCode, &byteCount )){
        	UART2PrintString( "OK=GET_MIN, VS_PROBE_CONTROL\r\n" );
        	UART2PrintString( "Byte Count=" );
			UART2PutDec(byteCount);
        	UART2PrintString( "\r\n" );
			packet_dump(temp,byteCount);
        	UART2PrintString( "\r\n" );
			DemoState = DEMO_STATE_GET_MAX;
		}
		break;
	case DEMO_STATE_GET_MAX:
		if(control(GET_MAX, VS_PROBE_CONTROL, 1, temp, param_len) == USB_SUCCESS){
        	UART2PrintString( "GET_MAX, VS_PROBE_CONTROL\r\n" );
			DemoState = DEMO_STATE_WAIT_GET_MAX;
		}
		break;
	case DEMO_STATE_WAIT_GET_MAX:
		if (USBHostTransferIsComplete( deviceAddress, 0, &errorCode, &byteCount )){
        	UART2PrintString( "OK=GET_MAX, VS_PROBE_CONTROL\r\n" );
        	UART2PrintString( "Byte Count=" );
			UART2PutDec(byteCount);
        	UART2PrintString( "\r\n" );
			packet_dump(temp,byteCount);
        	UART2PrintString( "\r\n" );
			DemoState = DEMO_STATE_GET_CUR;
		}
		break;
	case DEMO_STATE_GET_CUR:
		if(control(GET_CUR, VS_PROBE_CONTROL, 1, temp, param_len) == USB_SUCCESS){
        	UART2PrintString( "GET_CUR, VS_PROBE_CONTROL\r\n" );
			DemoState = DEMO_STATE_WAIT_GET_CUR;
		}
		break;
	case DEMO_STATE_WAIT_GET_CUR:
		if (USBHostTransferIsComplete( deviceAddress, 0, &errorCode, &byteCount )){
        	UART2PrintString( "OK=GET_CUR, VS_PROBE_CONTROL\r\n" );
        	UART2PrintString( "Byte Count=" );
			UART2PutDec(byteCount);
        	UART2PrintString( "\r\n" );
			packet_dump(temp,byteCount);
        	UART2PrintString( "\r\n" );
			DemoState =DEMO_STATE_SET_CUR ;
		}
		break;
	case DEMO_STATE_SET_CUR:
		for(j = 0;j < param_len;j++){
			temp[j] = 0;
		}
		temp[2] = 2;//フォーマットインデックス
		temp[3] = 1;//フレームインデックス
		/*temp[4] = 0x80;
		temp[5] = 0x84;
		temp[6] = 0x1E;
		temp[7] = 0x00;*/
		temp[4] = 0x15;//30Hz
		temp[5] = 0x16;
		temp[6] = 0x05;
		temp[7] = 0x00;
		if(control(SET_CUR, VS_PROBE_CONTROL, 1, temp, param_len) == USB_SUCCESS){
        	UART2PrintString( "SET_CUR, VS_PROBE_CONTROL\r\n" );
			DemoState = DEMO_STATE_WAIT_SET_CUR;
		}
		break;
	case DEMO_STATE_WAIT_SET_CUR:
		if (USBHostTransferIsComplete( deviceAddress, 0, &errorCode, &byteCount )){
        	UART2PrintString( "OK=SET_CUR, VS_PROBE_CONTROL\r\n" );
        	UART2PrintString( "Byte Count=" );
			UART2PutDec(byteCount);
        	UART2PrintString( "\r\n" );
			packet_dump(temp,byteCount);
        	UART2PrintString( "\r\n" );
			DemoState =DEMO_STATE_GET_CUR2 ;
		}
		break;
	case DEMO_STATE_GET_CUR2:
		if(control(GET_CUR, VS_PROBE_CONTROL, 1, temp, param_len) == USB_SUCCESS){
        	UART2PrintString( "GET_CUR2, VS_PROBE_CONTROL\r\n" );
			DemoState = DEMO_STATE_WAIT_GET_CUR2;
		}
		break;
	case DEMO_STATE_WAIT_GET_CUR2:
		if (USBHostTransferIsComplete( deviceAddress, 0, &errorCode, &byteCount )){
        	UART2PrintString( "OK=GET_CUR2, VS_PROBE_CONTROL\r\n" );
        	UART2PrintString( "Byte Count=" );
			UART2PutDec(byteCount);
        	UART2PrintString( "\r\n" );
			packet_dump(temp,byteCount);
        	UART2PrintString( "\r\n" );
			DemoState =DEMO_STATE_GET_INFO2 ;
		}
		break;
	case DEMO_STATE_GET_INFO2:
		if(control(GET_INFO, VS_COMMIT_CONTROL, 1, param, 1) == USB_SUCCESS){
        	UART2PrintString( "GET_INFO, VS_COMMIT_CONTROL\r\n" );
			DemoState = DEMO_STATE_WAIT_GET_INFO2;
		}
		break;
	case DEMO_STATE_WAIT_GET_INFO2:
		if (USBHostTransferIsComplete( deviceAddress, 0, &errorCode, &byteCount )){
        	UART2PrintString( "OK=GET_INFO, VS_COMMIT_CONTROL\r\n" );
        	UART2PrintString( "Byte Count=" );
			UART2PutDec(byteCount);
        	UART2PrintString( "\r\n" );
			packet_dump(param,byteCount);
        	UART2PrintString( "\r\n" );
			DemoState = DEMO_STATE_GET_CUR3;
		}
		break;
	case DEMO_STATE_GET_CUR3:
		if(control(GET_CUR, VS_COMMIT_CONTROL, 1, temp, param_len) == USB_SUCCESS){
        	UART2PrintString( "GET_CUR2, VS_COMMIT_CONTROL\r\n" );
			DemoState = DEMO_STATE_WAIT_GET_CUR3;
		}
		break;
	case DEMO_STATE_WAIT_GET_CUR3:
		if (USBHostTransferIsComplete( deviceAddress, 0, &errorCode, &byteCount )){
        	UART2PrintString( "OK=GET_CUR2, VS_COMMIT_CONTROL\r\n" );
        	UART2PrintString( "Byte Count=" );
			UART2PutDec(byteCount);
        	UART2PrintString( "\r\n" );
			packet_dump(temp,byteCount);
        	UART2PrintString( "\r\n" );
			DemoState =DEMO_STATE_SET_CUR2 ;
		}
		break;
	case DEMO_STATE_SET_CUR2:
		for(j = 0;j < param_len;j++){
			temp[j] = 0;
		}
		temp[2] = 2;
		temp[3] = 0x01;//640x480 30Hz
		//temp[3] = 0x02;//160x120 30Hz
		//temp[3] = 0x0C;//800x600
		temp[4] = 0x80;//5Hz
		temp[5] = 0x84;
		temp[6] = 0x1E;
		temp[7] = 0x00;
		/*temp[4] = 0x15;//30Hz
		temp[5] = 0x16;
		temp[6] = 0x05;
		temp[7] = 0x00;*/
		if(control(SET_CUR, VS_COMMIT_CONTROL, 1, temp, param_len) == USB_SUCCESS){
        	UART2PrintString( "SET_CUR, VS_COMMIT_CONTROL\r\n" );
			DemoState = DEMO_STATE_WAIT_SET_CUR2;
		}
		break;
	case DEMO_STATE_WAIT_SET_CUR2:
		if (USBHostTransferIsComplete( deviceAddress, 0, &errorCode, &byteCount )){
        	UART2PrintString( "OK=SET_CUR, VS_COMMIT_CONTROL\r\n" );
        	UART2PrintString( "Byte Count=" );
			UART2PutDec(byteCount);
        	UART2PrintString( "\r\n" );
			packet_dump(temp,byteCount);
        	UART2PrintString( "\r\n" );
			DemoState =DEMO_STATE_SET_ISOCHRONOUS ;
		}
		break;
	case DEMO_STATE_SET_ISOCHRONOUS:
		if(USBHostIssueDeviceRequest( deviceAddress, 0x01, USB_REQUEST_SET_INTERFACE,
            0x06/*Alternate Setting:0x1*/, 0x01/*interface:0x03*/, 0, NULL, USB_DEVICE_REQUEST_SET,
            0x00 )== USB_SUCCESS){
          	UART2PrintString( "USB_REQUEST_SET_INTERFACE=OK!\r\n" );
			DemoState =DEMO_STATE_WAIT_SET_ISOCHRONOUS ;
		}
		break;
	case DEMO_STATE_WAIT_SET_ISOCHRONOUS:
		if(USBHostReadIsochronous(deviceAddress,0x81,&isocData) == USB_SUCCESS){
          		//UART2PrintString( "USBHostReadIsochronous=OK!\r\n" );
			DemoState = DEMO_STATE_ERROR;
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
	static int aaa = 0;
	static long data_cnt = 0;
	static long data_cnt_bak = 0;
	static long jpeg_cnt = 0;
	static long jpeg_size = 0;
	static long jpeg_size_bak = 0;
	static long jpeg_ptr = 0;
	static long jpeg_start = 0;
    #ifdef USB_GENERIC_SUPPORT_SERIAL_NUMBERS
        BYTE i;
    #endif
	int j;
    // Handle specific events.
    switch ( (INT)event )
    {
        case EVENT_TRANSFER:         // A USB transfer has completed
		case EVENT_DATA_ISOC_READ:
			for(j = 0;j < size-1;j++){
				if(((BYTE*)data)[j] == 0xFF){
						if(((BYTE*)data)[j+1] == 0xD8){
							if(((BYTE*)data)[j+2] == 0xFF){
							if(((BYTE*)data)[j+3] == 0xE0){
								jpeg_size = data_cnt -jpeg_size_bak;
								jpeg_size_bak = data_cnt;
		            			/*UART2PrintString( "JPEG-SIZE=" );
								UART2PutDec((jpeg_size/100000) % 10);
								UART2PutDec((jpeg_size/10000) % 10);
								UART2PutDec((jpeg_size/1000) % 10);
								UART2PutDec((jpeg_size/100) % 10);
								UART2PutDec((jpeg_size/10) % 10);
								UART2PutDec(jpeg_size % 10);
		            			UART2PrintString( "JPEG-CNT=" );
								UART2PutDec((jpeg_cnt/1000) % 10);
								UART2PutDec((jpeg_cnt/100) % 10);
								UART2PutDec((jpeg_cnt/10) % 10);
								UART2PutDec(jpeg_cnt % 10);
		            			UART2PrintString( "\r\n" );*/
								if(jpeg_cnt == 1){
			            			UART2PrintString( "JPEG-SIZE=" );
									UART2PutDec((jpeg_ptr/100000) % 10);
									UART2PutDec((jpeg_ptr/10000) % 10);
									UART2PutDec((jpeg_ptr/1000) % 10);
									UART2PutDec((jpeg_ptr/100) % 10);
									UART2PutDec((jpeg_ptr/10) % 10);
									UART2PutDec(jpeg_ptr % 10);
		            				UART2PrintString( "\r\n" );
									packet_dump(jpeg,jpeg_ptr);
								}
								jpeg_cnt++;
								jpeg_start = j;
								//packet_dump((data + j),4);
							}
							
							}
						}
					}
			}
			if((size != 0x0C) && jpeg_cnt == 1){
				int i;
				for(i = 0x0C;i < size;i++){
					if(jpeg_ptr == 0){
            			UART2PrintString( "jpeg_start=" );
						i = jpeg_start;
						UART2PutDec(jpeg_cnt % 10);
            			UART2PrintString( "\r\n" );
					}
					jpeg[jpeg_ptr++] = ((BYTE*)data)[i];
				}
			}
			if(size != 0x0C){
			/*if(aaa >= 0 && aaa <= 0){
	            UART2PrintString( "DATA=" );
				packet_dump(data,size);
	            UART2PrintString( "\r\n ");
				aaa++;
			}*/
			}
			data_cnt += size;
			/*if(data_cnt > data_cnt_bak + 1024 * 10){
				//data_cnt = 1024 * 100;
	            UART2PrintString( "a" );
				UART2PutDec((data_cnt/1000000) % 10);
				UART2PutDec((data_cnt/100000) % 10);
				UART2PutDec((data_cnt/10000) % 10);
				UART2PutDec((data_cnt/1000) % 10);
				UART2PutDec((data_cnt/100) % 10);
				UART2PutDec((data_cnt/10) % 10);
				UART2PutDec(data_cnt % 10);
	            UART2PrintString( "\r\n" );
				data_cnt_bak = data_cnt;
			}*/

            return TRUE;
			break;
            UART2PrintString( "a" );
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
            UART2PrintString( "a" );
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

