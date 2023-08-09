#ifndef AUX_RESOURCE_REAP_H
#define AUX_RESOURCE_REAP_H

// Just the reaper def for the malloc wrapper

// Reaping methods (reaper.method):
//   "age[:seconds]": Reap resources older than an amount of seconds (default is 60 seconds).
//   "mem[:percentage]": Reap resources if memory usage goes over a certain percentage (default is 80).
//
// The default method is "mem" on Xbox and "age" otherwise
enum rcreap {
    // Reap unused resources.
    RCREAP_QUICK,
    // If using the "mem" method, reap unused resources in the order of last accessed to first accessed or behave like
    // RCREAP_QUICK otherwise.
    RCREAP_SHALLOW,
    // Reap all the unused resources.
    RCREAP_DEEP,
};

void resourceReaper(enum rcreap level);

#endif
