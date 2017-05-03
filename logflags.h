#ifndef LOGFLAGS_H
#define LOGFLAGS_H

typedef enum
{
	LOG_NONE		= 1 << 0,
	LOG_REFS		= 1 << 1,
	LOG_BACKTRACE	= 1 << 2,
	LOG_DEFAULT		= LOG_REFS,
	LOG_ALL			= LOG_REFS | LOG_BACKTRACE,
} LogFlags;

LogFlags get_log_flags ();

#endif
