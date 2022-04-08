/** @file File_Manager.c
 * See File_Manager.h for description.
 * @author Adrien RICCIARDI
 */
#include <assert.h>
#include <AT_Command.h>
#include <errno.h>
#include <fcntl.h>
#include <File_Manager.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <Utility.h>

//-------------------------------------------------------------------------------------------------
// Public functions
//-------------------------------------------------------------------------------------------------
void FileManagerListInitialize(TFileManagerList *Pointer_List)
{
	Pointer_List->Pointer_Head = NULL;
	Pointer_List->Files_Count = 0;
}

void FileManagerListAddFile(TFileManagerList *Pointer_List, char *Pointer_String_File_Name, unsigned File_Size, int Flags)
{
	TFileManagerFileListItem *Pointer_Item_To_Add, *Pointer_Temporary_Item;

	// Create the list item
	Pointer_Item_To_Add = malloc(sizeof(TFileManagerFileListItem));
	assert(Pointer_Item_To_Add != NULL);

	// Fill the item
	strncpy(Pointer_Item_To_Add->String_File_Name, Pointer_String_File_Name, sizeof(Pointer_Item_To_Add->String_File_Name) - 1);
	Pointer_Item_To_Add->String_File_Name[sizeof(Pointer_Item_To_Add->String_File_Name) - 1] = 0; // Make sure string is terminated, even if it was too long to fit in the name buffer
	Pointer_Item_To_Add->File_Size = File_Size;
	Pointer_Item_To_Add->Flags = Flags;
	Pointer_Item_To_Add->Pointer_Next_Item = NULL; // Tell the list browsing algorithms that this item is the last of the list

	// Initialize the list head if the list is empty
	if (Pointer_List->Pointer_Head == NULL) Pointer_List->Pointer_Head = Pointer_Item_To_Add;
	// Otherwise append the item at the end of the list
	else
	{
		// Find the list tail
		Pointer_Temporary_Item = Pointer_List->Pointer_Head;
		while (Pointer_Temporary_Item->Pointer_Next_Item != NULL) Pointer_Temporary_Item = Pointer_Temporary_Item->Pointer_Next_Item;

		// Add the file at the end of the list
		Pointer_Temporary_Item->Pointer_Next_Item = Pointer_Item_To_Add;
	}
	Pointer_List->Files_Count++;
}

void FileManagerListClear(TFileManagerList *Pointer_List)
{
	TFileManagerFileListItem *Pointer_Current_Item, *Pointer_Next_Item;

	// Free all list items
	Pointer_Current_Item = Pointer_List->Pointer_Head;
	while (Pointer_Current_Item != NULL)
	{
		Pointer_Next_Item = Pointer_Current_Item->Pointer_Next_Item;
		free(Pointer_Current_Item);
		Pointer_Current_Item = Pointer_Next_Item;
	}

	// Reset the list information
	Pointer_List->Pointer_Head = NULL;
	Pointer_List->Files_Count = 0;
}

void FileManagerListDisplay(TFileManagerList *Pointer_List)
{
	TFileManagerFileListItem *Pointer_Item;

	// Display list information
	printf("List address = 0x%p, files count = %d.\n", Pointer_List, Pointer_List->Files_Count);

	// Display content
	Pointer_Item = Pointer_List->Pointer_Head;
	while (Pointer_Item != NULL)
	{
		printf("Name = \"%s\", size = %u, flags = 0x%02X.\n", Pointer_Item->String_File_Name, Pointer_Item->File_Size, Pointer_Item->Flags);
		Pointer_Item = Pointer_Item->Pointer_Next_Item;
	}
}

