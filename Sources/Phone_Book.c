/** @file Phone_Book.c
 * See Phone_Book.h for description.
 * @author Adrien RICCIARDI
 */
#include <AT_Command.h>
#include <Log.h>
#include <Phone_Book.h>
#include <string.h>
#include <Utility.h>

//-------------------------------------------------------------------------------------------------
// Private constants
//-------------------------------------------------------------------------------------------------
/** Allow to turn on or off debug messages. */
#define PHONE_BOOK_IS_DEBUG_ENABLED 0

/** The maximum amount of phone book entries that can be handled by the program. */
#define PHONE_BOOK_MAXIMUM_ENTRIES 500

//-------------------------------------------------------------------------------------------------
// Private variables
//-------------------------------------------------------------------------------------------------
/** Cache all phone book entries read from the device. */
static TPhoneBookEntry Phone_Book_Entries[PHONE_BOOK_MAXIMUM_ENTRIES];
/** How many entries are stored in the phone book entries table. All these entries are valid and start from index 0. */
static int Phone_Book_Entries_Count;

//-------------------------------------------------------------------------------------------------
// Private functions
//-------------------------------------------------------------------------------------------------
/** Read a single phone book entry from the preselected phone book.
 * @param Serial_Port_ID The phone serial port.
 * @param Entry_Index The ID of the phone book entry to read.
 * @param Pointer_Entry On output, contain the relevant phone book entry data.
 * @return -1 if an error occurred,
 * @return 0 if the entry is empty,
 * @return 1 if the entry contains valid data.
 */
static int PhoneBookReadSingleEntry(TSerialPortID Serial_Port_ID, int Entry_Index, TPhoneBookEntry *Pointer_Entry)
{
	char String_Temporary[512], String_Name[448], *Pointer_String_Answer, Character;
	size_t i;

	// Send the entry reading command
	sprintf(String_Temporary, "AT+CPBR=%d", Entry_Index);
	if (ATCommandSendCommand(Serial_Port_ID, String_Temporary) != 0)
	{
		LOG("Error : failed to send the read phone entry %d command.\n", Entry_Index);
		return -1;
	}

	// Does the entry contain data ?
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1;
	if (strcmp(String_Temporary, "OK") == 0)
	{
		LOG_DEBUG(PHONE_BOOK_IS_DEBUG_ENABLED, "The entry %d is empty.\n", Entry_Index);
		return 0;
	}
	// Is this the expected answer ?
	if (strncmp("+CPBR:", String_Temporary, 6) != 0)
	{
		LOG("Error : unknown answer string when reading entry %d. Answer string : \"%s\".\n", Entry_Index, String_Temporary);
		return -1;
	}

	// Extract the phone number (if any)
	LOG_DEBUG(PHONE_BOOK_IS_DEBUG_ENABLED, "Raw CPBR AT answer : \"%s\".\n", String_Temporary);

	// Go up to the first opening double quote
	Pointer_String_Answer = String_Temporary;
	do
	{
		Character = *Pointer_String_Answer;
		if (Character == 0)
		{
			LOG("Error : malformed answer for entry %d, it does not contain the opening double quotes for the phone number.\n", Entry_Index);
			return -1;
		}
		Pointer_String_Answer++;
	} while (Character != '"');
	// Copy the number string
	i = 0;
	while (1)
	{
		Character = *Pointer_String_Answer;
		if (Character == 0)
		{
			LOG("Error : malformed answer for entry %d, it does not contain the ending double quotes for the phone number.\n", Entry_Index);
			return -1;
		}
		Pointer_String_Answer++; // By incrementing the string pointer before checking for the ending double quote, the double quote character is "absorbed" to ease the following string extraction steps
		if (Character == '"') break;
		Pointer_Entry->String_Number[i] = Character;
		i++;
		if (i == sizeof(Pointer_Entry->String_Number))
		{
			LOG("Error : there is not enough room in the phone number string to store the entry %d number.\n", Entry_Index);
			return -1;
		}
	}
	Pointer_Entry->String_Number[i] = 0; // Add the terminating zero
	UtilityNormalizePhoneNumber(Pointer_Entry->String_Number); // Discard the initial double zeros if any, as all numbers provided by the phone start directly with the country prefix
	LOG_DEBUG(PHONE_BOOK_IS_DEBUG_ENABLED, "Extracted phone number string (initial double zeros have been removed if any) : \"%s\".\n", Pointer_Entry->String_Number);

	// Go up to the third opening double quote
	do
	{
		Character = *Pointer_String_Answer;
		if (Character == 0)
		{
			LOG("Error : malformed answer for entry %d, it does not contain the opening double quotes for the phone book name.\n", Entry_Index);
			return -1;
		}
		Pointer_String_Answer++;
	} while (Character != '"');
	// Copy the name string
	i = 0;
	while (1)
	{
		Character = *Pointer_String_Answer;
		if (Character == 0)
		{
			LOG("Error : malformed answer for entry %d, it does not contain the ending double quotes for the phone book name.\n", Entry_Index);
			return -1;
		}
		if (Character == '"') break;
		Pointer_String_Answer++;
		String_Name[i] = Character;
		i++;
		if (i == sizeof(String_Name))
		{
			LOG("Error : there is not enough room in the phone book name string to store the entry %d name.\n", Entry_Index);
			return -1;
		}
	}
	String_Name[i] = 0; // Add the terminating zero
	LOG_DEBUG(PHONE_BOOK_IS_DEBUG_ENABLED, "Extracted raw (unconverted) phone book name string : \"%s\".\n", String_Name);
	// The string is encoded with an uncommon character set, convert to standard UTF-8
	if (UtilityConvertString(String_Name, Pointer_Entry->String_Name, UTILITY_CHARACTER_SET_WINDOWS_1252, UTILITY_CHARACTER_SET_UTF8, 0, sizeof(Pointer_Entry->String_Name)) < 0)
	{
		LOG("Error : failed to convert the phone book name string to UTF-8 when reading entry %d.\n", Entry_Index);
		return -1;
	}
	LOG_DEBUG(PHONE_BOOK_IS_DEBUG_ENABLED, "Converted phone book name string : \"%s\".\n", Pointer_Entry->String_Name);

	// Wait for the line separator
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1;
	if (String_Temporary[0] != 0)
	{
		LOG("Error : failed to receive the line separator answer when reading entry %d.\n", Entry_Index);
		return -1;
	}

	// Wait for the ending "OK" answer
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1;
	if (strcmp(String_Temporary, "OK") != 0)
	{
		LOG("Error : failed to receive the ending \"OK\" answer when reading entry %d.\n", Entry_Index);
		return -1;
	}

	return 1;
}

