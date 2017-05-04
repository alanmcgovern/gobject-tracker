#include <dlfcn.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include <glib-object.h>

#include "gobject-tracker.h"
#include "logflags.h"
#include "event.h"

/* Set to true when SIGUSR1 is sent. Causes us to dump the object list asynchronously */
static volatile bool should_dump_objects = false;

/* Static state to store gathered data. Will be initialized the first time `find_symbol is invoked. */
static volatile ObjectData tracker = { NULL, LOG_DEFAULT };

/* Ensure we don't corrupt our static state if ref/unref is invoked on multiple threads */
G_LOCK_DEFINE_STATIC (tracker_lock);

/* Used to clear out our hashtable when the tracked GObject is destroyed */
static void g_object_finalized (G_GNUC_UNUSED gpointer data, GObject *obj);

/* Limits the object types we wish to inspect */
static gboolean should_process_object (const char *obj_name);

/* Writes all the information about tracked objects to stdout */
static void dump_tracked_objects ();

/* Dumps our objects if `should_dump_objects` has been set to true */
static void maybe_dump_tracked_objects ();

/* Sets `should_dump_objects` to `true` */
static void _signal_handler (G_GNUC_UNUSED int signal);

/* Initializes our static state and then locates the symbol inside libgobject-*.dylib. */
static void * find_symbol (const char *func_name);

/* Helpers to handle the LogFlags */
static bool has_log_backtrace ();
static bool has_log_refs ();

/* Helpers to print out the number of living objects of each type */
static gint allocated_instance_comparer (gconstpointer base1, gconstpointer base2, gpointer user_data);
static void add_hashtable_keys_to_ptr_array (gpointer key, gpointer value, gpointer user_data);


static gboolean should_process_object (const char *obj_name)
{
	static int has_filter = 0;
	static const char *filter = NULL;

	if (has_filter == 0) {
		has_filter = 1;
		filter = g_getenv ("LOG_TYPE");
	}

	if (filter == NULL)
		return TRUE;
	else
		return (strncmp (filter, obj_name, strlen (filter)) == 0);
}

gpointer g_object_new (GType type, const char *first, ...)
{
	static gpointer (* g_object_new_valist_original) (GType, const char *, va_list) = NULL;

	va_list var_args;
	GObject *obj;
	const char *obj_name;

	if (!g_object_new_valist_original)
		g_object_new_valist_original = find_symbol ("g_object_new_valist");

	/* Only invoke this *after* we have initialized our static data */
	maybe_dump_tracked_objects ();

	va_start (var_args, first);
	obj = g_object_new_valist_original (type, first, var_args);
	va_end (var_args);

	obj_name = G_OBJECT_TYPE_NAME (obj);

	if (should_process_object (obj_name)) {
		G_LOCK (tracker_lock);

		g_object_weak_ref (obj, g_object_finalized, NULL);

		Event *event = event_created_new (obj->ref_count, has_log_backtrace ());
		GPtrArray *array = g_ptr_array_new_with_free_func ((GDestroyNotify) event_destroy);
		g_ptr_array_add (array, event);
		g_hash_table_insert (tracker.objects, obj, array);
		G_UNLOCK (tracker_lock);
	}

	return obj;
}

gpointer g_object_ref (gpointer object)
{
	static gpointer (* g_object_ref_original) (gpointer) = NULL;
	GObject *obj;
	GObject *ret;

	if (g_object_ref_original == NULL)
		g_object_ref_original = find_symbol ("g_object_ref");

	/* Only invoke this *after* we have initialized our static data */
	maybe_dump_tracked_objects ();

	obj = G_OBJECT (object);
	ret = g_object_ref_original (object);

	if (has_log_refs ()) {
		G_LOCK (tracker_lock);

		GPtrArray *events = (GPtrArray *) g_hash_table_lookup (tracker.objects, object);
		if (events)
			g_ptr_array_add (events, event_ref_new (obj->ref_count - 1, obj->ref_count, has_log_backtrace ()));
		
		G_UNLOCK (tracker_lock);
	}

	return ret;
}

void g_object_unref (gpointer object)
{
	static void (* g_object_unref_original) (gpointer) = NULL;
	GObject *obj;

	if (!g_object_unref_original)
		g_object_unref_original = find_symbol ("g_object_unref");

	/* Only invoke this *after* we have initialized our static data */
	maybe_dump_tracked_objects ();

	if (has_log_refs ()) {
		G_LOCK (tracker_lock);

		GPtrArray *events = (GPtrArray *) g_hash_table_lookup (tracker.objects, object);
		if (events) {
			obj = G_OBJECT (object);
			g_ptr_array_add (events, event_unref_new (obj->ref_count, obj->ref_count - 1, has_log_backtrace ()));
		}

		G_UNLOCK (tracker_lock);
	}

	g_object_unref_original (object);
}

static void g_object_finalized (G_GNUC_UNUSED gpointer data, GObject *obj)
{
	G_LOCK (tracker_lock);

	g_hash_table_remove (tracker.objects, obj);

	G_UNLOCK (tracker_lock);
}

