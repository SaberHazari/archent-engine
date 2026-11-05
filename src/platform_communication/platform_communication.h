#ifndef PLATFORM_COMMUNICATION_H
#define PLATFORM_COMMUNICATION_H

#include "archent_utils.h"

typedef struct ArchentState {
    bool running;
} ArchentState;

extern ArchentState archent_state;

#endif