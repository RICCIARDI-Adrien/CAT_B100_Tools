/** @file List.c
 * See List.h for description.
 * @author Adrien RICCIARDI
 */
#include <assert.h>
#include <List.h>
#include <Log.h>
#include <stddef.h>
#include <stdlib.h>

//-------------------------------------------------------------------------------------------------
// Public functions
//-------------------------------------------------------------------------------------------------
void ListInitialize(TList *Pointer_List)
{
	Pointer_List->Pointer_Head = NULL;
	Pointer_List->Items_Count = 0;
}

void ListAddItem(TList *Pointer_List, void *Pointer_Item_Data)
{
	TListItem *Pointer_Item_To_Add, *Pointer_Temporary_Item;

	// Create the list item
	Pointer_Item_To_Add = malloc(sizeof(TListItem));
	assert(Pointer_Item_To_Add != NULL);

	// Fill the item
	Pointer_Item_To_Add->Pointer_Data = Pointer_Item_Data;
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
	Pointer_List->Items_Count++;
}

void ListClear(TList *Pointer_List)
{
	TListItem *Pointer_Current_Item, *Pointer_Next_Item;
	int Cleared_Items_Count = 0;

	// Free all list items
	Pointer_Current_Item = Pointer_List->Pointer_Head;
	while (Pointer_Current_Item != NULL)
	{
		Pointer_Next_Item = Pointer_Current_Item->Pointer_Next_Item;
		free(Pointer_Current_Item->Pointer_Data);
		free(Pointer_Current_Item);
		Pointer_Current_Item = Pointer_Next_Item;
		Cleared_Items_Count++;
	}

	// Make sure the list was not corrupted
	assert(Cleared_Items_Count == Pointer_List->Items_Count);

	// Reset the list information
	Pointer_List->Pointer_Head = NULL;
	Pointer_List->Items_Count = 0;
}

void ListDisplay(TList *Pointer_List, char *Pointer_String_Name, void (*ListDisplayItemData)(void *Pointer_Item_Data))
{
	TListItem *Pointer_Item;

	// Make sure the callback function is present
	if (ListDisplayItemData == NULL)
	{
		LOG("Error : you must provide a callback function to display an item content.\n");
		return;
	}

	// Display list information
	LOG("List name : %s, address : %p, items count : %d.\n", Pointer_String_Name, Pointer_List, Pointer_List->Items_Count);

	// Display each item content
	Pointer_Item = Pointer_List->Pointer_Head;
	while (Pointer_Item != NULL)
	{
		ListDisplayItemData(Pointer_Item->Pointer_Data);
		Pointer_Item = Pointer_Item->Pointer_Next_Item;
	}
}
