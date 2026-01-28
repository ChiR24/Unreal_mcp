#include "Dom/JsonObject.h"
#include "DrawDebugHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#if __has_include("NiagaraActor.h")
#include "NiagaraActor.h"
#endif
#if __has_include("NiagaraComponent.h")
#include "NiagaraComponent.h"
#endif
#if __has_include("NiagaraSystem.h")
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemFactoryNew.h"
#endif
#if __has_include("Engine/PointLight.h")
#include "Engine/PointLight.h"
#endif
#if __has_include("Engine/SpotLight.h")
#include "Engine/SpotLight.h"
#endif
#if __has_include("Engine/DirectionalLight.h")
#include "Engine/DirectionalLight.h"
#endif
#if __has_include("Engine/RectLight.h")
#include "Engine/RectLight.h"
#endif
#if __has_include("Components/LightComponent.h")
#include "Components/LightComponent.h"
#endif
#if __has_include("Components/PointLightComponent.h")
#include "Components/PointLightComponent.h"
#endif
#if __has_include("Components/SpotLightComponent.h")
#include "Components/SpotLightComponent.h"
#endif
#if __has_include("Components/RectLightComponent.h")
#include "Components/RectLightComponent.h"
#endif
#if __has_include("Components/DirectionalLightComponent.h")
#include "Components/DirectionalLightComponent.h"
#endif
#endif

