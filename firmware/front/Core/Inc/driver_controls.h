#ifndef DRIVER_CONTROLS_H
#define DRIVER_CONTROLS_H

#include "can_messages.h"

/* These setters are the single integration point for future steering-wheel
 * buttons/rotaries. They are intentionally independent from GPIO assignments. */
void DriverControls_Init(void);
void DriverControls_SetTvPercent(uint8_t percent);
void DriverControls_SetRegenPercent(uint8_t percent);
void DriverControls_SetMode(DjyDriveMode mode);
void DriverControls_SetTvEnabled(bool enabled);
void DriverControls_SetRegenEnabled(bool enabled);
DjyDriverControl DriverControls_NextMessage(void);
DjyDriverControl DriverControls_GetCurrent(void);

#endif /* DRIVER_CONTROLS_H */
