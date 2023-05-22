/** @file File_Manager.c
 * See File_Manager.h for description.
 * @author Adrien RICCIARDI
 */
#include <assert.h>
#include <AT_Command.h>
#include <errno.h>
#include <fcntl.h>
#include <File_Manager.h>
#include <Log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <Utility.h>

//-------------------------------------------------------------------------------------------------
// Private constants
//-------------------------------------------------------------------------------------------------
/** Allow to turn on or off debug messages. */
#define FILE_MANAGER_IS_DEBUG_ENABLED 0

//-------------------------------------------------------------------------------------------------
// Public functions
//-------------------------------------------------------------------------------------------------
void FileManagerListAddFile(TList *Pointer_List, char *Pointer_String_File_Name, unsigned File_Size, int Flags)
{
	TFileManagerFileListItem *Pointer_Item;

	// Create the list item
	Pointer_Item = malloc(sizeof(TFileManagerFileListItem));
	assert(Pointer_Item != NULL);

	// Fill the item
	strncpy(Pointer_Item->String_File_Name, Pointer_String_File_Name, sizeof(Pointer_Item->String_File_Name) - 1);
	Pointer_Item->String_File_Name[sizeof(Pointer_Item->String_File_Name) - 1] = 0; // Make sure string is terminated, even if it was too long to fit in the name buffer
	Pointer_Item->File_Size = File_Size;
	Pointer_Item->Flags = Flags;

	ListAddItem(Pointer_List, Pointer_Item);
}

int FileManagerListDrives(TSerialPortID Serial_Port_ID, TList *Pointer_List)
{
	unsigned char Buffer[128];
	char String_Temporary[sizeof(Buffer) * 2], String_Drive_Name[sizeof(Buffer) * 2]; // Twice more characters are needed as bytes are converted to hexadecimal characters
	int Size, Return_Value = -1, Result;

	// Allow access to file manager
	if (ATCommandSendCommand(Serial_Port_ID, "AT+ESUO=3") != 0) return -1;
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1; // Wait for "OK"
	if (strcmp(String_Temporary, "OK") != 0)
	{
		LOG("Error : failed to send the AT command that enables the file manager.\n");
		return -1;
	}

	// Send the command
	if (ATCommandSendCommand(Serial_Port_ID, "AT+EFSL") < 0) goto Exit;

	// Wait for all file names to be received
	ListInitialize(Pointer_List);
	do
	{
		// Wait for a drive information string
		Result = ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary));
		if (Result == -2) LOG("Error : the 'list drives' command returned an unexpected error.\n");
		if (Result < 0) goto Exit;

		// Is this a drive record ?
		if (strncmp(String_Temporary, "+EFSL: ", 7) == 0)
		{
			// Extract drive name
			if (sscanf(String_Temporary, "+EFSL: \"%[0-9A-F]\"", String_Drive_Name) != 1)
			{
				LOG("Error : could not extract the drive name from the command answer \"%s\".\n", String_Temporary);
				goto Exit;
			}

			// Convert the drive name to binary UTF-16, so it can be converted to UTF-8
			Size = ATCommandConvertHexadecimalToBinary(String_Drive_Name, (unsigned char *) String_Temporary, sizeof(String_Temporary));
			if (Size < 0)
			{
				LOG("Error : could not convert the drive name hexadecimal string \"%s\" to binary.\n", String_Drive_Name);
				goto Exit;
			}

			// Convert the drive name to UTF-8
			Size = UtilityConvertString(String_Temporary, String_Drive_Name, UTILITY_CHARACTER_SET_UTF16_BIG_ENDIAN, UTILITY_CHARACTER_SET_UTF8, Size, sizeof(String_Drive_Name));
			if (Size < 0)
			{
				LOG("Error : could not convert a drive name from UTF-16 to UTF-8.\n");
				goto Exit;
			}
			String_Drive_Name[Size] = 0; // Make sure the string is terminated

			// Append the drive to the list
			FileManagerListAddFile(Pointer_List, String_Drive_Name, 0, 0);
		}
	} while (strcmp(String_Temporary, "OK") != 0);

	// Everything went fine
	Return_Value = 0;

