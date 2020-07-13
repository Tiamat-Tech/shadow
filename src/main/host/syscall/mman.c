/*
 * The Shadow Simulator
 * See LICENSE for licensing information
 */

#include "main/host/syscall/mman.h"

#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#include "main/host/descriptor/descriptor.h"
#include "main/host/descriptor/file.h"
#include "main/host/process.h"
#include "main/host/syscall/protected.h"
#include "main/host/thread.h"
#include "main/utility/syscall.h"
#include "support/logger/logger.h"

///////////////////////////////////////////////////////////
// Helpers
///////////////////////////////////////////////////////////

static int _syscallhandler_validateMmapArgsHelper(SysCallHandler* sys, int fd,
                                                  size_t len, int prot,
                                                  int flags,
                                                  File** file_desc_out) {
    /* At least one of these values is required according to man page. */
#ifndef MAP_SHARED_VALIDATE
#define MAP_SHARED_VALIDATE 0x03
#endif
    int reqFlags = (MAP_PRIVATE | MAP_SHARED | MAP_SHARED_VALIDATE);
    int reqProt = (PROT_NONE | PROT_READ | PROT_WRITE | PROT_EXEC);

    /* Need non-zero len, and at least one of the above options. */
    if (len == 0 || !(flags & reqFlags) || !(prot & reqProt)) {
        info("Invalid len (%zu), prot (%i), or flags (%i)", len, prot, flags);
        return -EINVAL;
    }

    /* We ignore the fd on anonymous mappings, otherwise it must refer to a
     * regular file. */
    if (fd <= 2 && !(flags & MAP_ANONYMOUS)) {
        info("Invalid fd %i and MAP_ANONYMOUS is not set in flags %i", fd,
             flags);
        return -EBADF;
    }

    /* We only need a file if it's not an anonymous mapping. */
    if (!(flags & MAP_ANONYMOUS)) {
        Descriptor* desc = process_getRegisteredDescriptor(sys->process, fd);
        int errcode = _syscallhandler_validateDescriptor(desc, DT_NONE);
        if (errcode) {
            info("Invalid fd %i", fd);
            return errcode;
        }

        if (descriptor_getType(desc) != DT_FILE) {
            info("Descriptor exists for fd %i, but is not a file type", fd);
            return -EACCES;
        }

        /* Success. We know we have a file type descriptor. */
        if (file_desc_out) {
            *file_desc_out = (File*)desc;
        }
    }

    return 0;
}

static int _syscallhandler_openPluginFile(SysCallHandler* sys, File* file) {
    utility_assert(file);

    int fd = descriptor_getHandle((Descriptor*)file);

    debug("Trying to open file %i in the plugin", fd);

    /* TODO: make sure we don't open special files like /dev/urandom,
     * /etc/localtime etc. in the plugin via mmap */

    /* file is in the shadow process, and we want to open it in the plugin. */
    char* abspath = file_getAbsolutePath(file);
    if (abspath == NULL) {
        debug("File %i has a NULL path.", fd);
        return -1;
    }

    /* We need enough mem for the string, but no more than PATH_MAX. */
    size_t maplen = strnlen(abspath, PATH_MAX - 1) + 1; // an extra 1 for null
    utility_assert(maplen > 1);

    debug("Opening path '%s' in plugin.", abspath);

    /* Get some memory in the plugin to write the path of the file to open. */
    PluginPtr pluginBufPtr = thread_mallocPluginPtr(sys->thread, maplen);

    /* Get a writeable pointer that can be flushed to the plugin. */
    char* pluginBuf = thread_getWriteablePtr(sys->thread, pluginBufPtr, maplen);

    /* Copy the path. */
    snprintf(pluginBuf, maplen, "%s", abspath);

    /* Flush the buffer to the plugin. */
    thread_flushPtrs(sys->thread);

    /* Get original flags when used to open the file,
       but be careful not to try re-creating or truncating it. */
    int flags = file_getFlags(file) & ~(O_CREAT|O_EXCL|O_TMPFILE|O_TRUNC);

    /* Instruct the plugin to open the file at the path we sent. */
    int result = thread_nativeSyscall(sys->thread, SYS_open, pluginBufPtr.val,
                                      flags, file_getMode(file));
    int err = syscall_rawReturnValueToErrno(result);
    if (err) {
        debug("Failed to open path '%s' in plugin, error %i: %s.", abspath, err, strerror(err));
    } else {
        debug("Successfully opened path '%s' in plugin, got plugin fd %i.",
              abspath, result);
    }

    /* Release the PluginPtr memory. */
    thread_freePluginPtr(sys->thread, pluginBufPtr, maplen);

    return result;
}

static void _syscallhandler_closePluginFile(SysCallHandler* sys, int pluginFD) {
    /* Instruct the plugin to close the file at given fd. */
    int result = thread_nativeSyscall(sys->thread, SYS_close, pluginFD);
    int err = syscall_rawReturnValueToErrno(result);
    if (err) {
        debug("Failed to close file at fd %i in plugin, error %i: %s.",
              pluginFD, -result, strerror(-result));
    } else {
        debug("Successfully closed file at fd %i in plugin.", pluginFD);
    }
}

///////////////////////////////////////////////////////////
// System Calls
///////////////////////////////////////////////////////////

SysCallReturn syscallhandler_mmap(SysCallHandler* sys,
                                  const SysCallArgs* args) {
    PluginPtr addrPtr = args->args[0].as_ptr; // void*
    size_t len = args->args[1].as_u64;
    int prot = args->args[2].as_i64;
    int flags = args->args[3].as_i64;
    int fd = args->args[4].as_i64;
    off_t offset = args->args[5].as_i64;

    debug("mmap called on fd %d for %zu bytes", fd, len);

    /* First check the input args to see if we can avoid doing the less
     * efficient shadow-plugin cross-process mmap procedure. */
    File* file_desc = NULL;
    int errcode = _syscallhandler_validateMmapArgsHelper(
        sys, fd, len, prot, flags, &file_desc);
    if (errcode) {
        return (SysCallReturn){.state = SYSCALL_DONE, .retval.as_i64 = errcode};
    }

    /* Now file_desc is null for an anonymous mapping, non-null otherwise. */
    int pluginFD = -1;

    if (file_desc) {
        pluginFD = _syscallhandler_openPluginFile(sys, file_desc);
        if (pluginFD < 0) {
            warning("mmap on fd %d for %zu bytes failed.", fd, len);
            return (SysCallReturn){
                .state = SYSCALL_DONE, .retval.as_i64 = -EACCES};
        }
    }

    /* Now we opened the file in the plugin, so we can ask them to mmap the
     * corresponding fd that is valid in the plugin. */
    long result = thread_nativeSyscall(
        sys->thread, SYS_mmap, addrPtr.val, len, prot, flags, pluginFD, offset);

    debug("Plugin-native mmap syscall at plugin addr %p with plugin fd %i for "
          "%zu bytes returned %p (%s)",
          (void*)addrPtr.val, pluginFD, len, (void*)result,
          strerror(syscall_rawReturnValueToErrno(result)));

    /* Close the file we asked them to open. */
    if (pluginFD >= 0) {
        _syscallhandler_closePluginFile(sys, pluginFD);
    }

    /* Done! Return their result back to them. */
    return (SysCallReturn){.state = SYSCALL_DONE, .retval.as_i64 = result};
}