/** @file List.c
 * See List.h for description.
 * @author Adrien RICCIARDI
 */
#include <assert.h>
#include <List.h>
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

	// Free all list items
	Pointer_Current_Item = Pointer_List->Pointer_Head;
	while (Pointer_Current_Item != NULL)
	{
		Pointer_Next_Item = Pointer_Current_Item->Pointer_Next_Item;
		free(Pointer_Current_Item->Pointer_Data);
		free(Pointer_Current_Item);
		Pointer_Current_Item = Pointer_Next_Item;
	}

	// Reset the list information
	Pointer_List->Pointer_Head = NULL;
	Pointer_List->Items_Count = 0;
}