bool UMcpAutomationBridgeSubsystem::HandleEffectAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  const bool bIsCreateEffect = Lower.Equals(TEXT("create_effect")) ||
                               Lower.StartsWith(TEXT("create_effect"));
  if (!bIsCreateEffect && !Lower.StartsWith(TEXT("spawn_")) &&
      !Lower.Equals(TEXT("set_niagara_parameter")) &&
      !Lower.Equals(TEXT("list_debug_shapes")) &&
      !Lower.Equals(TEXT("clear_debug_shapes")))
    return false;

  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : MakeShared<FJsonObject>();

  auto SendResponse = [&](bool bOk, const FString &Msg,
                          const TSharedPtr<FJsonObject> &ResObj,
                          const FString &ErrCode = FString()) {
    SendAutomationResponse(RequestingSocket, RequestId, bOk, Msg, ResObj,
                           ErrCode);
  };

  // Discovery: list available debug shape types
  if (Lower.Equals(TEXT("list_debug_shapes"))) {
    TArray<TSharedPtr<FJsonValue>> Shapes;
    Shapes.Add(MakeShared<FJsonValueString>(TEXT("sphere")));
    Shapes.Add(MakeShared<FJsonValueString>(TEXT("box")));
    Shapes.Add(MakeShared<FJsonValueString>(TEXT("circle")));
    Shapes.Add(MakeShared<FJsonValueString>(TEXT("line")));
    Shapes.Add(MakeShared<FJsonValueString>(TEXT("point")));
    Shapes.Add(MakeShared<FJsonValueString>(TEXT("coordinate")));
    Shapes.Add(MakeShared<FJsonValueString>(TEXT("cylinder")));
    Shapes.Add(MakeShared<FJsonValueString>(TEXT("cone")));
    Shapes.Add(MakeShared<FJsonValueString>(TEXT("capsule")));
    Shapes.Add(MakeShared<FJsonValueString>(TEXT("arrow")));
    Shapes.Add(MakeShared<FJsonValueString>(TEXT("plane")));

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetArrayField(TEXT("shapes"), Shapes);
    Resp->SetNumberField(TEXT("count"), Shapes.Num());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Available debug shape types"), Resp);
    return true;
  }

  // Handle create_effect tool with sub-actions
  if (Lower.Equals(TEXT("clear_debug_shapes"))) {
#if WITH_EDITOR
    if (GEditor && GEditor->GetEditorWorldContext().World()) {
      FlushPersistentDebugLines(GEditor->GetEditorWorldContext().World());
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Debug shapes cleared"), Resp);
      return true;
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor world not available"), nullptr,
                             TEXT("NO_WORLD"));
      return true;
    }
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Debug shape clearing requires editor build"),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
  }

  if (bIsCreateEffect || Lower.Equals(TEXT("create_niagara_system"))) {
    FString SubAction;
    LocalPayload->TryGetStringField(TEXT("action"), SubAction);

    if (Lower.Equals(TEXT("create_niagara_system"))) {
      SubAction = TEXT("create_niagara_system");
    }

    // Fallback: if action field in payload is empty, check if the top-level
    // Action is a specific tool (e.g. set_niagara_parameter) and use that as
    // sub-action.
    if (SubAction.IsEmpty() &&
        !Action.Equals(TEXT("create_effect"), ESearchCase::IgnoreCase)) {
      SubAction = Action;
    }

    const FString LowerSub = SubAction.ToLower();

    // Handle particle spawning
    if (LowerSub == TEXT("particle")) {
      FString Preset;
      LocalPayload->TryGetStringField(TEXT("preset"), Preset);
      if (Preset.IsEmpty()) {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), false);
        Resp->SetStringField(
            TEXT("error"),
            TEXT("preset parameter required for particle spawning"));
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Preset path required"), Resp,
                               TEXT("INVALID_ARGUMENT"));
        return true;
      }

      // Location and optional rotation/scale
      FVector Loc(0, 0, 0);
      if (LocalPayload->HasField(TEXT("location"))) {
        const TSharedPtr<FJsonValue> LocVal =
            LocalPayload->TryGetField(TEXT("location"));
        if (LocVal.IsValid()) {
          if (LocVal->Type == EJson::Array) {
            const TArray<TSharedPtr<FJsonValue>> &Arr = LocVal->AsArray();
            if (Arr.Num() >= 3)
              Loc =
                  FVector((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(),
                          (float)Arr[2]->AsNumber());
          } else if (LocVal->Type == EJson::Object) {
            const TSharedPtr<FJsonObject> O = LocVal->AsObject();
            if (O.IsValid())
              Loc = FVector(
                  (float)(O->HasField(TEXT("x")) ? GetJsonNumberField(O, TEXT("x"))
                                                 : 0.0),
                  (float)(O->HasField(TEXT("y")) ? GetJsonNumberField(O, TEXT("y"))
                                                 : 0.0),
                  (float)(O->HasField(TEXT("z")) ? GetJsonNumberField(O, TEXT("z"))
                                                 : 0.0));
          }
        }
      }

      // Rotation may be an array
      TArray<double> RotArr = {0, 0, 0};
      const TArray<TSharedPtr<FJsonValue>> *RA = nullptr;
      if (LocalPayload->TryGetArrayField(TEXT("rotation"), RA) && RA &&
          RA->Num() >= 3) {
        RotArr[0] = (*RA)[0]->AsNumber();
        RotArr[1] = (*RA)[1]->AsNumber();
        RotArr[2] = (*RA)[2]->AsNumber();
      }

      // Scale may be an array or a single numeric value
      TArray<double> ScaleArr = {1, 1, 1};
      const TArray<TSharedPtr<FJsonValue>> *ScaleJsonArr = nullptr;
      if (LocalPayload->TryGetArrayField(TEXT("scale"), ScaleJsonArr) &&
          ScaleJsonArr && ScaleJsonArr->Num() >= 3) {
        ScaleArr[0] = (*ScaleJsonArr)[0]->AsNumber();
        ScaleArr[1] = (*ScaleJsonArr)[1]->AsNumber();
        ScaleArr[2] = (*ScaleJsonArr)[2]->AsNumber();
      } else if (LocalPayload->TryGetNumberField(TEXT("scale"), ScaleArr[0])) {
        ScaleArr[1] = ScaleArr[2] = ScaleArr[0];
      }

      const bool bAutoDestroy =
          LocalPayload->HasField(TEXT("autoDestroy"))
              ? GetJsonBoolField(LocalPayload, TEXT("autoDestroy"))
              : false;

      // Duration (default: 5.0 seconds)
      const float Duration =
          LocalPayload->HasField(TEXT("duration"))
              ? (float)GetJsonNumberField(LocalPayload, TEXT("duration"))
              : 5.0f;

      // Size/Radius (default: 100.0)
      const float Size = LocalPayload->HasField(TEXT("size"))
                             ? (float)GetJsonNumberField(LocalPayload, TEXT("size"))
                             : 100.0f;

      // Thickness for lines (default: 2.0)
      const float Thickness =
          LocalPayload->HasField(TEXT("thickness"))
              ? (float)GetJsonNumberField(LocalPayload, TEXT("thickness"))
              : 2.0f;

      // Extract Color and ShapeType for debug drawing
      TArray<double> ColorArr = {255, 255, 255, 255};
      const TArray<TSharedPtr<FJsonValue>> *ColorJsonArr = nullptr;
      if (LocalPayload->TryGetArrayField(TEXT("color"), ColorJsonArr) &&
          ColorJsonArr && ColorJsonArr->Num() >= 3) {
        ColorArr[0] = (*ColorJsonArr)[0]->AsNumber();
        ColorArr[1] = (*ColorJsonArr)[1]->AsNumber();
        ColorArr[2] = (*ColorJsonArr)[2]->AsNumber();
        if (ColorJsonArr->Num() >= 4) {
          ColorArr[3] = (*ColorJsonArr)[3]->AsNumber();
        }
      }

      FString ShapeType = TEXT("sphere");
      LocalPayload->TryGetStringField(TEXT("shapeType"), ShapeType);

#if WITH_EDITOR
      if (!GEditor) {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), false);
        Resp->SetStringField(TEXT("error"),
                             TEXT("Editor not available for debug drawing"));
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Editor not available"), Resp,
                               TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
      }

      // Get the current world for debug drawing
      UWorld *World = GEditor->GetEditorWorldContext().World();
      if (!World) {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), false);
        Resp->SetStringField(TEXT("error"),
                             TEXT("No world available for debug drawing"));
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("No world available"), Resp,
                               TEXT("NO_WORLD"));
        return true;
      }

      const FColor DebugColor((uint8)ColorArr[0], (uint8)ColorArr[1],
                              (uint8)ColorArr[2], (uint8)ColorArr[3]);
      const FString LowerShapeType = ShapeType.ToLower();

      if (LowerShapeType == TEXT("sphere")) {
        DrawDebugSphere(World, Loc, Size, 16, DebugColor, false, Duration, 0,
                        Thickness);
      } else if (LowerShapeType == TEXT("box")) {
        FVector BoxSize = FVector(Size);
        if (LocalPayload->HasField(TEXT("boxSize"))) {
          const TArray<TSharedPtr<FJsonValue>> *BoxSizeArr = nullptr;
          if (LocalPayload->TryGetArrayField(TEXT("boxSize"), BoxSizeArr) &&
              BoxSizeArr && BoxSizeArr->Num() >= 3) {
            BoxSize = FVector((float)(*BoxSizeArr)[0]->AsNumber(),
                              (float)(*BoxSizeArr)[1]->AsNumber(),
                              (float)(*BoxSizeArr)[2]->AsNumber());
          }
        }
        DrawDebugBox(World, Loc, BoxSize, FRotator::ZeroRotator.Quaternion(),
                     DebugColor, false, Duration, 0, Thickness);
      } else if (LowerShapeType == TEXT("circle")) {
        DrawDebugCircle(World, Loc, Size, 32, DebugColor, false, Duration, 0,
                        Thickness, FVector::UpVector);
      } else if (LowerShapeType == TEXT("line")) {
        FVector EndLoc = Loc + FVector(100, 0, 0);
        if (LocalPayload->HasField(TEXT("endLocation"))) {
          const TSharedPtr<FJsonValue> EndVal =
              LocalPayload->TryGetField(TEXT("endLocation"));
          if (EndVal.IsValid()) {
            if (EndVal->Type == EJson::Array) {
              const TArray<TSharedPtr<FJsonValue>> &Arr = EndVal->AsArray();
              if (Arr.Num() >= 3)
                EndLoc = FVector((float)Arr[0]->AsNumber(),
                                 (float)Arr[1]->AsNumber(),
                                 (float)Arr[2]->AsNumber());
            } else if (EndVal->Type == EJson::Object) {
              const TSharedPtr<FJsonObject> O = EndVal->AsObject();
              if (O.IsValid())
                EndLoc = FVector((float)(O->HasField(TEXT("x"))
                                             ? GetJsonNumberField(O, TEXT("x"))
                                             : 0.0),
                                 (float)(O->HasField(TEXT("y"))
                                             ? GetJsonNumberField(O, TEXT("y"))
                                             : 0.0),
                                 (float)(O->HasField(TEXT("z"))
                                             ? GetJsonNumberField(O, TEXT("z"))
                                             : 0.0));
            }
          }
        }
        DrawDebugLine(World, Loc, EndLoc, DebugColor, false, Duration, 0,
                      Thickness);
      } else if (LowerShapeType == TEXT("point")) {
        DrawDebugPoint(World, Loc, Size, DebugColor, false, Duration);
      } else if (LowerShapeType == TEXT("coordinate")) {
        FRotator Rot = FRotator::ZeroRotator;
        if (LocalPayload->HasField(TEXT("rotation"))) {
          const TArray<TSharedPtr<FJsonValue>> *RotJsonArr = nullptr;
          if (LocalPayload->TryGetArrayField(TEXT("rotation"), RotJsonArr) &&
              RotJsonArr && RotJsonArr->Num() >= 3) {
            Rot = FRotator((float)(*RotJsonArr)[0]->AsNumber(),
                           (float)(*RotJsonArr)[1]->AsNumber(),
                           (float)(*RotJsonArr)[2]->AsNumber());
          }
        }
        DrawDebugCoordinateSystem(World, Loc, Rot, Size, false, Duration, 0,
                                  Thickness);
      } else if (LowerShapeType == TEXT("cylinder")) {
        FVector EndLoc = Loc + FVector(0, 0, 100);
        if (LocalPayload->HasField(TEXT("endLocation"))) {
          const TSharedPtr<FJsonValue> EndVal =
              LocalPayload->TryGetField(TEXT("endLocation"));
          if (EndVal.IsValid()) {
            if (EndVal->Type == EJson::Array) {
              const TArray<TSharedPtr<FJsonValue>> &Arr = EndVal->AsArray();
              if (Arr.Num() >= 3)
                EndLoc = FVector((float)Arr[0]->AsNumber(),
                                 (float)Arr[1]->AsNumber(),
                                 (float)Arr[2]->AsNumber());
            } else if (EndVal->Type == EJson::Object) {
              const TSharedPtr<FJsonObject> O = EndVal->AsObject();
              if (O.IsValid())
                EndLoc = FVector((float)(O->HasField(TEXT("x"))
                                             ? GetJsonNumberField(O, TEXT("x"))
                                             : 0.0),
                                 (float)(O->HasField(TEXT("y"))
                                             ? GetJsonNumberField(O, TEXT("y"))
                                             : 0.0),
                                 (float)(O->HasField(TEXT("z"))
                                             ? GetJsonNumberField(O, TEXT("z"))
                                             : 0.0));
            }
          }
        }
        DrawDebugCylinder(World, Loc, EndLoc, Size, 16, DebugColor, false,
                          Duration, 0, Thickness);
      } else if (LowerShapeType == TEXT("cone")) {
        FVector Direction = FVector::UpVector;
        if (LocalPayload->HasField(TEXT("direction"))) {
          const TSharedPtr<FJsonValue> DirVal =
              LocalPayload->TryGetField(TEXT("direction"));
          if (DirVal.IsValid()) {
            if (DirVal->Type == EJson::Array) {
              const TArray<TSharedPtr<FJsonValue>> &Arr = DirVal->AsArray();
              if (Arr.Num() >= 3)
                Direction = FVector((float)Arr[0]->AsNumber(),
                                    (float)Arr[1]->AsNumber(),
                                    (float)Arr[2]->AsNumber());
            } else if (DirVal->Type == EJson::Object) {
              const TSharedPtr<FJsonObject> O = DirVal->AsObject();
              if (O.IsValid())
                Direction = FVector((float)(O->HasField(TEXT("x"))
                                                ? GetJsonNumberField(O, TEXT("x"))
                                                : 0.0),
                                    (float)(O->HasField(TEXT("y"))
                                                ? GetJsonNumberField(O, TEXT("y"))
                                                : 0.0),
                                    (float)(O->HasField(TEXT("z"))
                                                ? GetJsonNumberField(O, TEXT("z"))
                                                : 0.0));
            }
          }
        }
        float Length = 100.0f;
        if (LocalPayload->HasField(TEXT("length"))) {
          Length = (float)GetJsonNumberField(LocalPayload, TEXT("length"));
        }
        // Default to a 45 degree cone if not specified
        float AngleWidth = FMath::DegreesToRadians(45.0f);
        float AngleHeight = FMath::DegreesToRadians(45.0f);

        if (LocalPayload->HasField(TEXT("angle"))) {
          float AngleDeg = (float)GetJsonNumberField(LocalPayload, TEXT("angle"));
          AngleWidth = AngleHeight = FMath::DegreesToRadians(AngleDeg);
        }

        DrawDebugCone(World, Loc, Direction, Length, AngleWidth, AngleHeight,
                      16, DebugColor, false, Duration, 0, Thickness);
      } else if (LowerShapeType == TEXT("capsule")) {
        FQuat Rot = FQuat::Identity;
        if (LocalPayload->HasField(TEXT("rotation"))) {
          const TArray<TSharedPtr<FJsonValue>> *RotJsonArr = nullptr;
          if (LocalPayload->TryGetArrayField(TEXT("rotation"), RotJsonArr) &&
              RotJsonArr && RotJsonArr->Num() >= 3) {
            Rot = FRotator((float)(*RotJsonArr)[0]->AsNumber(),
                           (float)(*RotJsonArr)[1]->AsNumber(),
                           (float)(*RotJsonArr)[2]->AsNumber())
                      .Quaternion();
          }
        }
        float HalfHeight = Size; // Default if not specified
        if (LocalPayload->HasField(TEXT("halfHeight"))) {
          HalfHeight = (float)GetJsonNumberField(LocalPayload, TEXT("halfHeight"));
        }
        DrawDebugCapsule(World, Loc, HalfHeight, Size, Rot, DebugColor, false,
                         Duration, 0, Thickness);
      } else if (LowerShapeType == TEXT("arrow")) {
        FVector EndLoc = Loc + FVector(100, 0, 0);
        if (LocalPayload->HasField(TEXT("endLocation"))) {
          // ... parsing logic same as line ...
          const TSharedPtr<FJsonValue> EndVal =
              LocalPayload->TryGetField(TEXT("endLocation"));
          if (EndVal.IsValid()) {
            if (EndVal->Type == EJson::Array) {
              const TArray<TSharedPtr<FJsonValue>> &Arr = EndVal->AsArray();
              if (Arr.Num() >= 3)
                EndLoc = FVector((float)Arr[0]->AsNumber(),
                                 (float)Arr[1]->AsNumber(),
                                 (float)Arr[2]->AsNumber());
            } else if (EndVal->Type == EJson::Object) {
              const TSharedPtr<FJsonObject> O = EndVal->AsObject();
              if (O.IsValid())
                EndLoc = FVector((float)(O->HasField(TEXT("x"))
                                             ? GetJsonNumberField(O, TEXT("x"))
                                             : 0.0),
                                 (float)(O->HasField(TEXT("y"))
                                             ? GetJsonNumberField(O, TEXT("y"))
                                             : 0.0),
                                 (float)(O->HasField(TEXT("z"))
                                             ? GetJsonNumberField(O, TEXT("z"))
                                             : 0.0));
            }
          }
        }
        float ArrowSize = Size > 0 ? Size : 10.0f;
        DrawDebugDirectionalArrow(World, Loc, EndLoc, ArrowSize, DebugColor,
                                  false, Duration, 0, Thickness);
      } else if (LowerShapeType == TEXT("plane")) {
        // Draw a simple plane using a box with 0 height or DrawDebugSolidPlane
        // if available but DrawDebugBox is safer for wireframe Using Box with
        // minimal Z thickness
        FVector BoxSize = FVector(Size, Size, 1.0f);
        if (LocalPayload->HasField(TEXT("boxSize"))) {
          // ... parsing ...
        }
        FQuat Rot = FQuat::Identity;
        if (LocalPayload->HasField(TEXT("rotation"))) {
          const TArray<TSharedPtr<FJsonValue>> *RotJsonArr = nullptr;
          if (LocalPayload->TryGetArrayField(TEXT("rotation"), RotJsonArr) &&
              RotJsonArr && RotJsonArr->Num() >= 3) {
            Rot = FRotator((float)(*RotJsonArr)[0]->AsNumber(),
                           (float)(*RotJsonArr)[1]->AsNumber(),
                           (float)(*RotJsonArr)[2]->AsNumber())
                      .Quaternion();
          }
        }
        DrawDebugBox(World, Loc, BoxSize, Rot, DebugColor, false, Duration, 0,
                     Thickness);
      } else {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), false);
        Resp->SetStringField(
            TEXT("error"),
            FString::Printf(TEXT("Unsupported shape type: %s"), *ShapeType));
        Resp->SetStringField(
            TEXT("supportedShapes"),
            TEXT("sphere, box, circle, line, point, coordinate, cylinder, "
                 "cone, capsule, arrow, plane"));
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Unsupported shape type"), Resp,
                               TEXT("UNSUPPORTED_SHAPE"));
        return true;
      }

      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("shapeType"), ShapeType);
      Resp->SetStringField(
          TEXT("location"),
          FString::Printf(TEXT("%.2f,%.2f,%.2f"), Loc.X, Loc.Y, Loc.Z));
      Resp->SetNumberField(TEXT("duration"), Duration);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Debug shape drawn"), Resp, FString());
      return true;
