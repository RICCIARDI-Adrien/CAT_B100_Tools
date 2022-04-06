/** @file File_Manager.c
 * See File_Manager.h for description.
 * @author Adrien RICCIARDI
 */
#include <assert.h>
#include <AT_Command.h>
#include <File_Manager.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
		// Wait for the information string
		Result = ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary));
		if (Result == -2) printf("Error : the specified path \"%s\" does not exist.\n", Pointer_String_Absolute_Path);
		if (Result < 0) goto Exit;

		// Discard empty strings (+EFSL answers are separated by empty strings)
		if (String_Temporary[0] == 0) continue;

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
