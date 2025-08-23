#include "log.c"
#include "utils.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static int errors = 0;

/*
 * Test the extended time format.
 */
static int test_time_format(void)
{
    struct tm tm_local = { 0 };
    struct timespec ts = { 0 };

    log_get_time(&ts);

    gmtime_r(&ts.tv_sec, &tm_local);

    Buffer buf = { 0 };

    // Check a general string with some standard strftime specifiers.

    log_add_time(&buf, &tm_local, &ts, "%Y-%m-%d %H:%M:%3S: bladibla");
    check_string(bufGet(&buf), "1970-01-01 12:34:56.988: bladibla");

    bufClear(&buf);

    // Check rounding

    log_add_time(&buf, &tm_local, &ts, "%6S");
    check_string(bufGet(&buf), "56.987654");

    bufClear(&buf);

    // Check default format

    log_add_time(&buf, &tm_local, &ts, "%S");
    check_string(bufGet(&buf), "56");

    bufClear(&buf);

    // Check no digits at all

    log_add_time(&buf, &tm_local, &ts, "%0S");
    check_string(bufGet(&buf), "57");

    bufClear(&buf);

    // Check multiple specifiers

    log_add_time(&buf, &tm_local, &ts, "%3S/%6S");
    check_string(bufGet(&buf), "56.988/56.987654");

    bufClear(&buf);

    return errors;
}

#define FILE_WRITER_TEST_FILE "/tmp/test_log_to_file.log"
#define FP_WRITER_TEST_FILE "/tmp/test_log_to_fp.log"
#define FD_WRITER_TEST_FILE "/tmp/test_log_to_fd.log"

#define check_file(filename, text) \
    _check_file(__FILE__, __LINE__, filename, text)

/*
 * Check that the file with <filename> contains exactly <text>. Complain and
 * return 1 if it doesn't, otherwise return 0.
 */
static int _check_file(const char *src, int line,
                      const char *filename, const char *text)
{
    struct stat statbuf;
    int fd;
    char *content;

    if ((fd = open(filename, O_RDONLY)) == -1) {
        fprintf(stderr,
                "%s:%d: Could not open file \"%s\" (%s).\n",
                src, line, filename, strerror(errno));
        return 1;
    }
    else if (fstat(fd, &statbuf) == -1) {
        fprintf(stderr,
                "%s:%d: Could not stat file \"%s\" (%s).\n",
                src, line, filename, strerror(errno));
        return 1;
    }
    else if ((content = calloc(1, statbuf.st_size + 1)) == NULL) {
        fprintf(stderr,
                "%s:%d: Could not allocate memory for file \"%s\" (%s).\n",
                src, line, filename, strerror(errno));
        return 1;
    }
    else if (read(fd, content, statbuf.st_size) != statbuf.st_size) {
        fprintf(stderr,
                "%s:%d: Could not read file \"%s\" (%s).\n",
                src, line, filename, strerror(errno));
        return 1;
    }

    int r = strncmp(content, text, statbuf.st_size);

    if (r != 0) {
        fprintf(stderr, "%s:%d: File \"%s\" does not match expectation.\n",
                src, line, filename);
        fprintf(stderr, "Expected:\n");
        fputs(text, stderr);
        fprintf(stderr, "Actual:\n");
        fputs(content, stderr);
    }

    free(content);

    return r == 0 ? 0 : 1;
}

