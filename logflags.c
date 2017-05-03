#include <string.h>
#include <glib-object.h>

#include "logflags.h"

LogFlags
get_log_flags ()
{
	const char *env;
	LogFlags flags;

	env = g_getenv ("LOG_FLAGS");
	flags = LOG_NONE;
	if (env) {
		if (strstr (env, "refs"))
			flags |= LOG_REFS;
		if (strstr (env, "backtrace"))
			flags |= LOG_BACKTRACE;
		if (strstr (env, "all"))
			flags |= LOG_ALL;
	} else {
		flags = LOG_DEFAULT;
	}

	return flags;
}
