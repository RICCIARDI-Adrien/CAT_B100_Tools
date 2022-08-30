/** @file File_Manager.h
 * List and download phone files.
 * @author Adrien RICCIARDI
 */
#ifndef H_FILE_MANAGER_H
#define H_FILE_MANAGER_H

#include <List.h>
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
} TFileManagerFileListItem;

//-------------------------------------------------------------------------------------------------
// Functions
//-------------------------------------------------------------------------------------------------
/** Append a new item made of the provided file information to the end of the list.
 * @param Pointer_List The list to add an item to the tail. This list must have been previously initialized.
 * @param Pointer_String_File_Name The file name string content will be copied to the newly added list item.
 * @param File_Size The file size value will be copied to the newly added list item.
 * @param Flags The flags value will be copied to the newly added list item.
 */
void FileManagerListAddFile(TList *Pointer_List, char *Pointer_String_File_Name, unsigned File_Size, int Flags);

/** Find all available drives (C:, D: and so on).
 * @param Serial_Port_ID The serial port the phone is connected to.
 * @param Pointer_List On output, contain the list of the drives. This variable must not contain a valid list already, otherwise this will create a memory leak.
 * @return -1 if an error occurred,
 * @return 0 on success.
 */
int FileManagerListDrives(TSerialPortID Serial_Port_ID, TList *Pointer_List);

/** Create a list containing all files and subdirectories in a specified directory, like ls. This function is not recursive and does not list the content of the subdirectories.
 * @param Serial_Port_ID The serial port the phone is connected to.
 * @param Pointer_String_Absolute_Path The path of the directory to list. The path must be absolute, directory separators are \ like on Windows.
 * @param Pointer_List On output, contain the list of the files. This variable must not contain a valid list already, otherwise this will create a memory leak.
 * @return -1 if an error occurred,
 * @return 0 on success.
 */
int FileManagerListDirectory(TSerialPortID Serial_Port_ID, char *Pointer_String_Absolute_Path, TList *Pointer_List);

/** Fancy displaying of a list of files, designed to look like the DOS "dir" command.
 * @param Pointer_List The list to display on the screen.
 */
void FileManagerDisplayDirectoryListing(TList *Pointer_List);

/** Retrieve a file content from the phone.
 * @param Serial_Port_ID The serial port the phone is connected to.
 * @param Pointer_String_Absolute_Phone_Path The file path and name. This must be the absolute path starting from the drive, directory separators are \ like on Windows.
 * @param Pointer_String_Destination_PC_Path The file path and name that will be created on the local PC.
 * @return -1 if an error occurred,
 * @return 0 on success.
 */
int FileManagerDownloadFile(TSerialPortID Serial_Port_ID, char *Pointer_String_Absolute_Phone_Path, char *Pointer_String_Destination_PC_Path);

/** Retrieve a directory files and all the subdirectories it contains, recreating the same directories tree on output.
 * @param Serial_Port_ID The serial port the phone is connected to.
 * @param Pointer_String_Absolute_Phone_Path The directory path. This must be the absolute path starting from the drive, directory separators are \ like on Windows.
 * @param Pointer_String_Destination_PC_Path The directory path that will be created on the local PC.
 * @return -1 if an error occurred,
 * @return 0 on success.
 */
int FileManagerDownloadDirectory(TSerialPortID Serial_Port_ID, char *Pointer_String_Absolute_Phone_Path, char *Pointer_String_Destination_PC_Path);

#endif
