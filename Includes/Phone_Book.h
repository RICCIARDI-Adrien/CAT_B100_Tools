/** @file Phone_Book.h
 * Retrieve the various phone books content.
 * @author Adrien RICCIARDI
 */
#ifndef H_PHONE_BOOK_H
#define H_PHONE_BOOK_H

#include <Serial_Port.h>

//-------------------------------------------------------------------------------------------------
// Types
//-------------------------------------------------------------------------------------------------
/** A phone entry content. */
typedef struct
{
	char String_Number[32];
	char String_Name[256];
} TPhoneBookEntry;

//-------------------------------------------------------------------------------------------------
// Functions
//-------------------------------------------------------------------------------------------------
/** TODO */
int PhoneBookReadAllEntries(TSerialPortID Serial_Port_ID);

/** TODO */
int PhoneBookGetNameFromNumber(char *Pointer_String_Number, char *Pointer_String_Name);

#endif
