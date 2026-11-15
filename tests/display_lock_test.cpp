#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// The production bodies below are extracted at configure time. These stubs
// expose only the dependencies they use, with controllable SDL3 bool results.
struct SDL_Surface {
    void *pixels;
    bool lockSucceeds;
    int locks = 0, unlocks = 0, held = 0;
};
static bool SDL_LockSurface(SDL_Surface *surface) {
    if (!surface) return false;
    ++surface->locks;
    if (!surface->lockSucceeds) return false;
    ++surface->held;
    return true;
}
static int unpairedUnlocks;
static void SDL_UnlockSurface(SDL_Surface *surface) {
    if (!surface || !surface->held) { ++unpairedUnlocks; return; }
    ++surface->unlocks;
    --surface->held;
}
struct rdTexFormat { int bpp, r_bits, r_shift, g_bits, g_shift, b_bits, b_shift; };
struct RasterInfo { int width, height, rowSize; rdTexFormat format; };
struct tVBuffer { SDL_Surface *sdlSurface; char *surface_lock_alloc; RasterInfo format; };
struct rdColor24 { unsigned char r, g, b; };
struct PreviewTexture { tVBuffer *texture_struct[1]; int alpha_en, color_transparent; };
struct rdTexinfo { PreviewTexture *texture_ptr; struct { int solidColor; } header; };
struct rdMaterial { int num_texinfo; rdTexinfo **texinfos; rdColor24 *palette_alloc; };
static rdMaterial *currentMaterial;
static rdColor24 colors[256];
static int materialColormap[1], pixelWrites;
static rdMaterial *editorMaterial(int) { return currentMaterial; }
static int rdMaterial_EnsureDataForced(rdMaterial *) { return 1; }
static rdColor24 *colormapColors(int) { return colors; }
static int palettePixel(tVBuffer *, int red, int, int) { return red; }
static void edCopy(void *to, const void *from, size_t bytes) { std::memcpy(to, from, bytes); }
static void stdPlatform_WriteByte16(unsigned char *destination, uint8_t value) {
    ++pixelWrites;
    *destination = value;
}

#include "display_lock_production.inc"
#include "display_preview_production.inc"

static void check(bool condition, const char *message) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static tVBuffer bufferFor(SDL_Surface *surface) {
    return {surface, nullptr, {1, 1, 1, {8, 0, 0, 0, 0, 0, 0}}};
}
static void previewCase(const char *name) {
    unsigned char sourcePixel = 42, destinationPixel = 7;
    SDL_Surface source{&sourcePixel, true}, destination{&destinationPixel, true};
    if (!std::strcmp(name, "preview_source_failure")) source.lockSucceeds = false;
    else if (!std::strcmp(name, "preview_destination_failure")) destination.lockSucceeds = false;
    else if (!std::strcmp(name, "preview_source_null_pixels")) source.pixels = nullptr;
    else if (!std::strcmp(name, "preview_destination_null_pixels")) destination.pixels = nullptr;
    else check(!std::strcmp(name, "preview_valid_pixels"), "recognized preview case");
    tVBuffer sourceBuffer = bufferFor(&source), destinationBuffer = bufferFor(&destination);
    PreviewTexture texture{{&sourceBuffer}, 0, 0};
    rdTexinfo info{&texture, {91}};
    rdTexinfo *infos[] = {&info};
    rdMaterial material{1, infos, colors};
    currentMaterial = &material;
    for (int i = 0; i < 256; ++i) colors[i].r = (unsigned char)i;
    drawMaterialPreview(&destinationBuffer, 0, 0, 0, 1, 1);
    check(source.locks == 1 && destination.locks == 1, "both preview locks attempted once");
    check(source.unlocks == (source.lockSucceeds ? 1 : 0), "source unlock matches acquired lock");
    check(destination.unlocks == (destination.lockSucceeds ? 1 : 0), "destination unlock matches acquired lock");
    check(!source.held && !destination.held && !unpairedUnlocks, "all and only acquired preview locks released");
    check(!sourceBuffer.surface_lock_alloc && !destinationBuffer.surface_lock_alloc, "preview lock pointers cleared");
    if (destination.lockSucceeds && destination.pixels) {
        check(pixelWrites == 1, "usable destination drawn once");
        check(destinationPixel == (source.lockSucceeds && source.pixels ? 42 : 91), "source pixels or solid fallback drawn");
    } else check(!pixelWrites && destinationPixel == 7, "unusable destination left untouched");
}
int main(int argc, char **argv) {
    check(argc == 2, "one test case required");
    const char *name = argv[1];
    unsigned char pixel = 42;
    SDL_Surface surface{&pixel, true};
    tVBuffer buffer = bufferFor(&surface);
    if (!std::strcmp(name, "null_buffer")) {
        check(!stdDisplay_VBufferLock(nullptr), "null buffer rejected");
        stdDisplay_VBufferUnlock(nullptr);
    } else if (!std::strcmp(name, "missing_surface")) {
        buffer.sdlSurface = nullptr;
        buffer.surface_lock_alloc = (char*)&pixel;
        check(!stdDisplay_VBufferLock(&buffer), "missing SDL surface rejected");
        check(!buffer.surface_lock_alloc, "missing surface clears stale pixel pointer");
        stdDisplay_VBufferUnlock(&buffer);
    } else if (!std::strcmp(name, "backend_failure")) {
        surface.lockSucceeds = false;
        buffer.surface_lock_alloc = (char*)&pixel;
        check(!stdDisplay_VBufferLock(&buffer), "SDL3 backend failure propagated");
        check(!buffer.surface_lock_alloc, "backend failure clears stale pixel pointer");
        check(surface.locks == 1 && !surface.held && !surface.unlocks, "failed backend lock is not unlocked");
    } else if (!std::strcmp(name, "valid_pixels") || !std::strcmp(name, "null_pixels")) {
        if (!std::strcmp(name, "null_pixels")) surface.pixels = nullptr;
        check(stdDisplay_VBufferLock(&buffer) == 1, "successful SDL3 lock reported");
        check(buffer.surface_lock_alloc == surface.pixels, "current SDL pixel pointer exposed");
        check(surface.held == 1, "successful lock held until caller unlocks");
        stdDisplay_VBufferUnlock(&buffer);
        check(!buffer.surface_lock_alloc && !surface.held && surface.unlocks == 1, "successful lock released exactly once");
    } else previewCase(name);
    check(!unpairedUnlocks, "no unpaired backend unlocks");
    std::printf("PASS: %s\n", name);
    return 0;
}
