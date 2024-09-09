# L3: The Lightweight Logging Library

`printf` debugging often leads to "Heisenbugs" -- the addition of the `printf`
causes the bug to go away or change. This is especially common when trying to
root-cause race conditions. This is because `printf`() is relatively slow,
and usually involves acquiring a lock, so it becomes a kind of
synchronisation point.

The Lightweight Logging Library ([l3](./include/l3.h)) has a fast but limited `printf` API:
the `l3_log()` function takes about 7ns on multi-threaded programs and as low as 1ns for
single-threaded ones (Intel Core i7-1365U).

Both routines are lockless implementations.

## Development Workflow

Simply include `l3.h` in your source file(s) where you wish to invoke the
L3 logging interfaces. Then, link against `l3.c` and `l3.S` (the latter is
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
struct {
    pid_t tid;          // User thread-ID
    int32_t loc;        // (Optional) Line-of-Code ID
    const char *msg;    // Diagnostic message literal
    uint64_t arg1;      // Argument value-1
    uint64_t arg2;      // Argument value-2
};
```

The array is backed by a memory-mapped file, filename given by `l3_init()`.
The logging routines simply do an atomic fetch-and-increment of a
global index to get a slot into the array, and then update that slot
with the calling thread ID, a pointer to a message string, and up to
two 64-bit arguments. (We store just the pointer to the string rather than
string itself because this is usually a lot faster and, of course, consumes
less storage space.)

**The address must be a pointer to a string literal.**

The `l3_dump.py` utility will map the pointer to find the string
literal to which it points from the executable, to generate a human-readable
dump of the log.

------

### Integration with the LOC package

The L3 logging methods are integrated with the Line-of-Code (LOC) decoding
package, to compactly track the exact line-of-code
where the L3 logging interfaces were invoked. This extra
diagnostic feature can be optionally enabled by the `L3_LOC_ENABLED` environment
variable. This environment variable is recognized as part of the `make`
build steps and also when executing the `l3_dump.py` script.

The LOC package provides two independent forms of LOC-encoding
techniques, which are enabled as follows:

1. `L3_LOC_ENABLED=1`: Default LOC-encoding that generates a few source files,
automated by the Python script,
[gen\_loc\_files.py](./LineOfCode/loc/gen_loc_files.py).
Provides `LOC_LINE()` and `LOC_FILE()` interfaces.

2. `L3_LOC_ENABLED=2`: LOC-encoding based on named ELF-sections,
referred to as the LOC-ELF encoding technique.
Dependent files, [loc.h](./LineOfCode/include/loc.h) and
[loc.c](./LineOfCode/src/loc.c), need to be compiled with your
project sources to access this form of encoding.
Additionally, provides the `LOC_FUNC()` interface to name the
function where the L3 logging API was invoked.
**Requires use of gcc compiler.**


------

**NOTE**: A few `Makefile` rules, defined under these environment variables'
check, are needed to incorporate the required LOC-files into your build system.
Check this project's [Makefile](./Makefile) to understand how-to apply
these rules to your project where the L3 / LOC packages will be incorporated.

------

### Sample Outputs from L3-LOC integration

When the LOC diagnostic is enabled, the diagnostic data
that is collected is unpacked to also report the file name
and line number where the L3-log-line was emitted.

A sample output looks like follows:

```
$ L3_LOC_ENABLED=1 ./l3_dump.py /tmp/l3.c-small-test.dat ./build/release/bin/test-use-cases/single-file-C-program

tid=170657 single-file-C-program/test-main.c:85  'Log-msg-Args(1,2)' arg1=1 arg2=2
tid=170657 single-file-C-program/test-main.c:86  'Potential memory overwrite (addr, size)' arg1=3735927486 arg2=1024
tid=170657 single-file-C-program/test-main.c:87  'Invalid buffer handle (addr)' arg1=3203378125 arg2=0
```

On each line, the `single-file-C-program/test-main.c:85` field indicates the
code-location where the log-line was generated.

The LOC-package generates this code-location information at compile-time, so
there is very negligible performance overhead to track this additional data.

------

The technique is simple, and effective. And fast.

## Documentation

See [Documentation](Docs/README.md) for preliminary documentation.

See [Build](Docs/build.md) for instructions on building L3 from source.

See [Test](Docs/test.md) for instructions on how to run L3 tests.