#else
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), false);
      Resp->SetStringField(TEXT("error"),
                           TEXT("Debug shape drawing requires editor build"));
      Resp->SetStringField(TEXT("shapeType"), ShapeType);
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("Debug shape drawing not available in non-editor build"), Resp,
          TEXT("NOT_AVAILABLE"));
      return true;
#endif
    }

    // Handle niagara sub-action (delegates to existing spawn_niagara logic)
    if (LowerSub == TEXT("niagara") || LowerSub == TEXT("spawn_niagara")) {
      // Reuse logic below
    } else if (LowerSub.Equals(TEXT("set_niagara_parameter"))) {
      FString SystemName;
      LocalPayload->TryGetStringField(TEXT("systemName"), SystemName);
      FString ParameterName;
      LocalPayload->TryGetStringField(TEXT("parameterName"), ParameterName);
      FString ParameterType;
      LocalPayload->TryGetStringField(TEXT("parameterType"), ParameterType);
      if (ParameterName.IsEmpty()) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("parameterName required"), nullptr,
                               TEXT("INVALID_ARGUMENT"));
        return true;
      }
      if (ParameterType.IsEmpty())
        ParameterType = TEXT("Float");

      UE_LOG(
          LogMcpAutomationBridgeSubsystem, Verbose,
          TEXT("SetNiagaraParameter: Looking for actor '%s' to set param '%s'"),
          *SystemName, *ParameterName);

