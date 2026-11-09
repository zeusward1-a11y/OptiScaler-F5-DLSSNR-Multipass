// DLSS-G's camera from the game's SR constants (OptiScaler/framegen/dlssg/DlssgGameCamera.h): the projection and the camera's
// motion go to DLSS-G, the position and the right/up/forward vectors do not (they would wake its scene change detection),
// and a game with no camera to give (identity matrices, as Hookshot sends then) or a broken one changes nothing.
//
// cl /nologo /EHsc /std:c++17 /I external\streamline tests\dlssg_game_camera_smoke.cpp && dlssg_game_camera_smoke.exe

#include "../OptiScaler/framegen/dlssg/DlssgGameCamera.h"

#include <cmath>
#include <cstdio>
#include <limits>

static int failures = 0;
#define CHECK(c)                                                                                                               \
    do                                                                                                                         \
    {                                                                                                                          \
        if (!(c))                                                                                                              \
        {                                                                                                                      \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                          \
            failures++;                                                                                                        \
        }                                                                                                                      \
    } while (0)

static sl::float4x4 Rows(const float v[16])
{
    sl::float4x4 m;
    for (uint32_t r = 0; r < 4; r++)
        m.setRow(r, { v[r * 4], v[r * 4 + 1], v[r * 4 + 2], v[r * 4 + 3] });
    return m;
}

static bool Same(const sl::float4x4& a, const sl::float4x4& b)
{
    for (uint32_t r = 0; r < 4; r++)
        if (a[r].x != b[r].x || a[r].y != b[r].y || a[r].z != b[r].z || a[r].w != b[r].w)
            return false;
    return true;
}

