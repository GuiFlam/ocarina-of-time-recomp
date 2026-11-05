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

// ==== nn::oe (SDK 22): applet-environment calls this game makes that ov_system.c lacks. ov_system.c's Initialize never
// opens the applet manager session, so a lifted nn::oe call that reads it faults on a null pointer.
// void SetGraphicsScaleChangedNotificationEnabled(bool): asks the system to signal when the output's graphics scale
// changes (a Switch 2 notion); nnMain calls it first thing (main+0x1202d90, 1.1.1). The host never changes scale.
static void ov_void(Cpu *c) { (void)c; }

HOST_OVERRIDE("_ZN2nn2oe42SetGraphicsScaleChangedNotificationEnabledEb", ov_void);

// GraphicsScale GetCurrentGraphicsScale(): the output's graphics scale as an enum; the SDK maps the applet manager's
// reply 10/20/30 to 0/1/2 (IGraphicsScaleController command 10) and aborts on any other value. Lichii's controller
// answers 10 (its graphics_scale_controller.h, GPL-3.0, read for behaviour), so 0 here too; nnMain stores it beside the
// operation and performance modes (main+0xe376dc, 1.1.1). What the three scales mean on hardware is not known yet.
static void ov_current_graphics_scale(Cpu *c) { ret_x(c, 0); }

// void GetDisplayVersion(DisplayVersion *out): the 16-byte version string of the running application ("1.1.1").
static void ov_display_version(Cpu *c) {
    char *out = arg_p(c, 0);
    if (!out) rt_fatal("nn::oe::GetDisplayVersion: null out");
    memset(out, 0, 16);
    snprintf(out, 16, "%d.%d.%d", rt_game_version / 100, rt_game_version / 10 % 10, rt_game_version % 10);
}

HOST_OVERRIDE("_ZN2nn2oe23GetCurrentGraphicsScaleEv", ov_current_graphics_scale);
HOST_OVERRIDE("_ZN2nn2oe17GetDisplayVersionEPNS0_14DisplayVersionE", ov_display_version);

// ==== nn::vi (SDK 22): display capability queries and layer metadata setters ov_gfx.c lacks, which this game's
// graphics bring-up makes. ov_gfx.c's Initialize never opens the vi service, so the lifted calls would fault; the
// host window has one fixed-rate SDR output, so the queries answer "not supported" and the setters succeed.
// Result QueryDisplayHdrStatus(bool *enabled, HdrOutputFormat *format, uint16_t *unknown, const Display *)
static void ov_vi_hdr_status(Cpu *c) {
    uint8_t *enabled = arg_p(c, 0); int32_t *format = arg_p(c, 1); uint16_t *u = arg_p(c, 2);
    if (enabled) *enabled = 0;
    if (format) *format = 0;
    if (u) *u = 0;
    ret_x(c, 0);
}
// Result QueryDisplayVrrSupported(bool *out, const Display *): no variable refresh rate
static void ov_vi_bool_false(Cpu *c) {
    uint8_t *out = arg_p(c, 0);
    if (out) *out = 0;
    ret_x(c, 0);
}
// Result QueryDisplayHdrPaperWhite(uint16_t *nits, const Display *): SDR reference white
static void ov_vi_paper_white(Cpu *c) {
    uint16_t *out = arg_p(c, 0);
    if (out) *out = 203;
    ret_x(c, 0);
}
// Result SetDisplayVrrCapability(Display *, bool), SetDisplay120HzRefreshRateCapability(Display *, bool),
// SetLayerExtendedSwapInterval(Layer *, int, int), ClearLayerExtendedSwapInterval(Layer *), and the four
// SetLayerNextHdr*Metadata(Layer *, ...): accepted and ignored; the present (gfx_window.c) owns the output's rate and
// format.
static void ov_vi_result_ok(Cpu *c) { ret_x(c, 0); }

HOST_OVERRIDE("_ZN2nn2vi21QueryDisplayHdrStatusEPbPNS0_15HdrOutputFormatEPtPKNS0_7DisplayE", ov_vi_hdr_status);
HOST_OVERRIDE("_ZN2nn2vi24QueryDisplayVrrSupportedEPbPKNS0_7DisplayE", ov_vi_bool_false);
HOST_OVERRIDE("_ZN2nn2vi25QueryDisplayHdrPaperWhiteEPtPKNS0_7DisplayE", ov_vi_paper_white);
HOST_OVERRIDE("_ZN2nn2vi23SetDisplayVrrCapabilityEPNS0_7DisplayEb", ov_vi_result_ok);
HOST_OVERRIDE("_ZN2nn2vi36SetDisplay120HzRefreshRateCapabilityEPNS0_7DisplayEb", ov_vi_result_ok);
HOST_OVERRIDE("_ZN2nn2vi28SetLayerExtendedSwapIntervalEPNS0_5LayerEii", ov_vi_result_ok);
HOST_OVERRIDE("_ZN2nn2vi30ClearLayerExtendedSwapIntervalEPNS0_5LayerE", ov_vi_result_ok);
HOST_OVERRIDE("_ZN2nn2vi35SetLayerNextHdrContentGamutMetadataEPNS0_5LayerENS0_15HdrContentGamutE", ov_vi_result_ok);
HOST_OVERRIDE("_ZN2nn2vi39SetLayerNextHdrToSdrToneMappingMetadataEPNS0_5LayerENS0_14HdrToneMappingE", ov_vi_result_ok);
HOST_OVERRIDE("_ZN2nn2vi52SetLayerNextHdrExtendedContentLuminanceRangeMetadataEPNS0_5LayerEtt", ov_vi_result_ok);
HOST_OVERRIDE("_ZN2nn2vi52SetLayerNextHdrStandardContentLuminanceRangeMetadataEPNS0_5LayerEtt", ov_vi_result_ok);
