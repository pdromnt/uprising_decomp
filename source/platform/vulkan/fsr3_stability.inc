    // Fixed low-resolution inputs expose the sampling cycle on stationary sharp
    // edges. Retain more existing history only when motion, disocclusion and
    // changing shading permit it. Preserve the SDK's rejection and rectification
    // rules: this never restores history whose base accumulation is zero.
    const FfxFloat32 fSourcePixelMotion = length(params.fMotionVector * FfxFloat32x2(RenderSize()));
    if (fSourcePixelMotion < (1.0f / 32.0f) && params.fReactiveMask < 0.001f &&
        params.fDisocclusion < 0.001f && params.fShadingChange < 0.1f && !params.bIsNewSample) {
        fBaseAccumulation *= 8.0f;
    }
