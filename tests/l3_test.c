#include "l3_test.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

/**
 * Support for different 'logging' types under L3's APIs.
 */
static const char * L3_logtype_name[] = {
                          "L3_LOG_unknown"
                        , "L3_LOG_MMAP"
                        , "L3_LOG_FPRINTF"
                        , "L3_LOG_WRITE"
                        , "L3_LOG_WRITE_MSG"
                };

/**
 * l3_test_logtype_name() - Map log-type ID to its name.
 */
static const char *
l3_test_logtype_name(l3_test_log_t logtype)
{
    return (((logtype < 0) || (logtype >= L3_LOGTYPE_MAX))
             ? L3_logtype_name[L3_LOG_UNDEF]
             : L3_logtype_name[logtype]);
}


L3_STATIC_ASSERT((L3_ARRAY_LEN(L3_logtype_name) == L3_LOGTYPE_MAX),
                  "Incorrect size of array L3_logtype_name[]");

static FILE *  l3_test_log_fh = NULL;   // L3_LOG_FPRINTF: Opened by fopen()

static int     l3_log_fd = -1;     // L3_LOG_WRITE: Opened by open()

/**
 * ****************************************************************************
 * Initialize L3's logging sub-system to use write() to named `path`.
 */
static int
l3_init_write(const char *path)
{
    if (!path) {
        fprintf(stderr, "%s: Invalid arg 'path'=%p\n", __func__, path);
        return -1;
    }

    l3_log_fd = open(path, (O_RDWR | O_CREAT | O_TRUNC), 0666);
    if (l3_log_fd == -1) {
        fprintf(stderr, "%s: Error opening log-file at '%p'. Error=%d\n",
                __func__, path, errno);
        return -1;
    }
    printf("Initialized write() logging to '%s'\n", path);
    return 0;
}

/**
 * ****************************************************************************
 * Initialize L3's logging sub-system to use fprintf() to named `path`.
 */
static int
l3_init_fprintf(const char *path)
{
    if (!path) {
        fprintf(stderr, "%s: Invalid arg 'path'=%p\n", __func__, path);
        return -1;
    }

    l3_test_log_fh = fopen(path, "w+");
    if (!l3_test_log_fh) {
        fprintf(stderr, "%s: Error opening log-file at '%p'. errno=%d\n",
                __func__, path, errno);
        perror("fopen failed");
        return -1;
    }
    printf("Initialized fprintf() logging to '%s'\n", path);
    return 0;
}

/**
 * ****************************************************************************
 * l3_test_log_init() - Initialize L3-logging sub-system, selecting the type of
 * logging to be performed.
 * ****************************************************************************
 */
int
l3_test_log_init(const l3_test_log_t logtype, const char *path)
{
    int rv = 0;
    switch (logtype) {
      case L3_LOG_MMAP:         // L3_LOG_DEFAULT:
        rv = l3_init(path);
        break;

      case L3_LOG_FPRINTF:
        rv = l3_init_fprintf(path);
        break;

      case L3_LOG_WRITE:
      case L3_LOG_WRITE_MSG:
        rv = l3_init_write(path);
        break;

      default:
        printf("Unsupported L3-logging type=%d\n", logtype);
        return -1;
    }
    return rv;
}

/**
 * ****************************************************************************
 * l3_test_log_deinit() - De-initialize L3-logging sub-system, selecting the type
 * of logging that was being performed. Finalize the file and close its handle.
 * ****************************************************************************
 */
int
l3_test_log_deinit(const l3_test_log_t logtype)
{
    int rv = 0;
    switch (logtype) {
      case L3_LOG_MMAP:         // L3_LOG_DEFAULT:
        rv = l3_deinit();
        break;

      case L3_LOG_FPRINTF:
        fflush(l3_test_log_fh);
        rv = fclose(l3_test_log_fh);
        break;

      case L3_LOG_WRITE:
#if !defined(__APPLE__)
        syncfs(l3_log_fd);
#endif  // !__APPLE__
        rv = close(l3_log_fd);
        break;

      default:
        printf("Unsupported L3-logging type=%d\n", logtype);
        return -1;
    }
    if (rv) {
        fprintf(stderr, "Error closing L3 log-file for logging type '%s'"
                        ", errno=%d\n",
                l3_test_logtype_name(logtype), errno);
    }
    return rv;
}

/**
 * l3_log_write() - 'C' interface to log L3 log-entries using write()
 * The user's msg is sprintf()'ed using 'msgfmt' format specifiers, requiring
 * 2 arguments.
 */
void
l3_log_write(const char *msgfmt, const uint64_t arg1, const uint64_t arg2)
{
    char msgbuf[255];

    // snprintf() returns #-bytes that _would_ have been written if the msgbuf
    // was large enough.
    int msglen = snprintf(msgbuf, sizeof(msgbuf), msgfmt, arg1, arg2);

    // So, write-to-the-log only the actual data that is in the buffer.
    int datalen = L3_MIN(msglen, sizeof(msgbuf));

    if (write(l3_log_fd, msgbuf, datalen) != datalen) {
        fprintf(stderr, "%s(): write() failed, datalen=%d. errno=%d\n",
                __func__, datalen, errno);
        return;
    }
    return;
}

/**
 * l3_log_fprintf() - Log a msg to a file using fprintf().
 */
void
l3_log_fprintf(const char *msg, const uint64_t arg1, const uint64_t arg2)
{
    fprintf(l3_test_log_fh, msg, arg1, arg2);
}
