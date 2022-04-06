/** @file File_Manager.h
 * List and download phone files.
 * @author Adrien RICCIARDI
 */
#ifndef H_FILE_MANAGER_H
#define H_FILE_MANAGER_H

#include <Serial_Port.h>

//-------------------------------------------------------------------------------------------------
// Constants and macros
//-------------------------------------------------------------------------------------------------
/** Tell whether a file item has the "archive" flag set. */
#define FILE_MANAGER_ATTRIBUTE_IS_ARCHIVE(Pointer_File_List_Item) (Pointer_File_List_Item->Flags & 0x20)
/** Tell whether a file item is a directory. */
#define FILE_MANAGER_ATTRIBUTE_IS_DIRECTORY(Pointer_File_List_Item) (Pointer_File_List_Item->Flags & 0x10)
/** Tell whether a file item has the "system" flag set. */
#define FILE_MANAGER_ATTRIBUTE_IS_SYSTEM(Pointer_File_List_Item) (Pointer_File_List_Item->Flags & 0x04)
/** Tell whether a file item has the "hidden" flag set. */
#define FILE_MANAGER_ATTRIBUTE_IS_HIDDEN(Pointer_File_List_Item) (Pointer_File_List_Item->Flags & 0x02)
/** Tell whether a file item has the "read only" flag set. */
#define FILE_MANAGER_ATTRIBUTE_IS_READ_ONLY(Pointer_File_List_Item) (Pointer_File_List_Item->Flags & 0x01)

//-------------------------------------------------------------------------------------------------
// Types
//-------------------------------------------------------------------------------------------------
/** Hold a phone file object, which can either be a file or a directory. */
typedef struct TFileManagerFileListItem
{
	char String_File_Name[256];
	unsigned int File_Size; //!< The phone is using system the FAT32 file system, so 32 bits should be enough.
	int Flags; //!< The flags byte looks like a lot the FAT file system "file attribute" field (offset 0x0B in a FAT directory entry).
	struct TFileManagerFileListItem *Pointer_Next_Item;
} TFileManagerFileListItem;

/** A list of files and directories, from a specific path. This list is not recursive, it does not contain another list for the nested directories. */
typedef struct
{
	TFileManagerFileListItem *Pointer_Head;
	int Files_Count;
} TFileManagerList;

//-------------------------------------------------------------------------------------------------
// Functions
//-------------------------------------------------------------------------------------------------
// File lists management functions
/** TODO */
void FileManagerListInitialize(TFileManagerList *Pointer_List);
/** TODO */
void FileManagerListAddFile(TFileManagerList *Pointer_List, char *Pointer_String_File_Name, unsigned File_Size, int Flags);
/** TODO */
void FileManagerListClear(TFileManagerList *Pointer_List);
/** TODO for debug purpose */
void FileManagerListDisplay(TFileManagerList *Pointer_List);

/** TODO */
unsigned int FileManagerListDrives(void); // int for error ?

int FileManagerListDirectory(TSerialPortID Serial_Port_ID, char *Pointer_String_Absolute_Path, TFileManagerList *Pointer_List);

/** Fancy displaying of a list of files, designed to look like the DOS "dir" command.
 * @param Pointer_List The list to display on the screen.
 */
void FileManagerDisplayDirectoryListing(TFileManagerList *Pointer_List);

#endif
