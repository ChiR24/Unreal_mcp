const ENGINE_LUT_PATH = '/Engine/EngineResources/DefaultTexture.DefaultTexture';

function successExpected(...alternatives) {
  return ['success', ...alternatives].join('|');
}

function errorExpected(...alternatives) {
  return ['error', ...alternatives].join('|');
}

function createNamedActionCase(scenario, action, args, extra = {}) {
  return {
    scenario,
    toolName: 'build_environment',
    arguments: { action, ...args },
    expected: extra.expected ?? 'success',
    ...(extra.assertions ? { assertions: extra.assertions } : {}),
    ...(extra.captureResult ? { captureResult: extra.captureResult } : {}),
    ...(extra.timeoutMs ? { timeoutMs: extra.timeoutMs } : {})
  };
}

function createRenderingContext(ts = Date.now()) {
  const folder = `/Game/MCPTest/Rendering_${ts}`;
  const folderAlias = folder.slice(1);
  const renderTargetName = `RT_Rendering_${ts}`;
  const renderTargetPath = `${folder}/${renderTargetName}`;

  return {
    ts,
    folder,
    folderAlias,
    renderTargetName,
    renderTargetPath,
    lightActor: `RenderingDirectional_${ts}`,
    targetActor: `RenderingTarget_${ts}`,
    lightmassVolume: `RenderingLightmass_${ts}`,
    lightingLightmassVolume: `RenderingLightingLightmass_${ts}`,
    postProcessVolume: `RenderingPP_${ts}`,
    attachedPostProcessVolume: `RenderingTarget_${ts}_PostProcessVolume`,
    sphereCapture: `RenderingSphereCapture_${ts}`,
    boxCapture: `RenderingBoxCapture_${ts}`,
    planarReflection: `RenderingPlanar_${ts}`,
    sceneCapture2d: `RenderingSceneCapture2D_${ts}`,
    sceneCaptureCube: `RenderingSceneCaptureCube_${ts}`,
    missingActor: `RenderingMissingActor_${ts}`,
    missingRenderTargetPath: `${folder}/RT_Missing_${ts}`,
    missingLutPath: `${folder}/T_MissingLut_${ts}`
  };
}

function createRenderingSetupCases(ctx) {
  return [
    {
      scenario: 'Setup: create rendering test folder',
      toolName: 'manage_asset',
      arguments: { action: 'create_folder', path: ctx.folder },
      expected: successExpected('already exists')
    },
    {
      scenario: 'Setup: create seeded render target',
      toolName: 'manage_asset',
      arguments: {
        action: 'create_render_target',
        name: ctx.renderTargetName,
        path: ctx.folder,
        width: 512,
        height: 512,
        format: 'RGBA16F'
      },
      expected: successExpected('already exists')
    },
    {
      scenario: 'Setup: create lightmass importance volume',
      toolName: 'manage_level_structure',
      arguments: { action: 'create_lightmass_importance_volume', volumeName: ctx.lightmassVolume },
      expected: successExpected('already exists')
    },
    {
      scenario: 'Setup: create post process volume',
      toolName: 'manage_level_structure',
      arguments: {
        action: 'create_post_process_volume',
        volumeName: ctx.postProcessVolume,
        priority: 1,
        blendRadius: 100,
        blendWeight: 1,
        bUnbound: true
      },
      expected: successExpected('already exists')
    },
    {
      scenario: 'Setup: spawn lighting-channel target actor',
      toolName: 'control_actor',
      arguments: {
        action: 'spawn',
        classPath: '/Engine/BasicShapes/Cube',
        actorName: ctx.targetActor,
        location: { x: 0, y: 0, z: 100 }
      },
      expected: successExpected('already exists')
    },
    createNamedActionCase(
      'Setup: configure reusable directional light',
      'configure_directional_light_atmosphere',
      {
        actorName: ctx.lightActor,
        location: { x: 400, y: 0, z: 300 },
        rotation: { pitch: -35, yaw: 45, roll: 0 },
        intensity: 12,
        settings: { bUsedAsAtmosphereSunLight: true }
      },
      {
        expected: successExpected('already exists'),
        captureResult: { key: 'renderingLightPath', fromField: 'result.actorPath' },
        assertions: [
          {
            path: 'structuredContent.result.actorName',
            includes: ctx.lightActor,
            label: 'directional light actor name matches request'
          }
        ]
      }
    )
  ];
}