Exit:
	// Disable file manager access, this seems mandatory to avoid hanging the whole AT communication (phone needs to be rebooted if this command is not issued, otherwise the AT communication is stuck)
	if (ATCommandSendCommand(Serial_Port_ID, "AT+ESUO=4") != 0) return -1;
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1; // Wait for "OK"
	if (strcmp(String_Temporary, "OK") != 0) LOG("Error : failed to send the AT command that disables the file manager.\n");
	return Return_Value;
}

int FileManagerListDirectory(TSerialPortID Serial_Port_ID, char *Pointer_String_Absolute_Path, TList *Pointer_List)
{
	unsigned char Buffer[512];
	char String_Temporary[sizeof(Buffer) * 2], String_File_Name[sizeof(Buffer) * 2]; // Twice more characters are needed as bytes are converted to hexadecimal characters
	int Size, Return_Value = -1, Result, Flags;
	unsigned int File_Size;

	// Allow access to file manager
	if (ATCommandSendCommand(Serial_Port_ID, "AT+ESUO=3") != 0) return -1;
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1; // Wait for "OK"
	if (strcmp(String_Temporary, "OK") != 0)
	{
		LOG("Error : failed to send the AT command that enables the file manager.\n");
		return -1;
	}

	// Convert the provided path to the character encoding the phone is expecting
	Size = UtilityConvertString(Pointer_String_Absolute_Path, Buffer, UTILITY_CHARACTER_SET_UTF8, UTILITY_CHARACTER_SET_UTF16_BIG_ENDIAN, 0, sizeof(Buffer));
	if (Size == -1)
	{
		LOG("Error : could not convert the path \"%s\" to UTF-16.\n", Pointer_String_Absolute_Path);
		goto Exit;
	}

	// Send the command
	strcpy(String_Temporary, "AT+EFSL=\"");
	ATCommandConvertBinaryToHexadecimal(Buffer, Size, &String_Temporary[9]); // Concatenate the converted path right after the command
	strcat(String_Temporary, "\"");
	if (ATCommandSendCommand(Serial_Port_ID, String_Temporary) < 0) goto Exit;

	// Wait for all file names to be received
	ListInitialize(Pointer_List);
	do
	{
		// Wait for a file information string
		Result = ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary));
		if (Result == -2) LOG("Error : the specified path \"%s\" does not exist.\n", Pointer_String_Absolute_Path);
		if (Result < 0) goto Exit;

		// Is this a file record ?
		if (strncmp(String_Temporary, "+EFSL: ", 7) == 0)
		{
			// Extract useful fields
			if (sscanf(String_Temporary, "+EFSL: \"%[0-9A-F]\", %u, %d", String_File_Name, &File_Size, &Flags) != 3)
			{
				LOG("Error : could not extract file information fields from the command answer \"%s\".\n", String_Temporary);
				goto Exit;
			}

			// Convert the file name to binary UTF-16, so it can be converted to UTF-8
			Size = ATCommandConvertHexadecimalToBinary(String_File_Name, (unsigned char *) String_Temporary, sizeof(String_Temporary));
			if (Size < 0)
			{
				LOG("Error : could not convert the file name hexadecimal string \"%s\" to binary.\n", String_File_Name);
				goto Exit;
			}

			// Convert the file name to UTF-8
			Size = UtilityConvertString(String_Temporary, String_File_Name, UTILITY_CHARACTER_SET_UTF16_BIG_ENDIAN, UTILITY_CHARACTER_SET_UTF8, Size, sizeof(String_File_Name));
			if (Size < 0)
			{
				LOG("Error : could not convert a file name from UTF-16 to UTF-8.\n");
				goto Exit;
			}
			String_File_Name[Size] = 0; // Make sure the string is terminated

			// Append the file to the list
			FileManagerListAddFile(Pointer_List, String_File_Name, File_Size, Flags);
		}
	} while (strcmp(String_Temporary, "OK") != 0);

	// Everything went fine
	Return_Value = 0;

