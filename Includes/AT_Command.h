/** @file AT_Command.h
 * Various utilities to deal with AT commands.
 * @author Adrien RICCIARDI
 */
#ifndef H_AT_COMMAND_H
#define H_AT_COMMAND_H

#include <Serial_Port.h>

//-------------------------------------------------------------------------------------------------
// Functions
//-------------------------------------------------------------------------------------------------
/** Read a command answer line from the serial port up to the terminating CRLF sequence, or until the string length is reached.
 * @param Serial_Port_ID The serial port to read line from.
 * @param Pointer_String_Answer On output, contain the received answer.
 * @param Maximum_Length The size of the answer string buffer. This value must include the room for the string terminating zero.
 * @return -3 if the provided string has not enough space to store the answer,
 * @return -2 if the read line is AT "ERROR<CRLF>",
 * @return -1 if the provided maximum length is too small,
 * @return 0 if a valid answer was read. In this case, a terminating zero is always added at the end of the string.
 */
int ATCommandReceiveAnswerLine(TSerialPortID Serial_Port_ID, char *Pointer_String_Answer, unsigned int Maximum_Length);

/** Send the command, append the terminating character CR at its end and discard the command echoing.
 * @param Serial_Port_ID The serial port to send the command to.
 * @param Pointer_String_Command The command to send, it must be a zero-terminated string without the final CRLF sequence (it is automatically appended by this function).
 * @return -1 if an error occurred,
 * @return 0 on success.
 */
int ATCommandSendCommand(TSerialPortID Serial_Port_ID, char *Pointer_String_Command);

/** TODO
 * @return -1 if the provided hexadecimal string is invalid (it contains non-hexadecimal characters, there is an odd amount of characters leading to incomplete hexadecimal bytes, ...),
 * @return 0 on success.
 */
int ATCommandConvertHexadecimalToBinary(char *Pointer_String_Hexadecimal, unsigned char *Pointer_Output_Buffer, unsigned int Output_Buffer_Size);

/** TODO */
void ATCommandConvertBinaryToHexadecimal(unsigned char *Pointer_Buffer, unsigned int Buffer_Size, char *Pointer_String_Hexadecimal);

#endif