typedef void (* mono_dllmap_insert) (void *assembly, const char *dll, const char *func, const char *tdll, const char *tfunc);
void gobject_tracker_init (void *libmono)
{
	if (!g_getenv ("DYLD_FORCE_FLAT_NAMESPACE")) {
		g_print ("\n");
		g_print ("You must export 'DYLD_FORCE_FLAT_NAMESPACE=1' to use the leak profiler.\n");
		g_print ("If you are compiling Xamarin Studio from source use 'make run-leaks' to do this automatically.\n\n");
		exit (1);
	}

	mono_dllmap_insert _mono_dllmap_insert = (mono_dllmap_insert) dlsym (libmono, "mono_dllmap_insert");
	if (_mono_dllmap_insert == NULL) {
		g_print ("Failed to locate the 'mono_dllmap_insert' symbol\n");
		exit (1);
	} else {
		_mono_dllmap_insert (NULL, "libgobject-2.0.0.dylib", "g_object_new", "libgobject-tracker.dylib", "g_object_new");
		_mono_dllmap_insert (NULL, "libgobject-2.0.0.dylib", "g_object_ref", "libgobject-tracker.dylib", "g_object_ref");
		_mono_dllmap_insert (NULL, "libgobject-2.0.0.dylib", "g_object_unref", "libgobject-tracker.dylib", "g_object_unref");

		_mono_dllmap_insert (NULL, "libgobject-2.0-0.dll", "g_object_new", "libgobject-tracker.dylib", "g_object_new");
		_mono_dllmap_insert (NULL, "libgobject-2.0-0.dll", "g_object_ref", "libgobject-tracker.dylib", "g_object_ref");
		_mono_dllmap_insert (NULL, "libgobject-2.0-0.dll", "g_object_unref", "libgobject-tracker.dylib", "g_object_unref");

		g_print ("Successfully added dllmaps\n");
	}
}

static gpointer find_symbol (const char *func_name)
{
	static void *libgobject_handle = NULL;
	void *func;

	G_LOCK (tracker_lock);

	if (libgobject_handle == NULL) {
		libgobject_handle = dlopen ("/Library/Frameworks/Mono.framework/Versions/Current/lib/libgobject-2.0.0.dylib", RTLD_LAZY);
		if (libgobject_handle == NULL)
			g_error ("Failed to open libgobject: %s", dlerror ());

		/* set up objects map */
		tracker.objects = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) g_ptr_array_unref);
		tracker.log_flags = get_log_flags ();

		/* Don't insert outselves into any child processes. */
		g_unsetenv ("DYLD_INSERT_LIBRARIES");

		signal (SIGUSR1, _signal_handler);
	}

	G_UNLOCK (tracker_lock);

	func = dlsym (libgobject_handle, func_name);
	if (func == NULL)
		g_error ("Could not find the symbol '%s': %s", func_name, dlerror ());

	return func;
}

static bool has_log_backtrace ()
{
	return (tracker.log_flags & LOG_BACKTRACE) == LOG_BACKTRACE;
}

static bool has_log_refs ()
{
	return (tracker.log_flags & LOG_REFS) == LOG_REFS;
}

static void dump_tracked_objects ()
{
	gint count;
	GHashTable *count_types;
	GPtrArray *events;
	int i;
	GHashTableIter iter;
	GObject *obj;
	const char *obj_name;
	GPtrArray *top_n_by_type;

	G_LOCK (tracker_lock);

	/* Dump all the individual objects and glob together our types */
	count_types = g_hash_table_new (NULL, NULL);
	top_n_by_type = g_ptr_array_new ();
	g_hash_table_iter_init (&iter, tracker.objects);
	while (g_hash_table_iter_next (&iter, (gpointer) &obj, (gpointer) &events)) {
		if (obj == NULL || obj->ref_count == 0)
			continue;

		obj_name = G_OBJECT_TYPE_NAME (obj);
		g_print ("%s - %p - (refcount: %d):\n", obj_name, obj, obj->ref_count);
		g_ptr_array_foreach (events, print_event, NULL);
		g_print ("\n");

		count = GPOINTER_TO_INT (g_hash_table_lookup (count_types, obj_name)) + 1;
		g_hash_table_insert (count_types, (void *) obj_name, GINT_TO_POINTER (count));
	}

	/* And all the keys to a g_ptr_array so we can sort the types by the number of objects there are alive */
	g_hash_table_foreach (count_types, add_hashtable_keys_to_ptr_array, top_n_by_type);
	g_ptr_array_sort_with_data (top_n_by_type, allocated_instance_comparer, count_types);

	g_print ("Living objects by allocated instances:\n");
	for (i = 0; i < (int) top_n_by_type->len; i++)
		g_print ("\t%d %s\n", GPOINTER_TO_INT (g_hash_table_lookup (count_types, top_n_by_type->pdata [i])), (char *) top_n_by_type->pdata [i]);
	g_print ("\n");

	g_hash_table_destroy (count_types);
	g_ptr_array_unref (top_n_by_type);

	g_print ("Total objects retained: %u\n", g_hash_table_size (tracker.objects));

	G_UNLOCK (tracker_lock);
}

static void maybe_dump_tracked_objects ()
{
	if (should_dump_objects) {
		should_dump_objects = false;
		dump_tracked_objects ();
	}
}

static void _signal_handler (G_GNUC_UNUSED int signal)
{
	should_dump_objects = true;
}

static void add_hashtable_keys_to_ptr_array (gpointer key, gpointer value, gpointer user_data)
{
	GPtrArray *by_type = (GPtrArray*) user_data;
	g_ptr_array_add (by_type, key);
}

static gint allocated_instance_comparer (gconstpointer base1, gconstpointer base2, gpointer user_data)
{
	GHashTable *by_type = (GHashTable *) user_data;
	char *left = *((char **) base1);
	char *right = *((char **) base2);

	int iddiff =  GPOINTER_TO_INT (g_hash_table_lookup (by_type, left)) - GPOINTER_TO_INT (g_hash_table_lookup (by_type, right));

	if (iddiff == 0)
		return 0;
	else if (iddiff < 0)
		return 1;
	else
		return -1;
}
