/** @file SMS.c
 * See SMS.h for description.
 * @author Adrien RICCIARDI
 */
#include <AT_Command.h>
#include <SMS.h>
#include <stdio.h>
#include <string.h>

//-------------------------------------------------------------------------------------------------
// Private functions
//-------------------------------------------------------------------------------------------------
/** TODO
 * @return -1 if the SMS with the specified number could not be read.
 */
int SMSDownloadSingleRecord(TSerialPortID Serial_Port_ID, int SMS_Number, char *Pointer_String_Content, unsigned int Maximum_Content_Length)
{
	char String_Command[64], String_Information[64];

	// Send the command
	sprintf(String_Command, "AT+EMGR=%d", SMS_Number);
	if (ATCommandSendCommand(Serial_Port_ID, String_Command) != 0) return -1;

	// Wait for the information string
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Information, sizeof(String_Information)) < 0) return -1;
	printf("info : %s\n", String_Information);

	// Wait for the message content
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, Pointer_String_Content, Maximum_Content_Length) < 0) return -1;
	printf("content : %s\n", Pointer_String_Content);

	// Wait for the standard OK
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Command, sizeof(String_Command)) < 0) return -1; // Recycle "String_Command" variable, wait for empty line before "OK"
	if (ATCommandReceiveAnswerLine(Serial_Port_ID, String_Command, sizeof(String_Command)) < 0) return -1; // Recycle "String_Command" variable, wait for "OK"
	printf("OK : %s\n", String_Command);
	if (strcmp(String_Command, "OK") != 0) return -1;

	return 0;
}

//-------------------------------------------------------------------------------------------------
// Public functions
//-------------------------------------------------------------------------------------------------
int SMSDownload(TSerialPortID Serial_Port_ID)
{
	static char test[2048];
	int i;

	for (i = 1; i <= 10; i++) SMSDownloadSingleRecord(Serial_Port_ID, i, test, sizeof(test));

	return 0;
}
