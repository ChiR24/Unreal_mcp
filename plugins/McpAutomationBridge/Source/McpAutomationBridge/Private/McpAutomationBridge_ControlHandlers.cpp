#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "Async/Async.h"
#include "Misc/ScopeExit.h"
#if WITH_EDITOR
#include "IPythonScriptPlugin.h"
#include "EditorAssetLibrary.h"
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#if __has_include("Subsystems/UnrealEditorSubsystem.h")
#include "Subsystems/UnrealEditorSubsystem.h"
#define MCP_HAS_UNREALEDITOR_SUBSYSTEM 1
#elif __has_include("UnrealEditorSubsystem.h")
#include "UnrealEditorSubsystem.h"
#define MCP_HAS_UNREALEDITOR_SUBSYSTEM 1
#endif
#if __has_include("Subsystems/LevelEditorSubsystem.h")
#include "Subsystems/LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#elif __has_include("LevelEditorSubsystem.h")
#include "LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#endif
#include "Engine/Blueprint.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleControlActorAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("control_actor"), ESearchCase::IgnoreCase) && !Lower.StartsWith(TEXT("control_actor"))) return false;

    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("control_actor payload missing."), TEXT("INVALID_PAYLOAD")); return true; }

    FString SubAction; Payload->TryGetStringField(TEXT("action"), SubAction);
    const FString LowerSub = SubAction.ToLower();

    // Validate basic required params depending on subaction
    if (LowerSub == TEXT("spawn"))
    {
        FString ClassPath; Payload->TryGetStringField(TEXT("classPath"), ClassPath);
        if (ClassPath.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("spawn requires classPath"), TEXT("INVALID_ARGUMENT")); return true; }
    }

    // Build python script templates for each subaction. Execution must be on GameThread.
