/*
 * Data fusion bridge header.
 * Integrates thermal data into cmg-cli output.
 */

#ifndef BRIDGE_H
#define BRIDGE_H

#include "config.h"

/* Opaque bridge structure */
typedef struct FuseBridge FuseBridge;

/* Bridge functions */
FuseBridge* bridge_create(ThermalSource *sources, int source_count, char **args, int arg_count, const char *time_format);
int bridge_run(FuseBridge *bridge);
void bridge_free(FuseBridge *bridge);

#endif /* BRIDGE_H */