function createRenderingHappyCases(ctx) {
  return [
    createNamedActionCase('RT: configure ray traced shadows', 'configure_ray_traced_shadows', {
      enabled: true,
      settings: { SamplesPerPixel: 1, ShadowDistance: 20000 }
    }, {
      expected: successExpected('unsupported', 'not available'),
      assertions: [{ path: 'structuredContent.result.subAction', equals: 'configure_ray_traced_shadows', label: 'ray traced shadows response names subAction' }]
    }),
    createNamedActionCase('RT: configure ray traced GI', 'configure_ray_traced_gi', {
      enabled: true,
      settings: { MaxBounces: 1, SamplesPerPixel: 1 }
    }, {
      expected: successExpected('unsupported', 'not available'),
      assertions: [{ path: 'structuredContent.result.subAction', equals: 'configure_ray_traced_gi', label: 'ray traced GI response names subAction' }]
    }),
    createNamedActionCase('RT: configure ray traced reflections', 'configure_ray_traced_reflections', {
      enabled: true,
      settings: { MaxRoughness: 0.8, SamplesPerPixel: 1 }
    }, { expected: successExpected('unsupported', 'not available') }),
    createNamedActionCase('RT: configure ray traced AO', 'configure_ray_traced_ao', {
      enabled: true,
      intensity: 0.5,
      settings: { Radius: 150 }
    }, { expected: successExpected('unsupported', 'not available') }),
    createNamedActionCase('RT: configure path tracing', 'configure_path_tracing', {
      enabled: false,
      settings: { SamplesPerPixel: 4, MaxBounces: 2 }
    }, { expected: successExpected('unsupported', 'not available') }),
    createNamedActionCase('RT: omit shadow sample setting without applying sample CVar', 'configure_ray_traced_shadows', {
      enabled: true
    }, {
      expected: successExpected('unsupported', 'not available'),
      assertions: [
        { path: 'structuredContent.result.appliedCVars', notIncludes: 'r.RayTracing.Shadows.SamplesPerPixel', label: 'omitted shadow samples are preserved' }
      ]
    }),
    createNamedActionCase('RT: omit GI bounce setting without applying bounce CVar', 'configure_ray_traced_gi', {
      enabled: true,
      settings: { SamplesPerPixel: 1 }
    }, {
      expected: successExpected('unsupported', 'not available'),
      assertions: [
        { path: 'structuredContent.result.appliedCVars', notIncludes: 'r.RayTracing.GlobalIllumination.MaxBounces', label: 'omitted GI bounces are preserved' }
      ]
    }),
    createNamedActionCase('RT: omit reflection roughness without applying roughness CVar', 'configure_ray_traced_reflections', {
      enabled: true,
      settings: { SamplesPerPixel: 1 }
    }, {
      expected: successExpected('unsupported', 'not available'),
      assertions: [
        { path: 'structuredContent.result.appliedCVars', notIncludes: 'r.RayTracing.Reflections.MaxRoughness', label: 'omitted reflection roughness is preserved' }
      ]
    }),
    createNamedActionCase('RT: omit AO intensity and radius without applying default CVars', 'configure_ray_traced_ao', {
      enabled: true,
      settings: {}
    }, {
      expected: successExpected('unsupported', 'not available'),
      assertions: [
        { path: 'structuredContent.result.appliedCVars', notIncludes: 'r.RayTracing.AmbientOcclusion.Intensity', label: 'omitted AO intensity is preserved' },
        { path: 'structuredContent.result.appliedCVars', notIncludes: 'r.RayTracing.AmbientOcclusion.Radius', label: 'omitted AO radius is preserved' }
      ]
    }),
    createNamedActionCase('RT: omit path-tracing bounce setting without applying bounce CVar', 'configure_path_tracing', {
      enabled: false,
      settings: { SamplesPerPixel: 4 }
    }, {
      expected: successExpected('unsupported', 'not available'),
      assertions: [
        { path: 'structuredContent.result.appliedCVars', notIncludes: 'r.PathTracing.MaxBounces', label: 'omitted path tracing bounces are preserved' }
      ]
    }),

    createNamedActionCase('Light Channels: set light channel', 'set_light_channel', {
      actorName: ctx.lightActor,
      channel: 1,
      enabled: true
    }, { assertions: [{ path: 'structuredContent.result.subAction', equals: 'set_light_channel', label: 'light channel response names subAction' }] }),
    createNamedActionCase('Light Channels: set actor light channel', 'set_actor_light_channel', {
      targetActor: ctx.targetActor,
      channel: 1,
      channels: [0, 1],
      enabled: true
    }, { assertions: [{ path: 'structuredContent.result.subAction', equals: 'set_actor_light_channel', label: 'actor light channel response names subAction' }] }),

    createNamedActionCase('Lightmass: configure lightmass settings', 'configure_lightmass_settings', {
      settings: { StaticLightingLevelScale: 0.5, NumIndirectLightingBounces: 2 }
    }),
    createNamedActionCase('Lightmass: build lighting quality', 'build_lighting_quality', {
      quality: 'Preview',
      settings: { buildReflectionCaptures: true }
    }, { timeoutMs: 120000 }),
    createNamedActionCase('Lightmass: configure indirect lighting cache', 'configure_indirect_lighting_cache', {
      targetActor: ctx.targetActor,
      settings: { IndirectLightingCacheQuality: 'ILCQ_Volume' }
    }),
    createNamedActionCase('Lightmass: create lightmass volume through build environment', 'create_lightmass_volume', {
      name: ctx.lightingLightmassVolume,
      location: { x: -500, y: 0, z: 250 }
    }, {
      expected: successExpected('already exists'),
      assertions: [{
        path: 'structuredContent.result.actorName',
        includes: ctx.lightingLightmassVolume,
        label: 'build environment lightmass volume name matches request'
      }]
    }),
    {
      scenario: 'Post Process: attach post process volume to target actor',
      toolName: 'manage_level_structure',
      arguments: {
        action: 'add_post_process_volume',
        actorPath: ctx.targetActor,
        volumeName: ctx.attachedPostProcessVolume,
        priority: 2,
        blendRadius: 50,
        blendWeight: 0.75,
        bUnbound: false
      },
      expected: successExpected('already exists'),
      assertions: [{
        path: 'structuredContent.result.attachedTo',
        includes: ctx.targetActor,
        label: 'post process volume attaches to target actor'
      }]
    },

    createNamedActionCase('Reflections: create sphere reflection capture', 'create_sphere_reflection_capture', {
      actorName: ctx.sphereCapture,
      location: { x: 150, y: 0, z: 150 }
    }, {
      expected: successExpected('already exists'),
      captureResult: { key: 'renderingSphereCapturePath', fromField: 'result.actorPath' },
      assertions: [{ path: 'structuredContent.result.actorName', includes: ctx.sphereCapture, label: 'sphere reflection capture actor name matches request' }],
      timeoutMs: 60000
    }),
    createNamedActionCase('Reflections: create box reflection capture', 'create_box_reflection_capture', {
      actorName: ctx.boxCapture,
      location: { x: 350, y: 0, z: 150 },
      settings: { BoxTransitionDistance: 100, BoxExtent: { x: 300, y: 300, z: 150 } }
    }, {
      expected: successExpected('already exists'),
      captureResult: { key: 'renderingBoxCapturePath', fromField: 'result.actorPath' },
      assertions: [{ path: 'structuredContent.result.actorName', includes: ctx.boxCapture, label: 'box reflection capture actor name matches request' }],
      timeoutMs: 60000
    }),
    createNamedActionCase('Reflections: configure capture offset', 'configure_capture_offset', {
      actorName: ctx.sphereCapture,
      captureOffset: { x: 25, y: 0, z: 10 }
    }, { timeoutMs: 60000 }),
    createNamedActionCase('Reflections: configure capture resolution', 'configure_reflection_capture_resolution', {
      actorName: ctx.sphereCapture,
      resolution: 256
    }, {
      assertions: [{ path: 'structuredContent.result.resolution', equals: 256, label: 'reflection capture resolution is reported' }],
      timeoutMs: 60000
    }),
    createNamedActionCase('Reflections: recapture scene', 'recapture_scene', {
      actorName: ctx.sphereCapture
    }, { timeoutMs: 60000 }),
    createNamedActionCase('Reflections: create planar reflection', 'create_planar_reflection', {
      actorName: ctx.planarReflection,
      location: { x: 550, y: 0, z: 100 },
      rotation: { pitch: 0, yaw: 0, roll: 0 }
    }, {
      expected: successExpected('already exists'),
      captureResult: { key: 'renderingPlanarPath', fromField: 'result.actorPath' },
      assertions: [{ path: 'structuredContent.result.actorName', includes: ctx.planarReflection, label: 'planar reflection actor name matches request' }],
      timeoutMs: 60000
    }),
    createNamedActionCase('Reflections: configure planar reflection', 'configure_planar_reflection', {
      actorName: ctx.planarReflection,
      settings: {
        ScreenPercentage: 50,
        DistanceFromPlaneFadeoutStart: 100,
        DistanceFromPlaneFadeoutEnd: 500
      }
    }),
    createNamedActionCase('Reflections: configure SSR settings', 'configure_ssr_settings', {
      actorName: ctx.postProcessVolume,
      settings: { ScreenSpaceReflectionIntensity: 80, ScreenSpaceReflectionQuality: 75, ScreenSpaceReflectionMaxRoughness: 0.8 }
    }),
    createNamedActionCase('Reflections: configure Lumen reflection settings', 'configure_lumen_reflection_settings', {
      actorName: ctx.postProcessVolume,
      settings: { LumenReflectionQuality: 3, LumenMaxRoughnessToTraceReflections: 0.8 }
    }),

    createNamedActionCase('Post Process: configure PP blend', 'configure_pp_blend', {
      actorName: ctx.postProcessVolume,
      infiniteUnbound: true,
      blendWeight: 0.9
    }, { assertions: [{ path: 'structuredContent.result.subAction', equals: 'configure_pp_blend', label: 'PP blend response names subAction' }] }),
    createNamedActionCase('Post Process: set white balance', 'set_pp_white_balance', {
      actorName: ctx.postProcessVolume,
      settings: { WhiteTemp: 6500, WhiteTint: 0.2 }
    }),
    createNamedActionCase('Post Process: set color grading', 'set_pp_color_grading', {
      actorName: ctx.postProcessVolume,
      settings: {
        ColorSaturation: { x: 1.05, y: 1.0, z: 1.0, w: 1.0 },
        ColorContrast: { x: 1.0, y: 1.02, z: 1.0, w: 1.0 }
      }
    }),
    createNamedActionCase('Post Process: set LUT', 'set_pp_lut', {
      actorName: ctx.postProcessVolume,
      lutPath: ENGINE_LUT_PATH
    }, { expected: successExpected('unsupported') }),
    createNamedActionCase('Post Process: configure tonemapper', 'configure_tonemapper', {
      actorName: ctx.postProcessVolume,
      settings: { FilmSlope: 0.88, FilmToe: 0.55, FilmShoulder: 0.26 }
    }),
    createNamedActionCase('Post Process: set tonemapper type', 'set_tonemapper_type', {
      actorName: ctx.postProcessVolume,
      method: 'Filmic'
    }),
    createNamedActionCase('Post Process: configure bloom', 'configure_bloom', {
      actorName: ctx.postProcessVolume,
      settings: { BloomIntensity: 1.25, BloomThreshold: 0.15 }
    }),
    createNamedActionCase('Post Process: set bloom intensity', 'set_bloom_intensity', {
      actorName: ctx.postProcessVolume,
      intensity: 1.5
    }),
    createNamedActionCase('Post Process: set bloom threshold', 'set_bloom_threshold', {
      actorName: ctx.postProcessVolume,
      threshold: 0.2,
      settings: { BloomThreshold: 0.2 }
    }),
    createNamedActionCase('Post Process: configure lens flare', 'configure_lens_flare', {
      actorName: ctx.postProcessVolume,
      enabled: true,
      settings: { LensFlareIntensity: 1.1, LensFlareThreshold: 8 }
    }),
    createNamedActionCase('Post Process: configure DOF', 'configure_dof', {
      actorName: ctx.postProcessVolume,
      enabled: true,
      settings: { DepthOfFieldFocalDistance: 450, DepthOfFieldFstop: 2.8 }
    }),
    createNamedActionCase('Post Process: set DOF method', 'set_dof_method', {
      actorName: ctx.postProcessVolume,
      method: 'CinematicDOF'
    }),
    createNamedActionCase('Post Process: set focal distance', 'set_focal_distance', {
      actorName: ctx.postProcessVolume,
      distance: 650,
      settings: { DepthOfFieldFocalDistance: 650 }
    }),
    createNamedActionCase('Post Process: set aperture', 'set_aperture', {
      actorName: ctx.postProcessVolume,
      aperture: 4,
      settings: { DepthOfFieldFstop: 4 }
    }),
    createNamedActionCase('Post Process: configure bokeh', 'configure_bokeh', {
      actorName: ctx.postProcessVolume,
      settings: { DepthOfFieldSensorWidth: 36, DepthOfFieldBladeCount: 7 }
    }),
    createNamedActionCase('Post Process: configure motion blur', 'configure_motion_blur', {
      actorName: ctx.postProcessVolume,
      enabled: true,
      settings: { MotionBlurAmount: 0.25, MotionBlurMax: 5 }
    }),
    createNamedActionCase('Post Process: set motion blur amount', 'set_motion_blur_amount', {
      actorName: ctx.postProcessVolume,
      amount: 0.35,
      settings: { MotionBlurAmount: 0.35 }
    }),
    createNamedActionCase('Post Process: set motion blur max', 'set_motion_blur_max', {
      actorName: ctx.postProcessVolume,
      amount: 8,
      settings: { MotionBlurMax: 8 }
    }),
    createNamedActionCase('Post Process: configure exposure', 'configure_exposure', {
      actorName: ctx.postProcessVolume,
      enabled: true,
      settings: { AutoExposureMinBrightness: 0.75, AutoExposureMaxBrightness: 1.25, AutoExposureBias: 0.1 }
    }),
    createNamedActionCase('Post Process: set exposure method', 'set_exposure_method', {
      actorName: ctx.postProcessVolume,
      method: 'Manual'
    }),
    createNamedActionCase('Post Process: set exposure compensation', 'set_exposure_compensation', {
      actorName: ctx.postProcessVolume,
      compensationValue: 0.25
    }),
    createNamedActionCase('Post Process: set exposure min max', 'set_exposure_min_max', {
      actorName: ctx.postProcessVolume,
      minBrightness: 0.85,
      maxBrightness: 1.15
    }),
    createNamedActionCase('Post Process: configure SSAO', 'configure_ssao', {
      actorName: ctx.postProcessVolume,
      enabled: true,
      intensity: 0.75,
      settings: { AmbientOcclusionRadius: 150, AmbientOcclusionPower: 1.0 }
    }),
    createNamedActionCase('Post Process: configure GTAO', 'configure_gtao', {
      actorName: ctx.postProcessVolume,
      enabled: true,
      settings: { AmbientOcclusionQuality: 75 }
    }),
    createNamedActionCase('Post Process: configure vignette', 'configure_vignette', {
      actorName: ctx.postProcessVolume,
      intensity: 0.35
    }),
    createNamedActionCase('Post Process: configure chromatic aberration', 'configure_chromatic_aberration', {
      actorName: ctx.postProcessVolume,
      intensity: 0.2,
      settings: { SceneFringeSaturation: 1.0 }
    }),
    createNamedActionCase('Post Process: configure grain', 'configure_grain', {
      actorName: ctx.postProcessVolume,
      intensity: 0.25,
      settings: { GrainJitter: 0.5 }
    }),
    createNamedActionCase('Post Process: configure screen percentage', 'configure_screen_percentage', {
      actorName: ctx.postProcessVolume,
      screenPercentage: 90
    }, { expected: successExpected('unsupported', 'not available') }),

    createNamedActionCase('Scene Capture: create scene capture 2D', 'create_scene_capture_2d', {
      actorName: ctx.sceneCapture2d,
      location: { x: 750, y: 0, z: 200 }
    }, {
      expected: successExpected('already exists'),
      captureResult: { key: 'renderingSceneCapture2dPath', fromField: 'result.actorPath' },
      assertions: [{ path: 'structuredContent.result.actorName', includes: ctx.sceneCapture2d, label: 'scene capture 2D actor name matches request' }]
    }),
    createNamedActionCase('Scene Capture: create scene capture cube', 'create_scene_capture_cube', {
      actorName: ctx.sceneCaptureCube,
      location: { x: 950, y: 0, z: 200 }
    }, {
      expected: successExpected('already exists'),
      captureResult: { key: 'renderingSceneCaptureCubePath', fromField: 'result.actorPath' },
      assertions: [{ path: 'structuredContent.result.actorName', includes: ctx.sceneCaptureCube, label: 'scene capture cube actor name matches request' }]
    }),
    createNamedActionCase('Scene Capture: assign render target', 'assign_render_target', {
      actorName: ctx.sceneCapture2d,
      renderTargetPath: ctx.renderTargetPath
    }, {
      assertions: [
        { path: 'structuredContent.result.renderTargetPath', includes: ctx.renderTargetName, label: 'assign render target reports the seeded render target' }
      ]
    }),
    createNamedActionCase('Scene Capture: configure capture resolution', 'configure_capture_resolution', {
      actorName: ctx.sceneCapture2d,
      resolution: 512
    }, {
      assertions: [
        { path: 'structuredContent.result.resolution', equals: 512, label: 'scene capture resolution is reported' },
        { path: 'structuredContent.result.renderTargetPath', includes: ctx.renderTargetName, label: 'scene capture resolution keeps the seeded render target' }
      ]
    }),
    createNamedActionCase('Scene Capture: configure capture source', 'configure_capture_source', {
      actorName: ctx.sceneCapture2d,
      captureSource: 'FinalColorLDR'
    }, { expected: successExpected('unsupported', 'not available') }),
    createNamedActionCase('Scene Capture: capture scene', 'capture_scene', {
      actorName: ctx.sceneCapture2d
    }, {
      expected: successExpected('not available'),
      assertions: [
        { path: 'structuredContent.result.captured', equals: true, label: 'capture scene reports a completed capture' }
      ]
    })
  ];
}

