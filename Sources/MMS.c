/** @file MMS.c
 * See MMS.h for description.
 * @author Adrien RICCIARDI
 */
#include <AT_Command.h>
#include <Log.h>
#include <MMS.h>
#include <stdio.h>
#include <string.h>
#include <Utility.h>

//-------------------------------------------------------------------------------------------------
// Private types
//-------------------------------------------------------------------------------------------------
/** All locations the MMS are stored in. */
typedef enum
{
	MMS_STORAGE_LOCATION_INBOX = 1,
	MMS_STORAGE_LOCATION_OUTBOX = 2,
	MMS_STORAGE_LOCATION_SENT = 4,
	MMS_STORAGE_LOCATION_DRAFTS = 8,
	MMS_STORAGE_LOCATION_TEMPLATES = 64
} TMMSStorageLocation;

/** All memory devices the MMS can be stored in. */
typedef enum
{
	MMS_STORAGE_DEVICE_PHONE = 2,
	MMS_STORAGE_DEVICE_SD_CARD = 4
} TMMSStorageDevice;

//-------------------------------------------------------------------------------------------------
// Private functions
//-------------------------------------------------------------------------------------------------
/** TODO */
static int MMSGetMessagesCount(TSerialPortID Serial_Port_ID, TMMSStorageLocation Storage_Location, TMMSStorageDevice Storage_Device, int *Pointer_Messages_Count, char *Pointer_String_Messages_Payload_Directory, char *Pointer_String_Database_File)
{
	char String_Temporary[256], String_Answer[256];
	unsigned char Buffer[256];
	int Size;

	// Send the command
	sprintf(String_Temporary, "AT+EMMSFS=%d,%d", Storage_Location, Storage_Device);
	printf("cmd envoyée : %s\n", String_Temporary);
	if (ATCommandSendCommand(Serial_Port_ID, String_Temporary) != 0) return -1;

	// Was the command successful ?
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Answer, sizeof(String_Answer)) < 0) return -1;
	printf("rep: %s\n",  String_Answer);
	if (strncmp(String_Answer, "+EMMSFS: 0,", 11) != 0)
	{
		LOG("Error : failed to send the AT command that queries the MMS count (storage location = %d, storage device = %d).\n", Storage_Location, Storage_Device);
		return -1;
	}
	// Wait for the empty line before "OK"
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1;
	// Wait for "OK"
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Temporary, sizeof(String_Temporary)) < 0) return -1;
	if (strcmp(String_Temporary, "OK") != 0)
	{
		LOG("Error : failed to send the AT command that queries the MMS count (storage location = %d, storage device = %d).\n", Storage_Location, Storage_Device);
		return -1;
	}

	// Extract messages count
	if (sscanf(String_Answer, "+EMMSFS: %*d, %d", Pointer_Messages_Count) != 1)
	{
		LOG("Error : failed to extract the MMS count (storage location = %d, storage device = %d).\n", Storage_Location, Storage_Device);
		return -1;
	}
	printf("Pointer_Messages_Count=%d\n", *Pointer_Messages_Count);

	// Do not parse the remaining fields if no message is available
	if (*Pointer_Messages_Count == 0) return 0;

	// Extract messages payload directory
	if (sscanf(String_Answer, "+EMMSFS: %*d, %*d, %*d, \"%[0-9A-F]\"", String_Temporary) != 1)
	{
		LOG("Error : failed to extract the MMS payload directory (storage location = %d, storage device = %d).\n", Storage_Location, Storage_Device);
		return -1;
	}
	printf("sto dir = %s\n", String_Temporary);
	// Convert the string to binary UTF-16, so it can be converted to UTF-8
	Size = ATCommandConvertHexadecimalToBinary(String_Temporary, Buffer, sizeof(Buffer));
	if (Size < 0)
	{
		LOG("Error : could not convert the MMS payload directory hexadecimal string \"%s\" to binary (storage location = %d, storage device = %d).\n", String_Temporary, Storage_Location, Storage_Device);
		return -1;
	}
	// Convert the string to UTF-8
	Size = UtilityConvertString(Buffer, Pointer_String_Messages_Payload_Directory, UTILITY_CHARACTER_SET_UTF16_BIG_ENDIAN, UTILITY_CHARACTER_SET_UTF8, Size, sizeof(String_Temporary));
	if (Size < 0)
	{
		LOG("Error : could not convert the MMS payload directory from UTF-16 to UTF-8 (storage location = %d, storage device = %d).\n", Storage_Location, Storage_Device);
		return -1;
	}
	Pointer_String_Messages_Payload_Directory[Size] = 0; // Make sure the string is terminated
	printf("sto dir conv = %s\n", Pointer_String_Messages_Payload_Directory);

	// Extract database file
	if (sscanf(String_Answer, "+EMMSFS: %*d, %*d, %*d, \"%*[0-9A-F]\", \"%[0-9A-F]\"", String_Temporary) != 1)
	{
		LOG("Error : failed to extract the MMS database file (storage location = %d, storage device = %d).\n", Storage_Location, Storage_Device);
		return -1;
	}
	printf("data fil = %s\n", String_Temporary);
	// Convert the string to binary UTF-16, so it can be converted to UTF-8
	Size = ATCommandConvertHexadecimalToBinary(String_Temporary, Buffer, sizeof(Buffer));
	if (Size < 0)
	{
		LOG("Error : could not convert the MMS database file hexadecimal string \"%s\" to binary (storage location = %d, storage device = %d).\n", String_Temporary, Storage_Location, Storage_Device);
		return -1;
	}
	// Convert the string to UTF-8
	Size = UtilityConvertString(Buffer, Pointer_String_Database_File, UTILITY_CHARACTER_SET_UTF16_BIG_ENDIAN, UTILITY_CHARACTER_SET_UTF8, Size, sizeof(String_Temporary));
	if (Size < 0)
	{
		LOG("Error : could not convert the MMS database file from UTF-16 to UTF-8 (storage location = %d, storage device = %d).\n", Storage_Location, Storage_Device);
		return -1;
	}
	Pointer_String_Database_File[Size] = 0; // Make sure the string is terminated
	printf("data fil conv = %s\n", Pointer_String_Database_File);

	return 0;
}