Exit:
	// Disable file manager access, this seems mandatory to avoid hanging the whole AT communication (phone needs to be rebooted if this command is not issued, otherwise the AT communication is stuck)
	if (ATCommandSendCommand(Serial_Port_ID, "AT+ESUO=4") != 0) return -1;
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1; // Wait for "OK"
	if (strcmp(String_Temporary, "OK") != 0) LOG("Error : failed to send the AT command that disables the file manager.\n");
	return Return_Value;
}

void FileManagerDisplayDirectoryListing(TList *Pointer_List)
{
	TListItem *Pointer_Item;
	TFileManagerFileListItem *Pointer_File_List_Item;

	Pointer_Item = Pointer_List->Pointer_Head;
	while (Pointer_Item != NULL)
	{
		Pointer_File_List_Item = Pointer_Item->Pointer_Data;

		// Display file attributes
		// Archive flag
		if (FILE_MANAGER_ATTRIBUTE_IS_ARCHIVE(Pointer_File_List_Item)) putchar('A');
		else  putchar('-');
		// Directory flag
		if (FILE_MANAGER_ATTRIBUTE_IS_DIRECTORY(Pointer_File_List_Item)) putchar('D');
		else putchar('-');
		// System flag
		if (FILE_MANAGER_ATTRIBUTE_IS_SYSTEM(Pointer_File_List_Item)) putchar('S');
		else putchar('-');
		// Hidden flag
		if (FILE_MANAGER_ATTRIBUTE_IS_HIDDEN(Pointer_File_List_Item)) putchar('H');
		else putchar('-');
		// Read only flag
		if (FILE_MANAGER_ATTRIBUTE_IS_READ_ONLY(Pointer_File_List_Item)) putchar('R');
		else putchar('-');

		// Display file size if this is a regular file
		if (FILE_MANAGER_ATTRIBUTE_IS_DIRECTORY(Pointer_File_List_Item)) printf("             ");
		else printf("  %11u", Pointer_File_List_Item->File_Size);

		// Display the file name
		printf("  %s", Pointer_File_List_Item->String_File_Name);
		// Add a trailing backslash for a directory name to make this more visual, but do not do that on "." and ".."
		if (FILE_MANAGER_ATTRIBUTE_IS_DIRECTORY(Pointer_File_List_Item) && (strcmp(Pointer_File_List_Item->String_File_Name, ".") != 0) && (strcmp(Pointer_File_List_Item->String_File_Name, "..") != 0)) putchar('\\');

		// Handle next file
		putchar('\n');
		Pointer_Item = Pointer_Item->Pointer_Next_Item;
	}
}

