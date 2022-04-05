/** @file Utility.h
 * Various utilities used by multiple parts of the program.
 * @author Adrien RICCIARDI
 */
#ifndef H_UTILITY_H
#define H_UTILITY_H

#include <stddef.h>

//-------------------------------------------------------------------------------------------------
// Types
//-------------------------------------------------------------------------------------------------
/** All supported character sets. */
typedef enum
{
	UTILITY_CHARACTER_SET_WINDOWS_1252,
	UTILITY_CHARACTER_SET_UTF16_BIG_ENDIAN,
	UTILITY_CHARACTER_SET_UTF8,
	UTILITY_CHARACTER_SETS_COUNT
} TUtilityCharacterSet;

//-------------------------------------------------------------------------------------------------
// Functions
//-------------------------------------------------------------------------------------------------
/** Create a directory if it does not exist yet.
 * @param Pointer_Directory_Name The directory name. The directory is created in the current directory only (this function can't create recursively multiple directories).
 * @return -1 if an error occurred,
 * @return 0 on success.
 * @note This function returns with success if the directory already exists.
 */
int UtilityCreateDirectory(char *Pointer_Directory_Name);

/** Convert a string from a character set to another.
 * @param Pointer_String_Source The string to convert.
 * @param Pointer_String_Destination On output, contain the converted string.
 * @param Source_Character_Set The source character set to convert from.
 * @param Destination_Character_Set The destination character set to convert to.
 * @param Source_String_Size The source string size in bytes (not in characters). If set to 0, the string length will be automatically determined, make sure that the string does not contain NULL bytes in the middle of the data.
 * @param Destination_String_Size The maximum size in bytes of the output string.
 * @return -1 if an error occurs,
 * @return A positive number on success, it indicates the destination string size in bytes.
 */
int UtilityConvertString(void *Pointer_String_Source, void *Pointer_String_Destination, TUtilityCharacterSet Source_Character_Set, TUtilityCharacterSet Destination_Character_Set, size_t Source_String_Size, size_t Destination_String_Size);

#endif