int main(void)
{
    FILE *fp = NULL;
    int   fd = -1;

    LogWriter *file_writer;
    LogWriter *fp_writer;
    LogWriter *fd_writer;
    LogWriter *buf_writer;

    // First test the extended time format.
    errors += test_time_format();

    // Open a writer to file FILE_WRITER_TEST_FILE...
    if ((file_writer = logFileWriter("%s", FILE_WRITER_TEST_FILE)) == NULL) {
        fprintf(stderr, "Couldn't open %s.\n", FILE_WRITER_TEST_FILE);
        errors++;
        goto quit;
    }

    // Set some attributes for this writer...
    logWithUniversalTime(file_writer, "%Y-%m-%d/%H:%M:%6S");
    logWithString(file_writer, " FILE ");
    logWithFunction(file_writer);
    logWithString(file_writer, "@");
    logWithFile(file_writer);
    logWithString(file_writer, ":");
    logWithLine(file_writer);
    logWithThreadId(file_writer);

    // Connect channel CH_ERR to it...
    logConnect(CH_ERR, file_writer);

    // And write a log message to CH_ERR.
    _logWrite(CH_ERR, true, "log.c", 1, "func", "This is an error.\n");

    // Check that the correct entry was made in the log file.
    errors += check_file(FILE_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.987654 FILE func@log.c:1 12345 "
            "This is an error.\n");

    // Open a file, and create a writer to the associated FILE pointer.
    if ((fp = fopen(FP_WRITER_TEST_FILE, "w")) == NULL) {
        fprintf(stderr, "Couldn't open %s.\n", FP_WRITER_TEST_FILE);
        errors++;
        goto quit;
    }
    else if ((fp_writer = logFPWriter(fp)) == NULL) {
        fprintf(stderr, "logFPWriter returned NULL.\n");
        errors++;
        goto quit;
    }

    // Attributes...
    logWithUniversalTime(fp_writer, "%Y-%m-%d/%H:%M:%6S");
    logWithString(fp_writer, "\tFP\t");
    logWithFunction(fp_writer);
    logWithString(fp_writer, "@");
    logWithFile(fp_writer);
    logWithString(fp_writer, ":");
    logWithLine(fp_writer);
    logWithSeparator(fp_writer, "\t");

    // Connect CH_INFO...
    logConnect(CH_INFO, fp_writer);

    // And write a message.
    _logWrite(CH_INFO, true, "log.c", 2, "func", "This is an info message.\n");

    // Make sure the correct message was written, and that the earlier log file
    // hasn't changed.
    errors += check_file(FP_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.987654\tFP\tfunc@log.c:2\t"
            "This is an info message.\n");

    errors += check_file(FILE_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.987654 FILE func@log.c:1 12345 "
            "This is an error.\n");

    // Same thing for a writer to a file descriptor.
    if ((fd = creat(FD_WRITER_TEST_FILE, 0666)) == -1) {
        fprintf(stderr, "Couldn't open %s.\n", FD_WRITER_TEST_FILE);
        errors++;
        goto quit;
    }
    else if ((fd_writer = logFDWriter(fd)) == NULL) {
        fprintf(stderr, "logFDWriter returned NULL.\n");
        errors++;
        goto quit;
    }

    logWithUniversalTime(fd_writer, "%Y-%m-%d/%H:%M:%6S");
    logWithString(fd_writer, ",FD,");
    logWithFunction(fd_writer);
    logWithString(fd_writer, "@");
    logWithFile(fd_writer);
    logWithString(fd_writer, ":");
    logWithLine(fd_writer);
    logWithSeparator(fd_writer, ",");

    logConnect(CH_DEBUG, fd_writer);

    _logWrite(CH_DEBUG, true, "log.c", 3, "func", "This is a debug message.\n");

    // Again, check the log file we just made and the earlier ones.
    errors += check_file(FD_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.987654,FD,func@log.c:3,"
            "This is a debug message.\n");

    errors += check_file(FP_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.987654\tFP\tfunc@log.c:2\t"
            "This is an info message.\n");

    errors += check_file(FILE_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.987654 FILE func@log.c:1 12345 "
            "This is an error.\n");

    // Finally create a writer to a Buffer.
    Buffer log_buffer = { 0 };

    if ((buf_writer = logBufferWriter(&log_buffer)) == NULL) {
        fprintf(stderr, "logBufferWriter returned NULL.\n");
        errors++;
        goto quit;
    }

    // We'll keep this one simple. Just a prefix.
    logWithString(buf_writer, "DEBUG: ");

    // Connect it to CH_DEBUG, which means the CH_DEBUG channel will now write
    // to this Buffer writer and also to the earlier FD writer!
    logConnect(CH_DEBUG, buf_writer);

    // Write a message to CH_DEBUG...
    _logWrite(CH_DEBUG, true, "log.c", 4, "func",
            "This is another debug message.\n");

    // Make sure it ended up in the log buffer...
    errors += check_string(bufGet(&log_buffer),
            "DEBUG: This is another debug message.\n");

    // And also in the FD writer.
    errors += check_file(FD_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.987654,FD,func@log.c:3,"
            "This is a debug message.\n"
            "1970-01-01/12:34:56.987654,FD,func@log.c:4,"
            "This is another debug message.\n");

    // And make sure the others haven't changed.
    errors += check_file(FP_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.987654\tFP\tfunc@log.c:2\t"
            "This is an info message.\n");

    errors += check_file(FILE_WRITER_TEST_FILE,
            "1970-01-01/12:34:56.987654 FILE func@log.c:1 12345 "
            "This is an error.\n");

quit:
    logReset();

    if (fp != NULL) { fclose(fp); fp = NULL; }
    if (fd >= 0)    { close(fd);  fd = -1; }

    if (access(FILE_WRITER_TEST_FILE, F_OK) == 0) {
        unlink(FILE_WRITER_TEST_FILE);
    }

    if (access(FP_WRITER_TEST_FILE, F_OK) == 0) {
        unlink(FP_WRITER_TEST_FILE);
    }

    if (access(FD_WRITER_TEST_FILE, F_OK) == 0) {
        unlink(FD_WRITER_TEST_FILE);
    }

    return errors;
}
