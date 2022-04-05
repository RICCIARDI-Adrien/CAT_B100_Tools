/** @file File_Manager.c
 * See File_Manager.h for description.
 * @author Adrien RICCIARDI
 */
#include <assert.h>
#include <File_Manager.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//-------------------------------------------------------------------------------------------------
// Public functions
//-------------------------------------------------------------------------------------------------
void FileManagerListInitialize(TFileManagerList *Pointer_List)
{
	Pointer_List->Pointer_Head = NULL;
	Pointer_List->Files_Count = 0;
}

void FileManagerListAddFile(TFileManagerList *Pointer_List, char *Pointer_String_File_Name, int Is_Directory)
{
	TFileManagerFileListItem *Pointer_Item_To_Add, *Pointer_Temporary_Item;

	// Create the list item
	Pointer_Item_To_Add = malloc(sizeof(TFileManagerFileListItem));
	assert(Pointer_Item_To_Add != NULL);
	// Fill the item
	strncpy(Pointer_Item_To_Add->String_File_Name, Pointer_String_File_Name, sizeof(Pointer_Item_To_Add->String_File_Name) - 1);
	Pointer_Item_To_Add->String_File_Name[sizeof(Pointer_Item_To_Add->String_File_Name) - 1] = 0; // Make sure string is terminated, even if it was too long to fit in the name buffer
	Pointer_Item_To_Add->Is_Directory = Is_Directory;
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
		printf("Name = \"%s\", is directory = %d.\n", Pointer_Item->String_File_Name, Pointer_Item->Is_Directory);
		Pointer_Item = Pointer_Item->Pointer_Next_Item;
	}
}