int FileManagerDownloadFile(TSerialPortID Serial_Port_ID, char *Pointer_String_Absolute_Phone_Path, char *Pointer_String_Destination_PC_Path)
{
	unsigned char Buffer[512];
	char String_Temporary[512], String_Payload[512];
	int File_Descriptor = -1, Return_Value = -1, Size, Result, Read_Index;

	// Try to create the output file first to make sure it can be accessed
	File_Descriptor = open(Pointer_String_Destination_PC_Path, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (File_Descriptor == -1)
	{
		LOG("Error : could not create the output file \"%s\" (%s).\n", Pointer_String_Destination_PC_Path, strerror(errno));
		goto Exit;
	}

	// Convert the provided path to the character encoding the phone is expecting
	Size = UtilityConvertString(Pointer_String_Absolute_Phone_Path, Buffer, UTILITY_CHARACTER_SET_UTF8, UTILITY_CHARACTER_SET_UTF16_BIG_ENDIAN, 0, sizeof(Buffer));
	if (Size == -1)
	{
		LOG("Error : could not convert the path \"%s\" to UTF-16.\n", Pointer_String_Absolute_Phone_Path);
		goto Exit;
	}

	// Allow access to file manager
	if (ATCommandSendCommand(Serial_Port_ID, "AT+ESUO=3") != 0) goto Exit;
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) goto Exit; // Wait for "OK"
	if (strcmp(String_Temporary, "OK") != 0)
	{
		LOG("Error : failed to send the AT command that enables the file manager.\n");
		goto Exit;
	}

	// Send the command
	strcpy(String_Temporary, "AT+EFSR=\"");
	ATCommandConvertBinaryToHexadecimal(Buffer, Size, &String_Temporary[9]); // Concatenate the converted path right after the command
	strcat(String_Temporary, "\"");
	if (ATCommandSendCommand(Serial_Port_ID, String_Temporary) < 0) goto Exit;

	// Receive all file chunks
	do
	{
		// Wait for a file chunk string
		Result = ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary));
		if (Result == -2) LOG("Error : the specified path \"%s\" does not exist.\n", Pointer_String_Absolute_Phone_Path);
		if (Result < 0) goto Exit;

		// Is this a chunk ?
		if (strncmp(String_Temporary, "+EFSR: ", 7) == 0)
		{
			// Extract chunk information
			if (sscanf(String_Temporary, "+EFSR: %*d, %*d, %d, %n", &Size, &Read_Index) != 1) // The scanf() 'n' modifier does not increase the count returned by the function
			{
				LOG("Error : could not extract file chunk information.\n");
				goto Exit;
			}

			// Make sure the chunk size won't exceed the destination buffer
			Size *= 2; // The chunk size represents the final size in bytes, however each byte is encoded by two hexadecimal characters, so take this into account
			if (Size > (int) sizeof(String_Payload))
			{
				LOG("Error : the chunk payload size is too big.\n");
				goto Exit;
			}
			LOG_DEBUG(FILE_MANAGER_IS_DEBUG_ENABLED, "Chunk payload size : %d.\n", Size);
			if (Size <= 0) continue;

			// Retrieve the payload
			if (sscanf(&String_Temporary[Read_Index], "\"%[0-9A-F]\"", String_Payload) != 1)
			{
				LOG("Error : failed to extract the payload from the file chunk.\n");
				goto Exit;
			}

			// Convert the payload to binary
			Size = ATCommandConvertHexadecimalToBinary(String_Payload, Buffer, sizeof(Buffer));
			if (Size < 0)
			{
				LOG("Error : could not convert file chunk payload from hexadecimal to binary.\n");
				goto Exit;
			}

			// Append the data to the file
			if (write(File_Descriptor, Buffer, Size) != Size)
			{
				LOG("Error : could not write the file chunk payload to the output file (%s).\n", strerror(errno));
				goto Exit;
			}
		}
	} while (strcmp(String_Temporary, "OK") != 0);

	// Everything went fine
	Return_Value = 0;

Exit:
	if (File_Descriptor != -1) close(File_Descriptor);

	// Disable file manager access, this seems mandatory to avoid hanging the whole AT communication (phone needs to be rebooted if this command is not issued, otherwise the AT communication is stuck)
	if (ATCommandSendCommand(Serial_Port_ID, "AT+ESUO=4") != 0) return -1;
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1; // Wait for "OK"
	if (strcmp(String_Temporary, "OK") != 0) LOG("Error : failed to send the AT command that disables the file manager.\n");
	return Return_Value;
}

