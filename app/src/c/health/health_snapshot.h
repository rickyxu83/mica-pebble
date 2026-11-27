/*
 * Copyright 2026 for Mica.
 * Licensed under the Apache License, Version 2.0.
 */

#ifndef HEALTH_SNAPSHOT_H
#define HEALTH_SNAPSHOT_H

// Registers an AppMessage handler that answers HEALTH_REQUEST messages
// from the phone with a snapshot of the watch's local HealthService data
// (steps, sleep, heart rate, activity, recent workouts) for the AI coach.
// Metrics the watch cannot provide are reported as -1, never invented.
void health_snapshot_init(void);

#endif