#if WITH_EDITOR
      if (!GEditor) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Editor not available"), nullptr,
                               TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
      }
      UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
      if (!ActorSS) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("EditorActorSubsystem not available"),
                               nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
      }

      const FName ParamName(*ParameterName);
      const TSharedPtr<FJsonValue> ValueField =
          LocalPayload->TryGetField(TEXT("value"));

      TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
      bool bApplied = false;

      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("SetNiagaraParameter: Looking for actor '%s'"), *SystemName);

      bool bActorFound = false;
      bool bComponentFound = false;

      for (AActor *Actor : AllActors) {
        if (!Actor)
          continue;
        if (!Actor->GetActorLabel().Equals(SystemName, ESearchCase::IgnoreCase))
          continue;

        bActorFound = true;
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("SetNiagaraParameter: Found actor '%s'"), *SystemName);
        UNiagaraComponent *NiComp =
            Actor->FindComponentByClass<UNiagaraComponent>();
        if (!NiComp) {
          UE_LOG(
              LogMcpAutomationBridgeSubsystem, Warning,
              TEXT("SetNiagaraParameter: Actor '%s' has no NiagaraComponent"),
              *SystemName);
          // Keep looking? No, actor label is unique-ish. But let's
          // assume unique.
          // But maybe we should break if we found the actor but no component?
          bComponentFound = false;
          break;
        }
        bComponentFound = true;

        if (ParameterType.Equals(TEXT("Float"), ESearchCase::IgnoreCase)) {
          double NumberValue = 0.0;
          bool bHasNumber =
              LocalPayload->TryGetNumberField(TEXT("value"), NumberValue);
          if (!bHasNumber && ValueField.IsValid()) {
            if (ValueField->Type == EJson::Number) {
              NumberValue = ValueField->AsNumber();
              bHasNumber = true;
            } else if (ValueField->Type == EJson::Object) {
              const TSharedPtr<FJsonObject> Obj = ValueField->AsObject();
              if (Obj.IsValid())
                bHasNumber = Obj->TryGetNumberField(TEXT("v"), NumberValue);
            }
          }
          if (bHasNumber) {
            NiComp->SetVariableFloat(ParamName,
                                     static_cast<float>(NumberValue));
            bApplied = true;
          }
        } else if (ParameterType.Equals(TEXT("Vector"),
                                        ESearchCase::IgnoreCase)) {
          const TSharedPtr<FJsonValue> Val =
              LocalPayload->TryGetField(TEXT("value"));
          UE_LOG(
              LogMcpAutomationBridgeSubsystem, Display,
              TEXT("SetNiagaraParameter: DEBUG - Processing Vector for '%s'"),
              *ParamName.ToString());

          const TArray<TSharedPtr<FJsonValue>> *ArrValue = nullptr;
          const TSharedPtr<FJsonObject> *ObjValue = nullptr;
          if (LocalPayload->TryGetArrayField(TEXT("value"), ArrValue) &&
              ArrValue && ArrValue->Num() >= 3) {
            const float X = static_cast<float>((*ArrValue)[0]->AsNumber());
            const float Y = static_cast<float>((*ArrValue)[1]->AsNumber());
            const float Z = static_cast<float>((*ArrValue)[2]->AsNumber());
            NiComp->SetVariableVec3(ParamName, FVector(X, Y, Z));
            bApplied = true;
            UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
                   TEXT("SetNiagaraParameter: DEBUG - Applied Vector from "
                        "Array: %f, %f, %f"),
                   X, Y, Z);
          } else if (LocalPayload->TryGetObjectField(TEXT("value"), ObjValue) &&
                     ObjValue) {
            double VX = 0, VY = 0, VZ = 0;
            (*ObjValue)->TryGetNumberField(TEXT("x"), VX);
            (*ObjValue)->TryGetNumberField(TEXT("y"), VY);
            (*ObjValue)->TryGetNumberField(TEXT("z"), VZ);
            NiComp->SetVariableVec3(ParamName,
                                    FVector((float)VX, (float)VY, (float)VZ));
            bApplied = true;
            UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
                   TEXT("SetNiagaraParameter: DEBUG - Applied Vector from "
                        "Object: %f, %f, %f"),
                   VX, VY, VZ);
          } else {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                   TEXT("SetNiagaraParameter: DEBUG - Failed to parse Vector "
                        "value."));
          }
        } else if (ParameterType.Equals(TEXT("Color"),
                                        ESearchCase::IgnoreCase)) {
          const TArray<TSharedPtr<FJsonValue>> *ArrValue = nullptr;
          if (LocalPayload->TryGetArrayField(TEXT("value"), ArrValue) &&
              ArrValue && ArrValue->Num() >= 3) {
            const float R = static_cast<float>((*ArrValue)[0]->AsNumber());
            const float G = static_cast<float>((*ArrValue)[1]->AsNumber());
            const float B = static_cast<float>((*ArrValue)[2]->AsNumber());
            const float Alpha =
                ArrValue->Num() > 3
                    ? static_cast<float>((*ArrValue)[3]->AsNumber())
                    : 1.0f;
            NiComp->SetVariableLinearColor(ParamName,
                                           FLinearColor(R, G, B, Alpha));
            bApplied = true;
          }
        } else if (ParameterType.Equals(TEXT("Bool"),
                                        ESearchCase::IgnoreCase)) {
          bool bValue = false;
          bool bHasBool = LocalPayload->TryGetBoolField(TEXT("value"), bValue);
          if (bHasBool) {
            NiComp->SetVariableBool(ParamName, bValue);
            bApplied = true;
          }
        }

        // If we found the actor and component but failed to apply, we stop
        // searching.
        break;
      }

      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), bApplied);
      Resp->SetBoolField(TEXT("applied"), bApplied);
      Resp->SetStringField(TEXT("actorName"), SystemName);
      Resp->SetStringField(TEXT("parameterName"), ParameterName);
      Resp->SetStringField(TEXT("parameterType"), ParameterType);

      if (bApplied) {
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Niagara parameter set"), Resp, FString());
      } else {
        FString ErrMsg = TEXT("Niagara parameter not applied");
        FString ErrCode = TEXT("SET_NIAGARA_PARAM_FAILED");

        if (!bActorFound) {
          ErrMsg = FString::Printf(TEXT("Actor '%s' not found"), *SystemName);
          ErrCode = TEXT("ACTOR_NOT_FOUND");
        } else if (!bComponentFound) {
          ErrMsg = FString::Printf(TEXT("Actor '%s' has no Niagara component"),
                                   *SystemName);
          ErrCode = TEXT("COMPONENT_NOT_FOUND");
        } else {
          // Check common failure reasons
          // Invalid Type?
          if (!ParameterType.Equals(TEXT("Float"), ESearchCase::IgnoreCase) &&
              !ParameterType.Equals(TEXT("Vector"), ESearchCase::IgnoreCase) &&
              !ParameterType.Equals(TEXT("Color"), ESearchCase::IgnoreCase) &&
              !ParameterType.Equals(TEXT("Bool"), ESearchCase::IgnoreCase)) {
            ErrMsg = FString::Printf(TEXT("Invalid parameter type: %s"),
                                     *ParameterType);
            ErrCode = TEXT("INVALID_ARGUMENT");
          }
        }

        SendAutomationResponse(RequestingSocket, RequestId, false, ErrMsg, Resp,
                               ErrCode);
      }
      return true;