/** Search for a phone number string in the whole phone book.
 * @param Pointer_String_Number The number to search for.
 * @return -1 if the number was not found,
 * @return 0 or a positive number if the number was found, corresponding to the number index in the phone book.
 * @note This function is looking for an exact match.
 */
static int PhoneBookSearchNumber(char *Pointer_String_Number)
{
	int i;

	for (i = 0; i < Phone_Book_Entries_Count; i++)
	{
		if (strcmp(Pointer_String_Number, Phone_Book_Entries[i].String_Number) == 0) return i;
	}

	return -1;
}

//-------------------------------------------------------------------------------------------------
// Public functions
//-------------------------------------------------------------------------------------------------
int PhoneBookReadAllEntries(TSerialPortID Serial_Port_ID)
{
	char String_Answer[256];
	int First_Index, Last_Index, i, Result, Failures_Count;
	TPhoneBookEntry Phone_Book_Entry;

	// Select the phone internal memory phone book
	if (ATCommandSendCommand(Serial_Port_ID, "AT+CPBS=\"ME\"") != 0)
	{
		LOG("Error : failed to select the phone internal phone book.\n");
		return -1;
	}
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Answer, sizeof(String_Answer)) < 0) return -1;
	if (strcmp(String_Answer, "OK") != 0)
	{
		LOG("Error : failed to send the command to select the phone internal phone book.\n");
		return -1;
	}

	// Find the amount of phone book indexes
	if (ATCommandSendCommand(Serial_Port_ID, "AT+CPBR=?") != 0)
	{
		LOG("Error : failed to select the phone internal phone book.\n");
		return -1;
	}
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Answer, sizeof(String_Answer)) < 0) return -1;
	if (sscanf(String_Answer, "+CPBR: (%d-%d)", &First_Index, &Last_Index) != 2)
	{
		LOG("Error : failed to retrieve the phone book first and last indexes.\n");
		return -1;
	}
	LOG_DEBUG(PHONE_BOOK_IS_DEBUG_ENABLED, "First index : %d, last index : %d.\n", First_Index, Last_Index);

	// Try to read all entries
	Phone_Book_Entries_Count = 0;
	for (i = First_Index; i < Last_Index; i++)
	{
		// Sometimes the reading of an entry fails, so retry several times before giving up
		for (Failures_Count = 0; Failures_Count < 3; Failures_Count++)
		{
			Result = PhoneBookReadSingleEntry(Serial_Port_ID, i, &Phone_Book_Entry);
			if (Result >= 0) break;
		}
		if (Failures_Count == 3)
		{
			LOG("Error : failed to retrieve the phone book entry of index %d.\n", i);
			return -1;
		}

		// Ignore the entry if it is empty
		if (Result == 1)
		{
			memcpy(&Phone_Book_Entries[Phone_Book_Entries_Count], &Phone_Book_Entry, sizeof(TPhoneBookEntry));
			Phone_Book_Entries_Count++;

			if (Phone_Book_Entries_Count >= PHONE_BOOK_MAXIMUM_ENTRIES)
			{
				LOG("Error : the program can store only %d valid phone book entries but the phone contains more valid entries. Increase the program maximum value and retry.\n", PHONE_BOOK_MAXIMUM_ENTRIES);
				return -1;
			}
		}
	}

	// Dump the entries table in debug mode
	LOG_DEBUG(PHONE_BOOK_IS_DEBUG_ENABLED, "Phone book entries table contains %d entries :\n", Phone_Book_Entries_Count);
	for (i = 0; i < Phone_Book_Entries_Count; i++) LOG_DEBUG(PHONE_BOOK_IS_DEBUG_ENABLED, "Entry %d : number=\"%s\", name=\"%s\".\n", i, Phone_Book_Entries[i].String_Number, Phone_Book_Entries[i].String_Name);

	return 0;
}

