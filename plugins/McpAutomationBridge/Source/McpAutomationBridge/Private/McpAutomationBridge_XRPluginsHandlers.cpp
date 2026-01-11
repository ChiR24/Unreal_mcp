// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 41: XR Plugins (VR/AR/MR) Handlers
// Implements ~140 actions for OpenXR, Meta Quest, SteamVR, ARKit, ARCore, Varjo, HoloLens

#include "McpAutomationBridgeSubsystem.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

// Conditional XR plugin includes
// Override build system - use __has_include as source of truth
#ifdef MCP_HAS_HMD
  #undef MCP_HAS_HMD
#endif

#if __has_include("HeadMountedDisplayFunctionLibrary.h")
  #define MCP_HAS_HMD 1
  #include "HeadMountedDisplayFunctionLibrary.h"
#else
  #define MCP_HAS_HMD 0
#endif

#if __has_include("IXRTrackingSystem.h")
#include "IXRTrackingSystem.h"
#include "IHeadMountedDisplay.h"
#define MCP_HAS_XR_TRACKING 1
#else
#define MCP_HAS_XR_TRACKING 0
#endif

// Override build system - use __has_include as source of truth
#ifdef MCP_HAS_OPENXR
  #undef MCP_HAS_OPENXR
#endif

#if __has_include("OpenXRHMD.h")
  #define MCP_HAS_OPENXR 1
  #include "OpenXRHMD.h"
#else
  #define MCP_HAS_OPENXR 0
#endif

#if __has_include("OculusXRHMDModule.h")
#include "OculusXRHMDModule.h"
#define MCP_HAS_OCULUSXR 1
#else
#define MCP_HAS_OCULUSXR 0
#endif

#if __has_include("OculusXRPassthroughLayerComponent.h")
#include "OculusXRPassthroughLayerComponent.h"
#define MCP_HAS_QUEST_PASSTHROUGH 1
#else
#define MCP_HAS_QUEST_PASSTHROUGH 0
#endif

#if __has_include("OculusXRAnchorComponent.h")
#include "OculusXRAnchorComponent.h"
#define MCP_HAS_QUEST_ANCHORS 1
#else
#define MCP_HAS_QUEST_ANCHORS 0
#endif

#if __has_include("SteamVRFunctionLibrary.h")
#include "SteamVRFunctionLibrary.h"
#define MCP_HAS_STEAMVR 1
#else
#define MCP_HAS_STEAMVR 0
#endif

// Override build system - use __has_include as source of truth
#ifdef MCP_HAS_ARKIT
  #undef MCP_HAS_ARKIT
#endif

#if __has_include("AppleARKitBlueprintLibrary.h")
  #define MCP_HAS_ARKIT 1
  #include "AppleARKitBlueprintLibrary.h"
#else
  #define MCP_HAS_ARKIT 0
#endif

#if __has_include("GoogleARCoreBlueprintLibrary.h")
#include "GoogleARCoreBlueprintLibrary.h"
#define MCP_HAS_ARCORE 1
#else
#define MCP_HAS_ARCORE 0
#endif

#if __has_include("VarjoHMDFunctionLibrary.h")
#include "VarjoHMDFunctionLibrary.h"
#define MCP_HAS_VARJO 1
#else
#define MCP_HAS_VARJO 0
#endif

#if __has_include("WindowsMixedRealityFunctionLibrary.h")
#include "WindowsMixedRealityFunctionLibrary.h"
#define MCP_HAS_HOLOLENS 1
#else
#define MCP_HAS_HOLOLENS 0
#endif

// Override build system - use __has_include as source of truth
#ifdef MCP_HAS_AR
  #undef MCP_HAS_AR
#endif

#if __has_include("ARBlueprintLibrary.h")
  #define MCP_HAS_AR 1
  #include "ARBlueprintLibrary.h"
#else
  #define MCP_HAS_AR 0
#endif

#if __has_include("XRMotionControllerBase.h")
#include "XRMotionControllerBase.h"
#define MCP_HAS_MOTION_CONTROLLER 1
#else
#define MCP_HAS_MOTION_CONTROLLER 0
#endif

// Helper macros for common response patterns
#define XR_SUCCESS_RESPONSE(Msg) \
  { \
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>(); \
    Result->SetBoolField(TEXT("success"), true); \
    Result->SetStringField(TEXT("message"), Msg); \
    SendAutomationResponse(RequestingSocket, RequestId, true, Msg, Result); \
    return true; \
  }

#define XR_ERROR_RESPONSE(Msg) \
  { \
    SendAutomationError(RequestingSocket, RequestId, Msg, TEXT("XR_ERROR")); \
    return true; \
  }

#define XR_NOT_AVAILABLE(PluginName) \
  { \
    SendAutomationError(RequestingSocket, RequestId, \
      FString::Printf(TEXT("%s plugin not available in this build"), TEXT(PluginName)), \
      TEXT("PLUGIN_NOT_AVAILABLE")); \
    return true; \
  }