#else
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("set_niagara_parameter requires editor build."), nullptr,
          TEXT("NOT_IMPLEMENTED"));
      return true;
#endif
    } else if (LowerSub.Equals(TEXT("activate_niagara"))) {
      FString SystemName;
      LocalPayload->TryGetStringField(TEXT("systemName"), SystemName);
      bool bReset = LocalPayload->HasField(TEXT("reset"))
                        ? GetJsonBoolField(LocalPayload, TEXT("reset"))
                        : true;

      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("ActivateNiagara: Looking for actor '%s'"), *SystemName);

#if WITH_EDITOR
      UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
      TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
      bool bFound = false;
      for (AActor *Actor : AllActors) {
        if (!Actor)
          continue;
        if (!Actor->GetActorLabel().Equals(SystemName, ESearchCase::IgnoreCase))
          continue;

        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("ActivateNiagara: Found actor '%s'"), *SystemName);
        UNiagaraComponent *NiComp =
            Actor->FindComponentByClass<UNiagaraComponent>();
        if (!NiComp)
          continue;

        NiComp->Activate(bReset);
        bFound = true;
        break;
      }
      if (bFound) {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("actorName"), SystemName);
        Resp->SetBoolField(TEXT("active"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Niagara system activated."), Resp);
      } else
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Niagara system not found."), nullptr,
                               TEXT("SYSTEM_NOT_FOUND"));
      return true;
#else
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("activate_niagara requires editor build."),
                             nullptr, TEXT("NOT_IMPLEMENTED"));
      return true;
#endif
    } else if (LowerSub.Equals(TEXT("deactivate_niagara"))) {
      FString SystemName;
      LocalPayload->TryGetStringField(TEXT("systemName"), SystemName);
      if (SystemName.IsEmpty())
        LocalPayload->TryGetStringField(TEXT("actorName"), SystemName);

#if WITH_EDITOR
      UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
      TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
      bool bFound = false;
      for (AActor *Actor : AllActors) {
        if (!Actor)
          continue;
        if (!Actor->GetActorLabel().Equals(SystemName, ESearchCase::IgnoreCase))
          continue;

        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("DeactivateNiagara: Found actor '%s'"), *SystemName);
        UNiagaraComponent *NiComp =
            Actor->FindComponentByClass<UNiagaraComponent>();
        if (!NiComp)
          continue;

        NiComp->Deactivate();
        bFound = true;
        break;
      }
      if (bFound) {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("actorName"), SystemName);
        Resp->SetBoolField(TEXT("active"), false);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Niagara system deactivated."), Resp);
      } else
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Niagara system not found."), nullptr,
                               TEXT("SYSTEM_NOT_FOUND"));
      return true;
#else
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("deactivate_niagara requires editor build."),
                             nullptr, TEXT("NOT_IMPLEMENTED"));
      return true;
#endif
    } else if (LowerSub.Equals(TEXT("advance_simulation"))) {
      FString SystemName;
      LocalPayload->TryGetStringField(TEXT("systemName"), SystemName);
      if (SystemName.IsEmpty())
        LocalPayload->TryGetStringField(TEXT("actorName"), SystemName);

      double DeltaTime = 0.1;
      LocalPayload->TryGetNumberField(TEXT("deltaTime"), DeltaTime);
      int32 Steps = 1;
      LocalPayload->TryGetNumberField(TEXT("steps"), Steps);

#if WITH_EDITOR
      UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
      TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
      bool bFound = false;
      for (AActor *Actor : AllActors) {
        if (!Actor)
          continue;
        if (!Actor->GetActorLabel().Equals(SystemName, ESearchCase::IgnoreCase))
          continue;

        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("AdvanceSimulation: Found actor '%s'"), *SystemName);
        UNiagaraComponent *NiComp =
            Actor->FindComponentByClass<UNiagaraComponent>();
        if (!NiComp)
          continue;

        for (int i = 0; i < Steps; i++) {
          NiComp->AdvanceSimulation(Steps, DeltaTime);
        }
        bFound = true;
        break;
      }
      if (bFound) {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("actorName"), SystemName);
        Resp->SetNumberField(TEXT("steps"), Steps);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Niagara simulation advanced."), Resp);
      } else
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Niagara system not found."), nullptr,
                               TEXT("SYSTEM_NOT_FOUND"));
      return true;
#else
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("advance_simulation requires editor build."),
                             nullptr, TEXT("NOT_IMPLEMENTED"));
      return true;
