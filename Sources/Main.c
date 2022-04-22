/** @file Main.c
 * Retrieve data from CAT B100 phone through the serial port interface.
 * @author Adrien RICCIARDI
 */
#include <AT_Command.h>
#include <File_Manager.h>
#include <MMS.h>
#include <Serial_Port.h>
#include <SMS.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Utility.h>

//-------------------------------------------------------------------------------------------------
// Private types
//-------------------------------------------------------------------------------------------------
/** All available command line interface commands. */
typedef enum
{
	MAIN_COMMAND_LIST_DRIVES,
	MAIN_COMMAND_LIST_DIRECTORY,
	MAIN_COMMAND_GET_FILE,
	MAIN_COMMAND_GET_ALL_MMS,
	MAIN_COMMAND_GET_ALL_SMS,
	MAIN_COMMANDS_COUNT
} TMainCommand;

//-------------------------------------------------------------------------------------------------
// Private functions
//-------------------------------------------------------------------------------------------------
/** Display the usage message.
 * @param Pointer_String_Program_Name The program name as invoked by the user.
 */
static void MainDisplayUsage(char *Pointer_String_Program_Name)
{
	printf("Usage : %s Serial_Port Command [Parameter_1] [Parameter_2]...\n"
		"File commands :\n"
		"  list-drives\n"
		"  list-directory <absolute path>\n"
		"  get-file <absolute file path on phone> <output file path on PC>\n"
		"MMS commands :\n"
		"  get-all-mms\n"
		"SMS commands :\n"
		"  get-all-sms\n", Pointer_String_Program_Name);
}

//-------------------------------------------------------------------------------------------------
// Entry point
//-------------------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
	char *Pointer_String_Serial_Port_Device, *Pointer_String_Argument_1 = NULL, *Pointer_String_Argument_2 = NULL;
	TSerialPortID Serial_Port_ID = SERIAL_PORT_INVALID_ID;
	int Return_Value = EXIT_FAILURE, i;
	TMainCommand Command = MAIN_COMMANDS_COUNT; // This value is invalid, this allows to detect if no known command was provided by the user
	TFileManagerList List;

	// Check parameters
	if (argc < 3)
	{
		MainDisplayUsage(argv[0]);
		return EXIT_FAILURE;
	}
	Pointer_String_Serial_Port_Device = argv[1]; // Serial port device is always the first argument

	// Parse command and parameters
	for (i = 2; i < argc; i++)
	{
		// MAIN_COMMAND_LIST_DRIVES
		if (strcmp(argv[i], "list-drives") == 0)
		{
			Command = MAIN_COMMAND_LIST_DRIVES;
			break;
		}
		// MAIN_COMMAND_LIST_DIRECTORY
		else if (strcmp(argv[i], "list-directory") == 0)
		{
			// Retrieve the mandatory argument
			i++;
			if (i == argc)
			{
				printf("Error : the list-directory command needs one argument, the absolute path to list.\n");
				MainDisplayUsage(argv[0]);
				return EXIT_FAILURE;
			}
			Pointer_String_Argument_1 = argv[i];

			Command = MAIN_COMMAND_LIST_DIRECTORY;
			break;
		}
		// MAIN_COMMAND_GET_FILE
		else if (strcmp(argv[i], "get-file") == 0)
		{
			// Retrieve the first mandatory argument
			i++;
			if (i == argc)
			{
				printf("Error : the get-file command needs two arguments, the file path on the phone and the output file path on the PC.\n");
				MainDisplayUsage(argv[0]);
				return EXIT_FAILURE;
			}
			Pointer_String_Argument_1 = argv[i];

			// Retrieve the second mandatory argument
			i++;
			if (i == argc)
			{
				printf("Error : the get-file command needs a second argument, the output file path on the PC.\n");
				MainDisplayUsage(argv[0]);
				return EXIT_FAILURE;
			}
			Pointer_String_Argument_2 = argv[i];

			Command = MAIN_COMMAND_GET_FILE;
			break;
		}
		// MAIN_COMMAND_GET_ALL_MMS
		else if (strcmp(argv[i], "get-all-mms") == 0)
		{
			Command = MAIN_COMMAND_GET_ALL_MMS;
			break;
		}
		// MAIN_COMMAND_GET_ALL_SMS
		else if (strcmp(argv[i], "get-all-sms") == 0)
		{
			Command = MAIN_COMMAND_GET_ALL_SMS;
			break;
		}
	}

	// Is the command known ?
	if (Command == MAIN_COMMANDS_COUNT)
	{
		printf("Error : unknown command.\n");
		MainDisplayUsage(argv[0]);
		return EXIT_FAILURE;
	}

	// Try to open serial port
	if (SerialPortOpen(Pointer_String_Serial_Port_Device, 115200, SERIAL_PORT_PARITY_NONE, &Serial_Port_ID) != 0)
	{
		printf("Error : failed to open serial port \"%s\".\n", Pointer_String_Serial_Port_Device);
		goto Exit;
	}

	// Try to create all destination directories
	if (UtilityCreateDirectory("Output") != 0) goto Exit;
	if (UtilityCreateDirectory("Output/SMS") != 0) goto Exit;

	// Run the command
	switch (Command)
	{
		case MAIN_COMMAND_LIST_DRIVES:
			if (FileManagerListDrives(Serial_Port_ID, &List) != 0)
			{
				printf("Error : failed to list the drives.\n");
				goto Exit;
			}
			FileManagerDisplayDirectoryListing(&List);
			FileManagerListClear(&List);
			break;

		case MAIN_COMMAND_LIST_DIRECTORY:
			if (FileManagerListDirectory(Serial_Port_ID, Pointer_String_Argument_1, &List) != 0)
			{
				printf("Error : failed to list the directory \"%s\".\n", Pointer_String_Argument_1);
				goto Exit;
			}
			FileManagerDisplayDirectoryListing(&List);
			FileManagerListClear(&List);
			break;

		case MAIN_COMMAND_GET_FILE:
			if (FileManagerDownloadFile(Serial_Port_ID, Pointer_String_Argument_1, Pointer_String_Argument_2) != 0)
			{
				printf("Error : could not get the file \"%s\".\n", Pointer_String_Argument_1);
				goto Exit;
			}
			printf("The file \"%s\" was successfully retrieved from the phone.\n", Pointer_String_Argument_1);
			break;

		case MAIN_COMMAND_GET_ALL_MMS:
			if (MMSDownloadAll(Serial_Port_ID) != 0)
			{
				printf("Error : failed to download MMS.\n");
				goto Exit;
			}
			printf("All MMS were successfully retrieved.\n");
			break;

		case MAIN_COMMAND_GET_ALL_SMS:
			if (SMSDownloadAll(Serial_Port_ID) != 0)
			{
				printf("Error : failed to download SMS.\n");
				goto Exit;
			}
			printf("All SMS were successfully retrieved.\n");
			break;

		default:
			printf("Error : the command %d implementation is missing.\n", Command);
			break;
	}

	// Everything went fine
	Return_Value = EXIT_SUCCESS;

Exit:
	if (Serial_Port_ID != SERIAL_PORT_INVALID_ID) SerialPortClose(Serial_Port_ID);
	return Return_Value;
}
