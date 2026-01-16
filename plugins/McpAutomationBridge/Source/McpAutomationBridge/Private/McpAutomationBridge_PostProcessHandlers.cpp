// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 29: Post-Process & Rendering Handlers for MCP Automation Bridge

#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/Paths.h"
#include "Math/UnrealMathUtility.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorAssetLibrary.h"

#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif

// Post-Process & Rendering Headers
#include "Engine/PostProcessVolume.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCaptureCube.h"
#include "Components/SceneCaptureComponentCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTargetCube.h"
#include "Components/ReflectionCaptureComponent.h"
#include "Components/SphereReflectionCaptureComponent.h"
#include "Components/BoxReflectionCaptureComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PlanarReflectionComponent.h"
#include "Components/LightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/RendererSettings.h"
#include "GameFramework/WorldSettings.h"
#include "LevelEditorSubsystem.h"
#include "UnrealEdGlobals.h"

#endif // WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandlePostProcessAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("manage_post_process"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("manage_post_process")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("manage_post_process payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  // Also check for action_type for compatibility
  if (SubAction.IsEmpty()) {
    Payload->TryGetStringField(TEXT("action_type"), SubAction);
  }
  const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("action"), LowerSub);
  bool bSuccess = true;
  FString Message = FString::Printf(TEXT("Post-process action '%s' completed"), *LowerSub);
  FString ErrorCode;

  if (!GEditor) {
    bSuccess = false;
    Message = TEXT("Editor not available");
    ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
    Resp->SetStringField(TEXT("error"), Message);
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
  }

  UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS) {
    bSuccess = false;
    Message = TEXT("EditorActorSubsystem not available");
    ErrorCode = TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING");
    Resp->SetStringField(TEXT("error"), Message);
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
  }

  // ========================================================================
  // CREATE POST PROCESS VOLUME
  // ========================================================================
  if (LowerSub == TEXT("create_post_process_volume")) {
    FVector Location(0, 0, 0);
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
      (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
      (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
      (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
    }
    
    FString Name;
    Payload->TryGetStringField(TEXT("name"), Name);
    if (Name.IsEmpty()) {
      Payload->TryGetStringField(TEXT("volumeName"), Name);
    }

    // Spawn post-process volume
    APostProcessVolume *PPV = SpawnActorInActiveWorld<APostProcessVolume>(
        APostProcessVolume::StaticClass(), Location, FRotator::ZeroRotator,
        Name.IsEmpty() ? TEXT("PostProcessVolume") : *Name);

    if (PPV) {
      // Configure infinite extent (unbound)
      bool bInfinite = true;
      Payload->TryGetBoolField(TEXT("infinite"), bInfinite);
      PPV->bUnbound = bInfinite;

      // Blend weight
      double BlendWeight = 1.0;
      if (Payload->TryGetNumberField(TEXT("blendWeight"), BlendWeight)) {
        PPV->BlendWeight = static_cast<float>(BlendWeight);
      }

      // Blend radius
      double BlendRadius = 100.0;
      if (Payload->TryGetNumberField(TEXT("blendRadius"), BlendRadius)) {
        PPV->BlendRadius = static_cast<float>(BlendRadius);
      }

      // Priority
      double Priority = 0.0;
      if (Payload->TryGetNumberField(TEXT("priority"), Priority)) {
        PPV->Priority = static_cast<float>(Priority);
      }

      // Enabled
      bool bEnabled = true;
      if (Payload->TryGetBoolField(TEXT("enabled"), bEnabled)) {
        PPV->bEnabled = bEnabled;
      }

      bSuccess = true;
      Message = TEXT("Post-process volume created");
      Resp->SetStringField(TEXT("actorName"), PPV->GetActorLabel());
      Resp->SetStringField(TEXT("volumeName"), PPV->GetName());
    } else {
      bSuccess = false;
      Message = TEXT("Failed to spawn post-process volume");
      ErrorCode = TEXT("SPAWN_FAILED");
    }
  }
  // ========================================================================
  // CONFIGURE PP BLEND
  // ========================================================================
  else if (LowerSub == TEXT("configure_pp_blend")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    if (ActorName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("volumeName"), ActorName);
    }

    APostProcessVolume *PPV = FindActorByLabelOrName<APostProcessVolume>(ActorName);
    if (PPV) {
      double BlendWeight = -1.0;
      if (Payload->TryGetNumberField(TEXT("blendWeight"), BlendWeight) && BlendWeight >= 0.0) {
        PPV->BlendWeight = static_cast<float>(BlendWeight);
      }

      double BlendRadius = -1.0;
      if (Payload->TryGetNumberField(TEXT("blendRadius"), BlendRadius) && BlendRadius >= 0.0) {
        PPV->BlendRadius = static_cast<float>(BlendRadius);
      }

      bSuccess = true;
      Message = TEXT("PP blend configured");
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("Post-process volume '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
  }
  // ========================================================================
  // CONFIGURE PP PRIORITY
  // ========================================================================
  else if (LowerSub == TEXT("configure_pp_priority")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    if (ActorName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("volumeName"), ActorName);
    }

    APostProcessVolume *PPV = FindActorByLabelOrName<APostProcessVolume>(ActorName);
    if (PPV) {
      double Priority = 0.0;
      if (Payload->TryGetNumberField(TEXT("priority"), Priority)) {
        PPV->Priority = static_cast<float>(Priority);
      }

      bSuccess = true;
      Message = TEXT("PP priority configured");
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("Post-process volume '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
  }
  // ========================================================================
  // GET POST PROCESS SETTINGS
  // ========================================================================
  else if (LowerSub == TEXT("get_post_process_settings")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    if (ActorName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("volumeName"), ActorName);
    }

    APostProcessVolume *PPV = FindActorByLabelOrName<APostProcessVolume>(ActorName);
    if (PPV) {
      TSharedPtr<FJsonObject> Settings = MakeShared<FJsonObject>();
      const FPostProcessSettings& S = PPV->Settings;

      // Bloom
      Settings->SetNumberField(TEXT("bloomIntensity"), S.BloomIntensity);
      Settings->SetNumberField(TEXT("bloomThreshold"), S.BloomThreshold);

      // DOF
      Settings->SetNumberField(TEXT("depthOfFieldFocalDistance"), S.DepthOfFieldFocalDistance);

      // Motion blur
      Settings->SetNumberField(TEXT("motionBlurAmount"), S.MotionBlurAmount);
      Settings->SetNumberField(TEXT("motionBlurMax"), S.MotionBlurMax);

      // Vignette
      Settings->SetNumberField(TEXT("vignetteIntensity"), S.VignetteIntensity);

      // Chromatic aberration
      Settings->SetNumberField(TEXT("sceneFringeIntensity"), S.SceneFringeIntensity);

      // Volume info
      Settings->SetNumberField(TEXT("blendWeight"), PPV->BlendWeight);
      Settings->SetNumberField(TEXT("blendRadius"), PPV->BlendRadius);
      Settings->SetNumberField(TEXT("priority"), PPV->Priority);
      Settings->SetBoolField(TEXT("unbound"), PPV->bUnbound);
      Settings->SetBoolField(TEXT("enabled"), PPV->bEnabled);

      Resp->SetObjectField(TEXT("postProcessSettings"), Settings);
      bSuccess = true;
      Message = TEXT("Post-process settings retrieved");
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("Post-process volume '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
  }
  // ========================================================================
  // CONFIGURE BLOOM
  // ========================================================================
  else if (LowerSub == TEXT("configure_bloom")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    if (ActorName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("volumeName"), ActorName);
    }

    APostProcessVolume *PPV = FindActorByLabelOrName<APostProcessVolume>(ActorName);
    if (PPV) {
      // CRITICAL: Must set bOverride flags before setting values
      double BloomIntensity = 0.0;
      if (Payload->TryGetNumberField(TEXT("bloomIntensity"), BloomIntensity)) {
        PPV->Settings.bOverride_BloomIntensity = true;
        PPV->Settings.BloomIntensity = static_cast<float>(BloomIntensity);
      }

      double BloomThreshold = 0.0;
      if (Payload->TryGetNumberField(TEXT("bloomThreshold"), BloomThreshold)) {
        PPV->Settings.bOverride_BloomThreshold = true;
        PPV->Settings.BloomThreshold = static_cast<float>(BloomThreshold);
      }

      double BloomSizeScale = 0.0;
      if (Payload->TryGetNumberField(TEXT("bloomSizeScale"), BloomSizeScale)) {
        PPV->Settings.bOverride_BloomSizeScale = true;
        PPV->Settings.BloomSizeScale = static_cast<float>(BloomSizeScale);
      }

      bSuccess = true;
      Message = TEXT("Bloom configured");
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("Post-process volume '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
  }
  // ========================================================================
  // CONFIGURE DOF
  // ========================================================================
  else if (LowerSub == TEXT("configure_dof")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    if (ActorName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("volumeName"), ActorName);
    }

    APostProcessVolume *PPV = FindActorByLabelOrName<APostProcessVolume>(ActorName);
    if (PPV) {
      double FocalDistance = 0.0;
      if (Payload->TryGetNumberField(TEXT("focalDistance"), FocalDistance)) {
        PPV->Settings.bOverride_DepthOfFieldFocalDistance = true;
        PPV->Settings.DepthOfFieldFocalDistance = static_cast<float>(FocalDistance);
      }

      double FocalRegion = 0.0;
      if (Payload->TryGetNumberField(TEXT("focalRegion"), FocalRegion)) {
        PPV->Settings.bOverride_DepthOfFieldFocalRegion = true;
        PPV->Settings.DepthOfFieldFocalRegion = static_cast<float>(FocalRegion);
      }

      double NearTransitionRegion = 0.0;
      if (Payload->TryGetNumberField(TEXT("nearTransitionRegion"), NearTransitionRegion)) {
        PPV->Settings.bOverride_DepthOfFieldNearTransitionRegion = true;
        PPV->Settings.DepthOfFieldNearTransitionRegion = static_cast<float>(NearTransitionRegion);
      }

      double FarTransitionRegion = 0.0;
      if (Payload->TryGetNumberField(TEXT("farTransitionRegion"), FarTransitionRegion)) {
        PPV->Settings.bOverride_DepthOfFieldFarTransitionRegion = true;
        PPV->Settings.DepthOfFieldFarTransitionRegion = static_cast<float>(FarTransitionRegion);
      }
      
      // DOF Method - Note: DepthOfFieldMethod property removed in UE 5.7
      // The engine now uses automatic DOF method selection based on quality settings
      // Keeping the parameter parsing for backwards compatibility but not applying it
      FString DepthOfFieldMethodStr;
      if (Payload->TryGetStringField(TEXT("depthOfFieldMethod"), DepthOfFieldMethodStr)) {
        // In UE 5.7+, DOF method is controlled by r.DepthOfFieldQuality CVar
        // BokehDOF, Gaussian, CircleDOF are selected automatically
        UE_LOG(LogTemp, Warning, TEXT("depthOfFieldMethod parameter ignored - UE 5.7 uses automatic DOF method selection"));
      }
      
      // Near/Far blur sizes
      double NearBlurSize = 0.0;
      if (Payload->TryGetNumberField(TEXT("nearBlurSize"), NearBlurSize)) {
        PPV->Settings.bOverride_DepthOfFieldNearBlurSize = true;
        PPV->Settings.DepthOfFieldNearBlurSize = static_cast<float>(NearBlurSize);
      }
      
      double FarBlurSize = 0.0;
      if (Payload->TryGetNumberField(TEXT("farBlurSize"), FarBlurSize)) {
        PPV->Settings.bOverride_DepthOfFieldFarBlurSize = true;
        PPV->Settings.DepthOfFieldFarBlurSize = static_cast<float>(FarBlurSize);
      }
      
      // Depth blur radius (for depthBlurRadius parameter)
      double DepthBlurRadius = 0.0;
      if (Payload->TryGetNumberField(TEXT("depthBlurRadius"), DepthBlurRadius)) {
        PPV->Settings.bOverride_DepthOfFieldDepthBlurRadius = true;
        PPV->Settings.DepthOfFieldDepthBlurRadius = static_cast<float>(DepthBlurRadius);
      }

      bSuccess = true;
      Message = TEXT("DOF configured");
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("Post-process volume '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
  }
  // ========================================================================
  // CONFIGURE MOTION BLUR
  // ========================================================================
  else if (LowerSub == TEXT("configure_motion_blur")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    if (ActorName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("volumeName"), ActorName);
    }

    APostProcessVolume *PPV = FindActorByLabelOrName<APostProcessVolume>(ActorName);
    if (PPV) {
      double MotionBlurAmount = -1.0;
      if (Payload->TryGetNumberField(TEXT("motionBlurAmount"), MotionBlurAmount)) {
        PPV->Settings.bOverride_MotionBlurAmount = true;
        PPV->Settings.MotionBlurAmount = static_cast<float>(MotionBlurAmount);
      }

      double MotionBlurMax = -1.0;
      if (Payload->TryGetNumberField(TEXT("motionBlurMax"), MotionBlurMax)) {
        PPV->Settings.bOverride_MotionBlurMax = true;
        PPV->Settings.MotionBlurMax = static_cast<float>(MotionBlurMax);
      }

      double MotionBlurPerObjectSize = -1.0;
      if (Payload->TryGetNumberField(TEXT("motionBlurPerObjectSize"), MotionBlurPerObjectSize)) {
        PPV->Settings.bOverride_MotionBlurPerObjectSize = true;
        PPV->Settings.MotionBlurPerObjectSize = static_cast<float>(MotionBlurPerObjectSize);
      }

      double MotionBlurTargetFPS = -1.0;
      if (Payload->TryGetNumberField(TEXT("motionBlurTargetFPS"), MotionBlurTargetFPS)) {
        PPV->Settings.bOverride_MotionBlurTargetFPS = true;
        PPV->Settings.MotionBlurTargetFPS = static_cast<int32>(MotionBlurTargetFPS);
      }

      bSuccess = true;
      Message = TEXT("Motion blur configured");
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("Post-process volume '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
  }
  // ========================================================================
  // CONFIGURE VIGNETTE
  // ========================================================================
  else if (LowerSub == TEXT("configure_vignette")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    if (ActorName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("volumeName"), ActorName);
    }

    APostProcessVolume *PPV = FindActorByLabelOrName<APostProcessVolume>(ActorName);
    if (PPV) {
      double VignetteIntensity = -1.0;
      if (Payload->TryGetNumberField(TEXT("vignetteIntensity"), VignetteIntensity)) {
        PPV->Settings.bOverride_VignetteIntensity = true;
        PPV->Settings.VignetteIntensity = static_cast<float>(VignetteIntensity);
      }

      bSuccess = true;
      Message = TEXT("Vignette configured");
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("Post-process volume '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
  }
  // ========================================================================
  // CONFIGURE CHROMATIC ABERRATION
  // ========================================================================
  else if (LowerSub == TEXT("configure_chromatic_aberration")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    if (ActorName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("volumeName"), ActorName);
    }

    APostProcessVolume *PPV = FindActorByLabelOrName<APostProcessVolume>(ActorName);
    if (PPV) {
      double Intensity = -1.0;
      if (Payload->TryGetNumberField(TEXT("chromaticAberrationIntensity"), Intensity)) {
        PPV->Settings.bOverride_SceneFringeIntensity = true;
        PPV->Settings.SceneFringeIntensity = static_cast<float>(Intensity);
      }

      double StartOffset = -1.0;
      if (Payload->TryGetNumberField(TEXT("chromaticAberrationStartOffset"), StartOffset)) {
        PPV->Settings.bOverride_ChromaticAberrationStartOffset = true;
        PPV->Settings.ChromaticAberrationStartOffset = static_cast<float>(StartOffset);
      }

      bSuccess = true;
      Message = TEXT("Chromatic aberration configured");
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("Post-process volume '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
  }
  // ========================================================================
  // CONFIGURE WHITE BALANCE
  // ========================================================================
  else if (LowerSub == TEXT("configure_white_balance")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    if (ActorName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("volumeName"), ActorName);
    }

    APostProcessVolume *PPV = FindActorByLabelOrName<APostProcessVolume>(ActorName);
    if (PPV) {
      double WhiteTemp = -999.0;
      if (Payload->TryGetNumberField(TEXT("whiteTemp"), WhiteTemp)) {
        PPV->Settings.bOverride_WhiteTemp = true;
        PPV->Settings.WhiteTemp = static_cast<float>(WhiteTemp);
      }

      double WhiteTint = -999.0;
      if (Payload->TryGetNumberField(TEXT("whiteTint"), WhiteTint)) {
        PPV->Settings.bOverride_WhiteTint = true;
        PPV->Settings.WhiteTint = static_cast<float>(WhiteTint);
      }

      bSuccess = true;
      Message = TEXT("White balance configured");
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("Post-process volume '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
  }
  // ========================================================================
  // CONFIGURE COLOR GRADING
  // ========================================================================
  else if (LowerSub == TEXT("configure_color_grading")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    if (ActorName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("volumeName"), ActorName);
    }

    APostProcessVolume *PPV = FindActorByLabelOrName<APostProcessVolume>(ActorName);
    if (PPV) {
      // Helper lambda to parse color from JSON object
      // Parse color/vector as FVector4 - accepts both r/g/b/a (color) and x/y/z/w (vector) conventions
      auto ParseColor = [](const TSharedPtr<FJsonObject> *ColorObj) -> FVector4 {
        FVector4 Color(1.0f, 1.0f, 1.0f, 1.0f);
        if (ColorObj && *ColorObj) {
          // Try x/y/z/w first (UE convention for FPostProcessSettings color multipliers)
          if (!(*ColorObj)->TryGetNumberField(TEXT("x"), Color.X)) {
            (*ColorObj)->TryGetNumberField(TEXT("r"), Color.X);
          }
          if (!(*ColorObj)->TryGetNumberField(TEXT("y"), Color.Y)) {
            (*ColorObj)->TryGetNumberField(TEXT("g"), Color.Y);
          }
          if (!(*ColorObj)->TryGetNumberField(TEXT("z"), Color.Z)) {
            (*ColorObj)->TryGetNumberField(TEXT("b"), Color.Z);
          }
          if (!(*ColorObj)->TryGetNumberField(TEXT("w"), Color.W)) {
            (*ColorObj)->TryGetNumberField(TEXT("a"), Color.W);
          }
        }
        return Color;
      };

      const TSharedPtr<FJsonObject> *ColorObj = nullptr;

      if (Payload->TryGetObjectField(TEXT("globalSaturation"), ColorObj)) {
        FVector4 C = ParseColor(ColorObj);
        PPV->Settings.bOverride_ColorSaturation = true;
        PPV->Settings.ColorSaturation = C;
      }

      if (Payload->TryGetObjectField(TEXT("globalContrast"), ColorObj)) {
        FVector4 C = ParseColor(ColorObj);
        PPV->Settings.bOverride_ColorContrast = true;
        PPV->Settings.ColorContrast = C;
      }

      if (Payload->TryGetObjectField(TEXT("globalGamma"), ColorObj)) {
        FVector4 C = ParseColor(ColorObj);
        PPV->Settings.bOverride_ColorGamma = true;
        PPV->Settings.ColorGamma = C;
      }

      if (Payload->TryGetObjectField(TEXT("globalGain"), ColorObj)) {
        FVector4 C = ParseColor(ColorObj);
        PPV->Settings.bOverride_ColorGain = true;
        PPV->Settings.ColorGain = C;
      }

      if (Payload->TryGetObjectField(TEXT("globalOffset"), ColorObj)) {
        FVector4 C = ParseColor(ColorObj);
        PPV->Settings.bOverride_ColorOffset = true;
        PPV->Settings.ColorOffset = C;
      }

      if (Payload->TryGetObjectField(TEXT("sceneColorTint"), ColorObj)) {
        FVector4 C = ParseColor(ColorObj);
        PPV->Settings.bOverride_SceneColorTint = true;
        PPV->Settings.SceneColorTint = FLinearColor(C.X, C.Y, C.Z, C.W);
      }

      bSuccess = true;
      Message = TEXT("Color grading configured");
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("Post-process volume '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
  }
  // ========================================================================
  // CONFIGURE FILM GRAIN
  // ========================================================================
  else if (LowerSub == TEXT("configure_film_grain")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    if (ActorName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("volumeName"), ActorName);
    }

    APostProcessVolume *PPV = FindActorByLabelOrName<APostProcessVolume>(ActorName);
    if (PPV) {
      double FilmGrainIntensity = -1.0;
      if (Payload->TryGetNumberField(TEXT("filmGrainIntensity"), FilmGrainIntensity)) {
        PPV->Settings.bOverride_FilmGrainIntensity = true;
        PPV->Settings.FilmGrainIntensity = static_cast<float>(FilmGrainIntensity);
      }

      double FilmGrainIntensityShadows = -1.0;
      if (Payload->TryGetNumberField(TEXT("filmGrainIntensityShadows"), FilmGrainIntensityShadows)) {
        PPV->Settings.bOverride_FilmGrainIntensityShadows = true;
        PPV->Settings.FilmGrainIntensityShadows = static_cast<float>(FilmGrainIntensityShadows);
      }

      double FilmGrainIntensityMidtones = -1.0;
      if (Payload->TryGetNumberField(TEXT("filmGrainIntensityMidtones"), FilmGrainIntensityMidtones)) {
        PPV->Settings.bOverride_FilmGrainIntensityMidtones = true;
        PPV->Settings.FilmGrainIntensityMidtones = static_cast<float>(FilmGrainIntensityMidtones);
      }

      double FilmGrainIntensityHighlights = -1.0;
      if (Payload->TryGetNumberField(TEXT("filmGrainIntensityHighlights"), FilmGrainIntensityHighlights)) {
        PPV->Settings.bOverride_FilmGrainIntensityHighlights = true;
        PPV->Settings.FilmGrainIntensityHighlights = static_cast<float>(FilmGrainIntensityHighlights);
      }

      bSuccess = true;
      Message = TEXT("Film grain configured");
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("Post-process volume '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
  }
  // ========================================================================
  // CONFIGURE LENS FLARES
  // ========================================================================
  else if (LowerSub == TEXT("configure_lens_flares")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    if (ActorName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("volumeName"), ActorName);
    }

    APostProcessVolume *PPV = FindActorByLabelOrName<APostProcessVolume>(ActorName);
    if (PPV) {
      double LensFlareIntensity = -1.0;
      if (Payload->TryGetNumberField(TEXT("lensFlareIntensity"), LensFlareIntensity)) {
        PPV->Settings.bOverride_LensFlareIntensity = true;
        PPV->Settings.LensFlareIntensity = static_cast<float>(LensFlareIntensity);
      }

      double LensFlareBokehSize = -1.0;
      if (Payload->TryGetNumberField(TEXT("lensFlareBokehSize"), LensFlareBokehSize)) {
        PPV->Settings.bOverride_LensFlareBokehSize = true;
        PPV->Settings.LensFlareBokehSize = static_cast<float>(LensFlareBokehSize);
      }

      double LensFlareThreshold = -1.0;
      if (Payload->TryGetNumberField(TEXT("lensFlareThreshold"), LensFlareThreshold)) {
        PPV->Settings.bOverride_LensFlareThreshold = true;
        PPV->Settings.LensFlareThreshold = static_cast<float>(LensFlareThreshold);
      }

      const TSharedPtr<FJsonObject> *TintObj = nullptr;
      if (Payload->TryGetObjectField(TEXT("lensFlareTint"), TintObj) && TintObj) {
        double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
        (*TintObj)->TryGetNumberField(TEXT("r"), R);
        (*TintObj)->TryGetNumberField(TEXT("g"), G);
        (*TintObj)->TryGetNumberField(TEXT("b"), B);
        (*TintObj)->TryGetNumberField(TEXT("a"), A);
        PPV->Settings.bOverride_LensFlareTint = true;
        PPV->Settings.LensFlareTint = FLinearColor(R, G, B, A);
      }

      bSuccess = true;
      Message = TEXT("Lens flares configured");
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("Post-process volume '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
  }
  // ========================================================================
  // CREATE SPHERE REFLECTION CAPTURE
  // ========================================================================
  else if (LowerSub == TEXT("create_sphere_reflection_capture")) {
    FVector Location(0, 0, 0);
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
      (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
      (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
      (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
    }
    
    FString Name;
    Payload->TryGetStringField(TEXT("name"), Name);

    // Spawn sphere reflection capture actor
    UClass *SphereCapClass = LoadClass<AActor>(nullptr, TEXT("/Script/Engine.SphereReflectionCapture"));
    if (SphereCapClass) {
      AActor *SphereCapture = SpawnActorInActiveWorld<AActor>(
          SphereCapClass, Location, FRotator::ZeroRotator,
          Name.IsEmpty() ? TEXT("SphereReflectionCapture") : *Name);

      if (SphereCapture) {
        USphereReflectionCaptureComponent *CaptureComp = SphereCapture->FindComponentByClass<USphereReflectionCaptureComponent>();
        if (CaptureComp) {
          double InfluenceRadius = 3000.0;
          if (Payload->TryGetNumberField(TEXT("influenceRadius"), InfluenceRadius)) {
            CaptureComp->InfluenceRadius = static_cast<float>(InfluenceRadius);
          }

          double Brightness = 1.0;
          if (Payload->TryGetNumberField(TEXT("brightness"), Brightness)) {
            CaptureComp->Brightness = static_cast<float>(Brightness);
          }

          // Mark dirty for recapture
          CaptureComp->MarkDirtyForRecapture();
        }

        bSuccess = true;
        Message = TEXT("Sphere reflection capture created");
        Resp->SetStringField(TEXT("actorName"), SphereCapture->GetActorLabel());
      } else {
        bSuccess = false;
        Message = TEXT("Failed to spawn sphere reflection capture");
        ErrorCode = TEXT("SPAWN_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("SphereReflectionCapture class not found");
      ErrorCode = TEXT("CLASS_NOT_FOUND");
    }
  }
  // ========================================================================
  // CREATE BOX REFLECTION CAPTURE
  // ========================================================================
  else if (LowerSub == TEXT("create_box_reflection_capture")) {
    FVector Location(0, 0, 0);
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
      (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
      (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
      (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
    }
    
    FString Name;
    Payload->TryGetStringField(TEXT("name"), Name);

    UClass *BoxCapClass = LoadClass<AActor>(nullptr, TEXT("/Script/Engine.BoxReflectionCapture"));
    if (BoxCapClass) {
      AActor *BoxCapture = SpawnActorInActiveWorld<AActor>(
          BoxCapClass, Location, FRotator::ZeroRotator,
          Name.IsEmpty() ? TEXT("BoxReflectionCapture") : *Name);

      if (BoxCapture) {
        UBoxReflectionCaptureComponent *CaptureComp = BoxCapture->FindComponentByClass<UBoxReflectionCaptureComponent>();
        if (CaptureComp) {
          // Box extent - In UE 5.7, use PreviewInfluenceBox component
          const TSharedPtr<FJsonObject> *ExtentObj = nullptr;
          if (Payload->TryGetObjectField(TEXT("boxExtent"), ExtentObj) && ExtentObj) {
            double X = 1000.0, Y = 1000.0, Z = 1000.0;
            (*ExtentObj)->TryGetNumberField(TEXT("x"), X);
            (*ExtentObj)->TryGetNumberField(TEXT("y"), Y);
            (*ExtentObj)->TryGetNumberField(TEXT("z"), Z);
            // UE 5.7: BoxExtent is now controlled via PreviewInfluenceBox
            if (CaptureComp->PreviewInfluenceBox) {
              CaptureComp->PreviewInfluenceBox->SetBoxExtent(FVector(X, Y, Z));
            }
          }

          double BoxTransitionDistance = 100.0;
          if (Payload->TryGetNumberField(TEXT("boxTransitionDistance"), BoxTransitionDistance)) {
            CaptureComp->BoxTransitionDistance = static_cast<float>(BoxTransitionDistance);
          }

          double Brightness = 1.0;
          if (Payload->TryGetNumberField(TEXT("brightness"), Brightness)) {
            CaptureComp->Brightness = static_cast<float>(Brightness);
          }

          CaptureComp->MarkDirtyForRecapture();
        }

        bSuccess = true;
        Message = TEXT("Box reflection capture created");
        Resp->SetStringField(TEXT("actorName"), BoxCapture->GetActorLabel());
      } else {
        bSuccess = false;
        Message = TEXT("Failed to spawn box reflection capture");
        ErrorCode = TEXT("SPAWN_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("BoxReflectionCapture class not found");
      ErrorCode = TEXT("CLASS_NOT_FOUND");
    }
  }
  // ========================================================================
  // CREATE PLANAR REFLECTION
  // ========================================================================
  else if (LowerSub == TEXT("create_planar_reflection")) {
    FVector Location(0, 0, 0);
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
      (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
      (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
      (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
    }

    FRotator Rotation(0, 0, 0);
    const TSharedPtr<FJsonObject> *RotObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("rotation"), RotObj) && RotObj) {
      (*RotObj)->TryGetNumberField(TEXT("pitch"), Rotation.Pitch);
      (*RotObj)->TryGetNumberField(TEXT("yaw"), Rotation.Yaw);
      (*RotObj)->TryGetNumberField(TEXT("roll"), Rotation.Roll);
    }
    
    FString Name;
    Payload->TryGetStringField(TEXT("name"), Name);

    UClass *PlanarCapClass = LoadClass<AActor>(nullptr, TEXT("/Script/Engine.PlanarReflection"));
    if (PlanarCapClass) {
      AActor *PlanarReflection = SpawnActorInActiveWorld<AActor>(
          PlanarCapClass, Location, Rotation,
          Name.IsEmpty() ? TEXT("PlanarReflection") : *Name);

      if (PlanarReflection) {
        UPlanarReflectionComponent *ReflComp = PlanarReflection->FindComponentByClass<UPlanarReflectionComponent>();
        if (ReflComp) {
          double ScreenPercentage = 100.0;
          if (Payload->TryGetNumberField(TEXT("screenPercentage"), ScreenPercentage)) {
            ReflComp->ScreenPercentage = static_cast<int32>(FMath::Clamp(ScreenPercentage, 25.0, 100.0));
          }
        }

        bSuccess = true;
        Message = TEXT("Planar reflection created");
        Resp->SetStringField(TEXT("actorName"), PlanarReflection->GetActorLabel());
      } else {
        bSuccess = false;
        Message = TEXT("Failed to spawn planar reflection");
        ErrorCode = TEXT("SPAWN_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("PlanarReflection class not found");
      ErrorCode = TEXT("CLASS_NOT_FOUND");
    }
  }
  // ========================================================================
  // RECAPTURE SCENE
  // ========================================================================
  else if (LowerSub == TEXT("recapture_scene")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);

    if (ActorName.IsEmpty()) {
      // Recapture all reflection captures in the world
      UWorld *World = GetActiveWorld();
      if (World) {
        // Update all reflection captures
        UReflectionCaptureComponent::UpdateReflectionCaptureContents(World);
        bSuccess = true;
        Message = TEXT("All reflection captures recaptured");
      } else {
        bSuccess = false;
        Message = TEXT("No world available");
        ErrorCode = TEXT("NO_WORLD");
      }
    } else {
      // Recapture specific actor
      AActor *Actor = FindActorByLabelOrName<AActor>(ActorName);
      if (Actor) {
        UReflectionCaptureComponent *CaptureComp = Actor->FindComponentByClass<UReflectionCaptureComponent>();
        if (CaptureComp) {
          CaptureComp->MarkDirtyForRecapture();
          UReflectionCaptureComponent::UpdateReflectionCaptureContents(Actor->GetWorld());
          bSuccess = true;
          Message = TEXT("Reflection capture recaptured");
        } else {
          bSuccess = false;
          Message = TEXT("Actor does not have a reflection capture component");
          ErrorCode = TEXT("NO_REFLECTION_COMPONENT");
        }
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // CREATE SCENE CAPTURE 2D
  // ========================================================================
  else if (LowerSub == TEXT("create_scene_capture_2d")) {
    FVector Location(0, 0, 0);
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
      (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
      (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
      (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
    }

    FRotator Rotation(0, 0, 0);
    const TSharedPtr<FJsonObject> *RotObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("rotation"), RotObj) && RotObj) {
      (*RotObj)->TryGetNumberField(TEXT("pitch"), Rotation.Pitch);
      (*RotObj)->TryGetNumberField(TEXT("yaw"), Rotation.Yaw);
      (*RotObj)->TryGetNumberField(TEXT("roll"), Rotation.Roll);
    }
    
    FString Name;
    Payload->TryGetStringField(TEXT("name"), Name);

    ASceneCapture2D *SceneCapture = SpawnActorInActiveWorld<ASceneCapture2D>(
        ASceneCapture2D::StaticClass(), Location, Rotation,
        Name.IsEmpty() ? TEXT("SceneCapture2D") : *Name);

    if (SceneCapture) {
      USceneCaptureComponent2D *CaptureComp = SceneCapture->GetCaptureComponent2D();
      if (CaptureComp) {
        double FOV = 90.0;
        if (Payload->TryGetNumberField(TEXT("fov"), FOV)) {
          CaptureComp->FOVAngle = static_cast<float>(FOV);
        }

        // Create render target if resolution specified
        double Resolution = 0.0;
        double Width = 0.0;
        double Height = 0.0;
        Payload->TryGetNumberField(TEXT("captureResolution"), Resolution);
        Payload->TryGetNumberField(TEXT("captureWidth"), Width);
        Payload->TryGetNumberField(TEXT("captureHeight"), Height);

        if (Resolution > 0 || (Width > 0 && Height > 0)) {
          int32 W = (Width > 0) ? static_cast<int32>(Width) : static_cast<int32>(Resolution);
          int32 H = (Height > 0) ? static_cast<int32>(Height) : static_cast<int32>(Resolution);

          // Check if user wants to save render target as an asset
          FString TextureTargetPath;
          Payload->TryGetStringField(TEXT("textureTargetPath"), TextureTargetPath);
          
          UTextureRenderTarget2D *RenderTarget = nullptr;
          
          if (!TextureTargetPath.IsEmpty()) {
            // Create render target as a persistent asset
            FString PackagePath = FPackageName::ObjectPathToPackageName(TextureTargetPath);
            FString AssetName = FPackageName::GetLongPackageAssetName(TextureTargetPath);
            if (AssetName.IsEmpty()) {
              AssetName = FPaths::GetBaseFilename(TextureTargetPath);
            }
            
            UPackage* Package = CreatePackage(*PackagePath);
            if (Package) {
              RenderTarget = NewObject<UTextureRenderTarget2D>(Package, *AssetName, RF_Public | RF_Standalone);
              if (RenderTarget) {
                RenderTarget->InitAutoFormat(W, H);
                RenderTarget->UpdateResourceImmediate();
                Package->MarkPackageDirty();
                FAssetRegistryModule::AssetCreated(RenderTarget);
                Resp->SetStringField(TEXT("renderTargetPath"), TextureTargetPath);
              }
            }
          }
          
          if (!RenderTarget) {
            // Create transient render target (runtime-only, not saved)
            RenderTarget = NewObject<UTextureRenderTarget2D>();
            RenderTarget->InitAutoFormat(W, H);
            RenderTarget->UpdateResourceImmediate();
          }
          
          CaptureComp->TextureTarget = RenderTarget;
        }

        // Capture source
        FString CaptureSource;
        if (Payload->TryGetStringField(TEXT("captureSource"), CaptureSource)) {
          if (CaptureSource.Equals(TEXT("FinalColorLDR"), ESearchCase::IgnoreCase)) {
            CaptureComp->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
          } else if (CaptureSource.Equals(TEXT("SceneColorHDR"), ESearchCase::IgnoreCase)) {
            CaptureComp->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
          } else if (CaptureSource.Equals(TEXT("SceneDepth"), ESearchCase::IgnoreCase)) {
            CaptureComp->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;
          } else if (CaptureSource.Equals(TEXT("Normal"), ESearchCase::IgnoreCase)) {
            CaptureComp->CaptureSource = ESceneCaptureSource::SCS_Normal;
          } else if (CaptureSource.Equals(TEXT("BaseColor"), ESearchCase::IgnoreCase)) {
            CaptureComp->CaptureSource = ESceneCaptureSource::SCS_BaseColor;
          }
        }
      }

      bSuccess = true;
      Message = TEXT("Scene capture 2D created");
      Resp->SetStringField(TEXT("actorName"), SceneCapture->GetActorLabel());
    } else {
      bSuccess = false;
      Message = TEXT("Failed to spawn scene capture 2D");
      ErrorCode = TEXT("SPAWN_FAILED");
    }
  }
  // ========================================================================
  // CREATE SCENE CAPTURE CUBE
  // ========================================================================
  else if (LowerSub == TEXT("create_scene_capture_cube")) {
    FVector Location(0, 0, 0);
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
      (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
      (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
      (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
    }
    
    FString Name;
    Payload->TryGetStringField(TEXT("name"), Name);

    ASceneCaptureCube *SceneCapture = SpawnActorInActiveWorld<ASceneCaptureCube>(
        ASceneCaptureCube::StaticClass(), Location, FRotator::ZeroRotator,
        Name.IsEmpty() ? TEXT("SceneCaptureCube") : *Name);

    if (SceneCapture) {
      USceneCaptureComponentCube *CaptureComp = SceneCapture->GetCaptureComponentCube();
      if (CaptureComp) {
        double Resolution = 256.0;
        Payload->TryGetNumberField(TEXT("captureResolution"), Resolution);
        
        // Check if user wants to save render target as an asset
        FString TextureTargetPath;
        Payload->TryGetStringField(TEXT("textureTargetPath"), TextureTargetPath);
        
        UTextureRenderTargetCube *RenderTarget = nullptr;
        
        if (!TextureTargetPath.IsEmpty()) {
          // Create render target as a persistent asset
          FString PackagePath = FPackageName::ObjectPathToPackageName(TextureTargetPath);
          FString AssetName = FPackageName::GetLongPackageAssetName(TextureTargetPath);
          if (AssetName.IsEmpty()) {
            AssetName = FPaths::GetBaseFilename(TextureTargetPath);
          }
          
          UPackage* Package = CreatePackage(*PackagePath);
          if (Package) {
            RenderTarget = NewObject<UTextureRenderTargetCube>(Package, *AssetName, RF_Public | RF_Standalone);
            if (RenderTarget) {
              RenderTarget->Init(static_cast<uint32>(Resolution), PF_FloatRGBA);
              RenderTarget->UpdateResourceImmediate();
              Package->MarkPackageDirty();
              FAssetRegistryModule::AssetCreated(RenderTarget);
              Resp->SetStringField(TEXT("renderTargetPath"), TextureTargetPath);
            }
          }
        }
        
        if (!RenderTarget) {
          // Create transient render target (runtime-only, not saved)
          RenderTarget = NewObject<UTextureRenderTargetCube>();
          RenderTarget->Init(static_cast<uint32>(Resolution), PF_FloatRGBA);
          RenderTarget->UpdateResourceImmediate();
        }
        
        CaptureComp->TextureTarget = RenderTarget;
      }

      bSuccess = true;
      Message = TEXT("Scene capture cube created");
      Resp->SetStringField(TEXT("actorName"), SceneCapture->GetActorLabel());
    } else {
      bSuccess = false;
      Message = TEXT("Failed to spawn scene capture cube");
      ErrorCode = TEXT("SPAWN_FAILED");
    }
  }
  // ========================================================================
  // CAPTURE SCENE
  // ========================================================================
  else if (LowerSub == TEXT("capture_scene")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);

    AActor *Actor = FindActorByLabelOrName<AActor>(ActorName);
    if (Actor) {
      USceneCaptureComponent2D *Capture2D = Actor->FindComponentByClass<USceneCaptureComponent2D>();
      USceneCaptureComponentCube *CaptureCube = Actor->FindComponentByClass<USceneCaptureComponentCube>();

      if (Capture2D) {
        Capture2D->CaptureScene();
        bSuccess = true;
        Message = TEXT("Scene captured (2D)");
      } else if (CaptureCube) {
        CaptureCube->CaptureScene();
        bSuccess = true;
        Message = TEXT("Scene captured (Cube)");
      } else {
        bSuccess = false;
        Message = TEXT("Actor does not have a scene capture component");
        ErrorCode = TEXT("NO_CAPTURE_COMPONENT");
      }
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
  }
  // ========================================================================
  // SET LIGHT CHANNEL
  // ========================================================================
  else if (LowerSub == TEXT("set_light_channel")) {
    FString LightActorName;
    Payload->TryGetStringField(TEXT("lightActorName"), LightActorName);
    if (LightActorName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("actorName"), LightActorName);
    }

    AActor *LightActor = FindActorByLabelOrName<AActor>(LightActorName);
    if (LightActor) {
      ULightComponent *LightComp = LightActor->FindComponentByClass<ULightComponent>();
      if (LightComp) {
        bool bChannel0 = true, bChannel1 = false, bChannel2 = false;
        Payload->TryGetBoolField(TEXT("channel0"), bChannel0);
        Payload->TryGetBoolField(TEXT("channel1"), bChannel1);
        Payload->TryGetBoolField(TEXT("channel2"), bChannel2);

        LightComp->SetLightingChannels(bChannel0, bChannel1, bChannel2);

        bSuccess = true;
        Message = TEXT("Light channel configured");

        TSharedPtr<FJsonObject> ChannelInfo = MakeShared<FJsonObject>();
        ChannelInfo->SetBoolField(TEXT("channel0"), bChannel0);
        ChannelInfo->SetBoolField(TEXT("channel1"), bChannel1);
        ChannelInfo->SetBoolField(TEXT("channel2"), bChannel2);
        Resp->SetObjectField(TEXT("lightChannels"), ChannelInfo);
      } else {
        bSuccess = false;
        Message = TEXT("Actor does not have a light component");
        ErrorCode = TEXT("NO_LIGHT_COMPONENT");
      }
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("Light actor '%s' not found"), *LightActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
  }
  // ========================================================================
  // SET ACTOR LIGHT CHANNEL
  // ========================================================================
  else if (LowerSub == TEXT("set_actor_light_channel")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);

    AActor *Actor = FindActorByLabelOrName<AActor>(ActorName);
    if (Actor) {
      bool bChannel0 = true, bChannel1 = false, bChannel2 = false;
      Payload->TryGetBoolField(TEXT("channel0"), bChannel0);
      Payload->TryGetBoolField(TEXT("channel1"), bChannel1);
      Payload->TryGetBoolField(TEXT("channel2"), bChannel2);

      // Set lighting channels on all primitive components
      TArray<UPrimitiveComponent*> PrimitiveComponents;
      Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

      int32 UpdatedCount = 0;
      for (UPrimitiveComponent *PrimComp : PrimitiveComponents) {
        if (PrimComp) {
          PrimComp->SetLightingChannels(bChannel0, bChannel1, bChannel2);
          UpdatedCount++;
        }
      }

      bSuccess = true;
      Message = FString::Printf(TEXT("Light channels set on %d components"), UpdatedCount);

      TSharedPtr<FJsonObject> ChannelInfo = MakeShared<FJsonObject>();
      ChannelInfo->SetBoolField(TEXT("channel0"), bChannel0);
      ChannelInfo->SetBoolField(TEXT("channel1"), bChannel1);
      ChannelInfo->SetBoolField(TEXT("channel2"), bChannel2);
      Resp->SetObjectField(TEXT("lightChannels"), ChannelInfo);
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
  }
  // ========================================================================
  // RAY TRACING SETTINGS (via Console Variables)
  // ========================================================================
  else if (LowerSub == TEXT("configure_ray_traced_shadows") ||
           LowerSub == TEXT("configure_ray_traced_gi") ||
           LowerSub == TEXT("configure_ray_traced_reflections") ||
           LowerSub == TEXT("configure_ray_traced_ao") ||
           LowerSub == TEXT("configure_path_tracing")) {
    // Ray tracing settings are typically configured via console variables
    // These affect the global rendering state
    
    if (LowerSub == TEXT("configure_ray_traced_shadows")) {
      bool bEnabled = true;
      Payload->TryGetBoolField(TEXT("rayTracedShadowsEnabled"), bEnabled);
      
      IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Shadows"));
      if (CVar) {
        CVar->Set(bEnabled ? 1 : 0);
        
        // Apply additional tuning parameters if provided
        double SamplesPerPixel = 0.0;
        if (Payload->TryGetNumberField(TEXT("rayTracedShadowsSamplesPerPixel"), SamplesPerPixel)) {
          IConsoleVariable *SPPCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Shadows.SamplesPerPixel"));
          if (SPPCVar) {
            SPPCVar->Set(static_cast<int32>(SamplesPerPixel));
          }
        }
        
        bSuccess = true;
        Message = FString::Printf(TEXT("Ray traced shadows %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
      } else {
        bSuccess = false;
        Message = TEXT("Ray tracing shadows CVar not found. Ensure ray tracing is enabled in Project Settings > Engine > Rendering > Hardware Ray Tracing, and your GPU supports DXR/RTX.");
        ErrorCode = TEXT("RAYTRACING_NOT_AVAILABLE");
      }
    }
    else if (LowerSub == TEXT("configure_ray_traced_gi")) {
      bool bEnabled = true;
      Payload->TryGetBoolField(TEXT("rayTracedGIEnabled"), bEnabled);
      
      IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.GlobalIllumination"));
      if (CVar) {
        CVar->Set(bEnabled ? 1 : 0);
        
        // Apply additional tuning parameters
        double MaxBounces = 0.0;
        if (Payload->TryGetNumberField(TEXT("rayTracedGIMaxBounces"), MaxBounces)) {
          IConsoleVariable *BounceCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.GlobalIllumination.MaxBounces"));
          if (BounceCVar) {
            BounceCVar->Set(static_cast<int32>(MaxBounces));
          }
        }
        
        double SamplesPerPixel = 0.0;
        if (Payload->TryGetNumberField(TEXT("rayTracedGISamplesPerPixel"), SamplesPerPixel)) {
          IConsoleVariable *SPPCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.GlobalIllumination.SamplesPerPixel"));
          if (SPPCVar) {
            SPPCVar->Set(static_cast<int32>(SamplesPerPixel));
          }
        }
        
        bSuccess = true;
        Message = FString::Printf(TEXT("Ray traced GI %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
      } else {
        bSuccess = false;
        Message = TEXT("Ray tracing GI CVar not found. Ensure ray tracing is enabled in Project Settings > Engine > Rendering > Hardware Ray Tracing, and your GPU supports DXR/RTX.");
        ErrorCode = TEXT("RAYTRACING_NOT_AVAILABLE");
      }
    }
    else if (LowerSub == TEXT("configure_ray_traced_reflections")) {
      bool bEnabled = true;
      Payload->TryGetBoolField(TEXT("rayTracedReflectionsEnabled"), bEnabled);
      
      IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Reflections"));
      if (CVar) {
        CVar->Set(bEnabled ? 1 : 0);
        
        // Apply additional tuning parameters
        double MaxBounces = 0.0;
        if (Payload->TryGetNumberField(TEXT("rayTracedReflectionsMaxBounces"), MaxBounces)) {
          IConsoleVariable *BounceCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Reflections.MaxBounces"));
          if (BounceCVar) {
            BounceCVar->Set(static_cast<int32>(MaxBounces));
          }
        }
        
        double SamplesPerPixel = 0.0;
        if (Payload->TryGetNumberField(TEXT("rayTracedReflectionsSamplesPerPixel"), SamplesPerPixel)) {
          IConsoleVariable *SPPCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Reflections.SamplesPerPixel"));
          if (SPPCVar) {
            SPPCVar->Set(static_cast<int32>(SamplesPerPixel));
          }
        }
        
        double MaxRoughness = 0.0;
        if (Payload->TryGetNumberField(TEXT("rayTracedReflectionsMaxRoughness"), MaxRoughness)) {
          IConsoleVariable *RoughCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Reflections.MaxRoughness"));
          if (RoughCVar) {
            RoughCVar->Set(static_cast<float>(MaxRoughness));
          }
        }
        
        bSuccess = true;
        Message = FString::Printf(TEXT("Ray traced reflections %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
      } else {
        bSuccess = false;
        Message = TEXT("Ray tracing reflections CVar not found. Ensure ray tracing is enabled in Project Settings > Engine > Rendering > Hardware Ray Tracing, and your GPU supports DXR/RTX.");
        ErrorCode = TEXT("RAYTRACING_NOT_AVAILABLE");
      }
    }
    else if (LowerSub == TEXT("configure_ray_traced_ao")) {
      bool bEnabled = true;
      Payload->TryGetBoolField(TEXT("rayTracedAOEnabled"), bEnabled);
      
      IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.AmbientOcclusion"));
      if (CVar) {
        CVar->Set(bEnabled ? 1 : 0);
        
        // Apply additional tuning parameters
        double Intensity = 0.0;
        if (Payload->TryGetNumberField(TEXT("rayTracedAOIntensity"), Intensity)) {
          IConsoleVariable *IntCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.AmbientOcclusion.Intensity"));
          if (IntCVar) {
            IntCVar->Set(static_cast<float>(Intensity));
          }
        }
        
        double Radius = 0.0;
        if (Payload->TryGetNumberField(TEXT("rayTracedAORadius"), Radius)) {
          IConsoleVariable *RadCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.AmbientOcclusion.Radius"));
          if (RadCVar) {
            RadCVar->Set(static_cast<float>(Radius));
          }
        }
        
        double SamplesPerPixel = 0.0;
        if (Payload->TryGetNumberField(TEXT("rayTracedAOSamplesPerPixel"), SamplesPerPixel)) {
          IConsoleVariable *SPPCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.AmbientOcclusion.SamplesPerPixel"));
          if (SPPCVar) {
            SPPCVar->Set(static_cast<int32>(SamplesPerPixel));
          }
        }
        
        bSuccess = true;
        Message = FString::Printf(TEXT("Ray traced AO %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
      } else {
        bSuccess = false;
        Message = TEXT("Ray tracing AO CVar not found. Ensure ray tracing is enabled in Project Settings > Engine > Rendering > Hardware Ray Tracing, and your GPU supports DXR/RTX.");
        ErrorCode = TEXT("RAYTRACING_NOT_AVAILABLE");
      }
    }
    else if (LowerSub == TEXT("configure_path_tracing")) {
      bool bEnabled = true;
      Payload->TryGetBoolField(TEXT("pathTracingEnabled"), bEnabled);
      
      IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PathTracing"));
      if (CVar) {
        CVar->Set(bEnabled ? 1 : 0);
        
        // Apply additional tuning parameters
        double SamplesPerPixel = 0.0;
        if (Payload->TryGetNumberField(TEXT("pathTracingSamplesPerPixel"), SamplesPerPixel)) {
          IConsoleVariable *SPPCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PathTracing.SamplesPerPixel"));
          if (SPPCVar) {
            SPPCVar->Set(static_cast<int32>(SamplesPerPixel));
          }
        }
        
        double MaxBounces = 0.0;
        if (Payload->TryGetNumberField(TEXT("pathTracingMaxBounces"), MaxBounces)) {
          IConsoleVariable *BounceCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PathTracing.MaxBounces"));
          if (BounceCVar) {
            BounceCVar->Set(static_cast<int32>(MaxBounces));
          }
        }
        
        double FilterWidth = 0.0;
        if (Payload->TryGetNumberField(TEXT("pathTracingFilterWidth"), FilterWidth)) {
          IConsoleVariable *FilterCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PathTracing.FilterWidth"));
          if (FilterCVar) {
            FilterCVar->Set(static_cast<float>(FilterWidth));
          }
        }
        
        bSuccess = true;
        Message = FString::Printf(TEXT("Path tracing %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
      } else {
        bSuccess = false;
        Message = TEXT("Path tracing CVar not found. Ensure path tracing is enabled in Project Settings > Engine > Rendering > Path Tracing, and your GPU supports DXR/RTX.");
        ErrorCode = TEXT("PATHTRACING_NOT_AVAILABLE");
      }
    }
  }
  // ========================================================================
  // LIGHTMASS SETTINGS
  // ========================================================================
  else if (LowerSub == TEXT("configure_lightmass_settings")) {
    // Lightmass settings are configured through World Settings
    UWorld *World = GetActiveWorld();
    if (World && World->GetWorldSettings()) {
      AWorldSettings *WorldSettings = World->GetWorldSettings();

      double NumIndirectBounces = -1.0;
      if (Payload->TryGetNumberField(TEXT("numIndirectBounces"), NumIndirectBounces)) {
        WorldSettings->LightmassSettings.NumIndirectLightingBounces = static_cast<int32>(NumIndirectBounces);
      }

      double IndirectLightingQuality = -1.0;
      if (Payload->TryGetNumberField(TEXT("indirectLightingQuality"), IndirectLightingQuality)) {
        WorldSettings->LightmassSettings.IndirectLightingQuality = static_cast<float>(IndirectLightingQuality);
      }

      const TSharedPtr<FJsonObject> *EnvColorObj = nullptr;
      if (Payload->TryGetObjectField(TEXT("environmentColor"), EnvColorObj) && EnvColorObj) {
        double R = 0.0, G = 0.0, B = 0.0;
        (*EnvColorObj)->TryGetNumberField(TEXT("r"), R);
        (*EnvColorObj)->TryGetNumberField(TEXT("g"), G);
        (*EnvColorObj)->TryGetNumberField(TEXT("b"), B);
        WorldSettings->LightmassSettings.EnvironmentColor = FColor(
          static_cast<uint8>(FMath::Clamp(R * 255.0, 0.0, 255.0)),
          static_cast<uint8>(FMath::Clamp(G * 255.0, 0.0, 255.0)),
          static_cast<uint8>(FMath::Clamp(B * 255.0, 0.0, 255.0))
        );
      }

      double EnvironmentIntensity = -1.0;
      if (Payload->TryGetNumberField(TEXT("environmentIntensity"), EnvironmentIntensity)) {
        WorldSettings->LightmassSettings.EnvironmentIntensity = static_cast<float>(EnvironmentIntensity);
      }

      bSuccess = true;
      Message = TEXT("Lightmass settings configured");

      TSharedPtr<FJsonObject> LightmassInfo = MakeShared<FJsonObject>();
      LightmassInfo->SetNumberField(TEXT("numIndirectBounces"), WorldSettings->LightmassSettings.NumIndirectLightingBounces);
      LightmassInfo->SetNumberField(TEXT("indirectLightingQuality"), WorldSettings->LightmassSettings.IndirectLightingQuality);
      Resp->SetObjectField(TEXT("lightmassInfo"), LightmassInfo);
    } else {
      bSuccess = false;
      Message = TEXT("World settings not available");
      ErrorCode = TEXT("WORLD_SETTINGS_NOT_AVAILABLE");
    }
  }
  // ========================================================================
  // BUILD LIGHTING QUALITY
  // ========================================================================
  else if (LowerSub == TEXT("build_lighting_quality")) {
    FString Quality;
    Payload->TryGetStringField(TEXT("quality"), Quality);

    // Execute build lighting with specified quality
    FString Command = TEXT("BUILD LIGHTING");
    if (Quality.Equals(TEXT("Preview"), ESearchCase::IgnoreCase)) {
      Command += TEXT(" QUALITY=Preview");
    } else if (Quality.Equals(TEXT("Medium"), ESearchCase::IgnoreCase)) {
      Command += TEXT(" QUALITY=Medium");
    } else if (Quality.Equals(TEXT("High"), ESearchCase::IgnoreCase)) {
      Command += TEXT(" QUALITY=High");
    } else if (Quality.Equals(TEXT("Production"), ESearchCase::IgnoreCase)) {
      Command += TEXT(" QUALITY=Production");
    }

    GEditor->Exec(GetActiveWorld(), *Command);
    bSuccess = true;
    Message = FString::Printf(TEXT("Lighting build initiated with quality: %s"), Quality.IsEmpty() ? TEXT("Default") : *Quality);
  }
  // ========================================================================
  // CONFIGURE INDIRECT LIGHTING CACHE
  // ========================================================================
  else if (LowerSub == TEXT("configure_indirect_lighting_cache")) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("indirectLightingCacheEnabled"), bEnabled);

    IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.IndirectLightingCache"));
    if (CVar) {
      CVar->Set(bEnabled ? 1 : 0);
      bSuccess = true;
      Message = FString::Printf(TEXT("Indirect lighting cache %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
    } else {
      bSuccess = false;
      Message = TEXT("Console variable not found");
      ErrorCode = TEXT("CVAR_NOT_FOUND");
    }
  }
  // ========================================================================
  // CONFIGURE VOLUMETRIC LIGHTMAP
  // ========================================================================
  else if (LowerSub == TEXT("configure_volumetric_lightmap")) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("volumetricLightmapEnabled"), bEnabled);

    IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VolumetricLightmap"));
    if (CVar) {
      CVar->Set(bEnabled ? 1 : 0);
      
      double DetailCellSize = -1.0;
      if (Payload->TryGetNumberField(TEXT("volumetricLightmapDetailCellSize"), DetailCellSize)) {
        IConsoleVariable *DetailCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VolumetricLightmapDetailCellSize"));
        if (DetailCVar) {
          DetailCVar->Set(static_cast<float>(DetailCellSize));
        }
      }

      bSuccess = true;
      Message = FString::Printf(TEXT("Volumetric lightmap %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
    } else {
      bSuccess = false;
      Message = TEXT("Console variable not found");
      ErrorCode = TEXT("CVAR_NOT_FOUND");
    }
  }
  // ========================================================================
  // UNKNOWN ACTION
  // ========================================================================
  else {
    bSuccess = false;
    Message = FString::Printf(TEXT("Unknown post-process action: %s"), *LowerSub);
    ErrorCode = TEXT("UNKNOWN_ACTION");
  }

  if (!bSuccess && !ErrorCode.IsEmpty()) {
    Resp->SetStringField(TEXT("error"), Message);
  }
  
  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
  return true;

#else
  // Editor not available
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("manage_post_process requires WITH_EDITOR."),
                      TEXT("EDITOR_ONLY"));
  return true;
#endif // WITH_EDITOR
}
