#include <stdio.h>
#include <stdlib.h>
#include <dbus/dbus.h>
#include <pthread.h>
#include <string.h>

#include "mydbus.h"

#define READSB_NAME  "io.github.readsb"
#define READSB_OPATH "/io/github/readsb"
#define READSB_IFACE "io.github.readsb"
#define READSB_NOTE  "readsb"

#define _6dB 1.9952623149688795
#define _5dB 1.7782794100389228
#define _4dB 1.5848931924611136
#define _3_5dB 1.4962356560944334
#define _3dB 1.4125375446227544
#define _2_5dB 1.333521432163324
#define _2dB 1.2589254117941673
#define _1dB 1.1220184543019633

struct dbus_variables g_dbusvars;

static DBusConnection* bus = NULL;
static DBusMessage *msg = NULL;
static int quitting = 0;
static pthread_mutex_t dbusThreadMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t dbusThreadCond = PTHREAD_COND_INITIALIZER;
static pthread_t dbusThread;
//static const char * mystruct = "(sd)";

static void *dbusThreadEntryPoint(void *arg) {
    (void)arg;
    DBusError err;
    DBusMessageIter args;

    dbus_error_init(&err);
    bus = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Bus Error (%s)\n", err.message);
        return (void*)1;
    }

    dbus_bus_add_match(bus, 
         "type='signal',path='/io/github/readsb'",
         NULL); // see signals from the given interface
    dbus_connection_flush(bus);
    if (dbus_error_is_set(&err)) { 
        fprintf(stderr, "Match Error (%s)\n", err.message);
        return (void*)2;
    }

    const struct timespec s = {
        .tv_sec = 0,
        .tv_nsec = 100000000, // 100 ms
    };

    pthread_mutex_lock(&dbusThreadMutex);
    while (!quitting) {
        dbus_connection_read_write(bus, 0);
        msg = dbus_connection_pop_message(bus);
        if (msg != NULL) {
            pthread_mutex_unlock(&dbusThreadMutex);
            char* sparam = "";
            double dparam = 0.0;
            if (dbus_message_is_signal(msg, "io.github.readsb", "SetVar")) {
                // read the parameters
                if (!dbus_message_iter_init(msg, &args)) {
                    fprintf(stderr, "Message has no arguments!\n"); 
                } else {
                    dbus_message_iter_get_basic(&args, &sparam);
                    if (!strncmp(sparam, "snr1", 4)) {
                        dbus_message_iter_get_basic(&args, &dparam);
                        g_dbusvars.demod_snr1 = dparam;
                    } else if (!strncmp(sparam, "snr2", 4)) {
                        dbus_message_iter_get_basic(&args, &dparam);
                        g_dbusvars.demod_snr2 = dparam;
                    } else {
                        fprintf(stderr, "Dont know key %s\n", sparam);
                    }
                        /*if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args)) 
                        fprintf(stderr, "Argument is not string!\n"); 
                    else {
                        dbus_message_iter_get_basic(&args, &sigvalue);
                        printf("Got Signal with value %s\n", sigvalue);
                    }*/
                }
             }

            else if (dbus_message_is_signal(msg, "io.github.readsb", "GetVar")) {
                char buf[1024];
                snprintf(buf, sizeof(buf), "snr1=%f snr2=%f", g_dbusvars.demod_snr1, g_dbusvars.demod_snr2);
                DBusMessage *reply = dbus_message_new_signal("/io/github/readsb", "io.github.readsb.Status", buf);
                dbus_connection_send(bus, reply, NULL);
                dbus_connection_flush(bus);
                dbus_message_unref(reply);
            }
            // free the message
            dbus_message_unref(msg);
        } else {
            pthread_cond_timedwait(&dbusThreadCond, &dbusThreadMutex, &s);
        }
    }
    pthread_mutex_unlock(&dbusThreadMutex);
    dbus_connection_flush(bus);
    return NULL;
}

int dbus_init() {
    g_dbusvars.demod_snr1 = _3dB;
    g_dbusvars.demod_snr2 = _5dB;
    return pthread_create(&dbusThread, NULL, dbusThreadEntryPoint, NULL);
}

int dbus_join() {
    void *ret;
    pthread_mutex_lock(&dbusThreadMutex);
    quitting = 1;
    pthread_cond_signal(&dbusThreadCond);
    pthread_mutex_unlock(&dbusThreadMutex);
    return pthread_join(dbusThread, &ret);
}
