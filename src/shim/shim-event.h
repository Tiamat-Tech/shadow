#ifndef SHD_SHIM_SHIM_EVENT_H_
#define SHD_SHIM_SHIM_EVENT_H_

// Communication between Shadow and the shim. This is a header-only library
// used in both places.

#include <arpa/inet.h>
#include <stdint.h>
#include <time.h>

#include "../main/host/shd-syscall-types.h"

typedef enum {
    SHD_SHIM_EVENT_NULL = 0,
    SHD_SHIM_EVENT_START = 1,
    SHD_SHIM_EVENT_STOP = 2,
    SHD_SHIM_EVENT_SYSCALL = 3,
    SHD_SHIM_EVENT_SYSCALL_COMPLETE = 4
} ShimEventID;

typedef struct _ShimEvent {
    ShimEventID event_id;

    union {
        struct {
            struct timespec ts;
        } data_nano_sleep;
        int rv; // TODO (rwails) hack, remove me
        struct {
            // We wrap this in the surrounding struct in case there's anything
            // else we end up needing in the message besides the literal struct
            // we're going to pass to the syscall handler.
            SysCallArgs syscall_args;
        } syscall;
        struct {
            SysCallReg retval;
        } syscall_complete;
    } event_data;

} ShimEvent;

void shimevent_recvEvent(int event_fd, ShimEvent* e);
void shimevent_sendEvent(int event_fd, const ShimEvent* e);

#endif // SHD_SHIM_SHIM_EVENT_H_