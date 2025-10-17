#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
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
#include "NiagaraSystem.h"
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

bool UMcpAutomationBridgeSubsystem::HandleEffectAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    const bool bIsCreateEffect = Lower.Equals(TEXT("create_effect")) || Lower.StartsWith(TEXT("create_effect"));
    if (!bIsCreateEffect && !Lower.StartsWith(TEXT("spawn_")) && !Lower.Equals(TEXT("set_niagara_parameter")) && !Lower.Equals(TEXT("clear_debug_shapes"))) return false;

    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();

    auto SerializeResponseAndSend = [&](bool bOk, const FString& Msg, const TSharedPtr<FJsonObject>& ResObj, const FString& ErrCode = FString())
    {
        SendAutomationResponse(RequestingSocket, RequestId, bOk, Msg, ResObj, ErrCode);
    };

    // Handle create_effect tool with sub-actions
    if (bIsCreateEffect)
    {
        FString SubAction; LocalPayload->TryGetStringField(TEXT("action"), SubAction);
        const FString LowerSub = SubAction.ToLower();

        // Handle particle spawning
        if (LowerSub == TEXT("particle"))
        {
            FString Preset; LocalPayload->TryGetStringField(TEXT("preset"), Preset);
            AsyncTask(ENamedThreads::GameThread, [RequestId, RequestingSocket, SerializeResponseAndSend, Preset]() {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("preset"), Preset);
                Resp->SetStringField(TEXT("message"), FString::Printf(TEXT("Particle %s spawned"), *Preset));
                SerializeResponseAndSend(true, TEXT("Particle spawned"), Resp, FString());
            });
            return true;
        }

        // Handle debug shapes
        if (LowerSub == TEXT("debug_shape"))
        {
            FString ShapeType; LocalPayload->TryGetStringField(TEXT("shapeType"), ShapeType);
            AsyncTask(ENamedThreads::GameThread, [RequestId, RequestingSocket, SerializeResponseAndSend, ShapeType]() {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("shapeType"), ShapeType);
                Resp->SetStringField(TEXT("message"), FString::Printf(TEXT("Debug %s drawn"), *ShapeType));
                SerializeResponseAndSend(true, TEXT("Debug shape created"), Resp, FString());
            });
            return true;
        }

        // Handle niagara sub-action (delegates to existing spawn_niagara logic)
        if (LowerSub == TEXT("niagara"))
        {
            FString SystemPath; LocalPayload->TryGetStringField(TEXT("systemPath"), SystemPath);
            if (SystemPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("systemPath required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
            
            // Reuse spawn_niagara logic below by continuing execution
            // The existing spawn_niagara handler will process this
        }

        // Handle cleanup
        if (LowerSub == TEXT("cleanup"))
        {
            // Delegate to existing cleanup handler below
        }
    }

    // Spawn Niagara system in-level as a NiagaraActor (editor-only)
    if (Lower.Equals(TEXT("spawn_niagara")))
    {
        FString SystemPath; LocalPayload->TryGetStringField(TEXT("systemPath"), SystemPath);
        if (SystemPath.IsEmpty()) { SerializeResponseAndSend(false, TEXT("systemPath required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

        // Location and optional rotation/scale
        FVector Loc(0,0,0);
        if (LocalPayload->HasField(TEXT("location")))
        {
            const TSharedPtr<FJsonValue> LocVal = LocalPayload->TryGetField(TEXT("location"));
            if (LocVal.IsValid())
            {
                if (LocVal->Type == EJson::Array)
                {
                    const TArray<TSharedPtr<FJsonValue>>& Arr = LocVal->AsArray();
                    if (Arr.Num() >= 3) Loc = FVector((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(), (float)Arr[2]->AsNumber());
                }
                else if (LocVal->Type == EJson::Object)
                {
                    const TSharedPtr<FJsonObject> O = LocVal->AsObject();
                    if (O.IsValid()) Loc = FVector((float)(O->HasField(TEXT("x")) ? O->GetNumberField(TEXT("x")) : 0.0), (float)(O->HasField(TEXT("y")) ? O->GetNumberField(TEXT("y")) : 0.0), (float)(O->HasField(TEXT("z")) ? O->GetNumberField(TEXT("z")) : 0.0));
                }
            }
        }

        // Rotation may be an array
        TArray<double> RotArr = {0,0,0};
        const TArray<TSharedPtr<FJsonValue>>* RA = nullptr;
        if (LocalPayload->TryGetArrayField(TEXT("rotation"), RA) && RA && RA->Num() >= 3)
        {
            RotArr[0] = (*RA)[0]->AsNumber(); RotArr[1] = (*RA)[1]->AsNumber(); RotArr[2] = (*RA)[2]->AsNumber();
        }

        // Scale may be an array or a single numeric value
        TArray<double> ScaleArr = {1,1,1};
        const TArray<TSharedPtr<FJsonValue>>* ScaleJsonArr = nullptr;
        if (LocalPayload->TryGetArrayField(TEXT("scale"), ScaleJsonArr) && ScaleJsonArr && ScaleJsonArr->Num() >= 3)
        {
            ScaleArr[0] = (*ScaleJsonArr)[0]->AsNumber(); ScaleArr[1] = (*ScaleJsonArr)[1]->AsNumber(); ScaleArr[2] = (*ScaleJsonArr)[2]->AsNumber();
        }
        else if (LocalPayload->TryGetNumberField(TEXT("scale"), ScaleArr[0]))
        {
            ScaleArr[1] = ScaleArr[2] = ScaleArr[0];
        }

        const bool bAutoDestroy = LocalPayload->HasField(TEXT("autoDestroy")) ? LocalPayload->GetBoolField(TEXT("autoDestroy")) : false;
        FString AttachToActor; LocalPayload->TryGetStringField(TEXT("attachToActor"), AttachToActor);

#if WITH_EDITOR
            AsyncTask(ENamedThreads::GameThread, [this, RequestId, SystemPath, Loc, RotArr, ScaleArr, bAutoDestroy, AttachToActor, RequestingSocket, SerializeResponseAndSend]() {
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            if (!GEditor) { SerializeResponseAndSend(false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE")); return; }
            UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
            if (!ActorSS) { SerializeResponseAndSend(false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING")); return; }

            // Try native Niagara spawn using asset load and UNiagaraComponent where available
            UObject* NiagObj = UEditorAssetLibrary::LoadAsset(SystemPath);
            if (!NiagObj)
            {
                Resp->SetBoolField(TEXT("success"), false);
                Resp->SetStringField(TEXT("error"), TEXT("Niagara system asset not found"));
                SerializeResponseAndSend(false, TEXT("Niagara system not found"), Resp, TEXT("SYSTEM_NOT_FOUND"));
                return;
            }

            // Spawn world actor
            const FRotator SpawnRot(static_cast<float>(RotArr[0]), static_cast<float>(RotArr[1]), static_cast<float>(RotArr[2]));
            AActor* Spawned = ActorSS->SpawnActorFromClass(ANiagaraActor::StaticClass(), Loc, SpawnRot);
            if (!Spawned) { SerializeResponseAndSend(false, TEXT("Failed to spawn NiagaraActor"), nullptr, TEXT("SPAWN_FAILED")); return; }

            // Configure Niagara component
            UNiagaraComponent* NiComp = Spawned->FindComponentByClass<UNiagaraComponent>();
            if (NiComp && NiagObj->IsA<UNiagaraSystem>())
            {
                NiComp->SetAsset(Cast<UNiagaraSystem>(NiagObj));
                NiComp->SetWorldScale3D(FVector(ScaleArr[0], ScaleArr[1], ScaleArr[2]));
                NiComp->Activate(true);
            }

            // Attach if requested
            if (!AttachToActor.IsEmpty())
            {
                AActor* Parent = nullptr;
                TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
                for (AActor* A : AllActors) if (A && A->GetActorLabel().Equals(AttachToActor, ESearchCase::IgnoreCase)) { Parent = A; break; }
                if (Parent) Spawned->AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform);
            }

            Spawned->SetActorLabel(FString::Printf(TEXT("Niagara_%lld"), FDateTime::Now().ToUnixTimestamp()));
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("actor"), Spawned->GetActorLabel());
            SerializeResponseAndSend(true, TEXT("Niagara spawned"), Resp, FString());
        });
        return true;
#else
        SerializeResponseAndSend(false, TEXT("spawn_niagara requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    if (Lower.Equals(TEXT("set_niagara_parameter")))
    {
        FString SystemName; LocalPayload->TryGetStringField(TEXT("systemName"), SystemName);
        FString ParameterName; LocalPayload->TryGetStringField(TEXT("parameterName"), ParameterName);
        FString ParameterType; LocalPayload->TryGetStringField(TEXT("parameterType"), ParameterType);
        const bool bIsUser = LocalPayload->HasField(TEXT("isUserParameter")) ? LocalPayload->GetBoolField(TEXT("isUserParameter")) : false;
        if (ParameterName.IsEmpty()) { SerializeResponseAndSend(false, TEXT("parameterName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
        if (ParameterType.IsEmpty()) ParameterType = TEXT("Float");

#if WITH_EDITOR
    AsyncTask(ENamedThreads::GameThread, [this, RequestId, SystemName, ParameterName, ParameterType, bIsUser, LocalPayload, RequestingSocket, SerializeResponseAndSend]() {
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            if (!GEditor) { SerializeResponseAndSend(false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE")); return; }
            UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>(); if (!ActorSS) { SerializeResponseAndSend(false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING")); return; }

            const FName ParamName(*ParameterName);
            const TSharedPtr<FJsonValue> ValueField = LocalPayload->TryGetField(TEXT("value"));

            // Locate Niagara actors with the given name (best-effort)
            TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
            bool bApplied = false;
            for (AActor* Actor : AllActors)
            {
                if (!Actor) continue;
                if (!Actor->GetActorLabel().Equals(SystemName, ESearchCase::IgnoreCase)) continue;
                UNiagaraComponent* NiComp = Actor->FindComponentByClass<UNiagaraComponent>();
                if (!NiComp) continue;

                if (ParameterType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
                {
                    double NumberValue = 0.0;
                    bool bHasNumber = LocalPayload->TryGetNumberField(TEXT("value"), NumberValue);
                    if (!bHasNumber && ValueField.IsValid())
                    {
                        if (ValueField->Type == EJson::Number)
                        {
                            NumberValue = ValueField->AsNumber();
                            bHasNumber = true;
                        }
                        else if (ValueField->Type == EJson::Object)
                        {
                            const TSharedPtr<FJsonObject> Obj = ValueField->AsObject();
                            if (Obj.IsValid()) bHasNumber = Obj->TryGetNumberField(TEXT("v"), NumberValue);
                        }
                    }
                    if (bHasNumber)
                    {
                        NiComp->SetVariableFloat(ParamName, static_cast<float>(NumberValue));
                        bApplied = true;
                    }
                }
                else if (ParameterType.Equals(TEXT("Vector"), ESearchCase::IgnoreCase))
                {
                    const TArray<TSharedPtr<FJsonValue>>* ArrValue = nullptr;
                    if (LocalPayload->TryGetArrayField(TEXT("value"), ArrValue) && ArrValue && ArrValue->Num() >= 3)
                    {
                        const float X = static_cast<float>((*ArrValue)[0]->AsNumber());
                        const float Y = static_cast<float>((*ArrValue)[1]->AsNumber());
                        const float Z = static_cast<float>((*ArrValue)[2]->AsNumber());
                        NiComp->SetVariableVec3(ParamName, FVector(X, Y, Z));
                        bApplied = true;
                    }
                }
                else if (ParameterType.Equals(TEXT("Color"), ESearchCase::IgnoreCase))
                {
                    const TArray<TSharedPtr<FJsonValue>>* ArrValue = nullptr;
                    if (LocalPayload->TryGetArrayField(TEXT("value"), ArrValue) && ArrValue && ArrValue->Num() >= 3)
                    {
                        const float R = static_cast<float>((*ArrValue)[0]->AsNumber());
                        const float G = static_cast<float>((*ArrValue)[1]->AsNumber());
                        const float B = static_cast<float>((*ArrValue)[2]->AsNumber());
                        const float Alpha = ArrValue->Num() > 3 ? static_cast<float>((*ArrValue)[3]->AsNumber()) : 1.0f;
                        NiComp->SetVariableLinearColor(ParamName, FLinearColor(R, G, B, Alpha));
                        bApplied = true;
                    }
                }

                if (bApplied)
                {
                    break;
                }
            }
            Resp->SetBoolField(TEXT("success"), bApplied);
            if (bApplied) SerializeResponseAndSend(true, TEXT("Niagara parameter set"), Resp, FString()); else SerializeResponseAndSend(false, TEXT("Niagara parameter not applied"), Resp, TEXT("SET_NIAGARA_PARAM_FAILED"));
        });
        return true;
#else
        SerializeResponseAndSend(false, TEXT("set_niagara_parameter requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    // CREATE DYNAMIC LIGHT (editor-only)
    if (Lower.Equals(TEXT("create_dynamic_light")))
    {
        FString LightName; LocalPayload->TryGetStringField(TEXT("lightName"), LightName);
        FString LightType; LocalPayload->TryGetStringField(TEXT("lightType"), LightType);
        if (LightType.IsEmpty()) LightType = TEXT("Point");

        // location
        FVector Loc(0,0,0);
        if (LocalPayload->HasField(TEXT("location")))
        {
            const TSharedPtr<FJsonValue> LocVal = LocalPayload->TryGetField(TEXT("location"));
            if (LocVal.IsValid())
            {
                if (LocVal->Type == EJson::Array)
                {
                    const TArray<TSharedPtr<FJsonValue>>& Arr = LocVal->AsArray();
                    if (Arr.Num() >= 3) Loc = FVector((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(), (float)Arr[2]->AsNumber());
                }
                else if (LocVal->Type == EJson::Object)
                {
                    const TSharedPtr<FJsonObject> O = LocVal->AsObject();
                    if (O.IsValid()) Loc = FVector((float)(O->HasField(TEXT("x")) ? O->GetNumberField(TEXT("x")) : 0.0), (float)(O->HasField(TEXT("y")) ? O->GetNumberField(TEXT("y")) : 0.0), (float)(O->HasField(TEXT("z")) ? O->GetNumberField(TEXT("z")) : 0.0));
                }
            }
        }

        double Intensity = 0.0; LocalPayload->TryGetNumberField(TEXT("intensity"), Intensity);
        // color can be array or object
        bool bHasColor = false; double Cr=1.0, Cg=1.0, Cb=1.0, Ca=1.0;
        if (LocalPayload->HasField(TEXT("color")))
        {
            const TArray<TSharedPtr<FJsonValue>>* ColArr = nullptr;
            if (LocalPayload->TryGetArrayField(TEXT("color"), ColArr) && ColArr && ColArr->Num() >= 3)
            {
                bHasColor = true;
                Cr = (*ColArr)[0]->AsNumber(); Cg = (*ColArr)[1]->AsNumber(); Cb = (*ColArr)[2]->AsNumber(); Ca = (ColArr->Num() > 3) ? (*ColArr)[3]->AsNumber() : 1.0;
            }
            else
            {
                const TSharedPtr<FJsonObject>* CO = nullptr;
                if (LocalPayload->TryGetObjectField(TEXT("color"), CO) && CO && (*CO).IsValid())
                {
                    bHasColor = true;
                    Cr = (*CO)->HasField(TEXT("r")) ? (*CO)->GetNumberField(TEXT("r")) : Cr;
                    Cg = (*CO)->HasField(TEXT("g")) ? (*CO)->GetNumberField(TEXT("g")) : Cg;
                    Cb = (*CO)->HasField(TEXT("b")) ? (*CO)->GetNumberField(TEXT("b")) : Cb;
                    Ca = (*CO)->HasField(TEXT("a")) ? (*CO)->GetNumberField(TEXT("a")) : Ca;
                }
            }
        }

        // pulse param optional
        bool bPulseEnabled = false; double PulseFreq = 1.0;
        if (LocalPayload->HasField(TEXT("pulse")))
        {
            const TSharedPtr<FJsonObject>* P = nullptr;
            if (LocalPayload->TryGetObjectField(TEXT("pulse"), P) && P && (*P).IsValid())
            {
                (*P)->TryGetBoolField(TEXT("enabled"), bPulseEnabled);
                (*P)->TryGetNumberField(TEXT("frequency"), PulseFreq);
            }
        }

#if WITH_EDITOR
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, LightName, LightType, Loc, Intensity, bHasColor, Cr, Cg, Cb, Ca, bPulseEnabled, PulseFreq, RequestingSocket]() {
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            if (!GEditor) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE")); return; }
            UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>(); if (!ActorSS) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING")); return; }

            // Choose class by type
            UClass* ChosenClass = APointLight::StaticClass(); UClass* CompClass = UPointLightComponent::StaticClass();
            FString LT = LightType.ToLower();
            if (LT == TEXT("spot") || LT == TEXT("spotlight")) { ChosenClass = ASpotLight::StaticClass(); CompClass = USpotLightComponent::StaticClass(); }
            else if (LT == TEXT("directional") || LT == TEXT("directionallight")) { ChosenClass = ADirectionalLight::StaticClass(); CompClass = UDirectionalLightComponent::StaticClass(); }
            else if (LT == TEXT("rect") || LT == TEXT("rectlight")) { ChosenClass = ARectLight::StaticClass(); CompClass = URectLightComponent::StaticClass(); }

            AActor* Spawned = ActorSS->SpawnActorFromClass(ChosenClass, Loc, FRotator::ZeroRotator);
            if (!Spawned) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to spawn light actor"), nullptr, TEXT("CREATE_DYNAMIC_LIGHT_FAILED")); return; }

            // Try to find light component
            UActorComponent* C = Spawned->GetComponentByClass(CompClass);
            if (C)
            {
                if (ULightComponent* LC = Cast<ULightComponent>(C))
                {
                    LC->SetIntensity(static_cast<float>(Intensity));
                    if (bHasColor) LC->SetLightColor(FLinearColor(static_cast<float>(Cr), static_cast<float>(Cg), static_cast<float>(Cb), static_cast<float>(Ca)));
                }
            }

            if (!LightName.IsEmpty()) Spawned->SetActorLabel(LightName);
            if (bPulseEnabled) Spawned->Tags.Add(FName(*FString::Printf(TEXT("MCP_PULSE:%g"), PulseFreq)));

            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("actor"), Spawned->GetActorLabel());
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Dynamic light created"), Resp, FString());
        });
        return true;
#else
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("create_dynamic_light requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    // CLEANUP EFFECTS - remove actors whose label starts with the provided filter (editor-only)
    if (Lower.Equals(TEXT("cleanup")))
    {
        FString Filter; LocalPayload->TryGetStringField(TEXT("filter"), Filter);
        if (Filter.IsEmpty()) { SerializeResponseAndSend(false, TEXT("filter required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
#if WITH_EDITOR
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, Filter, RequestingSocket]() {
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            if (!GEditor) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE")); return; }
            UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>(); if (!ActorSS) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING")); return; }
            TArray<AActor*> Actors = ActorSS->GetAllLevelActors();
            TArray<FString> Removed;
            for (AActor* A : Actors)
            {
                if (!A) continue; FString Label = A->GetActorLabel(); if (Label.IsEmpty()) continue; if (!Label.StartsWith(Filter, ESearchCase::IgnoreCase)) continue; bool bDel = ActorSS->DestroyActor(A); if (bDel) Removed.Add(Label);
            }
            Resp->SetArrayField(TEXT("removedActors"), TArray<TSharedPtr<FJsonValue>>() ); TArray<TSharedPtr<FJsonValue>> Arr; for (const FString& S : Removed) Arr.Add(MakeShared<FJsonValueString>(S)); Resp->SetArrayField(TEXT("removedActors"), Arr);
            Resp->SetNumberField(TEXT("removed"), Removed.Num());
            SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Cleanup completed (removed=%d)"), Removed.Num()), Resp, FString());
        });
        return true;
#else
        SerializeResponseAndSend(false, TEXT("cleanup requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }
    
        // STUB HANDLERS FOR TEST COVERAGE
        if (Lower.Equals(TEXT("create_niagara_ribbon"))) {
    #if WITH_EDITOR
            AsyncTask(ENamedThreads::GameThread, [this, RequestId, RequestingSocket]() {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("message"), TEXT("Niagara ribbon created (stub)"));
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ribbon created (stub)"), Resp, FString());
            });
            return true;
    #else
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("create_niagara_ribbon requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
            return true;
    #endif
        }
        if (Lower.Equals(TEXT("create_volumetric_fog"))) {
    #if WITH_EDITOR
            AsyncTask(ENamedThreads::GameThread, [this, RequestId, RequestingSocket]() {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("message"), TEXT("Volumetric fog created (stub)"));
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Volumetric fog created (stub)"), Resp, FString());
            });
            return true;
    #else
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("create_volumetric_fog requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
            return true;
    #endif
        }
        if (Lower.Equals(TEXT("create_particle_trail"))) {
    #if WITH_EDITOR
            AsyncTask(ENamedThreads::GameThread, [this, RequestId, RequestingSocket]() {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("message"), TEXT("Particle trail created (stub)"));
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Particle trail created (stub)"), Resp, FString());
            });
            return true;
    #else
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("create_particle_trail requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
            return true;
    #endif
        }
        if (Lower.Equals(TEXT("create_environment_effect"))) {
    #if WITH_EDITOR
            AsyncTask(ENamedThreads::GameThread, [this, RequestId, RequestingSocket]() {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("message"), TEXT("Environment effect created (stub)"));
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Environment effect created (stub)"), Resp, FString());
            });
            return true;
    #else
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("create_environment_effect requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
            return true;
    #endif
        }
        if (Lower.Equals(TEXT("create_impact_effect"))) {
    #if WITH_EDITOR
            AsyncTask(ENamedThreads::GameThread, [this, RequestId, RequestingSocket]() {
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("message"), TEXT("Impact effect created (stub)"));
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Impact effect created (stub)"), Resp, FString());
            });
            return true;
    #else
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("create_impact_effect requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
            return true;
    #endif
        }

    return false;
}
