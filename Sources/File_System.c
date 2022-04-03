/** @file File_System.c
 * See File_System.h for description.
 * @author Adrien RICCIARDI
 */
#include <File_System.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//-------------------------------------------------------------------------------------------------
// Public functions
//-------------------------------------------------------------------------------------------------
int FileSystemCreateDirectory(char *Pointer_Directory_Name)
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