int FileManagerListDirectory(TSerialPortID Serial_Port_ID, char *Pointer_String_Absolute_Path, TFileManagerList *Pointer_List)
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
		printf("Error : failed to send the AT command that enables the file manager.\n");
		return -1;
	}

	// Convert the provided path to the character encoding the phone is expecting
	Size = UtilityConvertString(Pointer_String_Absolute_Path, Buffer, UTILITY_CHARACTER_SET_UTF8, UTILITY_CHARACTER_SET_UTF16_BIG_ENDIAN, 0, sizeof(Buffer));
	if (Size == -1)
	{
		printf("Error : could not convert the path \"%s\" to UTF-16.\n", Pointer_String_Absolute_Path);
		goto Exit;
	}

	// Send the command
	strcpy(String_Temporary, "AT+EFSL=\"");
	ATCommandConvertBinaryToHexadecimal(Buffer, Size, &String_Temporary[9]); // Concatenate the converted path right after the command
	strcat(String_Temporary, "\"");
	if (ATCommandSendCommand(Serial_Port_ID, String_Temporary) < 0) goto Exit;

	// Wait for all file names to be received
	FileManagerListInitialize(Pointer_List);
	do
	{
		// Wait for a file information string
		Result = ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary));
		if (Result == -2) printf("Error : the specified path \"%s\" does not exist.\n", Pointer_String_Absolute_Path);
		if (Result < 0) goto Exit;

		// Is this a file record ?
		if (strncmp(String_Temporary, "+EFSL: ", 7) == 0)
		{
			// Extract useful fields
			if (sscanf(String_Temporary, "+EFSL: \"%[0-9A-F]\", %u, %d", String_File_Name, &File_Size, &Flags) != 3)
			{
				printf("Error : could not extract file information fields from the command answer \"%s\".\n", String_Temporary);
				goto Exit;
			}

			// Convert the file name to binary UTF-16, so it can be converted to UTF-8
			Size = ATCommandConvertHexadecimalToBinary(String_File_Name, (unsigned char *) String_Temporary, sizeof(String_Temporary));
			if (Size < 0)
			{
				printf("Error : could not convert the file name hexadecimal string \"%s\" to binary.\n", String_File_Name);
				goto Exit;
			}

			// Convert the file name to UTF-8
			Size = UtilityConvertString(String_Temporary, String_File_Name, UTILITY_CHARACTER_SET_UTF16_BIG_ENDIAN, UTILITY_CHARACTER_SET_UTF8, Size, sizeof(String_File_Name));
			if (Size < 0)
			{
				printf("Error : could not convert a file name from UTF-16 to UTF-8.\n");
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
	if (strcmp(String_Temporary, "OK") != 0) printf("Error : failed to send the AT command that disables the file manager.\n");
	return Return_Value;
}

void FileManagerDisplayDirectoryListing(TFileManagerList *Pointer_List)
{
	TFileManagerFileListItem *Pointer_Item;

	Pointer_Item = Pointer_List->Pointer_Head;
	while (Pointer_Item != NULL)
	{
		// Display file attributes
		// Archive flag
		if (FILE_MANAGER_ATTRIBUTE_IS_ARCHIVE(Pointer_Item)) putchar('A');
		else  putchar('-');
		// Directory flag
		if (FILE_MANAGER_ATTRIBUTE_IS_DIRECTORY(Pointer_Item)) putchar('D');
		else putchar('-');
		// System flag
		if (FILE_MANAGER_ATTRIBUTE_IS_SYSTEM(Pointer_Item)) putchar('S');
		else putchar('-');
		// Hidden flag
		if (FILE_MANAGER_ATTRIBUTE_IS_HIDDEN(Pointer_Item)) putchar('H');
		else putchar('-');
		// Read only flag
		if (FILE_MANAGER_ATTRIBUTE_IS_READ_ONLY(Pointer_Item)) putchar('R');
		else putchar('-');

		// Display file size if this is a regular file
		if (FILE_MANAGER_ATTRIBUTE_IS_DIRECTORY(Pointer_Item)) printf("             ");
		else printf("  %11u", Pointer_Item->File_Size);

		// Display the file name
		printf("  %s", Pointer_Item->String_File_Name);
		// Add a trailing backslash for a directory name to make this more visual, but do not do that on "." and ".."
		if (FILE_MANAGER_ATTRIBUTE_IS_DIRECTORY(Pointer_Item) && (strcmp(Pointer_Item->String_File_Name, ".") != 0) && (strcmp(Pointer_Item->String_File_Name, "..") != 0)) putchar('\\');

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
	File_Descriptor = open(Pointer_String_Destination_PC_Path, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (File_Descriptor == -1)
	{
		printf("Error : could not create the output file \"%s\" (%s).\n", Pointer_String_Destination_PC_Path, strerror(errno));
		return -1;
	}

	// Convert the provided path to the character encoding the phone is expecting
	Size = UtilityConvertString(Pointer_String_Absolute_Phone_Path, Buffer, UTILITY_CHARACTER_SET_UTF8, UTILITY_CHARACTER_SET_UTF16_BIG_ENDIAN, 0, sizeof(Buffer));
	if (Size == -1)
	{
		printf("Error : could not convert the path \"%s\" to UTF-16.\n", Pointer_String_Absolute_Phone_Path);
		goto Exit;
	}

	// Allow access to file manager
	if (ATCommandSendCommand(Serial_Port_ID, "AT+ESUO=3") != 0) return -1;
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1; // Wait for "OK"
	if (strcmp(String_Temporary, "OK") != 0)
	{
		printf("Error : failed to send the AT command that enables the file manager.\n");
		return -1;
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
		if (Result == -2) printf("Error : the specified path \"%s\" does not exist.\n", Pointer_String_Absolute_Phone_Path);
		if (Result < 0) goto Exit;

		// Is this a chunk ?
		if (strncmp(String_Temporary, "+EFSR: ", 7) == 0)
		{
			// Extract chunk information
			if (sscanf(String_Temporary, "+EFSR: %*d, %*d, %d, %n", &Size, &Read_Index) != 1) // The scanf() 'n' modifier does not increase the count returned by the function
			{
				printf("Error : could not extract file chunk information.\n");
				goto Exit;
			}

			// Make sure the chunk size won't exceed the destination buffer
			Size *= 2; // The chunk size represents the final size in bytes, however each byte is encoded by two hexadecimal characters, so take this into account
			if (Size > (int) sizeof(String_Payload))
			{
				printf("Error : the chunk payload size is too big.\n");
				goto Exit;
			}

			// Retrieve the payload
			if (sscanf(&String_Temporary[Read_Index], "\"%[0-9A-F]\"", String_Payload) != 1)
			{
				printf("Error : failed to extract the payload from the file chunk.\n");
				goto Exit;
			}

			// Convert the payload to binary
			Size = ATCommandConvertHexadecimalToBinary(String_Payload, Buffer, sizeof(Buffer));
			if (Size < 0)
			{
				printf("Error : could not convert file chunk payload from hexadecimal to binary.\n");
				goto Exit;
			}

			// Append the data to the file
			if (write(File_Descriptor, Buffer, Size) != Size)
			{
				printf("Error : could not write the file chunk payload to the output file (%s).\n", strerror(errno));
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
	if (strcmp(String_Temporary, "OK") != 0) printf("Error : failed to send the AT command that disables the file manager.\n");
	return Return_Value;
}
