#pragma once

// The game's camera for DLSS-G in the Vulkan route "Bridge, game's upscaler" (NativeFrameGenerationOnly): a game that calls
// Streamline for DLSS super resolution only (Hookshot, the Zelda Ocarina of Time PC recomp) still sends its camera with
// slSetConstants; StreamlineHooks keeps it and UpscaleStart hands it to the frame it starts (DLSSG_Dx12::SetGameCameraConstants).
//
// Taken: the projection (cameraViewToClip, clipToCameraView), the camera's motion (clipToPrevClip, prevClipToClip, the lens
// matrix) and near, far, FOV and aspect. Without them DLSS-G sees an identity clip-to-previous-clip: a still camera, everything
// moving by its vectors alone, and the scenery smears where a turn uncovers it.
//
// Left out: the position and the right/up/forward vectors. With them too (the whole camera, 09/11/2026) the motion felt less
// fluid than with no camera at all, base frame rate unchanged; with the matrices alone it stayed fluid and the scenery smeared
// less on turns (the player's A/B). The documented consumer of the vectors is DLSS-G's auto scene change detection (ASCD,
// ProgrammingGuideDLSS_G.md 20.0), which runs when they are orthonormal: Dispatch's constant ones are too, so ASCD ran in every
// build, and what changed is that the game's rotate. Suspected, not proven (DLSS-G is closed): it took quick turns for scene cuts
// and left those frames ungenerated. Streamline's own helper derives clipToPrevClip from position and vectors
// (sl_matrix_helpers.h recalculateCameraMatrices), so the two now disagree on purpose: a still camera in the vectors, the turn in
// the matrices; no documented check compares them. A real cut still comes as the reset flag from the game's upscaler call.

#include <sl_consts.h>

#include <cmath>

namespace dlssg_camera
{

// A perspective camera: the game's view-to-clip (Streamline's row vectors) puts view z in clip w, row 2 column 3. A game with no
// camera to give sends identity matrices (Hookshot's frame_constants), which would replace the synthetic perspective with none.
inline bool HasPerspective(const sl::Constants& game)
{
    const float w = game.cameraViewToClip[2].w;

    if (!std::isfinite(w) || std::fabs(w) < 1e-6f)
        return false;

    for (uint32_t r = 0; r < 4; r++)
    {
        const auto& v = game.cameraViewToClip[r];
        const auto& p = game.clipToPrevClip[r];

        if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z) || !std::isfinite(v.w) ||
            !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || !std::isfinite(p.w))
            return false;
    }

    return game.cameraNear > 0.0f && game.cameraFar > game.cameraNear && game.cameraFOV > 0.0f &&
           game.cameraAspectRatio > 0.0f;
}

// The game's matrices and lens into the constants Dispatch builds; position and vectors stay as Dispatch set them
inline void ApplyMatrices(sl::Constants& out, const sl::Constants& game)
{
    out.cameraViewToClip = game.cameraViewToClip;
    out.clipToCameraView = game.clipToCameraView;
    out.clipToLensClip = game.clipToLensClip;
    out.clipToPrevClip = game.clipToPrevClip;
    out.prevClipToClip = game.prevClipToClip;
    out.cameraPinholeOffset = game.cameraPinholeOffset;
    out.cameraNear = game.cameraNear;
    out.cameraFar = game.cameraFar;
    out.cameraFOV = game.cameraFOV;
    out.cameraAspectRatio = game.cameraAspectRatio;
}

} // namespace dlssg_camera
