#ifndef MYDBUS_H
#define MYDBUS_H

struct dbus_variables {
    double demod_snr1;
    double demod_snr2;
};

extern struct dbus_variables g_dbusvars;

int dbus_init();
int dbus_join();

#endif
