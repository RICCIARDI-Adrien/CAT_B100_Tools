/** @file File_Manager.h
 * List and download phone files.
 * @author Adrien RICCIARDI
 */
#ifndef H_FILE_MANAGER_H
#define H_FILE_MANAGER_H

//-------------------------------------------------------------------------------------------------
// Types
//-------------------------------------------------------------------------------------------------
/** Hold a phone file object, which can either be a file or a directory. */
typedef struct TFileManagerFileListItem
{
	char String_File_Name[256];
	int Is_Directory;
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
void FileManagerListAddFile(TFileManagerList *Pointer_List, char *Pointer_String_File_Name, int Is_Directory);
/** TODO */
void FileManagerListClear(TFileManagerList *Pointer_List);
/** TODO for debug purpose */
void FileManagerListDisplay(TFileManagerList *Pointer_List);

/** TODO */
unsigned int FileManagerListDrives(void); // int for error ?

int FileManagerListDirectory(char *Pointer_String_Absolute_Path, TFileManagerList *Pointer_List);

#endif
