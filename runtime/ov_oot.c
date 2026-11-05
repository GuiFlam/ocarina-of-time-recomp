// SDK overrides that Ocarina of Time needs and the TotK, FE3H and MP4 hosts do not have. They live here, apart from the
// files taken from metroid-prime-4-recomp (ov_system.c, ov_input.c, ...), so those stay close to their source; an
// override that turns out to be game independent can move into them later. The pattern follows totk-pc-recomp
// runtime/ov_system.c: the SDK function is replaced by symbol (host.h HOST_OVERRIDE), so the service behind it is
// never opened. metroid-prime-4-recomp runtime/ov_mp4.c has the same role for that game; none of its nine entries
// (nn::oe performance configuration and default display resolution, nn::hid gyroscope drift, six-axis rest and touch
// screen at the detail layer) is imported by this game's main or subsdk0 (checked against the NSO import tables,
// 2026-11-05), so this file starts empty, as MP4's did against FE3H's.
//
// Each entry names the call that needs it and the game version it was seen in.
#include <stdio.h>
#include <string.h>

#include "host.h"
#include "ov.h"
