#include <execinfo.h>
#include <stdbool.h>
#include <stdlib.h>

#include <glib-object.h>

#include "event.h"
#include "logflags.h"

/* Gather the managed backtrace */
extern char *mono_pmip (void *ip);

Event *
event_new (EventType type, bool capture_backtrace)
{
	Event *event;

	event = calloc (1, sizeof (Event));
	event->type = type;
	if (capture_backtrace) {
		event->backtrace = malloc (sizeof (void*) * BACKTRACE_DEPTH);
		event->symbols = backtrace (event->backtrace, BACKTRACE_DEPTH);
	}
	return event;
}

Event *
event_created_new (int new_ref_count, bool capture_backtrace)
{	
	Event *event;

	event = event_new (EVENT_CREATED, capture_backtrace);
	event->new_ref_count = new_ref_count;
	return event;
}

Event *
event_ref_new (int old_ref_count, int new_ref_count, bool capture_backtrace)
{	
	Event *event;

	event = event_new (EVENT_REF, capture_backtrace);
	event->old_ref_count = old_ref_count;
	event->new_ref_count = new_ref_count;
	return event;
}

Event *
event_unref_new (int old_ref_count, int new_ref_count, bool capture_backtrace)
{	
	Event *event;

	event = event_new (EVENT_UNREF, capture_backtrace);
	event->old_ref_count = old_ref_count;
	event->new_ref_count = new_ref_count;
	return event;
}

void
event_destroy (Event *event)
{
	free (event->backtrace);
	free (event);
}

void
print_event (gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	Event *event = (Event *)data;
	if (event->type == EVENT_CREATED)
		g_print ("\tCreated %d -> %d\n", event->old_ref_count, event->new_ref_count);
	else if (event->type == EVENT_REF)
		g_print ("\tRef %d -> %d\n", event->old_ref_count, event->new_ref_count);
	else if (event->type == EVENT_UNREF)
		g_print ("\tUnref %d -> %d\n", event->old_ref_count, event->new_ref_count);
	else
		g_print ("\tUnknown event...\n");

	print_trace (event->backtrace, event->symbols);
}

void
print_trace (void **array, int symbols)
{
	char **names;
	char *managed_frame;
	int i;

	names = backtrace_symbols (array, symbols);
	for (i = 1; i < symbols; ++i) {
		if ((managed_frame = mono_pmip (array [i])) != NULL)
			g_print ("\t%d %s\n", i, managed_frame);
		else
			g_print ("\t%s\n", names [i]);
	}
}
