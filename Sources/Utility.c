/** @file Utility.c
 * See Utility.h for description.
 * @author Adrien RICCIARDI
 */
#include <errno.h>
#include <iconv.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <Utility.h>

//-------------------------------------------------------------------------------------------------
// Public functions
//-------------------------------------------------------------------------------------------------
int UtilityCreateDirectory(char *Pointer_Directory_Name)
{
	struct stat Status;

	// Retrieve directory information
	if (stat(Pointer_Directory_Name, &Status) == 0)
	{
		// The directory is existing, make sure this is really a directory
		if (!(Status.st_mode & S_IFDIR))
		{
			printf("Error : the \"%s\" file already exists but is not a directory.\n", Pointer_Directory_Name);
			return -1;
		}
	}
	else
	{
		// Do not consider the "directory does not exist" error as we want to create a directory that may not exist yet
		if (errno != ENOENT)
		{
			printf("Error : could not get directory \"%s\" status (%s).\n", Pointer_Directory_Name, strerror(errno));
			return -1;
		}

		// Try to create the directory with standard user permissions
		if (mkdir(Pointer_Directory_Name, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0)
		{
			printf("Error : could not create the directory (%s)\n", strerror(errno));
			return -1;
		}
	}

	return 0;
}

int UtilityConvertString(void *Pointer_String_Source, void *Pointer_String_Destination, TUtilityCharacterSet Source_Character_Set, TUtilityCharacterSet Destination_Character_Set, size_t Source_String_Size, size_t Destination_String_Size)
{
	static char *Pointer_String_Character_Sets[] =
	{
		// UTILITY_CHARACTER_SET_WINDOWS_1252
		"WINDOWS-1252",
		// UTILITY_CHARACTER_SET_UTF16_BIG_ENDIAN
		"UTF-16BE",
		// UTILITY_CHARACTER_SET_UTF8
		"UTF-8"
	};
	iconv_t Conversion_Descriptor;
	char *Pointer_String_Source_Character_Set, *Pointer_String_Destination_Character_Set, *Pointer_String_Source_Characters, *Pointer_String_Destination_Characters;
	int Return_Value = -1;
	size_t Destination_String_Available_Bytes;

	// Select the correct character sets
	if ((Source_Character_Set < 0) || (Source_Character_Set >= UTILITY_CHARACTER_SETS_COUNT))
	{
		printf("Error : unknown source character set %d, aborting string conversion.\n", Source_Character_Set);
		return Return_Value;
	}
	Pointer_String_Source_Character_Set = Pointer_String_Character_Sets[Source_Character_Set];
	if ((Destination_Character_Set < 0) || (Destination_Character_Set >= UTILITY_CHARACTER_SETS_COUNT))
	{
		printf("Error : unknown destination character set %d, aborting string conversion.\n", Destination_Character_Set);
		return Return_Value;
	}
	Pointer_String_Destination_Character_Set = Pointer_String_Character_Sets[Destination_Character_Set];

	// Configure character sets
	Conversion_Descriptor = iconv_open(Pointer_String_Destination_Character_Set, Pointer_String_Source_Character_Set);
	if (Conversion_Descriptor == (iconv_t) -1)
	{
		printf("Error : failed to create the character set conversion descriptor, aborting text conversion.\n");
		return Return_Value;
	}

	// Determine source string size if the size is not provided, make sure there are no trailing zero bytes in the string !
	if (Source_String_Size == 0) Source_String_Size = strlen(Pointer_String_Source);

	// Do conversion
	Pointer_String_Source_Characters = Pointer_String_Source;
	Pointer_String_Destination_Characters = Pointer_String_Destination;
	Destination_String_Available_Bytes = Destination_String_Size;
	if (iconv(Conversion_Descriptor, &Pointer_String_Source_Characters, &Source_String_Size, &Pointer_String_Destination_Characters, &Destination_String_Available_Bytes) == (size_t) -1) printf("Error : text conversion failed (%s).\n", strerror(errno));
	else Return_Value = (int) (Destination_String_Size - Destination_String_Available_Bytes); // Determine how many bytes of the destination buffer have been used
	iconv_close(Conversion_Descriptor);

	return Return_Value;
}
