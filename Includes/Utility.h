/** @file Utility.h
 * Various utilities used by multiple parts of the program.
 * @author Adrien RICCIARDI
 */
#ifndef H_UTILITY_H
#define H_UTILITY_H

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

#endif
