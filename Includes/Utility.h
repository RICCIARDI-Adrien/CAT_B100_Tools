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
	UTILITY_CHARACTER_SET_UTF16_BIG_ENDIAN
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

/** Convert a non-ASCII string to UTF-8.
 * @param Pointer_String_Source The string to convert.
 * @param Pointer_Converted_String On output, contain the string converted to UTF-8.
 * @param Source_Character_Set The source character set to convert from.
 * @param Source_String_Size The source string size in bytes (not in characters). If set to 0, the string length will be automatically determined, make sure that the string does not contain NULL bytes in the middle of the data.
 * @param Destination_String_Size The maximum size in bytes of the output string.
 * @return -1 if an error occurs,
 * @return 0 on success.
 */
int UtilityConvertStringToUTF8(void *Pointer_String_Source, char *Pointer_Converted_String, TUtilityCharacterSet Source_Character_Set, size_t Source_String_Size, size_t Destination_String_Size);

#endif