function createRenderingAdversarialCases(ctx) {
  return [
    {
      scenario: 'Adversarial: reject invalid render target resolution',
      toolName: 'manage_asset',
      arguments: {
        action: 'create_render_target',
        name: `RT_Invalid_${ctx.ts}`,
        path: ctx.folder,
        width: 0,
        height: -1
      },
      expected: errorExpected('invalid')
    },
    {
      scenario: 'Adversarial: reject unsafe render target allocation',
      toolName: 'manage_asset',
      arguments: {
        action: 'create_render_target',
        name: `RT_Unsafe_${ctx.ts}`,
        path: ctx.folder,
        width: 8192,
        height: 8192,
        format: 'RGBA32F'
      },
      expected: errorExpected('safe allocation', 'invalid')
    },
    {
      scenario: 'Adversarial: duplicate render target create is idempotent',
      toolName: 'manage_asset',
      arguments: {
        action: 'create_render_target',
        name: ctx.renderTargetName,
        path: ctx.folder,
        width: 512,
        height: 512
      },
      expected: successExpected('already exists')
    },
    createNamedActionCase('Adversarial: reject oversized ray-traced shadow samples', 'configure_ray_traced_shadows', {
      enabled: true,
      settings: { SamplesPerPixel: 1000000000 }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject oversized ray-traced GI bounces', 'configure_ray_traced_gi', {
      enabled: true,
      settings: { SamplesPerPixel: 1, MaxBounces: 1000000000 }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject invalid ray-traced reflection roughness', 'configure_ray_traced_reflections', {
      enabled: true,
      settings: { SamplesPerPixel: 1, MaxRoughness: 5 }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject oversized ray-traced AO radius', 'configure_ray_traced_ao', {
      enabled: true,
      intensity: 1,
      settings: { Radius: 1000000000 }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject oversized path-tracing workload', 'configure_path_tracing', {
      enabled: true,
      settings: { SamplesPerPixel: 1000000000, MaxBounces: 1000000000 }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject nonnumeric ray-traced shadow samples', 'configure_ray_traced_shadows', {
      enabled: true,
      settings: { SamplesPerPixel: 'many' }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject null ray-traced GI samples from JSON transport', 'configure_ray_traced_gi', {
      enabled: true,
      settings: { SamplesPerPixel: null, MaxBounces: 1 }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject fractional ray-traced GI bounces', 'configure_ray_traced_gi', {
      enabled: true,
      settings: { SamplesPerPixel: 1, MaxBounces: 1.5 }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject below-min ray-traced reflection roughness', 'configure_ray_traced_reflections', {
      enabled: true,
      settings: { SamplesPerPixel: 1, MaxRoughness: -0.1 }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject nonnumeric ray-traced AO intensity', 'configure_ray_traced_ao', {
      enabled: true,
      intensity: 'bright',
      settings: { Radius: 150 }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject below-min path-tracing bounces', 'configure_path_tracing', {
      enabled: true,
      settings: { SamplesPerPixel: 1, MaxBounces: -1 }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject below-min ray-traced shadow samples', 'configure_ray_traced_shadows', {
      enabled: true,
      settings: { SamplesPerPixel: 0 }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject fractional ray-traced shadow samples', 'configure_ray_traced_shadows', {
      enabled: true,
      settings: { SamplesPerPixel: 1.5 }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject nonnumeric ray-traced reflection samples', 'configure_ray_traced_reflections', {
      enabled: true,
      settings: { SamplesPerPixel: null, MaxRoughness: 0.5 }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject fractional ray-traced reflection samples', 'configure_ray_traced_reflections', {
      enabled: true,
      settings: { SamplesPerPixel: 1.5, MaxRoughness: 0.5 }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject below-min ray-traced AO intensity', 'configure_ray_traced_ao', {
      enabled: true,
      intensity: -0.1,
      settings: { Radius: 150 }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject nonnumeric ray-traced AO radius', 'configure_ray_traced_ao', {
      enabled: true,
      intensity: 1,
      settings: { Radius: 'wide' }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject below-min ray-traced AO radius', 'configure_ray_traced_ao', {
      enabled: true,
      intensity: 1,
      settings: { Radius: -1 }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject nonnumeric path-tracing samples', 'configure_path_tracing', {
      enabled: true,
      settings: { SamplesPerPixel: 'many', MaxBounces: 1 }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject fractional path-tracing samples', 'configure_path_tracing', {
      enabled: true,
      settings: { SamplesPerPixel: 1.5, MaxBounces: 1 }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject fractional scalar light channel', 'set_light_channel', {
      actorName: ctx.lightActor,
      channel: 1.5,
      enabled: true
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject missing actor light channel target', 'set_actor_light_channel', {
      targetActor: ctx.missingActor,
      channel: 2,
      enabled: true
    }, { expected: errorExpected('not found') }),
    createNamedActionCase('Adversarial: reject unsupported capture source enum', 'configure_capture_source', {
      actorName: ctx.sceneCapture2d,
      captureSource: 'DefinitelyNotACaptureSource'
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject oversized reflection capture resolution', 'configure_reflection_capture_resolution', {
      actorName: ctx.sphereCapture,
      resolution: 100000
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject oversized scene capture resolution', 'configure_capture_resolution', {
      actorName: ctx.sceneCapture2d,
      resolution: 100000
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject oversized screen percentage', 'configure_screen_percentage', {
      screenPercentage: 100000
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject oversized planar reflection screen percentage', 'configure_planar_reflection', {
      actorName: ctx.planarReflection,
      settings: { ScreenPercentage: 100000 }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject malformed post-process settings', 'configure_bloom', {
      actorName: ctx.postProcessVolume,
      settings: { BloomIntensity: { unexpected: true } }
    }, { expected: errorExpected('invalid') }),
    createNamedActionCase('Adversarial: reject missing LUT asset', 'set_pp_lut', {
      actorName: ctx.postProcessVolume,
      lutPath: ctx.missingLutPath
    }, { expected: errorExpected('not found', 'invalid') }),
    createNamedActionCase('Adversarial: reject missing render target assignment', 'assign_render_target', {
      actorName: ctx.sceneCapture2d,
      renderTargetPath: ctx.missingRenderTargetPath
    }, { expected: errorExpected('not found', 'invalid') }),
    createNamedActionCase('Adversarial: reject missing capture actor', 'capture_scene', {
      actorName: ctx.missingActor
    }, { expected: errorExpected('not found') }),
    // Path-traversal adversarial cases for the new render string fields.
    // The C++ side uses LoadObject / EditorAssetLibrary which restricts to
    // project content roots; the new public surface should reject paths
    // outside /Game (or surface a controlled error). Any test that lets
    // `..` or absolute host paths reach engine code would catch a
    // regression of the asset-path security boundary.
    {
      scenario: 'Adversarial: reject parent-segment in render target path',
      toolName: 'manage_asset',
      arguments: {
        action: 'create_render_target',
        name: 'EvilRT',
        width: 64,
        height: 64,
        path: '/Game/../../../etc/Evil'
      },
      expected: errorExpected('not found', 'invalid', 'unsupported')
    },
    createNamedActionCase('Adversarial: reject parent-segment in LUT asset path', 'set_pp_lut', {
      actorName: ctx.postProcessVolume,
      lutPath: '/Game/../../Engine/Content/EngineMaterials/Default'
    }, { expected: errorExpected('not found', 'invalid', 'unsupported') }),
    createNamedActionCase('Adversarial: reject absolute host path in render target', 'assign_render_target', {
      actorName: ctx.sceneCapture2d,
      renderTargetPath: '/etc/passwd'
    }, { expected: errorExpected('not found', 'invalid', 'unsupported') }),
    createNamedActionCase('Adversarial: reject Windows absolute path in render target', 'assign_render_target', {
      actorName: ctx.sceneCapture2d,
      renderTargetPath: 'C:\\Windows\\System32\\drivers\\etc\\hosts'
    }, { expected: errorExpected('not found', 'invalid', 'unsupported') })
  ];
}