#endif
    } else if (LowerSub.Equals(TEXT("create_dynamic_light"))) {
      FString LightName;
      LocalPayload->TryGetStringField(TEXT("lightName"), LightName);
      FString LightType;
      LocalPayload->TryGetStringField(TEXT("lightType"), LightType);
      if (LightType.IsEmpty())
        LightType = TEXT("Point");

      // location
      FVector Loc(0, 0, 0);
      if (LocalPayload->HasField(TEXT("location"))) {
        const TSharedPtr<FJsonValue> LocVal =
            LocalPayload->TryGetField(TEXT("location"));
        if (LocVal.IsValid()) {
          if (LocVal->Type == EJson::Array) {
            const TArray<TSharedPtr<FJsonValue>> &Arr = LocVal->AsArray();
            if (Arr.Num() >= 3)
              Loc =
                  FVector((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(),
                          (float)Arr[2]->AsNumber());
          } else if (LocVal->Type == EJson::Object) {
            const TSharedPtr<FJsonObject> O = LocVal->AsObject();
            if (O.IsValid())
              Loc = FVector(
                  (float)(O->HasField(TEXT("x")) ? GetJsonNumberField(O, TEXT("x"))
                                                 : 0.0),
                  (float)(O->HasField(TEXT("y")) ? GetJsonNumberField(O, TEXT("y"))
                                                 : 0.0),
                  (float)(O->HasField(TEXT("z")) ? GetJsonNumberField(O, TEXT("z"))
                                                 : 0.0));
          }
        }
      }

      double Intensity = 0.0;
      LocalPayload->TryGetNumberField(TEXT("intensity"), Intensity);
      // color can be array or object
      bool bHasColor = false;
      double Cr = 1.0, Cg = 1.0, Cb = 1.0, Ca = 1.0;
      if (LocalPayload->HasField(TEXT("color"))) {
        const TArray<TSharedPtr<FJsonValue>> *ColArr = nullptr;
        if (LocalPayload->TryGetArrayField(TEXT("color"), ColArr) && ColArr &&
            ColArr->Num() >= 3) {
          bHasColor = true;
          Cr = (*ColArr)[0]->AsNumber();
          Cg = (*ColArr)[1]->AsNumber();
          Cb = (*ColArr)[2]->AsNumber();
          Ca = (ColArr->Num() > 3) ? (*ColArr)[3]->AsNumber() : 1.0;
        } else {
          const TSharedPtr<FJsonObject> *CO = nullptr;
          if (LocalPayload->TryGetObjectField(TEXT("color"), CO) && CO &&
              (*CO).IsValid()) {
            bHasColor = true;
            Cr = (*CO)->HasField(TEXT("r")) ? GetJsonNumberField(*CO, TEXT("r"))
                                            : Cr;
            Cg = (*CO)->HasField(TEXT("g")) ? GetJsonNumberField(*CO, TEXT("g"))
                                            : Cg;
            Cb = (*CO)->HasField(TEXT("b")) ? GetJsonNumberField(*CO, TEXT("b"))
                                            : Cb;
            Ca = (*CO)->HasField(TEXT("a")) ? GetJsonNumberField(*CO, TEXT("a"))
                                            : Ca;
          }
        }
      }

      // pulse param optional
      bool bPulseEnabled = false;
      double PulseFreq = 1.0;
      if (LocalPayload->HasField(TEXT("pulse"))) {
        const TSharedPtr<FJsonObject> *P = nullptr;
        if (LocalPayload->TryGetObjectField(TEXT("pulse"), P) && P &&
            (*P).IsValid()) {
          (*P)->TryGetBoolField(TEXT("enabled"), bPulseEnabled);
          (*P)->TryGetNumberField(TEXT("frequency"), PulseFreq);
        }
      }

#if WITH_EDITOR
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      if (!GEditor) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Editor not available"), nullptr,
                               TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
      }
      UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
      if (!ActorSS) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("EditorActorSubsystem not available"),
                               nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
      }

      UClass *ChosenClass = APointLight::StaticClass();
      UClass *CompClass = UPointLightComponent::StaticClass();
      FString LT = LightType.ToLower();
      if (LT == TEXT("spot") || LT == TEXT("spotlight")) {
        ChosenClass = ASpotLight::StaticClass();
        CompClass = USpotLightComponent::StaticClass();
      } else if (LT == TEXT("directional") || LT == TEXT("directionallight")) {
        ChosenClass = ADirectionalLight::StaticClass();
        CompClass = UDirectionalLightComponent::StaticClass();
      } else if (LT == TEXT("rect") || LT == TEXT("rectlight")) {
        ChosenClass = ARectLight::StaticClass();
        CompClass = URectLightComponent::StaticClass();
      }

      AActor *Spawned = SpawnActorInActiveWorld<AActor>(ChosenClass, Loc,
                                                        FRotator::ZeroRotator);
      if (!Spawned) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Failed to spawn light actor"), nullptr,
                               TEXT("CREATE_DYNAMIC_LIGHT_FAILED"));
        return true;
      }

      UActorComponent *C = Spawned->GetComponentByClass(CompClass);
      if (C) {
        if (ULightComponent *LC = Cast<ULightComponent>(C)) {
          LC->SetIntensity(static_cast<float>(Intensity));
          if (bHasColor) {
            LC->SetLightColor(
                FLinearColor(static_cast<float>(Cr), static_cast<float>(Cg),
                             static_cast<float>(Cb), static_cast<float>(Ca)));
          }
        }
      }

      if (!LightName.IsEmpty()) {
        Spawned->SetActorLabel(LightName);
      }
      if (bPulseEnabled) {
        Spawned->Tags.Add(
            FName(*FString::Printf(TEXT("MCP_PULSE:%g"), PulseFreq)));
      }

      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("actor"), Spawned->GetActorLabel());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Dynamic light created"), Resp, FString());
      return true;
#else
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("create_dynamic_light requires editor build."), nullptr,
          TEXT("NOT_IMPLEMENTED"));
      return true;
#endif
    } else if (LowerSub.Equals(TEXT("cleanup"))) {
      FString Filter;
      LocalPayload->TryGetStringField(TEXT("filter"), Filter);
      if (Filter.IsEmpty()) {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetNumberField(TEXT("removed"), 0);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Cleanup skipped (empty filter)"), Resp,
                               FString());
        return true;
      }
#if WITH_EDITOR
      if (!GEditor) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Editor not available"), nullptr,
                               TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
      }
      UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
      if (!ActorSS) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("EditorActorSubsystem not available"),
                               nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
      }
      TArray<AActor *> Actors = ActorSS->GetAllLevelActors();
      TArray<FString> Removed;
      for (AActor *A : Actors) {
        if (!A)
          continue;
        FString Label = A->GetActorLabel();
        if (Label.IsEmpty())
          continue;
        if (!Label.StartsWith(Filter, ESearchCase::IgnoreCase))
          continue;
        bool bDel = ActorSS->DestroyActor(A);
        if (bDel)
          Removed.Add(Label);
      }
      TArray<TSharedPtr<FJsonValue>> Arr;
      for (const FString &S : Removed)
        Arr.Add(MakeShared<FJsonValueString>(S));
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetArrayField(TEXT("removedActors"), Arr);
      Resp->SetNumberField(TEXT("removed"), Removed.Num());
      SendAutomationResponse(
          RequestingSocket, RequestId, true,
          FString::Printf(TEXT("Cleanup completed (removed=%d)"),
                          Removed.Num()),
          Resp, FString());
      return true;
#else
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("cleanup requires editor build."), nullptr,
                             TEXT("NOT_IMPLEMENTED"));
      return true;