int FileManagerDownloadDirectory(TSerialPortID Serial_Port_ID, char *Pointer_String_Absolute_Phone_Path, char *Pointer_String_Destination_PC_Path)
{
	TList List_Files;
	TListItem *Pointer_Item;
	TFileManagerFileListItem *Pointer_File_List_Item;
	int Return_Value = -1;
	char String_Source_File_Name[512], String_Output_File_Name[512];

	// Find all directories and files located in this directory
	LOG_DEBUG(FILE_MANAGER_IS_DEBUG_ENABLED, "Listing directory \"%s\" :\n", Pointer_String_Absolute_Phone_Path);
	if (FileManagerListDirectory(Serial_Port_ID, Pointer_String_Absolute_Phone_Path, &List_Files) != 0)
	{
		LOG("Error : could not list the directory \"%s\".\n", Pointer_String_Absolute_Phone_Path);
		return -1;
	}
	#if FILE_MANAGER_IS_DEBUG_ENABLED
		FileManagerDisplayDirectoryListing(&List_Files);
	#endif

	// Create the output directory
	if (UtilityCreateDirectory(Pointer_String_Destination_PC_Path) != 0)
	{
		LOG("Error : could not create the output directory \"%s\".\n", Pointer_String_Destination_PC_Path);
		goto Exit_Free_List;
	}

	// Process each file
	Pointer_Item = List_Files.Pointer_Head;
	while (Pointer_Item != NULL)
	{
		Pointer_File_List_Item = Pointer_Item->Pointer_Data;

		// Display the processed file for debugging purpose
		LOG_DEBUG(FILE_MANAGER_IS_DEBUG_ENABLED, "Processing the %s \"%s\".\n", FILE_MANAGER_ATTRIBUTE_IS_DIRECTORY(Pointer_File_List_Item) ? "directory" : "file", Pointer_File_List_Item->String_File_Name);

		// Bypass the special directories "." and ".."
		if (strcmp(Pointer_File_List_Item->String_File_Name, ".") == 0) goto Next_File;
		if (strcmp(Pointer_File_List_Item->String_File_Name, "..") == 0) goto Next_File;

		// Create the source file path
		snprintf(String_Source_File_Name, sizeof(String_Source_File_Name), "%s\\%s", Pointer_String_Absolute_Phone_Path, Pointer_File_List_Item->String_File_Name);
		LOG_DEBUG(FILE_MANAGER_IS_DEBUG_ENABLED, "Source file path : \"%s\".\n", String_Source_File_Name);

		// Create the output file path
		snprintf(String_Output_File_Name, sizeof(String_Output_File_Name), "%s/%s", Pointer_String_Destination_PC_Path, Pointer_File_List_Item->String_File_Name);
		LOG_DEBUG(FILE_MANAGER_IS_DEBUG_ENABLED, "Output file path : \"%s\".\n", String_Output_File_Name);

		// Download the file if this is the case
		if (!FILE_MANAGER_ATTRIBUTE_IS_DIRECTORY(Pointer_File_List_Item))
		{
			// Try to download the file
			printf("Downloading the file \"%s\"...\n", String_Source_File_Name);
			if (FileManagerDownloadFile(Serial_Port_ID, String_Source_File_Name, String_Output_File_Name) != 0)
			{
				LOG("Error : failed to download the file \"%s\".\n", String_Source_File_Name);
				goto Exit_Free_List;
			}
		}
		// Recurse into the directory if this is the case
		else
		{
			printf("Scanning the directory \"%s\"...\n", String_Source_File_Name);
			if (FileManagerDownloadDirectory(Serial_Port_ID, String_Source_File_Name, String_Output_File_Name) != 0)
			{
				LOG("Error : failed to scan the directory \"%s\".\n", String_Source_File_Name);
				goto Exit_Free_List;
			}
		}

Next_File:
		Pointer_Item = Pointer_Item->Pointer_Next_Item;
	}

	// Everything went fine
	Return_Value = 0;

Exit_Free_List:
	ListClear(&List_Files);

	return Return_Value;
}

