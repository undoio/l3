# L3: The Lightweight Logging Library

`printf` debugging often leads to "Heisenbugs" -- the addition of the `printf`
causes the bug to go away or change. This is especially common when trying to
root-cause race conditions. This is because `printf`() is relatively slow,
and usually involves acquiring a lock, so it becomes a kind of
synchronisation point.

The Lightweight Logging Library (l3) has a fast but limited `printf` API:
- `l3_log_simple()`: Takes about 10ns (Intel Core i7-1365U)
- `l3_log_fast()`: An even faster, but more limited interface, which takes about 7ns.

Both routines are lockless implementations.

## Development Workflow

Simply include `l3.h` and link against `l3.c` and `l3.S` (the latter is
an assembly file currently only available for x86-64 ). Then call
`l3_init()` to configure the filename to which the logs will be written.

To extract the trace debugging strings from the configured log-file name,
the logs need to be post-processed using the `l3_dump.py` Python
script.
Do this by running `l3_dump.py`, passing the logfile
and the executable as its command-line arguments. E.g.,

```
$ l3_dump.py mylog a.out
tid=923512 'Potential memory overwrite' arg1=1 arg2=0
tid=923514 'Invalid buffer handle' arg1=2 arg2=0
```

The implementation is very simple. The log is a circular ring-buffer of structs:

```
    struct {pid_t tid; const char *msg; uint64_t arg1, uint64_t arg2};
```

The array is backed by a memory-mapped file, filename given by `l3_init()`.
The logging routines simply do an atomic fetch-and-increment of a
global index to get a slot into the array, and then update that slot
with the calling thread ID, a pointer to a message string, and up to
two 64-bit arguments. (We store just the pointer to the string rather than
string itself because this is usually a lot faster and, of course, consumes
less storage space.)  The pointer should be to a string literal.

The `l3_dump.py` utility will map the pointer to find the string
literal to which it points from the executable, to generate a human-readable
dump of the log.

The technique is simple, but effective. And fast.

## Documentation

See [Documentation](Docs/README.md) for preliminary documentation.

See [Build](Docs/build.md) for instructions on building L3 from source.
