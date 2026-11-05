// SDK overrides that Ocarina of Time needs and the TotK and FE3H hosts do not have. They live here, apart from the
// files taken from fe3h-pc-recomp (ov_system.c, ov_input.c, ...), so those stay close to their source; an override
// that turns out to be game independent can move into them later. The pattern follows totk-pc-recomp
// runtime/ov_system.c: the SDK function is replaced by symbol (host.h HOST_OVERRIDE), so the service behind it is
// never opened. fe3h-pc-recomp runtime/ov_fe3h.c has the same role for that game and the examples (offline nifm and
// socket stubs, older account calls); none of its entries applies here, as this game's main imports no network
// namespace (docs/plan.md, Phase 0).
//
// Each entry names the call that needs it and the game version it was seen in.
#include <stdio.h>
#include <string.h>

#include "host.h"
#include "ov.h"

// ==== nn::oe: the application environment calls this game makes that ov_system.c lacks. nnMain (main+0x97c618) asks
// for the performance configuration right after requesting it, and ov_system.c's Initialize never opens apm, so the
// lifted call read a null session.
// PerformanceConfiguration GetPerformanceConfiguration(PerformanceMode mode): the configuration last requested for
// that mode (ov_system.c SetPerformanceConfiguration), or the SDK's default before any request.
static void ov_get_performance_config(Cpu *c) {
    int64_t mode = (int32_t)arg_x(c, 0);
    if (mode < 0 || mode > 1) rt_fatal("nn::oe::GetPerformanceConfiguration: invalid mode %lld", (long long)mode);
    ret_x(c, ov_oe_perf_config[mode]);
}

// void GetDefaultDisplayResolution(int *width, int *height): the console's output size, 1080p docked and 720p handheld
// (OOT_OPERATION_MODE); the game sizes its swapchain from it.
static void ov_default_display_resolution(Cpu *c) {
    int32_t *w = arg_p(c, 0), *h = arg_p(c, 1);
    int handheld = ov_handheld();
    if (w) *w = handheld ? 1280 : 1920;
    if (h) *h = handheld ? 720 : 1080;
}

// void GetDefaultDisplayResolutionChangeEvent(os::SystemEvent *event): the event the applet manager signals when the
// output size changes; the SystemEvent class holds its SystemEventType as its first member, and the host binds one
// that never fires, as ov_system.c does for nfp's availability event.
static void ov_default_display_resolution_event(Cpu *c) {
    uint64_t ev = arg_x(c, 0);
    if (!ev) rt_fatal("nn::oe::GetDefaultDisplayResolutionChangeEvent: null event");
    kern_bind_system_event(c, ev, 0);
}

HOST_OVERRIDE("_ZN2nn2oe27GetPerformanceConfigurationENS0_15PerformanceModeE", ov_get_performance_config);
HOST_OVERRIDE("_ZN2nn2oe27GetDefaultDisplayResolutionEPiS1_", ov_default_display_resolution);
HOST_OVERRIDE("_ZN2nn2oe38GetDefaultDisplayResolutionChangeEventEPNS_2os11SystemEventE", ov_default_display_resolution_event);

// ==== nn::hid: calls this game makes that ov_input.c lacks, replaced at the SDK's detail layer as ov_input.c does
// (the public wrappers run lifted and call these). The game sets the gyroscope drift mode of its six-axis sensor at
// start-up (main+0xaab920), which opened the hid service, and asks whether the sensor is at rest; it initializes the
// touch screen too, which the host has no device for.
static void ov_result_ok(Cpu *c) { ret_x(c, 0); }   // Result SetGyroscopeZeroDriftMode(handle, mode), ResetGyroscopeZeroDriftMode(handle), InitializeTouchScreen(int, int)

// Result GetGyroscopeZeroDriftMode(GyroscopeZeroDriftMode *out, SixAxisSensorHandle): Standard (1), the default
static void ov_gyro_drift_mode(Cpu *c) {
    int32_t *out = arg_p(c, 0);
    if (out) *out = 1;
    ret_x(c, 0);
}

// Result IsSixAxisSensorAtRest(bool *out, const SixAxisSensorHandle &): a gamepad on a desk, or none, is at rest
static void ov_six_axis_at_rest(Cpu *c) {
    uint8_t *out = arg_p(c, 0);
    if (out) *out = 1;
    ret_x(c, 0);
}

// Result GetTouchScreenStates<16>(int *count, TouchScreenState<16> *out, int max): no touch screen, so no states;
// the public template returns the count without reading a state.
static void ov_touch_states(Cpu *c) {
    int32_t *count = arg_p(c, 0);
    if (count) *count = 0;
    ret_x(c, 0);
}

HOST_OVERRIDE("_ZN2nn3hid6detail25SetGyroscopeZeroDriftModeENS0_19SixAxisSensorHandleENS0_22GyroscopeZeroDriftModeE", ov_result_ok);
HOST_OVERRIDE("_ZN2nn3hid6detail27ResetGyroscopeZeroDriftModeENS0_19SixAxisSensorHandleE", ov_result_ok);
HOST_OVERRIDE("_ZN2nn3hid6detail25GetGyroscopeZeroDriftModeEPNS0_22GyroscopeZeroDriftModeENS0_19SixAxisSensorHandleE", ov_gyro_drift_mode);
HOST_OVERRIDE("_ZN2nn3hid6detail21IsSixAxisSensorAtRestEPbRKNS0_19SixAxisSensorHandleE", ov_six_axis_at_rest);
HOST_OVERRIDE("_ZN2nn3hid6detail21InitializeTouchScreenEii", ov_result_ok);
HOST_OVERRIDE("_ZN2nn3hid6detail20GetTouchScreenStatesILm16EEENS_6ResultEPiPNS0_16TouchScreenStateIXT_EEEi", ov_touch_states);

