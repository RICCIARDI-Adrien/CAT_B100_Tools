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
/** Cache all phone book entries.
 * @param Serial_Port_ID The phone serial port.
 * @return -1 if an error occurred,
 * @return 0 on success.
 */
int PhoneBookReadAllEntries(TSerialPortID Serial_Port_ID);

/** Search in the phone book for the name matching a specified phone number.
 * @param Pointer_String_Number The number to search for.
 * @param Pointer_String_Name On output, contain the matching name, or, if the phone number could not be found, the phone number itself.
 * @return 0 if the number was not found,
 * @return 1 if the number was found.
 */
int PhoneBookGetNameFromNumber(char *Pointer_String_Number, char *Pointer_String_Name);

#endif