#if WITH_EDITOR
    AsyncTask(ENamedThreads::GameThread, [this, RequestId, Payload, LowerSub, RequestingSocket]() {
        if (!GEditor)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
            return;
        }

        UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
        if (!ActorSS)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
            return;
        }

        auto FindActorByName = [&](const FString& Target) -> AActor*
        {
            if (Target.IsEmpty())
            {
                return nullptr;
            }
            AActor* Found = nullptr;
            TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
            for (AActor* A : AllActors)
            {
                if (!A)
                {
                    continue;
                }
                if (A->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase) ||
                    A->GetName().Equals(Target, ESearchCase::IgnoreCase) ||
                    A->GetPathName().Equals(Target, ESearchCase::IgnoreCase))
                {
                    Found = A;
                    break;
                }
            }
            if (!Found)
            {
                if (UObject* Obj = UEditorAssetLibrary::LoadAsset(Target))
                {
                    Found = Cast<AActor>(Obj);
                }
            }
            return Found;
        };

        if (LowerSub == TEXT("spawn"))
        {
            FString ClassPath; Payload->TryGetStringField(TEXT("classPath"), ClassPath);
            FString ActorName; Payload->TryGetStringField(TEXT("actorName"), ActorName);

            FVector Location = FVector::ZeroVector;
            FRotator Rotation = FRotator::ZeroRotator;

            if (const TSharedPtr<FJsonValue> LocValue = Payload->TryGetField(TEXT("location")))
            {
                if (LocValue->Type == EJson::Array)
                {
                    const TArray<TSharedPtr<FJsonValue>>& Arr = LocValue->AsArray();
                    if (Arr.Num() >= 3)
                    {
                        Location = FVector(static_cast<float>(Arr[0]->AsNumber()), static_cast<float>(Arr[1]->AsNumber()), static_cast<float>(Arr[2]->AsNumber()));
                    }
                }
                else if (LocValue->Type == EJson::Object)
                {
                    ReadVectorField(LocValue->AsObject(), TEXT(""), Location, Location);
                }
            }

            if (const TSharedPtr<FJsonValue> RotValue = Payload->TryGetField(TEXT("rotation")))
            {
                if (RotValue->Type == EJson::Array)
                {
                    const TArray<TSharedPtr<FJsonValue>>& Arr = RotValue->AsArray();
                    if (Arr.Num() >= 3)
                    {
                        Rotation = FRotator(static_cast<float>(Arr[0]->AsNumber()), static_cast<float>(Arr[1]->AsNumber()), static_cast<float>(Arr[2]->AsNumber()));
                    }
                }
                else if (RotValue->Type == EJson::Object)
                {
                    ReadRotatorField(RotValue->AsObject(), TEXT(""), Rotation, Rotation);
                }
            }

            UClass* ResolvedClass = nullptr;
            if (ClassPath.StartsWith(TEXT("/")) || ClassPath.Contains(TEXT("/")))
            {
                if (UObject* Loaded = UEditorAssetLibrary::LoadAsset(ClassPath))
                {
                    if (UBlueprint* BP = Cast<UBlueprint>(Loaded))
                    {
                        ResolvedClass = BP->GeneratedClass;
                    }
                    else if (UClass* C = Cast<UClass>(Loaded))
                    {
                        ResolvedClass = C;
                    }
                }
            }
            if (!ResolvedClass)
            {
                ResolvedClass = ResolveClassByName(ClassPath);
            }

            if (!ResolvedClass)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("Class not found"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor class not found"), Resp, TEXT("CLASS_NOT_FOUND"));
                return;
            }

            AActor* Spawned = ActorSS->SpawnActorFromClass(ResolvedClass, Location, Rotation);
            if (!Spawned)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("Failed to spawn actor"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to spawn actor"), Resp, TEXT("SPAWN_FAILED"));
                return;
            }

            if (!ActorName.IsEmpty())
            {
                Spawned->SetActorLabel(ActorName);
            }

            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("actorName"), Spawned->GetActorLabel());
            Resp->SetStringField(TEXT("actorPath"), Spawned->GetPathName());
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor spawned"), Resp, FString());
            return;
        }

        if (LowerSub == TEXT("delete") || LowerSub == TEXT("remove"))
        {
            FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
            AActor* Found = FindActorByName(TargetName);
            if (!Found)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("Actor not found"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), Resp, TEXT("ACTOR_NOT_FOUND"));
                return;
            }

            const bool bDeleted = ActorSS->DestroyActor(Found);
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetBoolField(TEXT("success"), bDeleted);
            if (bDeleted)
            {
                Resp->SetStringField(TEXT("deleted"), Found->GetActorLabel());
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor deleted"), Resp, FString());
            }
            else
            {
                Resp->SetStringField(TEXT("error"), TEXT("Failed to delete actor"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to delete actor"), Resp, TEXT("DELETE_FAILED"));
            }
            return;
        }

        if (LowerSub == TEXT("apply_force") || LowerSub == TEXT("apply_force_to_actor"))
        {
            FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
            const TSharedPtr<FJsonObject>* ForceObj = nullptr;
            Payload->TryGetObjectField(TEXT("force"), ForceObj);

            FVector ForceVector = FVector::ZeroVector;
            if (ForceObj && (*ForceObj).IsValid())
            {
                double Fx = 0, Fy = 0, Fz = 0;
                (*ForceObj)->TryGetNumberField(TEXT("x"), Fx);
                (*ForceObj)->TryGetNumberField(TEXT("y"), Fy);
                (*ForceObj)->TryGetNumberField(TEXT("z"), Fz);
                ForceVector = FVector(static_cast<float>(Fx), static_cast<float>(Fy), static_cast<float>(Fz));
            }

            AActor* Found = FindActorByName(TargetName);
            if (!Found)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("Actor not found"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), Resp, TEXT("ACTOR_NOT_FOUND"));
                return;
            }

            UPrimitiveComponent* Prim = Found->FindComponentByClass<UPrimitiveComponent>();
            if (!Prim)
            {
                if (UStaticMeshComponent* SMC = Found->FindComponentByClass<UStaticMeshComponent>())
                {
                    Prim = SMC;
                }
            }

            if (!Prim)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetStringField(TEXT("error"), TEXT("No component to apply force"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("No component to apply force"), Resp, TEXT("NO_COMPONENT"));
                return;
            }

            Prim->AddForce(ForceVector);
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetBoolField(TEXT("success"), true);
            TArray<TSharedPtr<FJsonValue>> Applied;
            Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.X));
            Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.Y));
            Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.Z));
            Resp->SetArrayField(TEXT("applied"), Applied);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Force applied"), Resp, FString());
            return;
        }

        if (LowerSub == TEXT("list") || LowerSub == TEXT("list_actors"))
        {
            TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
            TArray<TSharedPtr<FJsonValue>> Arr;
            Arr.Reserve(AllActors.Num());

            for (AActor* A : AllActors)
            {
                if (!A)
                {
                    continue;
                }
                TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
                Entry->SetStringField(TEXT("name"), A->GetName());
                Entry->SetStringField(TEXT("label"), A->GetActorLabel());
                Entry->SetStringField(TEXT("class"), A->GetClass() ? A->GetClass()->GetPathName() : TEXT(""));
                Entry->SetStringField(TEXT("path"), A->GetPathName());
                Arr.Add(MakeShared<FJsonValueObject>(Entry));
            }

            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetArrayField(TEXT("actors"), Arr);
            Resp->SetNumberField(TEXT("count"), Arr.Num());
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor list retrieved"), Resp, FString());
            return;
        }

        if (LowerSub == TEXT("get") || LowerSub == TEXT("get_actor") || LowerSub == TEXT("get_actor_by_name"))
        {
            FString TargetName; Payload->TryGetStringField(TEXT("actorName"), TargetName);
            if (TargetName.IsEmpty())
            {
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("actorName required"), nullptr, TEXT("INVALID_ARGUMENT"));
                return;
            }

            AActor* Found = FindActorByName(TargetName);
            if (!Found)
            {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetBoolField(TEXT("success"), false);
                Resp->SetStringField(TEXT("error"), TEXT("Actor not found"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), Resp, TEXT("ACTOR_NOT_FOUND"));
                return;
            }

            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("name"), Found->GetName());
            Resp->SetStringField(TEXT("label"), Found->GetActorLabel());
            Resp->SetStringField(TEXT("path"), Found->GetPathName());
            Resp->SetStringField(TEXT("class"), Found->GetClass() ? Found->GetClass()->GetPathName() : TEXT(""));
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor resolved"), Resp, FString());
            return;
        }

        SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Unknown actor control action: %s"), *LowerSub), nullptr, TEXT("UNKNOWN_ACTION"));
    });
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor control requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("control_editor"), ESearchCase::IgnoreCase) && !Lower.StartsWith(TEXT("control_editor"))) return false;

    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("control_editor payload missing."), TEXT("INVALID_PAYLOAD")); return true; }

    FString SubAction; Payload->TryGetStringField(TEXT("action"), SubAction);
    const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR
    AsyncTask(ENamedThreads::GameThread, [this, RequestId, Payload, LowerSub, RequestingSocket]() mutable {
        FMcpPythonOutputCapture OutputCapture;
        const bool bCaptureLogs = (GLog != nullptr);
        if (bCaptureLogs) { GLog->AddOutputDevice(&OutputCapture); }
        ON_SCOPE_EXIT { if (bCaptureLogs && GLog) { GLog->RemoveOutputDevice(&OutputCapture); } };

        if (!FModuleManager::Get().IsModuleLoaded(TEXT("PythonScriptPlugin"))) FModuleManager::LoadModulePtr<IPythonScriptPlugin>(TEXT("PythonScriptPlugin"));
        IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
        if (!PythonPlugin)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("PythonScriptPlugin not available; cannot perform editor control."), nullptr, TEXT("PYTHON_PLUGIN_DISABLED"));
            return;
        }

        FString Script;
        if (LowerSub == TEXT("play"))
        {
#if WITH_EDITOR
            // Use LevelEditorSubsystem via GEditor where available
            if (GEditor)
            {
                if (ULevelEditorSubsystem* LES = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>())
                {
                    LES->EditorPlaySimulate();
                    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                    Resp->SetBoolField(TEXT("success"), true);
                    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Play requested via LevelEditorSubsystem"), Resp, FString());
                    return;
                }
            }
#endif
            // Fallback to Python script if editor subsystem not available
            Script = TEXT(R"PY(
import unreal, json
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les:
    try:
        les.editor_play_simulate()
        print('RESULT:' + json.dumps({'success': True, 'method': 'LevelEditorSubsystem'}))
    except Exception as e:
        print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
else:
    print('RESULT:' + json.dumps({'success': False, 'error': 'LevelEditorSubsystem not available'}))
)PY");
        }
        else if (LowerSub == TEXT("stop"))
        {
#if WITH_EDITOR
            if (GEditor)
            {
                if (ULevelEditorSubsystem* LES = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>())
                {
                    LES->EditorRequestEndPlay();
                    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                    Resp->SetBoolField(TEXT("success"), true);
                    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Stop requested via LevelEditorSubsystem"), Resp, FString());
                    return;
                }
            }
#endif
            Script = TEXT(R"PY(
import unreal, json
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les:
    try:
        les.editor_request_end_play()
        print('RESULT:' + json.dumps({'success': True, 'method': 'LevelEditorSubsystem'}))
    except Exception as e:
        print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
else:
    print('RESULT:' + json.dumps({'success': False, 'error': 'LevelEditorSubsystem not available'}))
)PY");
        }
        else if (LowerSub == TEXT("set_camera"))
        {
            const TSharedPtr<FJsonObject>* Loc = nullptr; FVector Location(0,0,0); FRotator Rotation(0,0,0);
            if (Payload->TryGetObjectField(TEXT("location"), Loc) && Loc && (*Loc).IsValid()) ReadVectorField(*Loc, TEXT(""), Location, Location);
            if (Payload->TryGetObjectField(TEXT("rotation"), Loc) && Loc && (*Loc).IsValid()) ReadRotatorField(*Loc, TEXT(""), Rotation, Rotation);
#if WITH_EDITOR
            // Prefer native UnrealEditorSubsystem when available
    #if defined(MCP_HAS_UNREALEDITOR_SUBSYSTEM)
            if (GEditor)
            {
                USubsystem* Dummy = nullptr; // dummy to avoid unused macro warnings
                UUnrealEditorSubsystem* UES = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();
                if (UES)
                {
                    // We're already on the game thread (outer AsyncTask ensures this), so call directly
                    UES->SetLevelViewportCameraInfo(Location, Rotation);
    #if defined(MCP_HAS_LEVELEDITOR_SUBSYSTEM)
                    if (ULevelEditorSubsystem* LES = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>())
                    {
                        LES->EditorInvalidateViewports();
                    }
    #endif
                    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetBoolField(TEXT("success"), true);
                    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Camera set (UnrealEditorSubsystem)"), Resp, FString());
                    return;
                }
            }
    #endif
#endif
            // Fallback to Python if native subsystem unavailable
            Script = FString::Printf(TEXT(R"PY(
import unreal, json
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
location = unreal.Vector(%f, %f, %f)
rotation = unreal.Rotator(%f, %f, %f)
if ues:
    ues.set_level_viewport_camera_info(location, rotation)
    try:
        if les:
            les.editor_invalidate_viewports()
    except Exception:
        pass
    print('RESULT:' + json.dumps({'success': True}))
else:
    print('RESULT:' + json.dumps({'success': False, 'error': 'UnrealEditorSubsystem not available'}))
)PY"), Location.X, Location.Y, Location.Z, Rotation.Pitch, Rotation.Yaw, Rotation.Roll);
        }
        else if (LowerSub == TEXT("set_view_mode"))
        {
            FString Mode; Payload->TryGetStringField(TEXT("viewMode"), Mode);
#if WITH_EDITOR
            if (GEditor)
            {
                // Map common aliases to viewmode names used by console
                FString LowerMode = Mode.ToLower();
                FString Chosen;
                if (LowerMode == TEXT("lit")) Chosen = TEXT("Lit");
                else if (LowerMode == TEXT("unlit")) Chosen = TEXT("Unlit");
                else if (LowerMode == TEXT("wireframe")) Chosen = TEXT("Wireframe");
                else if (LowerMode == TEXT("detaillighting")) Chosen = TEXT("DetailLighting");
                else if (LowerMode == TEXT("lightingonly")) Chosen = TEXT("LightingOnly");
                else if (LowerMode == TEXT("lightcomplexity")) Chosen = TEXT("LightComplexity");
                else if (LowerMode == TEXT("shadercomplexity")) Chosen = TEXT("ShaderComplexity");
                else if (LowerMode == TEXT("lightmapdensity")) Chosen = TEXT("LightmapDensity");
                else if (LowerMode == TEXT("stationarylightoverlap")) Chosen = TEXT("StationaryLightOverlap");
                else if (LowerMode == TEXT("reflectionoverride")) Chosen = TEXT("ReflectionOverride");
                else Chosen = Mode; // pass-through unknown tokens to console

                const FString Cmd = FString::Printf(TEXT("viewmode %s"), *Chosen);
                if (GEditor->Exec(nullptr, *Cmd))
                {
                    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetBoolField(TEXT("success"), true); Resp->SetStringField(TEXT("viewMode"), Chosen);
                    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("View mode set"), Resp, FString());
                    return;
                }
            }
#endif
            // Fallback to Python if native exec not available
            const FString EscMode = Mode.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));
            Script = FString::Printf(TEXT(R"PY(
import unreal, json
mode = r"%s"
try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les:
        # Map simple names to engine view modes where possible
        vm = None
        try:
            if mode.lower() == 'lit':
                vm = unreal.ViewMode.VMI_Lit
            elif mode.lower() == 'unlit':
                vm = unreal.ViewMode.VMI_Unlit
            elif mode.lower() == 'wireframe':
                vm = unreal.ViewMode.VMI_Wireframe
            if vm is not None:
                les.set_viewport_show_flags(vm)
            print('RESULT:' + json.dumps({'success': True, 'viewMode': mode}))
        except Exception as e:
            print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
    else:
        print('RESULT:' + json.dumps({'success': False, 'error': 'LevelEditorSubsystem not available'}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
)PY"), *EscMode);
        }
        else
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Unknown editor control action: %s"), *LowerSub), nullptr, TEXT("UNKNOWN_ACTION"));
            return;
        }

        // Legacy Python fallback removed for editor control actions.
        // Native paths are handled above (play/stop/set_camera/set_view_mode).
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Python fallback removed for editor control actions. Use native handlers or call execute_editor_python explicitly (deprecated)."), nullptr, TEXT("PYTHON_FALLBACK_REMOVED"));
        return;
    });
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor control requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
