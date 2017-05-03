Compilation:

Just run `make` inside the main directory.

Usage:

Export these two environment variables and then launch Xamarin Studio:

	
	DYLD_FORCE_FLAT_NAMESPACE=1
	DYLD_INSERT_LIBRARIES=/full/path/to/gobject-tracker/libgobject-tracker.dylib
	

Send the `USR1` signal to your running Xamarin Studio instance to queue a
dump of all living objects.

	$ kill -USR1 $pid_of_xamarin_studio

You will have to trigger the creation, ref or unref of a gtk object in order
to perform the queued object dump. It is not safe to attempt to perform the
dump inside a signal handler. You can trigger this by just mousing over
Xamarin Studio.

Additional features:

By default all object creation/ref/unref operations will be logged. If you wish
to gather stacktraces for each event just export this environment variable:

	LOG_FLAGS=all

If you wish to gather data for just one object type export something like this:

	LOG_TYPE=GtkFrame