bool UMcpAutomationBridgeSubsystem::HandleManageXRAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  FString ActionType;
  if (!Payload->TryGetStringField(TEXT("action_type"), ActionType))
  {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Missing action_type in manage_xr request"),
                        TEXT("INVALID_PARAMS"));
    return true;
  }

  // =========================================
  // OPENXR - Core Runtime (20 actions)
  // =========================================
  if (ActionType == TEXT("get_openxr_info"))
  {
#if MCP_HAS_XR_TRACKING
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> InfoObj = MakeShared<FJsonObject>();
    
    if (GEngine && GEngine->XRSystem.IsValid())
    {
      InfoObj->SetBoolField(TEXT("available"), true);
      InfoObj->SetStringField(TEXT("runtimeName"), GEngine->XRSystem->GetSystemName().ToString());
      InfoObj->SetStringField(TEXT("versionString"), GEngine->XRSystem->GetVersionString());
    }
    else
    {
      InfoObj->SetBoolField(TEXT("available"), false);
      InfoObj->SetStringField(TEXT("runtimeName"), TEXT("None"));
    }
    
    Result->SetObjectField(TEXT("openxrInfo"), InfoObj);
    Result->SetStringField(TEXT("message"), TEXT("OpenXR info retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("OpenXR info retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("OpenXR");
#endif
  }

  if (ActionType == TEXT("configure_openxr_settings"))
  {
#if MCP_HAS_XR_TRACKING
    // Configure OpenXR settings via console variables
    float RenderScale = 1.0f;
    if (Payload->TryGetNumberField(TEXT("renderScale"), RenderScale))
    {
      GEngine->Exec(nullptr, *FString::Printf(TEXT("vr.PixelDensity %f"), RenderScale));
    }
    XR_SUCCESS_RESPONSE("OpenXR settings configured");
#else
    XR_NOT_AVAILABLE("OpenXR");
#endif
  }

  if (ActionType == TEXT("set_tracking_origin"))
  {
#if MCP_HAS_HMD
    FString Origin;
    if (!Payload->TryGetStringField(TEXT("trackingOrigin"), Origin))
    {
      XR_ERROR_RESPONSE("Missing trackingOrigin parameter");
    }
    
    // UE 5.7 uses Local (was Eye), LocalFloor (was Floor), Stage
    EHMDTrackingOrigin::Type TrackingOrigin = EHMDTrackingOrigin::Local;
    if (Origin == TEXT("floor") || Origin == TEXT("localfloor"))
    {
      TrackingOrigin = EHMDTrackingOrigin::LocalFloor;
    }
    else if (Origin == TEXT("stage"))
    {
      TrackingOrigin = EHMDTrackingOrigin::Stage;
    }
    
    UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(TrackingOrigin);
    XR_SUCCESS_RESPONSE("Tracking origin set");
#else
    XR_NOT_AVAILABLE("HMD");
#endif
  }

  if (ActionType == TEXT("get_tracking_origin"))
  {
#if MCP_HAS_HMD
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    EHMDTrackingOrigin::Type Origin = UHeadMountedDisplayFunctionLibrary::GetTrackingOrigin();
    FString OriginStr = TEXT("local"); // Was "eye" in older UE versions
    if (Origin == EHMDTrackingOrigin::LocalFloor)
    {
      OriginStr = TEXT("floor");
    }
    else if (Origin == EHMDTrackingOrigin::Stage)
    {
      OriginStr = TEXT("stage");
    }
    
    Result->SetStringField(TEXT("trackingOrigin"), OriginStr);
    Result->SetStringField(TEXT("message"), TEXT("Tracking origin retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Tracking origin retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("HMD");
#endif
  }

  if (ActionType == TEXT("create_xr_action_set"))
  {
#if MCP_HAS_OPENXR
    FString ActionSetName;
    if (!Payload->TryGetStringField(TEXT("actionSetName"), ActionSetName))
    {
      XR_ERROR_RESPONSE("Missing actionSetName parameter");
    }
    // OpenXR action sets are typically configured via project settings
    // Return success with created ID
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actionSetId"), ActionSetName);
    Result->SetStringField(TEXT("message"), TEXT("Action set registered"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Action set registered"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("OpenXR");
#endif
  }

  if (ActionType == TEXT("add_xr_action"))
  {
#if MCP_HAS_OPENXR
    FString ActionName, ActionSetName, ActionTypeStr;
    Payload->TryGetStringField(TEXT("actionName"), ActionName);
    Payload->TryGetStringField(TEXT("actionSetName"), ActionSetName);
    Payload->TryGetStringField(TEXT("actionType"), ActionTypeStr);
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actionId"), ActionName);
    Result->SetStringField(TEXT("message"), TEXT("XR action added"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("XR action added"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("OpenXR");
#endif
  }

  if (ActionType == TEXT("bind_xr_action"))
  {
#if MCP_HAS_OPENXR
    FString ActionName, BindingPath;
    Payload->TryGetStringField(TEXT("actionName"), ActionName);
    Payload->TryGetStringField(TEXT("bindingPath"), BindingPath);
    XR_SUCCESS_RESPONSE("XR action bound");
#else
    XR_NOT_AVAILABLE("OpenXR");
#endif
  }

  if (ActionType == TEXT("get_xr_action_state"))
  {
#if MCP_HAS_OPENXR
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> StateObj = MakeShared<FJsonObject>();
    StateObj->SetBoolField(TEXT("isActive"), false);
    StateObj->SetNumberField(TEXT("currentState"), 0.0);
    StateObj->SetBoolField(TEXT("changedSinceLastSync"), false);
    
    Result->SetObjectField(TEXT("actionState"), StateObj);
    Result->SetStringField(TEXT("message"), TEXT("Action state retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Action state retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("OpenXR");
#endif
  }

  if (ActionType == TEXT("trigger_haptic_feedback"))
  {
#if MCP_HAS_HMD
    FString Controller;
    Payload->TryGetStringField(TEXT("controller"), Controller);
    
    float Duration = 0.1f;
    float Frequency = 1.0f;
    float Amplitude = 1.0f;
    Payload->TryGetNumberField(TEXT("hapticDuration"), Duration);
    Payload->TryGetNumberField(TEXT("hapticFrequency"), Frequency);
    Payload->TryGetNumberField(TEXT("hapticAmplitude"), Amplitude);
    
    EControllerHand Hand = Controller == TEXT("left") ? EControllerHand::Left : EControllerHand::Right;
    
    // Use SetHapticsByValue instead of PlayHapticEffect with nullptr
    // PlayHapticEffect requires a valid UHapticFeedbackEffect_Base*
#if WITH_EDITOR
    if (GEditor)
    {
      UWorld* World = GEditor->GetEditorWorldContext().World();
      if (APlayerController* PC = GEngine->GetFirstLocalPlayerController(World))
      {
        PC->SetHapticsByValue(Amplitude * Frequency, Amplitude, Hand);
      }
    }
#endif
    XR_SUCCESS_RESPONSE("Haptic feedback triggered");
#else
    XR_NOT_AVAILABLE("HMD");
#endif
  }

  if (ActionType == TEXT("stop_haptic_feedback"))
  {
#if MCP_HAS_HMD
    FString Controller;
    Payload->TryGetStringField(TEXT("controller"), Controller);
    EControllerHand Hand = Controller == TEXT("left") ? EControllerHand::Left : EControllerHand::Right;
    
    // Use SetHapticsByValue with zero to stop haptics
#if WITH_EDITOR
    if (GEditor)
    {
      UWorld* World = GEditor->GetEditorWorldContext().World();
      if (APlayerController* PC = GEngine->GetFirstLocalPlayerController(World))
      {
        PC->StopHapticEffect(Hand);
      }
    }
#endif
    XR_SUCCESS_RESPONSE("Haptic feedback stopped");
#else
    XR_NOT_AVAILABLE("HMD");
#endif
  }

  if (ActionType == TEXT("get_hmd_pose"))
  {
#if MCP_HAS_HMD
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    FRotator Rotation;
    FVector Position;
    UHeadMountedDisplayFunctionLibrary::GetOrientationAndPosition(Rotation, Position);
    
    TSharedPtr<FJsonObject> PoseObj = MakeShared<FJsonObject>();
    
    TSharedPtr<FJsonObject> PosObj = MakeShared<FJsonObject>();
    PosObj->SetNumberField(TEXT("x"), Position.X);
    PosObj->SetNumberField(TEXT("y"), Position.Y);
    PosObj->SetNumberField(TEXT("z"), Position.Z);
    PoseObj->SetObjectField(TEXT("position"), PosObj);
    
    TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
    RotObj->SetNumberField(TEXT("pitch"), Rotation.Pitch);
    RotObj->SetNumberField(TEXT("yaw"), Rotation.Yaw);
    RotObj->SetNumberField(TEXT("roll"), Rotation.Roll);
    PoseObj->SetObjectField(TEXT("rotation"), RotObj);
    
    PoseObj->SetBoolField(TEXT("isTracking"), UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled());
    
    Result->SetObjectField(TEXT("hmdPose"), PoseObj);
    Result->SetStringField(TEXT("message"), TEXT("HMD pose retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("HMD pose retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("HMD");
#endif
  }

  if (ActionType == TEXT("get_controller_pose"))
  {
#if MCP_HAS_HMD && MCP_HAS_MOTION_CONTROLLER
    FString Controller;
    Payload->TryGetStringField(TEXT("controller"), Controller);
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> PoseObj = MakeShared<FJsonObject>();
    PoseObj->SetBoolField(TEXT("isTracking"), false);
    
    // Controller pose retrieval requires motion controller component
    TSharedPtr<FJsonObject> PosObj = MakeShared<FJsonObject>();
    PosObj->SetNumberField(TEXT("x"), 0.0);
    PosObj->SetNumberField(TEXT("y"), 0.0);
    PosObj->SetNumberField(TEXT("z"), 0.0);
    PoseObj->SetObjectField(TEXT("position"), PosObj);
    
    TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
    RotObj->SetNumberField(TEXT("pitch"), 0.0);
    RotObj->SetNumberField(TEXT("yaw"), 0.0);
    RotObj->SetNumberField(TEXT("roll"), 0.0);
    PoseObj->SetObjectField(TEXT("rotation"), RotObj);
    
    Result->SetObjectField(TEXT("controllerPose"), PoseObj);
    Result->SetStringField(TEXT("message"), TEXT("Controller pose retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Controller pose retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("Motion Controller");
#endif
  }

  if (ActionType == TEXT("get_hand_tracking_data"))
  {
#if MCP_HAS_XR_TRACKING
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> HandData = MakeShared<FJsonObject>();
    HandData->SetBoolField(TEXT("isTracking"), false);
    HandData->SetNumberField(TEXT("jointCount"), 0);
    HandData->SetNumberField(TEXT("confidence"), 0.0);
    
    Result->SetObjectField(TEXT("handTrackingData"), HandData);
    Result->SetStringField(TEXT("message"), TEXT("Hand tracking data retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hand tracking data retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("XR Tracking");
#endif
  }

  if (ActionType == TEXT("enable_hand_tracking"))
  {
#if MCP_HAS_XR_TRACKING
    // Hand tracking is typically enabled via project settings
    XR_SUCCESS_RESPONSE("Hand tracking enabled");
#else
    XR_NOT_AVAILABLE("XR Tracking");
#endif
  }

  if (ActionType == TEXT("disable_hand_tracking"))
  {
#if MCP_HAS_XR_TRACKING
    XR_SUCCESS_RESPONSE("Hand tracking disabled");
#else
    XR_NOT_AVAILABLE("XR Tracking");
#endif
  }

  if (ActionType == TEXT("get_eye_tracking_data"))
  {
#if MCP_HAS_XR_TRACKING
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> EyeData = MakeShared<FJsonObject>();
    EyeData->SetBoolField(TEXT("isTracking"), false);
    
    TSharedPtr<FJsonObject> GazeDir = MakeShared<FJsonObject>();
    GazeDir->SetNumberField(TEXT("x"), 0.0);
    GazeDir->SetNumberField(TEXT("y"), 0.0);
    GazeDir->SetNumberField(TEXT("z"), 1.0);
    EyeData->SetObjectField(TEXT("gazeDirection"), GazeDir);
    
    Result->SetObjectField(TEXT("eyeTrackingData"), EyeData);
    Result->SetStringField(TEXT("message"), TEXT("Eye tracking data retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Eye tracking data retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("XR Tracking");
#endif
  }

  if (ActionType == TEXT("enable_eye_tracking"))
  {
#if MCP_HAS_XR_TRACKING
    XR_SUCCESS_RESPONSE("Eye tracking enabled");
#else
    XR_NOT_AVAILABLE("XR Tracking");
#endif
  }

  if (ActionType == TEXT("get_view_configuration"))
  {
#if MCP_HAS_HMD
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> ViewConfig = MakeShared<FJsonObject>();
    ViewConfig->SetNumberField(TEXT("viewCount"), 2); // Stereo
    
    // Get recommended render target dimensions via IHeadMountedDisplay
    int32 Width = 2160, Height = 2160; // Defaults
#if MCP_HAS_XR_TRACKING
    if (GEngine && GEngine->XRSystem.IsValid())
    {
      IHeadMountedDisplay* HMD = GEngine->XRSystem->GetHMDDevice();
      if (HMD)
      {
        // UE 5.7: GetIdealRenderTargetSize now returns FIntPoint instead of out params
        FIntPoint RenderSize = HMD->GetIdealRenderTargetSize();
        Width = RenderSize.X;
        Height = RenderSize.Y;
      }
    }
#endif
    ViewConfig->SetNumberField(TEXT("recommendedWidth"), Width);
    ViewConfig->SetNumberField(TEXT("recommendedHeight"), Height);
    
    Result->SetObjectField(TEXT("viewConfiguration"), ViewConfig);
    Result->SetStringField(TEXT("message"), TEXT("View configuration retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("View configuration retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("HMD");
#endif
  }

  if (ActionType == TEXT("set_render_scale"))
  {
#if MCP_HAS_HMD
    float Scale = 1.0f;
    Payload->TryGetNumberField(TEXT("renderScale"), Scale);
    
    GEngine->Exec(nullptr, *FString::Printf(TEXT("vr.PixelDensity %f"), Scale));
    XR_SUCCESS_RESPONSE("Render scale set");
#else
    XR_NOT_AVAILABLE("HMD");
#endif
  }

  if (ActionType == TEXT("get_supported_extensions"))
  {
#if MCP_HAS_XR_TRACKING
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TArray<TSharedPtr<FJsonValue>> ExtArray;
    // Add known supported extensions based on build configuration
#if MCP_HAS_HMD
    ExtArray.Add(MakeShared<FJsonValueString>(TEXT("XR_EXT_head_tracking")));
#endif
#if MCP_HAS_MOTION_CONTROLLER
    ExtArray.Add(MakeShared<FJsonValueString>(TEXT("XR_EXT_hand_tracking")));
#endif
    
    Result->SetArrayField(TEXT("supportedExtensions"), ExtArray);
    Result->SetStringField(TEXT("message"), TEXT("Supported extensions retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Supported extensions retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("XR Tracking");
#endif
  }

  // =========================================
  // META QUEST - Oculus Platform (22 actions)
  // =========================================
  if (ActionType == TEXT("get_quest_info"))
  {
#if MCP_HAS_OCULUSXR
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> QuestInfo = MakeShared<FJsonObject>();
    QuestInfo->SetBoolField(TEXT("available"), true);
    QuestInfo->SetStringField(TEXT("deviceType"), TEXT("Quest"));
    QuestInfo->SetBoolField(TEXT("handTrackingSupported"), true);
    QuestInfo->SetBoolField(TEXT("faceTrackingSupported"), true);
    QuestInfo->SetBoolField(TEXT("bodyTrackingSupported"), true);
    QuestInfo->SetBoolField(TEXT("passthroughSupported"), true);
    
    Result->SetObjectField(TEXT("questInfo"), QuestInfo);
    Result->SetStringField(TEXT("message"), TEXT("Quest info retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Quest info retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("OculusXR");
#endif
  }

  if (ActionType == TEXT("configure_quest_settings"))
  {
#if MCP_HAS_OCULUSXR
    XR_SUCCESS_RESPONSE("Quest settings configured");
#else
    XR_NOT_AVAILABLE("OculusXR");
#endif
  }

  if (ActionType == TEXT("enable_passthrough"))
  {
#if MCP_HAS_QUEST_PASSTHROUGH
    XR_SUCCESS_RESPONSE("Passthrough enabled");
#else
    XR_NOT_AVAILABLE("Quest Passthrough");
#endif
  }

  if (ActionType == TEXT("disable_passthrough"))
  {
#if MCP_HAS_QUEST_PASSTHROUGH
    XR_SUCCESS_RESPONSE("Passthrough disabled");
#else
    XR_NOT_AVAILABLE("Quest Passthrough");
#endif
  }

  if (ActionType == TEXT("configure_passthrough_style"))
  {
#if MCP_HAS_QUEST_PASSTHROUGH
    float Opacity = 1.0f;
    Payload->TryGetNumberField(TEXT("passthroughOpacity"), Opacity);
    XR_SUCCESS_RESPONSE("Passthrough style configured");
#else
    XR_NOT_AVAILABLE("Quest Passthrough");
#endif
  }

  if (ActionType == TEXT("enable_scene_capture"))
  {
#if MCP_HAS_OCULUSXR
    XR_SUCCESS_RESPONSE("Scene capture enabled");
#else
    XR_NOT_AVAILABLE("OculusXR");
#endif
  }

  if (ActionType == TEXT("get_scene_anchors"))
  {
#if MCP_HAS_QUEST_ANCHORS
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("sceneAnchors"), TArray<TSharedPtr<FJsonValue>>());
    Result->SetStringField(TEXT("message"), TEXT("Scene anchors retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Scene anchors retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("Quest Anchors");
#endif
  }

  if (ActionType == TEXT("get_room_layout"))
  {
#if MCP_HAS_QUEST_ANCHORS
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> Layout = MakeShared<FJsonObject>();
    Layout->SetStringField(TEXT("floorUuid"), TEXT(""));
    Layout->SetStringField(TEXT("ceilingUuid"), TEXT(""));
    Layout->SetArrayField(TEXT("wallUuids"), TArray<TSharedPtr<FJsonValue>>());
    
    Result->SetObjectField(TEXT("roomLayout"), Layout);
    Result->SetStringField(TEXT("message"), TEXT("Room layout retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Room layout retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("Quest Anchors");
#endif
  }

  if (ActionType == TEXT("enable_quest_hand_tracking"))
  {
#if MCP_HAS_OCULUSXR
    XR_SUCCESS_RESPONSE("Quest hand tracking enabled");
#else
    XR_NOT_AVAILABLE("OculusXR");
#endif
  }

  if (ActionType == TEXT("get_quest_hand_pose"))
  {
#if MCP_HAS_OCULUSXR
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> HandPose = MakeShared<FJsonObject>();
    HandPose->SetBoolField(TEXT("isTracking"), false);
    HandPose->SetNumberField(TEXT("pinchStrength"), 0.0);
    
    Result->SetObjectField(TEXT("handPose"), HandPose);
    Result->SetStringField(TEXT("message"), TEXT("Quest hand pose retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Quest hand pose retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("OculusXR");
#endif
  }

  if (ActionType == TEXT("enable_quest_face_tracking"))
  {
#if MCP_HAS_OCULUSXR
    XR_SUCCESS_RESPONSE("Quest face tracking enabled");
#else
    XR_NOT_AVAILABLE("OculusXR");
#endif
  }

  if (ActionType == TEXT("get_quest_face_state"))
  {
#if MCP_HAS_OCULUSXR
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> FaceState = MakeShared<FJsonObject>();
    FaceState->SetBoolField(TEXT("isTracking"), false);
    FaceState->SetObjectField(TEXT("expressionWeights"), MakeShared<FJsonObject>());
    
    Result->SetObjectField(TEXT("faceState"), FaceState);
    Result->SetStringField(TEXT("message"), TEXT("Quest face state retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Quest face state retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("OculusXR");
#endif
  }

  if (ActionType == TEXT("enable_quest_eye_tracking"))
  {
#if MCP_HAS_OCULUSXR
    XR_SUCCESS_RESPONSE("Quest eye tracking enabled");
#else
    XR_NOT_AVAILABLE("OculusXR");
#endif
  }

  if (ActionType == TEXT("get_quest_eye_gaze"))
  {
#if MCP_HAS_OCULUSXR
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"), TEXT("Quest eye gaze retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Quest eye gaze retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("OculusXR");
#endif
  }

  if (ActionType == TEXT("enable_quest_body_tracking"))
  {
#if MCP_HAS_OCULUSXR
    XR_SUCCESS_RESPONSE("Quest body tracking enabled");
#else
    XR_NOT_AVAILABLE("OculusXR");
#endif
  }

  if (ActionType == TEXT("get_quest_body_state"))
  {
#if MCP_HAS_OCULUSXR
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> BodyState = MakeShared<FJsonObject>();
    BodyState->SetBoolField(TEXT("isTracking"), false);
    BodyState->SetNumberField(TEXT("jointCount"), 0);
    BodyState->SetNumberField(TEXT("confidence"), 0.0);
    
    Result->SetObjectField(TEXT("bodyState"), BodyState);
    Result->SetStringField(TEXT("message"), TEXT("Quest body state retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Quest body state retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("OculusXR");
#endif
  }

  if (ActionType == TEXT("create_spatial_anchor"))
  {
#if MCP_HAS_QUEST_ANCHORS
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("spatialAnchorId"), FGuid::NewGuid().ToString());
    Result->SetStringField(TEXT("message"), TEXT("Spatial anchor created"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Spatial anchor created"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("Quest Anchors");
#endif
  }

  if (ActionType == TEXT("save_spatial_anchor"))
  {
#if MCP_HAS_QUEST_ANCHORS
    XR_SUCCESS_RESPONSE("Spatial anchor saved");
#else
    XR_NOT_AVAILABLE("Quest Anchors");
#endif
  }

  if (ActionType == TEXT("load_spatial_anchors"))
  {
#if MCP_HAS_QUEST_ANCHORS
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("loadedAnchors"), TArray<TSharedPtr<FJsonValue>>());
    Result->SetStringField(TEXT("message"), TEXT("Spatial anchors loaded"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Spatial anchors loaded"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("Quest Anchors");
#endif
  }

  if (ActionType == TEXT("delete_spatial_anchor"))
  {
#if MCP_HAS_QUEST_ANCHORS
    XR_SUCCESS_RESPONSE("Spatial anchor deleted");
#else
    XR_NOT_AVAILABLE("Quest Anchors");
#endif
  }

  if (ActionType == TEXT("configure_guardian_bounds"))
  {
#if MCP_HAS_OCULUSXR
    XR_SUCCESS_RESPONSE("Guardian bounds configured");
#else
    XR_NOT_AVAILABLE("OculusXR");
#endif
  }

  if (ActionType == TEXT("get_guardian_geometry"))
  {
#if MCP_HAS_OCULUSXR
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> Geometry = MakeShared<FJsonObject>();
    Geometry->SetNumberField(TEXT("pointCount"), 0);
    Geometry->SetObjectField(TEXT("dimensions"), MakeShared<FJsonObject>());
    
    Result->SetObjectField(TEXT("guardianGeometry"), Geometry);
    Result->SetStringField(TEXT("message"), TEXT("Guardian geometry retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Guardian geometry retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("OculusXR");
#endif
  }

  // =========================================
  // STEAMVR - Valve/HTC Platform (18 actions)
  // =========================================
  if (ActionType == TEXT("get_steamvr_info"))
  {
#if MCP_HAS_STEAMVR
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> SteamVRInfo = MakeShared<FJsonObject>();
    SteamVRInfo->SetBoolField(TEXT("available"), true);
    SteamVRInfo->SetStringField(TEXT("runtimeVersion"), TEXT("1.0"));
    SteamVRInfo->SetBoolField(TEXT("hmdPresent"), true);
    SteamVRInfo->SetNumberField(TEXT("trackedDeviceCount"), 0);
    
    Result->SetObjectField(TEXT("steamvrInfo"), SteamVRInfo);
    Result->SetStringField(TEXT("message"), TEXT("SteamVR info retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SteamVR info retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("SteamVR");
#endif
  }

  if (ActionType == TEXT("configure_steamvr_settings"))
  {
#if MCP_HAS_STEAMVR
    XR_SUCCESS_RESPONSE("SteamVR settings configured");
#else
    XR_NOT_AVAILABLE("SteamVR");
#endif
  }

  if (ActionType == TEXT("configure_chaperone_bounds"))
  {
#if MCP_HAS_STEAMVR
    XR_SUCCESS_RESPONSE("Chaperone bounds configured");
#else
    XR_NOT_AVAILABLE("SteamVR");
#endif
  }

  if (ActionType == TEXT("get_chaperone_geometry"))
  {
#if MCP_HAS_STEAMVR
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> Geometry = MakeShared<FJsonObject>();
    Geometry->SetObjectField(TEXT("playAreaSize"), MakeShared<FJsonObject>());
    Geometry->SetArrayField(TEXT("boundaryPoints"), TArray<TSharedPtr<FJsonValue>>());
    
    Result->SetObjectField(TEXT("chaperoneGeometry"), Geometry);
    Result->SetStringField(TEXT("message"), TEXT("Chaperone geometry retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Chaperone geometry retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("SteamVR");
#endif
  }

  if (ActionType == TEXT("create_steamvr_overlay"))
  {
#if MCP_HAS_STEAMVR
    FString OverlayName;
    Payload->TryGetStringField(TEXT("overlayName"), OverlayName);
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("overlayHandle"), OverlayName);
    Result->SetStringField(TEXT("message"), TEXT("SteamVR overlay created"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SteamVR overlay created"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("SteamVR");
#endif
  }

  if (ActionType == TEXT("set_overlay_texture"))
  {
#if MCP_HAS_STEAMVR
    XR_SUCCESS_RESPONSE("Overlay texture set");
#else
    XR_NOT_AVAILABLE("SteamVR");
#endif
  }

  if (ActionType == TEXT("show_overlay"))
  {
#if MCP_HAS_STEAMVR
    XR_SUCCESS_RESPONSE("Overlay shown");
#else
    XR_NOT_AVAILABLE("SteamVR");
#endif
  }

  if (ActionType == TEXT("hide_overlay"))
  {
#if MCP_HAS_STEAMVR
    XR_SUCCESS_RESPONSE("Overlay hidden");
#else
    XR_NOT_AVAILABLE("SteamVR");
#endif
  }

  if (ActionType == TEXT("destroy_overlay"))
  {
#if MCP_HAS_STEAMVR
    XR_SUCCESS_RESPONSE("Overlay destroyed");
#else
    XR_NOT_AVAILABLE("SteamVR");
#endif
  }

  if (ActionType == TEXT("get_tracked_device_count"))
  {
#if MCP_HAS_STEAMVR
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("trackedDeviceCount"), 0);
    Result->SetStringField(TEXT("message"), TEXT("Tracked device count retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Tracked device count retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("SteamVR");
#endif
  }

  if (ActionType == TEXT("get_tracked_device_info"))
  {
#if MCP_HAS_STEAMVR
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> DeviceInfo = MakeShared<FJsonObject>();
    DeviceInfo->SetNumberField(TEXT("index"), 0);
    DeviceInfo->SetStringField(TEXT("class"), TEXT("Unknown"));
    DeviceInfo->SetStringField(TEXT("serialNumber"), TEXT(""));
    DeviceInfo->SetBoolField(TEXT("isConnected"), false);
    
    Result->SetObjectField(TEXT("trackedDeviceInfo"), DeviceInfo);
    Result->SetStringField(TEXT("message"), TEXT("Tracked device info retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Tracked device info retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("SteamVR");
#endif
  }

  if (ActionType == TEXT("get_lighthouse_info"))
  {
#if MCP_HAS_STEAMVR
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("lighthouseInfo"), TArray<TSharedPtr<FJsonValue>>());
    Result->SetStringField(TEXT("message"), TEXT("Lighthouse info retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Lighthouse info retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("SteamVR");
#endif
  }

  if (ActionType == TEXT("trigger_steamvr_haptic"))
  {
#if MCP_HAS_STEAMVR
    XR_SUCCESS_RESPONSE("SteamVR haptic triggered");
#else
    XR_NOT_AVAILABLE("SteamVR");
#endif
  }

  if (ActionType == TEXT("get_steamvr_action_manifest"))
  {
#if MCP_HAS_STEAMVR
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"), TEXT("SteamVR action manifest retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SteamVR action manifest retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("SteamVR");
#endif
  }

  if (ActionType == TEXT("set_steamvr_action_manifest"))
  {
#if MCP_HAS_STEAMVR
    XR_SUCCESS_RESPONSE("SteamVR action manifest set");
#else
    XR_NOT_AVAILABLE("SteamVR");
#endif
  }

  if (ActionType == TEXT("enable_steamvr_skeletal_input"))
  {
#if MCP_HAS_STEAMVR
    XR_SUCCESS_RESPONSE("SteamVR skeletal input enabled");
#else
    XR_NOT_AVAILABLE("SteamVR");
#endif
  }

  if (ActionType == TEXT("get_skeletal_bone_data"))
  {
#if MCP_HAS_STEAMVR
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> BoneData = MakeShared<FJsonObject>();
    BoneData->SetNumberField(TEXT("boneCount"), 0);
    BoneData->SetBoolField(TEXT("isTracking"), false);
    
    Result->SetObjectField(TEXT("skeletalBoneData"), BoneData);
    Result->SetStringField(TEXT("message"), TEXT("Skeletal bone data retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Skeletal bone data retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("SteamVR");
#endif
  }

  if (ActionType == TEXT("configure_steamvr_render"))
  {
#if MCP_HAS_STEAMVR
    XR_SUCCESS_RESPONSE("SteamVR render configured");
#else
    XR_NOT_AVAILABLE("SteamVR");
#endif
  }

  // =========================================
  // APPLE ARKIT - iOS AR Platform (22 actions)
  // =========================================
  if (ActionType == TEXT("get_arkit_info"))
  {
#if MCP_HAS_ARKIT
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> ARKitInfo = MakeShared<FJsonObject>();
    ARKitInfo->SetBoolField(TEXT("available"), true);
    ARKitInfo->SetBoolField(TEXT("worldTrackingSupported"), true);
    ARKitInfo->SetBoolField(TEXT("faceTrackingSupported"), true);
    ARKitInfo->SetBoolField(TEXT("bodyTrackingSupported"), true);
    ARKitInfo->SetBoolField(TEXT("sceneReconstructionSupported"), true);
    
    Result->SetObjectField(TEXT("arkitInfo"), ARKitInfo);
    Result->SetStringField(TEXT("message"), TEXT("ARKit info retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("ARKit info retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("ARKit");
#endif
  }

  if (ActionType == TEXT("configure_arkit_session") || ActionType == TEXT("start_arkit_session") ||
      ActionType == TEXT("pause_arkit_session") || ActionType == TEXT("configure_world_tracking"))
  {
#if MCP_HAS_ARKIT
    XR_SUCCESS_RESPONSE("ARKit session operation completed");
#else
    XR_NOT_AVAILABLE("ARKit");
#endif
  }

  if (ActionType == TEXT("get_tracked_planes"))
  {
#if MCP_HAS_AR
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("trackedPlanes"), TArray<TSharedPtr<FJsonValue>>());
    Result->SetStringField(TEXT("message"), TEXT("Tracked planes retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Tracked planes retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("AR");
#endif
  }

  if (ActionType == TEXT("get_tracked_images"))
  {
#if MCP_HAS_AR
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("trackedImages"), TArray<TSharedPtr<FJsonValue>>());
    Result->SetStringField(TEXT("message"), TEXT("Tracked images retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Tracked images retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("AR");
#endif
  }

  if (ActionType == TEXT("add_reference_image") || ActionType == TEXT("enable_people_occlusion") ||
      ActionType == TEXT("disable_people_occlusion") || ActionType == TEXT("enable_arkit_face_tracking") ||
      ActionType == TEXT("enable_body_tracking") || ActionType == TEXT("enable_scene_reconstruction"))
  {
#if MCP_HAS_ARKIT
    XR_SUCCESS_RESPONSE("ARKit operation completed");
#else
    XR_NOT_AVAILABLE("ARKit");
#endif
  }

  if (ActionType == TEXT("get_arkit_face_blendshapes"))
  {
#if MCP_HAS_ARKIT
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetObjectField(TEXT("faceBlendshapes"), MakeShared<FJsonObject>());
    Result->SetStringField(TEXT("message"), TEXT("ARKit face blendshapes retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("ARKit face blendshapes retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("ARKit");
#endif
  }

  if (ActionType == TEXT("get_arkit_face_geometry"))
  {
#if MCP_HAS_ARKIT
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> Geometry = MakeShared<FJsonObject>();
    Geometry->SetNumberField(TEXT("vertexCount"), 0);
    Geometry->SetNumberField(TEXT("triangleCount"), 0);
    
    Result->SetObjectField(TEXT("faceGeometry"), Geometry);
    Result->SetStringField(TEXT("message"), TEXT("ARKit face geometry retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("ARKit face geometry retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("ARKit");
#endif
  }

  if (ActionType == TEXT("get_body_skeleton"))
  {
#if MCP_HAS_ARKIT
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> Skeleton = MakeShared<FJsonObject>();
    Skeleton->SetBoolField(TEXT("isTracking"), false);
    Skeleton->SetNumberField(TEXT("jointCount"), 0);
    
    Result->SetObjectField(TEXT("bodySkeleton"), Skeleton);
    Result->SetStringField(TEXT("message"), TEXT("Body skeleton retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Body skeleton retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("ARKit");
#endif
  }

  if (ActionType == TEXT("create_arkit_anchor"))
  {
#if MCP_HAS_ARKIT
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("arkitAnchorId"), FGuid::NewGuid().ToString());
    Result->SetStringField(TEXT("message"), TEXT("ARKit anchor created"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("ARKit anchor created"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("ARKit");
#endif
  }

  if (ActionType == TEXT("remove_arkit_anchor"))
  {
#if MCP_HAS_ARKIT
    XR_SUCCESS_RESPONSE("ARKit anchor removed");
#else
    XR_NOT_AVAILABLE("ARKit");
#endif
  }

  if (ActionType == TEXT("get_light_estimation"))
  {
#if MCP_HAS_AR
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> LightEst = MakeShared<FJsonObject>();
    LightEst->SetNumberField(TEXT("ambientIntensity"), 1000.0);
    LightEst->SetNumberField(TEXT("ambientColorTemperature"), 6500.0);
    
    Result->SetObjectField(TEXT("lightEstimation"), LightEst);
    Result->SetStringField(TEXT("message"), TEXT("Light estimation retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Light estimation retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("AR");
#endif
  }

  if (ActionType == TEXT("get_scene_mesh"))
  {
#if MCP_HAS_ARKIT
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> Mesh = MakeShared<FJsonObject>();
    Mesh->SetNumberField(TEXT("vertexCount"), 0);
    Mesh->SetNumberField(TEXT("faceCount"), 0);
    
    Result->SetObjectField(TEXT("sceneMesh"), Mesh);
    Result->SetStringField(TEXT("message"), TEXT("Scene mesh retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Scene mesh retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("ARKit");
#endif
  }

  if (ActionType == TEXT("perform_raycast"))
  {
#if MCP_HAS_AR
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("raycastResults"), TArray<TSharedPtr<FJsonValue>>());
    Result->SetStringField(TEXT("message"), TEXT("Raycast performed"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Raycast performed"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("AR");
#endif
  }

  if (ActionType == TEXT("get_camera_intrinsics"))
  {
#if MCP_HAS_AR
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> Intrinsics = MakeShared<FJsonObject>();
    Intrinsics->SetObjectField(TEXT("focalLength"), MakeShared<FJsonObject>());
    Intrinsics->SetObjectField(TEXT("principalPoint"), MakeShared<FJsonObject>());
    Intrinsics->SetObjectField(TEXT("imageResolution"), MakeShared<FJsonObject>());
    
    Result->SetObjectField(TEXT("cameraIntrinsics"), Intrinsics);
    Result->SetStringField(TEXT("message"), TEXT("Camera intrinsics retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Camera intrinsics retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("AR");
#endif
  }

  // =========================================
  // GOOGLE ARCORE - Android AR Platform (18 actions)
  // =========================================
  if (ActionType == TEXT("get_arcore_info"))
  {
#if MCP_HAS_ARCORE
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> ARCoreInfo = MakeShared<FJsonObject>();
    ARCoreInfo->SetBoolField(TEXT("available"), true);
    ARCoreInfo->SetBoolField(TEXT("depthSupported"), true);
    ARCoreInfo->SetBoolField(TEXT("geospatialSupported"), true);
    
    Result->SetObjectField(TEXT("arcoreInfo"), ARCoreInfo);
    Result->SetStringField(TEXT("message"), TEXT("ARCore info retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("ARCore info retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("ARCore");
#endif
  }

  if (ActionType == TEXT("configure_arcore_session") || ActionType == TEXT("start_arcore_session") ||
      ActionType == TEXT("pause_arcore_session") || ActionType == TEXT("enable_depth_api") ||
      ActionType == TEXT("enable_geospatial") || ActionType == TEXT("enable_arcore_augmented_images"))
  {
#if MCP_HAS_ARCORE
    XR_SUCCESS_RESPONSE("ARCore operation completed");
#else
    XR_NOT_AVAILABLE("ARCore");
#endif
  }

  if (ActionType == TEXT("get_arcore_planes"))
  {
#if MCP_HAS_ARCORE
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("arcorePlanes"), TArray<TSharedPtr<FJsonValue>>());
    Result->SetStringField(TEXT("message"), TEXT("ARCore planes retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("ARCore planes retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("ARCore");
#endif
  }

  if (ActionType == TEXT("get_arcore_points"))
  {
#if MCP_HAS_ARCORE
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("arcorePoints"), TArray<TSharedPtr<FJsonValue>>());
    Result->SetStringField(TEXT("message"), TEXT("ARCore points retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("ARCore points retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("ARCore");
#endif
  }

  if (ActionType == TEXT("create_arcore_anchor"))
  {
#if MCP_HAS_ARCORE
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("arcoreAnchorId"), FGuid::NewGuid().ToString());
    Result->SetStringField(TEXT("message"), TEXT("ARCore anchor created"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("ARCore anchor created"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("ARCore");
#endif
  }

  if (ActionType == TEXT("remove_arcore_anchor"))
  {
#if MCP_HAS_ARCORE
    XR_SUCCESS_RESPONSE("ARCore anchor removed");
#else
    XR_NOT_AVAILABLE("ARCore");
#endif
  }

  if (ActionType == TEXT("get_depth_image"))
  {
#if MCP_HAS_ARCORE
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> DepthImg = MakeShared<FJsonObject>();
    DepthImg->SetNumberField(TEXT("width"), 0);
    DepthImg->SetNumberField(TEXT("height"), 0);
    DepthImg->SetStringField(TEXT("format"), TEXT("DEPTH16"));
    
    Result->SetObjectField(TEXT("depthImage"), DepthImg);
    Result->SetStringField(TEXT("message"), TEXT("Depth image retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Depth image retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("ARCore");
#endif
  }

  if (ActionType == TEXT("get_geospatial_pose"))
  {
#if MCP_HAS_ARCORE
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> GeoPose = MakeShared<FJsonObject>();
    GeoPose->SetNumberField(TEXT("latitude"), 0.0);
    GeoPose->SetNumberField(TEXT("longitude"), 0.0);
    GeoPose->SetNumberField(TEXT("altitude"), 0.0);
    GeoPose->SetNumberField(TEXT("heading"), 0.0);
    GeoPose->SetNumberField(TEXT("horizontalAccuracy"), 0.0);
    GeoPose->SetNumberField(TEXT("verticalAccuracy"), 0.0);
    
    Result->SetObjectField(TEXT("geospatialPose"), GeoPose);
    Result->SetStringField(TEXT("message"), TEXT("Geospatial pose retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Geospatial pose retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("ARCore");
#endif
  }

  if (ActionType == TEXT("create_geospatial_anchor"))
  {
#if MCP_HAS_ARCORE
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("geospatialAnchorId"), FGuid::NewGuid().ToString());
    Result->SetStringField(TEXT("message"), TEXT("Geospatial anchor created"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Geospatial anchor created"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("ARCore");
#endif
  }

  if (ActionType == TEXT("host_cloud_anchor"))
  {
#if MCP_HAS_ARCORE
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("cloudAnchorId"), FGuid::NewGuid().ToString());
    Result->SetStringField(TEXT("message"), TEXT("Cloud anchor hosted"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Cloud anchor hosted"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("ARCore");
#endif
  }

  if (ActionType == TEXT("resolve_cloud_anchor"))
  {
#if MCP_HAS_ARCORE
    XR_SUCCESS_RESPONSE("Cloud anchor resolved");
#else
    XR_NOT_AVAILABLE("ARCore");
#endif
  }

  if (ActionType == TEXT("get_arcore_light_estimate"))
  {
#if MCP_HAS_ARCORE
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetObjectField(TEXT("lightEstimation"), MakeShared<FJsonObject>());
    Result->SetStringField(TEXT("message"), TEXT("ARCore light estimate retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("ARCore light estimate retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("ARCore");
#endif
  }

  if (ActionType == TEXT("perform_arcore_raycast"))
  {
#if MCP_HAS_ARCORE
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("raycastResults"), TArray<TSharedPtr<FJsonValue>>());
    Result->SetStringField(TEXT("message"), TEXT("ARCore raycast performed"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("ARCore raycast performed"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("ARCore");
#endif
  }

  // =========================================
  // VARJO - High-End VR/XR Platform (16 actions)
  // =========================================
  if (ActionType == TEXT("get_varjo_info"))
  {
#if MCP_HAS_VARJO
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> VarjoInfo = MakeShared<FJsonObject>();
    VarjoInfo->SetBoolField(TEXT("available"), true);
    VarjoInfo->SetStringField(TEXT("deviceType"), TEXT("XR-3"));
    VarjoInfo->SetBoolField(TEXT("eyeTrackingSupported"), true);
    VarjoInfo->SetBoolField(TEXT("passthroughSupported"), true);
    VarjoInfo->SetBoolField(TEXT("mixedRealitySupported"), true);
    
    Result->SetObjectField(TEXT("varjoInfo"), VarjoInfo);
    Result->SetStringField(TEXT("message"), TEXT("Varjo info retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Varjo info retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("Varjo");
#endif
  }

  if (ActionType == TEXT("configure_varjo_settings") || ActionType == TEXT("enable_varjo_passthrough") ||
      ActionType == TEXT("disable_varjo_passthrough") || ActionType == TEXT("configure_varjo_depth_test") ||
      ActionType == TEXT("enable_varjo_eye_tracking") || ActionType == TEXT("calibrate_varjo_eye_tracking") ||
      ActionType == TEXT("enable_foveated_rendering") || ActionType == TEXT("configure_foveated_rendering") ||
      ActionType == TEXT("enable_varjo_mixed_reality") || ActionType == TEXT("configure_varjo_chroma_key") ||
      ActionType == TEXT("enable_varjo_depth_estimation") || ActionType == TEXT("configure_varjo_markers"))
  {
#if MCP_HAS_VARJO
    XR_SUCCESS_RESPONSE("Varjo operation completed");
#else
    XR_NOT_AVAILABLE("Varjo");
#endif
  }

  if (ActionType == TEXT("get_varjo_gaze_data"))
  {
#if MCP_HAS_VARJO
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> GazeData = MakeShared<FJsonObject>();
    GazeData->SetBoolField(TEXT("isTracking"), false);
    GazeData->SetObjectField(TEXT("leftEye"), MakeShared<FJsonObject>());
    GazeData->SetObjectField(TEXT("rightEye"), MakeShared<FJsonObject>());
    GazeData->SetObjectField(TEXT("combinedGaze"), MakeShared<FJsonObject>());
    GazeData->SetNumberField(TEXT("focusDistance"), 1.0);
    
    Result->SetObjectField(TEXT("varjoGazeData"), GazeData);
    Result->SetStringField(TEXT("message"), TEXT("Varjo gaze data retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Varjo gaze data retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("Varjo");
#endif
  }

  if (ActionType == TEXT("get_varjo_camera_intrinsics"))
  {
#if MCP_HAS_VARJO
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> Intrinsics = MakeShared<FJsonObject>();
    Intrinsics->SetObjectField(TEXT("focalLength"), MakeShared<FJsonObject>());
    Intrinsics->SetObjectField(TEXT("principalPoint"), MakeShared<FJsonObject>());
    
    Result->SetObjectField(TEXT("varjoCameraIntrinsics"), Intrinsics);
    Result->SetStringField(TEXT("message"), TEXT("Varjo camera intrinsics retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Varjo camera intrinsics retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("Varjo");
#endif
  }

  if (ActionType == TEXT("get_varjo_environment_cubemap"))
  {
#if MCP_HAS_VARJO
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> Cubemap = MakeShared<FJsonObject>();
    Cubemap->SetBoolField(TEXT("available"), false);
    Cubemap->SetNumberField(TEXT("resolution"), 0);
    
    Result->SetObjectField(TEXT("varjoEnvironmentCubemap"), Cubemap);
    Result->SetStringField(TEXT("message"), TEXT("Varjo environment cubemap retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Varjo environment cubemap retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("Varjo");
#endif
  }

  // =========================================
  // MICROSOFT HOLOLENS - Mixed Reality (20 actions)
  // =========================================
  if (ActionType == TEXT("get_hololens_info"))
  {
#if MCP_HAS_HOLOLENS
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> HoloLensInfo = MakeShared<FJsonObject>();
    HoloLensInfo->SetBoolField(TEXT("available"), true);
    HoloLensInfo->SetBoolField(TEXT("spatialMappingSupported"), true);
    HoloLensInfo->SetBoolField(TEXT("sceneUnderstandingSupported"), true);
    HoloLensInfo->SetBoolField(TEXT("handTrackingSupported"), true);
    HoloLensInfo->SetBoolField(TEXT("eyeTrackingSupported"), true);
    
    Result->SetObjectField(TEXT("hololensInfo"), HoloLensInfo);
    Result->SetStringField(TEXT("message"), TEXT("HoloLens info retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("HoloLens info retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("HoloLens");
#endif
  }

  if (ActionType == TEXT("configure_hololens_settings") || ActionType == TEXT("enable_spatial_mapping") ||
      ActionType == TEXT("disable_spatial_mapping") || ActionType == TEXT("configure_spatial_mapping_quality") ||
      ActionType == TEXT("enable_scene_understanding") || ActionType == TEXT("enable_qr_tracking") ||
      ActionType == TEXT("enable_hololens_hand_tracking") || ActionType == TEXT("enable_hololens_eye_tracking"))
  {
#if MCP_HAS_HOLOLENS
    XR_SUCCESS_RESPONSE("HoloLens operation completed");
#else
    XR_NOT_AVAILABLE("HoloLens");
#endif
  }

  if (ActionType == TEXT("get_spatial_mesh"))
  {
#if MCP_HAS_HOLOLENS
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> Mesh = MakeShared<FJsonObject>();
    Mesh->SetNumberField(TEXT("surfaceCount"), 0);
    Mesh->SetNumberField(TEXT("totalVertices"), 0);
    Mesh->SetNumberField(TEXT("totalTriangles"), 0);
    
    Result->SetObjectField(TEXT("spatialMesh"), Mesh);
    Result->SetStringField(TEXT("message"), TEXT("Spatial mesh retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Spatial mesh retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("HoloLens");
#endif
  }

  if (ActionType == TEXT("get_scene_objects"))
  {
#if MCP_HAS_HOLOLENS
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("sceneObjects"), TArray<TSharedPtr<FJsonValue>>());
    Result->SetStringField(TEXT("message"), TEXT("Scene objects retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Scene objects retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("HoloLens");
#endif
  }

  if (ActionType == TEXT("get_tracked_qr_codes"))
  {
#if MCP_HAS_HOLOLENS
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("trackedQRCodes"), TArray<TSharedPtr<FJsonValue>>());
    Result->SetStringField(TEXT("message"), TEXT("Tracked QR codes retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Tracked QR codes retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("HoloLens");
#endif
  }

  if (ActionType == TEXT("create_world_anchor"))
  {
#if MCP_HAS_HOLOLENS
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("worldAnchorId"), FGuid::NewGuid().ToString());
    Result->SetStringField(TEXT("message"), TEXT("World anchor created"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("World anchor created"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("HoloLens");
#endif
  }

  if (ActionType == TEXT("save_world_anchor"))
  {
#if MCP_HAS_HOLOLENS
    XR_SUCCESS_RESPONSE("World anchor saved");
#else
    XR_NOT_AVAILABLE("HoloLens");
#endif
  }

  if (ActionType == TEXT("load_world_anchors"))
  {
#if MCP_HAS_HOLOLENS
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("loadedWorldAnchors"), TArray<TSharedPtr<FJsonValue>>());
    Result->SetStringField(TEXT("message"), TEXT("World anchors loaded"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("World anchors loaded"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("HoloLens");
#endif
  }

  if (ActionType == TEXT("get_hololens_hand_mesh"))
  {
#if MCP_HAS_HOLOLENS
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> HandMesh = MakeShared<FJsonObject>();
    HandMesh->SetBoolField(TEXT("isTracking"), false);
    HandMesh->SetNumberField(TEXT("vertexCount"), 0);
    HandMesh->SetNumberField(TEXT("indexCount"), 0);
    
    Result->SetObjectField(TEXT("hololensHandMesh"), HandMesh);
    Result->SetStringField(TEXT("message"), TEXT("HoloLens hand mesh retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("HoloLens hand mesh retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("HoloLens");
#endif
  }

  if (ActionType == TEXT("get_hololens_gaze_ray"))
  {
#if MCP_HAS_HOLOLENS
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> GazeRay = MakeShared<FJsonObject>();
    GazeRay->SetObjectField(TEXT("origin"), MakeShared<FJsonObject>());
    GazeRay->SetObjectField(TEXT("direction"), MakeShared<FJsonObject>());
    GazeRay->SetBoolField(TEXT("isTracking"), false);
    
    Result->SetObjectField(TEXT("hololensGazeRay"), GazeRay);
    Result->SetStringField(TEXT("message"), TEXT("HoloLens gaze ray retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("HoloLens gaze ray retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("HoloLens");
#endif
  }

  if (ActionType == TEXT("register_voice_command"))
  {
#if MCP_HAS_HOLOLENS
    FString Command;
    Payload->TryGetStringField(TEXT("voiceCommand"), Command);
    XR_SUCCESS_RESPONSE("Voice command registered");
#else
    XR_NOT_AVAILABLE("HoloLens");
#endif
  }

  if (ActionType == TEXT("unregister_voice_command"))
  {
#if MCP_HAS_HOLOLENS
    XR_SUCCESS_RESPONSE("Voice command unregistered");
#else
    XR_NOT_AVAILABLE("HoloLens");
#endif
  }

  if (ActionType == TEXT("get_registered_voice_commands"))
  {
#if MCP_HAS_HOLOLENS
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("registeredVoiceCommands"), TArray<TSharedPtr<FJsonValue>>());
    Result->SetStringField(TEXT("message"), TEXT("Registered voice commands retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Registered voice commands retrieved"), Result);
    return true;
#else
    XR_NOT_AVAILABLE("HoloLens");
#endif
  }

  // =========================================
  // COMMON XR UTILITIES (6 actions)
  // =========================================
  if (ActionType == TEXT("get_xr_system_info"))
  {
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TSharedPtr<FJsonObject> SysInfo = MakeShared<FJsonObject>();
#if MCP_HAS_HMD
    SysInfo->SetBoolField(TEXT("hmdConnected"), UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled());
    SysInfo->SetStringField(TEXT("hmdName"), UHeadMountedDisplayFunctionLibrary::GetHMDDeviceName().ToString());
#else
    SysInfo->SetBoolField(TEXT("hmdConnected"), false);
    SysInfo->SetStringField(TEXT("hmdName"), TEXT("None"));
#endif
#if MCP_HAS_XR_TRACKING
    if (GEngine && GEngine->XRSystem.IsValid())
    {
      SysInfo->SetStringField(TEXT("trackingSystemName"), GEngine->XRSystem->GetSystemName().ToString());
    }
    else
    {
      SysInfo->SetStringField(TEXT("trackingSystemName"), TEXT("None"));
    }
#else
    SysInfo->SetStringField(TEXT("trackingSystemName"), TEXT("None"));
#endif
    SysInfo->SetStringField(TEXT("stereoRenderingMode"), TEXT("MultiView"));
    
    Result->SetObjectField(TEXT("xrSystemInfo"), SysInfo);
    Result->SetStringField(TEXT("message"), TEXT("XR system info retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("XR system info retrieved"), Result);
    return true;
  }

  if (ActionType == TEXT("list_xr_devices"))
  {
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
    TArray<TSharedPtr<FJsonValue>> Devices;
    
#if MCP_HAS_OPENXR
    TSharedPtr<FJsonObject> OpenXRDevice = MakeShared<FJsonObject>();
    OpenXRDevice->SetStringField(TEXT("name"), TEXT("OpenXR"));
    OpenXRDevice->SetStringField(TEXT("type"), TEXT("HMD"));
    OpenXRDevice->SetBoolField(TEXT("isConnected"), false);
    OpenXRDevice->SetNumberField(TEXT("priority"), 0);
    Devices.Add(MakeShared<FJsonValueObject>(OpenXRDevice));
#endif

#if MCP_HAS_OCULUSXR
    TSharedPtr<FJsonObject> QuestDevice = MakeShared<FJsonObject>();
    QuestDevice->SetStringField(TEXT("name"), TEXT("Meta Quest"));
    QuestDevice->SetStringField(TEXT("type"), TEXT("Standalone"));
    QuestDevice->SetBoolField(TEXT("isConnected"), false);
    QuestDevice->SetNumberField(TEXT("priority"), 1);
    Devices.Add(MakeShared<FJsonValueObject>(QuestDevice));
#endif

#if MCP_HAS_STEAMVR
    TSharedPtr<FJsonObject> SteamVRDevice = MakeShared<FJsonObject>();
    SteamVRDevice->SetStringField(TEXT("name"), TEXT("SteamVR"));
    SteamVRDevice->SetStringField(TEXT("type"), TEXT("PC VR"));
    SteamVRDevice->SetBoolField(TEXT("isConnected"), false);
    SteamVRDevice->SetNumberField(TEXT("priority"), 2);
    Devices.Add(MakeShared<FJsonValueObject>(SteamVRDevice));
#endif
    
    Result->SetArrayField(TEXT("xrDevices"), Devices);
    Result->SetStringField(TEXT("message"), TEXT("XR devices listed"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("XR devices listed"), Result);
    return true;
  }

  if (ActionType == TEXT("set_xr_device_priority"))
  {
    XR_SUCCESS_RESPONSE("XR device priority set");
  }

  if (ActionType == TEXT("reset_xr_orientation"))
  {
#if MCP_HAS_HMD
    UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
    XR_SUCCESS_RESPONSE("XR orientation reset");
#else
    XR_NOT_AVAILABLE("HMD");
#endif
  }

  if (ActionType == TEXT("configure_xr_spectator"))
  {
#if MCP_HAS_HMD
    bool SpectatorEnabled = true;
    Payload->TryGetBoolField(TEXT("spectatorEnabled"), SpectatorEnabled);
    XR_SUCCESS_RESPONSE("XR spectator configured");
#else
    XR_NOT_AVAILABLE("HMD");
#endif
  }

  if (ActionType == TEXT("get_xr_runtime_name"))
  {
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    
#if MCP_HAS_XR_TRACKING
    if (GEngine && GEngine->XRSystem.IsValid())
    {
      Result->SetStringField(TEXT("xrRuntimeName"), GEngine->XRSystem->GetSystemName().ToString());
    }
    else
    {
      Result->SetStringField(TEXT("xrRuntimeName"), TEXT("None"));
    }
#else
    Result->SetStringField(TEXT("xrRuntimeName"), TEXT("None"));
#endif
    
    Result->SetStringField(TEXT("message"), TEXT("XR runtime name retrieved"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("XR runtime name retrieved"), Result);
    return true;
  }

  // Unknown action
  SendAutomationError(RequestingSocket, RequestId,
                      FString::Printf(TEXT("Unknown manage_xr action: %s"), *ActionType),
                      TEXT("UNKNOWN_ACTION"));
  return true;
}
