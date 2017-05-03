#include <stdbool.h>

#ifndef EVENT_H
#define EVENT_H

#define BACKTRACE_DEPTH 50

typedef enum {
	EVENT_CREATED = 0,
	EVENT_REF = 1,
	EVENT_UNREF = 2,
} EventType;

typedef struct {
	EventType type;
	void **backtrace;
	int symbols;
	int old_ref_count;
	int new_ref_count;
} Event;

/* Functions to deal with tracking ref/unref/creation events */
Event * event_new			(EventType type, bool capture_backtrace);
Event * event_created_new	(int ref_count, bool capture_backtrace);
Event * event_ref_new		(int old_ref_count, int new_ref_count, bool capture_backtrace);
Event * event_unref_new		(int old_ref_count, int new_ref_count, bool capture_backtrace);
void	event_destroy		(Event *event);

/* Helper functions to print out our data */
void print_trace (void **array, int symbols);
void print_event (gpointer data, gpointer user_data);

#endif