#endif
    }
  }

  // Spawn Niagara system in-level as a NiagaraActor (editor-only)
  bool bSpawnNiagara = Lower.Equals(TEXT("spawn_niagara"));
  if (bIsCreateEffect) {
    FString Sub;
    LocalPayload->TryGetStringField(TEXT("action"), Sub);
    FString LowerSub = Sub.ToLower();
    if (LowerSub == TEXT("niagara") || LowerSub == TEXT("spawn_niagara"))
      bSpawnNiagara = true;
    // If SubAction is empty and Action is create_effect, we fallthrough to
    // legacy behavior below
  }

  if (bSpawnNiagara) {
    FString SystemPath;
    LocalPayload->TryGetStringField(TEXT("systemPath"), SystemPath);
    if (SystemPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("systemPath required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Guard against non-existent assets to prevent LoadPackage warnings
    if (!UEditorAssetLibrary::DoesAssetExist(SystemPath)) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Niagara system asset not found: %s"),
                          *SystemPath),
          nullptr, TEXT("SYSTEM_NOT_FOUND"));
      return true;
    }

    // Location and optional rotation/scale
    FVector Loc(0, 0, 0);
    if (LocalPayload->HasField(TEXT("location"))) {
      const TSharedPtr<FJsonValue> LocVal =
          LocalPayload->TryGetField(TEXT("location"));
      if (LocVal.IsValid()) {
        if (LocVal->Type == EJson::Array) {
          const TArray<TSharedPtr<FJsonValue>> &Arr = LocVal->AsArray();
          if (Arr.Num() >= 3)
            Loc = FVector((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(),
                          (float)Arr[2]->AsNumber());
        } else if (LocVal->Type == EJson::Object) {
          const TSharedPtr<FJsonObject> O = LocVal->AsObject();
          if (O.IsValid())
            Loc = FVector(
                (float)(O->HasField(TEXT("x")) ? GetJsonNumberField(O, TEXT("x"))
                                               : 0.0),
                (float)(O->HasField(TEXT("y")) ? GetJsonNumberField(O, TEXT("y"))
                                               : 0.0),
                (float)(O->HasField(TEXT("z")) ? GetJsonNumberField(O, TEXT("z"))
                                               : 0.0));
        }
      }
    }

    // Rotation may be an array
    TArray<double> RotArr = {0, 0, 0};
    const TArray<TSharedPtr<FJsonValue>> *RA = nullptr;
    if (LocalPayload->TryGetArrayField(TEXT("rotation"), RA) && RA &&
        RA->Num() >= 3) {
      RotArr[0] = (*RA)[0]->AsNumber();
      RotArr[1] = (*RA)[1]->AsNumber();
      RotArr[2] = (*RA)[2]->AsNumber();
    }

    // Scale may be an array or a single numeric value
    TArray<double> ScaleArr = {1, 1, 1};
    const TArray<TSharedPtr<FJsonValue>> *ScaleJsonArr = nullptr;
    if (LocalPayload->TryGetArrayField(TEXT("scale"), ScaleJsonArr) &&
        ScaleJsonArr && ScaleJsonArr->Num() >= 3) {
      ScaleArr[0] = (*ScaleJsonArr)[0]->AsNumber();
      ScaleArr[1] = (*ScaleJsonArr)[1]->AsNumber();
      ScaleArr[2] = (*ScaleJsonArr)[2]->AsNumber();
    } else if (LocalPayload->TryGetNumberField(TEXT("scale"), ScaleArr[0])) {
      ScaleArr[1] = ScaleArr[2] = ScaleArr[0];
    }

    const bool bAutoDestroy =
        LocalPayload->HasField(TEXT("autoDestroy"))
            ? GetJsonBoolField(LocalPayload, TEXT("autoDestroy"))
            : false;
    FString AttachToActor;
    LocalPayload->TryGetStringField(TEXT("attachToActor"), AttachToActor);

#if WITH_EDITOR
    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }
    UEditorActorSubsystem *ActorSS =
        GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("EditorActorSubsystem not available"),
                             nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
      return true;
    }

    UObject *NiagObj = UEditorAssetLibrary::LoadAsset(SystemPath);
    if (!NiagObj) {
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), false);
      Resp->SetStringField(TEXT("error"),
                           TEXT("Niagara system asset not found"));
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Niagara system not found"), Resp,
                             TEXT("SYSTEM_NOT_FOUND"));
      return true;
    }

    const FRotator SpawnRot(static_cast<float>(RotArr[0]),
                            static_cast<float>(RotArr[1]),
                            static_cast<float>(RotArr[2]));
    AActor *Spawned = SpawnActorInActiveWorld<AActor>(
        ANiagaraActor::StaticClass(), Loc, SpawnRot);
    if (!Spawned) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to spawn NiagaraActor"), nullptr,
                             TEXT("SPAWN_FAILED"));
      return true;
    }

    UNiagaraComponent *NiComp =
        Spawned->FindComponentByClass<UNiagaraComponent>();
    if (NiComp && NiagObj->IsA<UNiagaraSystem>()) {
      NiComp->SetAsset(Cast<UNiagaraSystem>(NiagObj));
      NiComp->SetWorldScale3D(FVector(ScaleArr[0], ScaleArr[1], ScaleArr[2]));
      NiComp->Activate(true); // Set to true
    }

    if (!AttachToActor.IsEmpty()) {
      AActor *Parent = nullptr;
      TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
      for (AActor *A : AllActors) {
        if (A &&
            A->GetActorLabel().Equals(AttachToActor, ESearchCase::IgnoreCase)) {
          Parent = A;
          break;
        }
      }
      if (Parent) {
        Spawned->AttachToActor(Parent,
                               FAttachmentTransformRules::KeepWorldTransform);
      }
    }

    // Set actor label
    FString Name;
    LocalPayload->TryGetStringField(TEXT("name"), Name);
    if (Name.IsEmpty())
      LocalPayload->TryGetStringField(TEXT("actorName"), Name);

    if (!Name.IsEmpty()) {
      Spawned->SetActorLabel(Name);
    } else {
      Spawned->SetActorLabel(FString::Printf(
          TEXT("Niagara_%lld"), FDateTime::Now().ToUnixTimestamp()));
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
           TEXT("spawn_niagara: Spawned actor '%s' (ID: %u)"),
           *Spawned->GetActorLabel(), Spawned->GetUniqueID());

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actor"), Spawned->GetActorLabel());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Niagara spawned"), Resp, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("spawn_niagara requires editor build."),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
  }

  // CLEANUP EFFECTS - remove actors whose label starts with the provided filter
  // (editor-only)
  bool bCleanup = Lower.Equals(TEXT("cleanup"));
  if (bIsCreateEffect) {
    FString Sub;
    LocalPayload->TryGetStringField(TEXT("action"), Sub);
    if (Sub.ToLower() == TEXT("cleanup"))
      bCleanup = true;
  }

  if (bCleanup) {
    FString Filter;
    LocalPayload->TryGetStringField(TEXT("filter"), Filter);
    // Allow empty filter as a no-op success
    if (Filter.IsEmpty()) {
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetNumberField(TEXT("removed"), 0);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Cleanup skipped (empty filter)"), Resp,
                             FString());
      return true;
    }
