/** @file Main.c
 * Retrieve data from CAT B100 phone through the serial port interface.
 * @author Adrien RICCIARDI
 */
#include <AT_Command.h>
#include <File_Manager.h>
#include <List.h>
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
	MAIN_COMMAND_SEND_FILE,
	MAIN_COMMAND_GET_DIRECTORY,
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
		"  get-file <absolute file path on the phone> <output file path on the PC>\n"
		"  send-file <source file path on the PC> <absolute target file path on the phone>\n"
		"  get-directory <absolute directory path on the phone> <output directory path on the PC>\n"
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
	char *Pointer_String_Serial_Port_Device, *Pointer_String_Argument_1 = NULL, *Pointer_String_Argument_2 = NULL, String_Date[12]; // The GCC standard tells that the date string is always 11-character long
	TSerialPortID Serial_Port_ID = SERIAL_PORT_INVALID_ID;
	int Return_Value = EXIT_FAILURE, i;
	TMainCommand Command = MAIN_COMMANDS_COUNT; // This value is invalid, this allows to detect if no known command was provided by the user
	TList List;

	// Display the program banner
	strcpy(String_Date, __DATE__); // Get a copy of the literal date string, so it is easy to get an offset from the copy
	printf("+--------------------------------+\n"
		"|        CAT B100 tools          |\n"
		"| (C) 2022-%s Adrien RICCIARDI |\n"
		"+--------------------------------+\n", &String_Date[7]); // The year field is the last part of the date string, so there is no need to extract the year field from the string

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
		// MAIN_COMMAND_SEND_FILE
		else if (strcmp(argv[i], "send-file") == 0)
		{
			// Retrieve the first mandatory argument
			i++;
			if (i == argc)
			{
				printf("Error : the send-file command needs two arguments, the source file path on the PC and the target file path on the phone.\n");
				MainDisplayUsage(argv[0]);
				return EXIT_FAILURE;
			}
			Pointer_String_Argument_1 = argv[i];

			// Retrieve the second mandatory argument
			i++;
			if (i == argc)
			{
				printf("Error : the send-file command needs a second argument, the target file path on the phone.\n");
				MainDisplayUsage(argv[0]);
				return EXIT_FAILURE;
			}
			Pointer_String_Argument_2 = argv[i];

			Command = MAIN_COMMAND_SEND_FILE;
			break;
		}
		// MAIN_COMMAND_GET_DIRECTORY
		else if (strcmp(argv[i], "get-directory") == 0)
		{
			// Retrieve the first mandatory argument
			i++;
			if (i == argc)
			{
				printf("Error : the get-directory command needs two arguments, the directory path on the phone and the output directory path on the PC.\n");
				MainDisplayUsage(argv[0]);
				return EXIT_FAILURE;
			}
			Pointer_String_Argument_1 = argv[i];

			// Retrieve the second mandatory argument
			i++;
			if (i == argc)
			{
				printf("Error : the get-directory command needs a second argument, the output directory path on the PC.\n");
				MainDisplayUsage(argv[0]);
				return EXIT_FAILURE;
			}
			Pointer_String_Argument_2 = argv[i];

			Command = MAIN_COMMAND_GET_DIRECTORY;
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

	// Try to create the root destination directory
	if (UtilityCreateDirectory("Output") != 0) goto Exit;

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
			ListClear(&List);
			break;

		case MAIN_COMMAND_LIST_DIRECTORY:
			if (FileManagerListDirectory(Serial_Port_ID, Pointer_String_Argument_1, &List) != 0)
			{
				printf("Error : failed to list the directory \"%s\".\n", Pointer_String_Argument_1);
				goto Exit;
			}
			FileManagerDisplayDirectoryListing(&List);
			ListClear(&List);
			break;

		case MAIN_COMMAND_GET_FILE:
			printf("Downloading the file \"%s\" from the phone...\n", Pointer_String_Argument_1);
			if (FileManagerDownloadFile(Serial_Port_ID, Pointer_String_Argument_1, Pointer_String_Argument_2) != 0)
			{
				printf("Error : could not get the file \"%s\".\n", Pointer_String_Argument_1);
				goto Exit;
			}
			printf("The file \"%s\" was successfully retrieved from the phone.\n", Pointer_String_Argument_1);
			break;

		case MAIN_COMMAND_SEND_FILE:
			printf("Sending the file \"%s\" to the phone...\n", Pointer_String_Argument_1);
			if (FileManagerSendFile(Serial_Port_ID, Pointer_String_Argument_1, Pointer_String_Argument_2) != 0)
			{
				printf("Error : could not send the file \"%s\".\n", Pointer_String_Argument_1);
				goto Exit;
			}
			printf("The file \"%s\" was successfully sent to the phone.\n", Pointer_String_Argument_1);
			break;

		case MAIN_COMMAND_GET_DIRECTORY:
			if (FileManagerDownloadDirectory(Serial_Port_ID, Pointer_String_Argument_1, Pointer_String_Argument_2) != 0)
			{
				printf("Error : could not get the directory \"%s\".\n", Pointer_String_Argument_1);
				goto Exit;
			}
			printf("The directory \"%s\" content was successfully retrieved from the phone.\n", Pointer_String_Argument_1);
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
