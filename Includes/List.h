/** @file List.h
 * A generic list implementation that can be wrapped by other modules.
 * @author Adrien RICCIARDI
 */
#ifndef H_LIST_H
#define H_LIST_H

//-------------------------------------------------------------------------------------------------
// Types
//-------------------------------------------------------------------------------------------------
typedef struct TListItem
{
	void *Pointer_Data;
	struct TListItem *Pointer_Next_Item;
} TListItem;

/** A list that can contain any data. */
typedef struct
{
	TListItem *Pointer_Head;
	int Items_Count;
} TList;

//-------------------------------------------------------------------------------------------------
// Functions
//-------------------------------------------------------------------------------------------------
/** Clear all internal fields of the list to make it ready to use by other list functions.
 * @param Pointer_List The list to initialize.
 * @warning This function assumes that the list is not initialized or has been already cleared with ListClear(). Passing an initialized list to this function will result in a memory leak.
 */
void ListInitialize(TList *Pointer_List);

/** Append a new generic item to the list.
 * @param Pointer_List The list to add an item to the tail. This list must have been previously initialized.
 * @param Pointer_Item The item that will be added at the list tail. The item data memory must have been allocated with malloc().
 * @warning This function assumes that the list has already been initialized.
 */
void ListAddItem(TList *Pointer_List, void *Pointer_Item_Data);

/** Free all resources used by a list.
 * @param Pointer_List The list to release resources from.
 * @warning This function assumes that the list has already been initialized. Passing an uninitialized list can lead to a crash.
 */
void ListClear(TList *Pointer_List);

/** Display each list single item.
 * @param Pointer_List The list to display content.
 * @param Pointer_String_Name The list name to be displayed before all items content, this is useful when multiple different lists are displayed to identify the correct one.
 * @param ListDisplayItemData A callback function that will be called for each list item. This function is responsible for displaying the specific item content.
 * @note This function is intended for debugging purpose.
 */
void ListDisplay(TList *Pointer_List, char *Pointer_String_Name, void (*ListDisplayItemData)(void *Pointer_Item_Data));

#endif
