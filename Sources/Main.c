/** @file Main.c
 * Retrieve data from CAT B100 phone through the serial port interface.
 * @author Adrien RICCIARDI
 */
#include <File_System.h>
#include <Serial_Port.h>
#include <SMS.h>
#include <stdio.h>
#include <stdlib.h>

//-------------------------------------------------------------------------------------------------
// Entry point
//-------------------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
	char *Pointer_String_Serial_Port_Device;
	TSerialPortID Serial_Port_ID = SERIAL_PORT_INVALID_ID;
	int Return_Value = EXIT_FAILURE;

	// Check parameters
	if (argc != 2)
	{
		printf("Usage : %s Serial_Port\n", argv[0]);
		return EXIT_FAILURE;
	}
	Pointer_String_Serial_Port_Device = argv[1];

	// Try to open serial port
	if (SerialPortOpen(Pointer_String_Serial_Port_Device, 115200, SERIAL_PORT_PARITY_NONE, &Serial_Port_ID) != 0)
	{
		printf("Error : failed to open serial port \"%s\".\n", Pointer_String_Serial_Port_Device);
		goto Exit;
	}

	// Try to create all destination directories
	if (FileSystemCreateDirectory("Output") != 0) return -1;
	if (FileSystemCreateDirectory("Output/SMS") != 0) return -1;

	// TEST
	if (SMSDownloadAll(Serial_Port_ID) != 0)
	{
		printf("Error : failed to download SMS.\n");
		goto Exit;
	}

	// Everything went fine
	Return_Value = EXIT_SUCCESS;

Exit:
	if (Serial_Port_ID != SERIAL_PORT_INVALID_ID) SerialPortClose(Serial_Port_ID);
	return Return_Value;
}
