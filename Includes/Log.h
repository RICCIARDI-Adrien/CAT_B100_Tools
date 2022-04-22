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

#endif