function createRenderingCleanupCases(ctx) {
  return [
    {
      scenario: 'Cleanup: delete scene capture 2D actor',
      toolName: 'control_actor',
      arguments: { action: 'delete', actorName: ctx.sceneCapture2d },
      expected: successExpected('not found')
    },
    {
      scenario: 'Cleanup: delete scene capture cube actor',
      toolName: 'control_actor',
      arguments: { action: 'delete', actorName: ctx.sceneCaptureCube },
      expected: successExpected('not found')
    },
    {
      scenario: 'Cleanup: delete planar reflection actor',
      toolName: 'control_actor',
      arguments: { action: 'delete', actorName: ctx.planarReflection },
      expected: successExpected('not found')
    },
    {
      scenario: 'Cleanup: delete sphere reflection capture actor',
      toolName: 'control_actor',
      arguments: { action: 'delete', actorName: ctx.sphereCapture },
      expected: successExpected('not found')
    },
    {
      scenario: 'Cleanup: delete box reflection capture actor',
      toolName: 'control_actor',
      arguments: { action: 'delete', actorName: ctx.boxCapture },
      expected: successExpected('not found')
    },
    {
      scenario: 'Cleanup: delete directional light actor',
      toolName: 'control_actor',
      arguments: { action: 'delete', actorName: ctx.lightActor },
      expected: successExpected('not found')
    },
    {
      scenario: 'Cleanup: delete target actor',
      toolName: 'control_actor',
      arguments: { action: 'delete', actorName: ctx.targetActor },
      expected: successExpected('not found')
    },
    {
      scenario: 'Cleanup: delete attached post process volume actor',
      toolName: 'control_actor',
      arguments: { action: 'delete', actorName: ctx.attachedPostProcessVolume },
      expected: successExpected('not found')
    },
    {
      scenario: 'Cleanup: delete post process volume actor',
      toolName: 'control_actor',
      arguments: { action: 'delete', actorName: ctx.postProcessVolume },
      expected: successExpected('not found')
    },
    {
      scenario: 'Cleanup: delete lightmass volume actor',
      toolName: 'control_actor',
      arguments: { action: 'delete', actorName: ctx.lightmassVolume },
      expected: successExpected('not found')
    },
    {
      scenario: 'Cleanup: delete build environment lightmass volume actor',
      toolName: 'control_actor',
      arguments: { action: 'delete', actorName: ctx.lightingLightmassVolume },
      expected: successExpected('not found')
    },
    {
      scenario: 'Cleanup: delete seeded render target asset',
      toolName: 'manage_asset',
      arguments: { action: 'delete', path: ctx.renderTargetPath, force: true },
      expected: successExpected('not found')
    },
    {
      scenario: 'Cleanup: delete rendering test folder',
      toolName: 'manage_asset',
      arguments: { action: 'delete', path: ctx.folder, force: true },
      expected: successExpected('not found')
    }
  ];
}

function createRenderingCaseSet(ts = Date.now()) {
  const context = createRenderingContext(ts);
  const setupCases = createRenderingSetupCases(context);
  const happyCases = createRenderingHappyCases(context);
  const adversarialCases = createRenderingAdversarialCases(context);
  const cleanupCases = createRenderingCleanupCases(context);

  return {
    context,
    setupCases,
    happyCases,
    adversarialCases,
    cleanupCases,
    suiteCases: [...setupCases, ...happyCases, ...adversarialCases, ...cleanupCases]
  };
}

module.exports = {
  createRenderingAdversarialCases,
  createRenderingCaseSet,
  createRenderingCleanupCases,
  createRenderingContext,
  createRenderingHappyCases,
  createRenderingSetupCases
};
