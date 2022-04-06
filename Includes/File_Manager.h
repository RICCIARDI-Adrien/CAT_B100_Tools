/** @file File_Manager.h
 * List and download phone files.
 * @author Adrien RICCIARDI
 */
#ifndef H_FILE_MANAGER_H
#define H_FILE_MANAGER_H

#include <Serial_Port.h>

//-------------------------------------------------------------------------------------------------
// Types
//-------------------------------------------------------------------------------------------------
/** Hold a phone file object, which can either be a file or a directory. */
typedef struct TFileManagerFileListItem
{
	char String_File_Name[256];
	unsigned int File_Size; //!< The phone is using system the FAT32 file system, so 32 bits should be enough.
	int Flags;
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

#endif