int FileManagerSendFile(TSerialPortID Serial_Port_ID, char *Pointer_String_Source_PC_Path, char *Pointer_String_Absolute_Phone_Path)
{
	int File_Descriptor = -1, Return_Value = -1, Size, Is_End_Of_File_Reached;
	unsigned int Chunk_Size_Bytes;
	char String_Temporary[512], String_Chunk_Command[1024];
	unsigned char Buffer[512];
	ssize_t Bytes_Count;

	// Try to open the file to send to make sure it is existing
	File_Descriptor = open(Pointer_String_Source_PC_Path, O_RDONLY);
	if (File_Descriptor == -1)
	{
		LOG("Error : could not open the source file \"%s\". (%s)\n", Pointer_String_Source_PC_Path, strerror(errno));
		return -1;
	}

	// Allow access to file manager
	if (ATCommandSendCommand(Serial_Port_ID, "AT+ESUO=3") != 0) goto Exit;
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) goto Exit; // Wait for "OK"
	if (strcmp(String_Temporary, "OK") != 0)
	{
		LOG("Error : failed to send the AT command that enables the file manager.\n");
		goto Exit;
	}

	// Retrieve the maximum transfer chunk size
	if (ATCommandSendCommand(Serial_Port_ID, "AT+EFSW?") != 0) goto Exit;
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) goto Exit; // Wait for chunk size value
	if (sscanf(String_Temporary, "+EFSW: %d", &Chunk_Size_Bytes) != 1)
	{
		LOG("Error : could not convert the transfer chunk size to a number.\n");
		goto Exit;
	}
	Chunk_Size_Bytes /= 2; // The command returns the raw data size, where each byte is encoded by two hexadecimal characters, so divide by two to get the real payload size in bytes
	LOG_DEBUG(FILE_MANAGER_IS_DEBUG_ENABLED, "Transfer chunk size in bytes : %d.\n", Chunk_Size_Bytes);
	// Make sure the chunk transfer size won't overflow the internal buffer
	if (Chunk_Size_Bytes > sizeof(Buffer))
	{
		Chunk_Size_Bytes = sizeof(Buffer);
		LOG_DEBUG(FILE_MANAGER_IS_DEBUG_ENABLED, "Transfer chunk size is greater than the internal buffer, limiting it to %zu bytes.\n", sizeof(Buffer));
	}

	// Convert the provided path to the character encoding the phone is expecting
	Size = UtilityConvertString(Pointer_String_Absolute_Phone_Path, Buffer, UTILITY_CHARACTER_SET_UTF8, UTILITY_CHARACTER_SET_UTF16_BIG_ENDIAN, 0, sizeof(Buffer));
	if (Size == -1)
	{
		LOG("Error : could not convert the path \"%s\" to UTF-16.\n", Pointer_String_Absolute_Phone_Path);
		goto Exit;
	}

	// Try to create and open the target file on the phone
	strcpy(String_Temporary, "AT+EFSW=0,\"");
	ATCommandConvertBinaryToHexadecimal(Buffer, Size, &String_Temporary[11]); // Concatenate the converted path right after the command
	strcat(String_Temporary, "\"");
	if (ATCommandSendCommand(Serial_Port_ID, String_Temporary) < 0) goto Exit;
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) goto Exit; // Wait for "OK"
	if (strcmp(String_Temporary, "OK") != 0)
	{
		LOG("Error : failed to send the AT command that create and open the file.\n");
		goto Exit;
	}

	// Send the file content
	do
	{
		// Read a chunk from the source file
		Bytes_Count = read(File_Descriptor, Buffer, Chunk_Size_Bytes);
		if (Bytes_Count == -1)
		{
			LOG("Error : failed to read a chunk of data from the source file (%s).\n", strerror(errno));
			goto Exit;
		}

		// Send a chunk of data
		if (Bytes_Count < Chunk_Size_Bytes) Is_End_Of_File_Reached = 1;
		else Is_End_Of_File_Reached = 0;
		ATCommandConvertBinaryToHexadecimal(Buffer, Bytes_Count, String_Temporary); // The file payload is expected to be sent in hexadecimal
		snprintf(String_Chunk_Command, sizeof(String_Chunk_Command), "AT+EFSW=2,%d,%zu,\"%s\"", Is_End_Of_File_Reached, Bytes_Count, String_Temporary);
		if (ATCommandSendCommand(Serial_Port_ID, String_Chunk_Command) < 0) goto Exit;
		if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) goto Exit; // Wait for "OK"
		if (strcmp(String_Temporary, "OK") != 0)
		{
			LOG("Error : failed to send the AT command that sends a chunk of the file.\n");
			goto Exit;
		}
	} while (Bytes_Count == Chunk_Size_Bytes);

	// Close the file
	if (ATCommandSendCommand(Serial_Port_ID, "AT+EFSW=1") != 0) goto Exit;
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) goto Exit; // Wait for chunk size value
	if (strcmp(String_Temporary, "OK") != 0)
	{
		LOG("Error : failed to send the AT command that closes the file.\n");
		goto Exit;
	}

	// Everything went fine
	Return_Value = 0;

Exit:
	if (File_Descriptor != -1) close(File_Descriptor);

	// Disable file manager access, this seems mandatory to avoid hanging the whole AT communication (phone needs to be rebooted if this command is not issued, otherwise the AT communication is stuck)
	if (ATCommandSendCommand(Serial_Port_ID, "AT+ESUO=4") != 0) return -1;
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1; // Wait for "OK"
	if (strcmp(String_Temporary, "OK") != 0) LOG("Error : failed to send the AT command that disables the file manager.\n");
	return Return_Value;
}
