# Board-side HAL implementation of the xtrack_app interfaces.
#
# The App layer (component `xtrack_app`) declares the HAL interfaces in
# hal/hal_clock.h, hal/hal_gps.h and hal/hal_power.h and never implements them.
# This component provides the ESP32-S3 side:
#
#   hal_clock.c  date/time -- build time as a fallback base + system clock
#   hal_gps.c    simulated circular track (same math as the PC simulator)
#   hal_power.c  simulated battery cycle
#   hal_init.c   hal_init() entry, same contract as the PC simulator
#
# Simulation is deliberate: it lets the App be validated on real hardware
# before the GNSS/PMU capabilities are wired in. Each file marks the swap
# point for the real BSP capability with a TODO(real) note.

idf_component_register(
    SRCS
        "hal_clock.c"
        "hal_gps.c"
        "hal_power.c"
        "hal_init.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        xtrack_app
    PRIV_REQUIRES
        esp_timer
    # The App component only sees the HAL interfaces and links against these
    # implementations indirectly, so the linker has no reason to pull this
    # archive in on its own (IDF orders it before xtrack_app). WHOLE_ARCHIVE
    # forces every object in, which is what an implementation-only component
    # needs.
    WHOLE_ARCHIVE
)
