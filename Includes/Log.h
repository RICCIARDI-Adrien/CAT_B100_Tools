/** @file Log.h
 * Simple logging system that displays messages to the console.
 * @author Adrien RICCIARDI
 */
#ifndef H_LOG_H
#define H_LOG_H

#include <stdio.h>

//-------------------------------------------------------------------------------------------------
// Constants and macros
//-------------------------------------------------------------------------------------------------
/** @def LOG(String_Message, ...)
 * Display a message preceded by the function name and the current line.
 * @param String_Message The message to display.
 */
#define LOG(String_Message, ...) printf("[%s():%d] " String_Message, __FUNCTION__, __LINE__, ##__VA_ARGS__)

/** @def LOG_DEBUG(Is_Enabled, String_Message, ...)
 * Display a message preceded by the function name and the current line only when enabled at compilation time.
 * @param Is_Enabled Set to 1 to embed the message in the final application, set to 0 to remove the message from the compiled application.
 * @param String_Message The message to display.
 */
#define LOG_DEBUG(Is_Enabled, String_Message, ...) do { if (Is_Enabled) printf("[%s():%d] " String_Message, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while (0)

#endif
