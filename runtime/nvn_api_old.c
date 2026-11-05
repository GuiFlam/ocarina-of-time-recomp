// From metroid-prime-4-recomp runtime/nvn_api_old.c @ 586e91e1b6f38c9e512e39f2d47a046fdf273cf0 (MIT, Copyright (c) 2026 Jeff Aigner), copied unchanged.
// From fe3h-pc-recomp runtime/nvn_api_old.c @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c) 2026 Jeff Aigner), copied unchanged.
// NVN entry points of SDKs older than TotK's that FE3H (SDK 6.5.0) uses, on top of the front end taken from
// totk-pc-recomp (nvn_api_*.c). The names are in nvn_procs.inc (tools/gen_nvn_procs.py).
#include "nvn_api.h"

void nvnp_nvnWindowAcquireTexture(Cpu *c);   // nvn_api_obj.c

// NVNqueueAcquireTextureResult nvnQueueAcquireTexture(NVNqueue *q, NVNwindow *w, int *index): the older way to take
// the next window image, without a sync. It waits as nvnWindowAcquireTexture does (until the image's previous present
// plus one period); the results share their values (0 = success).
NVN_API(nvnQueueAcquireTexture) {
    c->x[0] = c->x[1];   // nvnWindowAcquireTexture(NVNwindow *w, NVNsync *s, int *index)
    c->x[1] = 0;
    nvnp_nvnWindowAcquireTexture(c);
}

// void nvnQueueWaitSync(NVNqueue *q, const NVNsync *s): the queue's later work waits for s on the GPU. As with
// nvnCommandBufferWaitSync (nvn_api_cmd.c), every submit replays in order on one host queue, and window syncs are
// satisfied when the acquire returns, so there is nothing to order.
NVN_API(nvnQueueWaitSync) { (void)c; }

// void nvnDeviceApplyDeferredFinalizes(NVNdevice *d, int age): the front end finalizes objects when the game asks, so
// none are waiting.
NVN_API(nvnDeviceApplyDeferredFinalizes) { (void)c; }