#if WITH_EDITOR
    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }
    UEditorActorSubsystem *ActorSS =
        GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("EditorActorSubsystem not available"),
                             nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
      return true;
    }
    TArray<AActor *> Actors = ActorSS->GetAllLevelActors();
    TArray<FString> Removed;
    for (AActor *A : Actors) {
      if (!A) {
        continue;
      }
      FString Label = A->GetActorLabel();
      if (Label.IsEmpty()) {
        continue;
      }
      if (!Label.StartsWith(Filter, ESearchCase::IgnoreCase)) {
        continue;
      }
      bool bDel = ActorSS->DestroyActor(A);
      if (bDel) {
        Removed.Add(Label);
      }
    }
    TArray<TSharedPtr<FJsonValue>> Arr;
    for (const FString &S : Removed) {
      Arr.Add(MakeShared<FJsonValueString>(S));
    }
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetArrayField(TEXT("removedActors"), Arr);
    Resp->SetNumberField(TEXT("removed"), Removed.Num());
    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Cleanup completed (removed=%d)"), Removed.Num()),
        Resp, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("cleanup requires editor build."), nullptr,
                           TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
  }

  // STUB HANDLERS FOR TEST COVERAGE - NOW IMPLEMENTED
  bool bCreateRibbon = Lower.Equals(TEXT("create_niagara_ribbon"));
  bool bCreateFog = Lower.Equals(TEXT("create_volumetric_fog"));
  bool bCreateTrail = Lower.Equals(TEXT("create_particle_trail"));
  bool bCreateEnv = Lower.Equals(TEXT("create_environment_effect"));
  bool bCreateImpact = Lower.Equals(TEXT("create_impact_effect"));

  if (bIsCreateEffect) {
    FString Sub;
    LocalPayload->TryGetStringField(TEXT("action"), Sub);
    FString LSub = Sub.ToLower();
    if (LSub == TEXT("create_niagara_ribbon"))
      bCreateRibbon = true;
    if (LSub == TEXT("create_volumetric_fog"))
      bCreateFog = true;
    if (LSub == TEXT("create_particle_trail"))
      bCreateTrail = true;
    if (LSub == TEXT("create_environment_effect"))
      bCreateEnv = true;
    if (LSub == TEXT("create_impact_effect"))
      bCreateImpact = true;
  }

  if (bCreateRibbon) {
    // Require systemPath
    return CreateNiagaraEffect(RequestId, Payload, RequestingSocket,
                               TEXT("create_niagara_ribbon"), FString());
  }
  if (bCreateFog) {
    return CreateNiagaraEffect(RequestId, Payload, RequestingSocket,
                               TEXT("create_volumetric_fog"), FString());
  }
  if (bCreateTrail) {
    return CreateNiagaraEffect(RequestId, Payload, RequestingSocket,
                               TEXT("create_particle_trail"), FString());
  }
  if (bCreateEnv) {
    return CreateNiagaraEffect(RequestId, Payload, RequestingSocket,
                               TEXT("create_environment_effect"), FString());
  }
  if (bCreateImpact) {
    return CreateNiagaraEffect(RequestId, Payload, RequestingSocket,
                               TEXT("create_impact_effect"), FString());
  }

  return false;
}

// Helper function to create Niagara effects with default systems
bool UMcpAutomationBridgeSubsystem::CreateNiagaraEffect(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, const FString &EffectName,
    const FString &DefaultSystemPath) {
#if WITH_EDITOR
  if (!GEditor) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("error"), TEXT("Editor not available"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Editor not available"), Resp,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("error"),
                         TEXT("EditorActorSubsystem not available"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("EditorActorSubsystem not available"), Resp,
                           TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
    return true;
  }

  // Get custom system path or use default
  FString SystemPath = DefaultSystemPath;
  Payload->TryGetStringField(TEXT("systemPath"), SystemPath);

  if (SystemPath.IsEmpty()) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("systemPath is required for %s. Please provide a "
                             "valid asset path (e.g. /Game/Effects/MySystem)"),
                        *EffectName));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("systemPath required"), Resp,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Location
  FVector Loc(0, 0, 0);
  if (Payload->HasField(TEXT("location"))) {
    const TSharedPtr<FJsonValue> LocVal =
        Payload->TryGetField(TEXT("location"));
    if (LocVal.IsValid()) {
      if (LocVal->Type == EJson::Array) {
        const TArray<TSharedPtr<FJsonValue>> &Arr = LocVal->AsArray();
        if (Arr.Num() >= 3)
          Loc = FVector((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(),
                        (float)Arr[2]->AsNumber());
      } else if (LocVal->Type == EJson::Object) {
        const TSharedPtr<FJsonObject> O = LocVal->AsObject();
        if (O.IsValid())
          Loc = FVector(
              (float)(O->HasField(TEXT("x")) ? GetJsonNumberField(O, TEXT("x"))
                                             : 0.0),
              (float)(O->HasField(TEXT("y")) ? GetJsonNumberField(O, TEXT("y"))
                                             : 0.0),
              (float)(O->HasField(TEXT("z")) ? GetJsonNumberField(O, TEXT("z"))
                                             : 0.0));
      }
    }
  }

  // Load the Niagara system
  if (!UEditorAssetLibrary::DoesAssetExist(SystemPath)) {
    SendAutomationResponse(
        RequestingSocket, RequestId, false,
        FString::Printf(TEXT("Niagara system asset not found: %s"),
                        *SystemPath),
        nullptr, TEXT("SYSTEM_NOT_FOUND"));
    return true;
  }

  UObject *NiagObj = UEditorAssetLibrary::LoadAsset(SystemPath);
  if (!NiagObj) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("error"), TEXT("Niagara system asset not found"));
    Resp->SetStringField(TEXT("systemPath"), SystemPath);
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Niagara system not found"), Resp,
                           TEXT("SYSTEM_NOT_FOUND"));
    return true;
  }

  // Spawn the actor
  AActor *Spawned = SpawnActorInActiveWorld<AActor>(
      ANiagaraActor::StaticClass(), Loc, FRotator::ZeroRotator);
  if (!Spawned) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("error"), TEXT("Failed to spawn Niagara actor"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Failed to spawn Niagara actor"), Resp,
                           TEXT("SPAWN_FAILED"));
    return true;
  }

  // Configure the Niagara component
  UNiagaraComponent *NiComp =
      Spawned->FindComponentByClass<UNiagaraComponent>();
  if (NiComp && NiagObj->IsA<UNiagaraSystem>()) {
    NiComp->SetAsset(Cast<UNiagaraSystem>(NiagObj));
    NiComp->Activate(true);
  }

  // Set actor label
  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);
  if (Name.IsEmpty())
    Payload->TryGetStringField(TEXT("actorName"), Name);

  if (!Name.IsEmpty()) {
    Spawned->SetActorLabel(Name);
  } else {
    Spawned->SetActorLabel(FString::Printf(
        TEXT("%s_%lld"), *EffectName.Replace(TEXT("create_"), TEXT("")),
        FDateTime::Now().ToUnixTimestamp()));
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("CreateNiagaraEffect: Spawned actor '%s' (ID: %u)"),
         *Spawned->GetActorLabel(), Spawned->GetUniqueID());

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("effectType"), EffectName);
  Resp->SetStringField(TEXT("systemPath"), SystemPath);
  Resp->SetStringField(TEXT("actorName"), Spawned->GetActorLabel());
  Resp->SetNumberField(TEXT("actorId"), Spawned->GetUniqueID());
  SendAutomationResponse(
      RequestingSocket, RequestId, true,
      FString::Printf(TEXT("%s created successfully"), *EffectName), Resp,
      FString());
  return true;
#else
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), false);
  Resp->SetStringField(TEXT("error"),
                       TEXT("Effect creation requires editor build"));
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Effect creation not available in non-editor build"), Resp,
      TEXT("NOT_AVAILABLE"));
  return true;
#endif
}