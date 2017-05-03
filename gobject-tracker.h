#include <stdbool.h>

#include "logflags.h"

#ifndef GOBJECT_TRACKER_H
#define GOBJECT_TRACKER_H

/* Struct to store our data */
typedef struct {
	GHashTable *objects;	/* owned */
	LogFlags log_flags;
} ObjectData;

/* GObject overrides */
gpointer	g_object_new (GType type, const char *first, ...);
gpointer	g_object_ref (gpointer object);
void		g_object_unref (gpointer object);

#endif