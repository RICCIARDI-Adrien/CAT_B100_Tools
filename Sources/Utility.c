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

int UtilityConvertStringToUTF8(void *Pointer_String_Source, char *Pointer_Converted_String, TUtilityCharacterSet Source_Character_Set, size_t Source_String_Size, size_t Destination_String_Size)
{
	iconv_t Conversion_Descriptor;
	char *Pointer_String_Character_Set, *Pointer_String_Source_Characters;
	int Return_Value = -1;

	// Select the correct character set
	switch (Source_Character_Set)
	{
		case UTILITY_CHARACTER_SET_WINDOWS_1252:
			Pointer_String_Character_Set = "WINDOWS-1252";
			break;

		case UTILITY_CHARACTER_SET_UTF16_BIG_ENDIAN:
			Pointer_String_Character_Set = "UTF-16BE";
			break;

		default:
			printf("Error : unknown character set %d, aborting string conversion.\n", Source_Character_Set);
			return Return_Value;
	}

	// Configure character sets
	Conversion_Descriptor = iconv_open("UTF-8", Pointer_String_Character_Set);
	if (Conversion_Descriptor == (iconv_t) -1)
	{
		printf("Error : failed to create the character set conversion descriptor, skipping text conversion to UTF-8.\n");
		return Return_Value;
	}

	// Determine source string size if the size is not provided, make sure there are no trailing zero bytes in the string !
	if (Source_String_Size == 0) Source_String_Size = strlen(Pointer_String_Source);

	// Do conversion
	Pointer_String_Source_Characters = Pointer_String_Source;
	if (iconv(Conversion_Descriptor, &Pointer_String_Source_Characters, &Source_String_Size, &Pointer_Converted_String, &Destination_String_Size) == (size_t) -1) printf("Error : text conversion to UTF-8 failed.\n");
	else Return_Value = 0;
	iconv_close(Conversion_Descriptor);

	return Return_Value;
}