int PhoneBookGetNameFromNumber(char *Pointer_String_Number, char *Pointer_String_Name)
{
	int Index;
	char String_Temporary[256];

	// Make sure the provided number is not empty
	if (Pointer_String_Number[0] == 0)
	{
		LOG("Error : the provided number string is empty.\n");
		goto Exit_Number_Not_Found;
	}

	// Try to find the number as-is
	Index = PhoneBookSearchNumber(Pointer_String_Number);
	if (Index >= 0)
	{
		LOG_DEBUG(PHONE_BOOK_IS_DEBUG_ENABLED, "The number has been found as-is at index %d of the phone book table.\n", Index);
		goto Exit_Number_Found;
	}

	// Number was not found, try without the country prefix
	strcpy(String_Temporary, &Pointer_String_Number[1]); // Copy the number discarding the first digit of the country code
	String_Temporary[0] = '0'; // Replace what was the second digit of the country code by the zero character
	Index = PhoneBookSearchNumber(String_Temporary);
	if (Index >= 0)
	{
		LOG_DEBUG(PHONE_BOOK_IS_DEBUG_ENABLED, "The number has been found after removing the country code at index %d of the phone book table.\n", Index);
		goto Exit_Number_Found;
	}

Exit_Number_Not_Found:
	// No matching number was found, so provide the phone number as the name
	LOG_DEBUG(PHONE_BOOK_IS_DEBUG_ENABLED, "The number was not found in the phone book table, providing the number in place of the name.\n");
	strcpy(Pointer_String_Name, Pointer_String_Number);
	return 0;

Exit_Number_Found:
	// If the number was found but the name is empty, still return the phone number as the name
	if (Phone_Book_Entries[Index].String_Name[0] == 0) strcpy(Pointer_String_Name, Pointer_String_Number);
	else strcpy(Pointer_String_Name, Phone_Book_Entries[Index].String_Name);
	return 1;
}