static bool Same(const sl::float3& a, const sl::float3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

// What Hookshot sends for a frame of Hyrule Field (vk_dlss.cpp frame_constants: a D3D-style perspective, row vectors, its
// clip-to-previous-clip for a turn), with its orthonormal camera vectors
static sl::Constants GameFrame()
{
    sl::Constants c {};
    const float n = 0.1f, f = 25005.9f, ys = 1.0f / std::tan(0.8726647f / 2), xs = ys / (16.0f / 9.0f);
    const float zs = f / (f - n), zt = -n * f / (f - n);
    const float p[16] = { xs, 0, 0, 0, 0, ys, 0, 0, 0, 0, zs, 1, 0, 0, zt, 0 };
    c.cameraViewToClip = Rows(p);
    const float inv[16] = { 1 / xs, 0, 0, 0, 0, 1 / ys, 0, 0, 0, 0, 0, 1 / zt, 0, 0, 1, -zs / zt };
    c.clipToCameraView = Rows(inv);
    const float turn[16] = { 0.99f, 0, 0.02f, 0, 0, 1, 0, 0, -0.02f, 0, 0.99f, 0, 0.004f, 0, 0, 1 };
    c.clipToPrevClip = Rows(turn);
    const float back[16] = { 1.01f, 0, -0.02f, 0, 0, 1, 0, 0, 0.02f, 0, 1.01f, 0, -0.004f, 0, 0, 1 };
    c.prevClipToClip = Rows(back);
    const float id[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    c.clipToLensClip = Rows(id);
    c.cameraPos = { 1520.5f, 112.0f, -3305.25f };
    c.cameraRight = { 0.8f, 0.0f, 0.6f };
    c.cameraUp = { 0.0f, 1.0f, 0.0f };
    c.cameraFwd = { -0.6f, 0.0f, 0.8f };
    c.cameraNear = n, c.cameraFar = f, c.cameraFOV = 0.8726647f, c.cameraAspectRatio = 16.0f / 9.0f;
    c.cameraPinholeOffset = { 0.0f, 0.0f };
    return c;
}

// What DLSSG_Dx12::Dispatch has built before the game's camera: the synthetic perspective and constant vectors
static sl::Constants Synthetic()
{
    sl::Constants c {};
    const float p[16] = { 1.4f, 0, 0, 0, 0, 2.4f, 0, 0, 0, 0, -1.0f, -1, 0, 0, -0.2f, 0 };
    c.cameraViewToClip = Rows(p);
    c.cameraPos = { 0.0f, 0.0f, 0.0f };
    c.cameraUp = { 0.0f, 0.0f, 1.0f };
    c.cameraRight = { 0.0f, 1.0f, 0.0f };
    c.cameraFwd = { 1.0f, 0.0f, 0.0f };
    c.cameraNear = 0.1f, c.cameraFar = 100000.0f, c.cameraFOV = 1.0472f, c.cameraAspectRatio = 1.7778f;
    return c;
}

int main()
{
    // the matrices and lens come from the game
    const sl::Constants game = GameFrame();
    CHECK(dlssg_camera::HasPerspective(game));
    sl::Constants out = Synthetic();
    const sl::Constants before = out;
    dlssg_camera::ApplyMatrices(out, game);
    CHECK(Same(out.cameraViewToClip, game.cameraViewToClip));
    CHECK(Same(out.clipToCameraView, game.clipToCameraView));
    CHECK(Same(out.clipToPrevClip, game.clipToPrevClip));
    CHECK(Same(out.prevClipToClip, game.prevClipToClip));
    CHECK(Same(out.clipToLensClip, game.clipToLensClip));
    CHECK(out.cameraNear == game.cameraNear && out.cameraFar == game.cameraFar);
    CHECK(out.cameraFOV == game.cameraFOV && out.cameraAspectRatio == game.cameraAspectRatio);

    // the position and the vectors stay Dispatch's: constant, so the scene change detection sees a still camera
    CHECK(Same(out.cameraPos, before.cameraPos));
    CHECK(Same(out.cameraUp, before.cameraUp));
    CHECK(Same(out.cameraRight, before.cameraRight));
    CHECK(Same(out.cameraFwd, before.cameraFwd));
    CHECK(!Same(out.cameraFwd, game.cameraFwd));

    // no camera from the game (Hookshot's identity matrices and its default near/far/FOV): not taken
    sl::Constants none {};
    const float id[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    none.cameraViewToClip = none.clipToCameraView = none.clipToPrevClip = none.prevClipToClip = Rows(id);
    none.cameraNear = 0.1f, none.cameraFar = 1000.0f, none.cameraFOV = 1.0f, none.cameraAspectRatio = 1.7778f;
    CHECK(!dlssg_camera::HasPerspective(none));

    // a real projection whose clip-to-previous-clip Hookshot could not invert (it keeps the identity then): taken, the still camera
    // of the synthetic build plus the game's projection
    sl::Constants still = game;
    still.clipToPrevClip = still.prevClipToClip = Rows(id);
    CHECK(dlssg_camera::HasPerspective(still));
    sl::Constants out2 = Synthetic();
    dlssg_camera::ApplyMatrices(out2, still);
    CHECK(Same(out2.clipToPrevClip, still.clipToPrevClip) && Same(out2.cameraViewToClip, game.cameraViewToClip));

    // broken ones: not taken
    sl::Constants nan = game;
    nan.clipToPrevClip[1].z = std::numeric_limits<float>::quiet_NaN();
    CHECK(!dlssg_camera::HasPerspective(nan));
    sl::Constants inf = game;
    inf.cameraViewToClip[0].x = std::numeric_limits<float>::infinity();
    CHECK(!dlssg_camera::HasPerspective(inf));
    sl::Constants flat = game;
    flat.cameraFar = flat.cameraNear;
    CHECK(!dlssg_camera::HasPerspective(flat));
    sl::Constants nofov = game;
    nofov.cameraFOV = 0.0f;
    CHECK(!dlssg_camera::HasPerspective(nofov));
    sl::Constants zeroed {};
    CHECK(!dlssg_camera::HasPerspective(zeroed));

    std::printf("%s: %d failures\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