//-------------------------------------------------------------------------------------------------
// Public functions
//-------------------------------------------------------------------------------------------------
int MMSDownloadAll(TSerialPortID Serial_Port_ID)
{
	static TMMSStorageLocation Storage_Location_Lookup_Table[] =
	{
		MMS_STORAGE_LOCATION_INBOX,
		MMS_STORAGE_LOCATION_OUTBOX,
		MMS_STORAGE_LOCATION_SENT,
		MMS_STORAGE_LOCATION_DRAFTS,
		MMS_STORAGE_LOCATION_TEMPLATES
	};
	static TMMSStorageDevice Storage_Device_Lookup_Table[] =
	{
		MMS_STORAGE_DEVICE_PHONE,
		MMS_STORAGE_DEVICE_SD_CARD
	};
	int Messages_Count;
	unsigned int Location_Index, Device_Index;
	char String_Messages_Payload_Directory[128], String_Database_File[128];

	// Try all possible messages storage combinations
	for (Device_Index = 0; Device_Index < UTILITY_ARRAY_SIZE(Storage_Device_Lookup_Table); Device_Index++)
	{
		for (Location_Index = 0; Location_Index < UTILITY_ARRAY_SIZE(Storage_Location_Lookup_Table); Location_Index++)
		{
			if (MMSGetMessagesCount(Serial_Port_ID, Storage_Location_Lookup_Table[Location_Index], Storage_Device_Lookup_Table[Device_Index], &Messages_Count, String_Messages_Payload_Directory, String_Database_File) != 0) return -1;
			printf("Messages_Count=%d\n", Messages_Count);
		}
	}

	return 0;
}
