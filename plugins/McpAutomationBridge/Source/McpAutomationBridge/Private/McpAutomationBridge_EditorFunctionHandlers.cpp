#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#if WITH_EDITOR
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#if __has_include("Subsystems/UnrealEditorSubsystem.h")
#include "Subsystems/UnrealEditorSubsystem.h"
#elif __has_include("UnrealEditorSubsystem.h")
#include "UnrealEditorSubsystem.h"
#endif
#if __has_include("Subsystems/LevelEditorSubsystem.h")
#include "Subsystems/LevelEditorSubsystem.h"
#elif __has_include("LevelEditorSubsystem.h")
#include "LevelEditorSubsystem.h"
#endif
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#if __has_include("EditorLoadingAndSavingUtils.h")
#include "EditorLoadingAndSavingUtils.h"
#elif __has_include("FileHelpers.h")
#include "FileHelpers.h"
#endif
#include "Misc/Base64.h"
#include "Factories/Factory.h"
#include "UObject/SoftObjectPath.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundCue.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleExecuteEditorFunction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("execute_editor_function"), ESearchCase::IgnoreCase) && !Lower.Contains(TEXT("execute_editor_function"))) return false;

    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("execute_editor_function payload missing."), TEXT("INVALID_PAYLOAD")); return true; }

    FString FunctionName; Payload->TryGetStringField(TEXT("functionName"), FunctionName);
    if (FunctionName.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("functionName required"), TEXT("INVALID_ARGUMENT")); return true; }

    const FString FN = FunctionName.ToUpper();

#if WITH_EDITOR
    // Dispatch a handful of well-known functions to native handlers
    if (FN == TEXT("GET_ALL_ACTORS") || FN == TEXT("GET_ALL_ACTORS_SIMPLE"))
    {
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, RequestingSocket]() {
            if (!GEditor) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE")); return; }
            UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>(); if (!ActorSS) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING")); return; }
            TArray<AActor*> Actors = ActorSS->GetAllLevelActors(); TArray<TSharedPtr<FJsonValue>> Arr; Arr.Reserve(Actors.Num());
            for (AActor* A : Actors) { if (!A) continue; TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>(); E->SetStringField(TEXT("name"), A->GetName()); E->SetStringField(TEXT("label"), A->GetActorLabel()); E->SetStringField(TEXT("path"), A->GetPathName()); E->SetStringField(TEXT("class"), A->GetClass() ? A->GetClass()->GetPathName() : TEXT("")); Arr.Add(MakeShared<FJsonValueObject>(E)); }
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>(); Result->SetArrayField(TEXT("actors"), Arr); Result->SetNumberField(TEXT("count"), Arr.Num()); SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor list"), Result, FString());
        });
        return true;
    }

    if (FN == TEXT("SPAWN_ACTOR") || FN == TEXT("SPAWN_ACTOR_AT_LOCATION"))
    {
        FString ClassPath; Payload->TryGetStringField(TEXT("class_path"), ClassPath); if (ClassPath.IsEmpty()) Payload->TryGetStringField(TEXT("classPath"), ClassPath);
        const TSharedPtr<FJsonObject>* LocObj = nullptr; FVector Loc(0,0,0); FRotator Rot(0,0,0);
        if (Payload->TryGetObjectField(TEXT("params"), LocObj) && LocObj && (*LocObj).IsValid())
        {
            const TSharedPtr<FJsonObject>& P = *LocObj; ReadVectorField(P, TEXT("location"), Loc, Loc); ReadRotatorField(P, TEXT("rotation"), Rot, Rot);
        }
        else
        {
            if (const TSharedPtr<FJsonValue> LocVal = Payload->TryGetField(TEXT("location")))
            {
                if (LocVal->Type == EJson::Array)
                {
                    const TArray<TSharedPtr<FJsonValue>>& A = LocVal->AsArray(); if (A.Num() >= 3) Loc = FVector((float)A[0]->AsNumber(), (float)A[1]->AsNumber(), (float)A[2]->AsNumber());
                }
                else if (LocVal->Type == EJson::Object)
                {
                    const TSharedPtr<FJsonObject> LocObject = LocVal->AsObject(); if (LocObject.IsValid()) ReadVectorField(LocObject, TEXT("location"), Loc, Loc);
                }
            }
        }

        AsyncTask(ENamedThreads::GameThread, [this, RequestId, ClassPath, Loc, Rot, RequestingSocket]() {
            if (!GEditor) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE")); return; }
            UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>(); if (!ActorSS) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING")); return; }
            UClass* Resolved = nullptr;
            if (!ClassPath.IsEmpty())
            {
                Resolved = ResolveClassByName(ClassPath);
            }
            if (!Resolved) { TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>(); Err->SetStringField(TEXT("error"), TEXT("Class not found")); SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Class not found"), Err, TEXT("CLASS_NOT_FOUND")); return; }
            AActor* Spawned = ActorSS->SpawnActorFromClass(Resolved, Loc, Rot); if (!Spawned) { TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>(); Err->SetStringField(TEXT("error"), TEXT("Spawn failed")); SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Spawn failed"), Err, TEXT("SPAWN_FAILED")); return; }
            TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>(); Out->SetStringField(TEXT("actorName"), Spawned->GetActorLabel()); Out->SetStringField(TEXT("actorPath"), Spawned->GetPathName()); Out->SetBoolField(TEXT("success"), true); SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor spawned"), Out, FString());
        });
        return true;
    }

    if (FN == TEXT("DELETE_ACTOR") || FN == TEXT("DESTROY_ACTOR"))
    {
        FString Target; Payload->TryGetStringField(TEXT("actor_name"), Target); if (Target.IsEmpty()) Payload->TryGetStringField(TEXT("actorName"), Target);
        if (Target.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("actor_name required"), TEXT("INVALID_ARGUMENT")); return true; }
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, Target, RequestingSocket]() {
            if (!GEditor) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE")); return; }
            UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>(); if (!ActorSS) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING")); return; }
            AActor* Found = nullptr; for (AActor* A : ActorSS->GetAllLevelActors()) { if (!A) continue; if (A->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase) || A->GetName().Equals(Target, ESearchCase::IgnoreCase) || A->GetPathName().Equals(Target, ESearchCase::IgnoreCase)) { Found = A; break; } }
            if (!Found) { TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>(); Err->SetStringField(TEXT("error"), TEXT("Actor not found")); SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), Err, TEXT("ACTOR_NOT_FOUND")); return; }
            bool bDeleted = ActorSS->DestroyActor(Found); TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>(); Out->SetBoolField(TEXT("success"), bDeleted); if (bDeleted) { Out->SetStringField(TEXT("deleted"), Found->GetActorLabel()); SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor deleted"), Out, FString()); } else { Out->SetStringField(TEXT("error"), TEXT("Delete failed")); SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Delete failed"), Out, TEXT("DELETE_FAILED")); }
        });
        return true;
    }

    if (FN == TEXT("ASSET_EXISTS") || FN == TEXT("ASSET_EXISTS_SIMPLE"))
    {
        FString PathToCheck;
        // Accept either top-level 'path' or nested params.path
        if (!Payload->TryGetStringField(TEXT("path"), PathToCheck))
        {
            const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
            if (Payload->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr && (*ParamsPtr).IsValid())
            {
                (*ParamsPtr)->TryGetStringField(TEXT("path"), PathToCheck);
            }
        }
        if (PathToCheck.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("path required"), TEXT("INVALID_ARGUMENT")); return true; }

        // Perform check on game thread
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, PathToCheck, RequestingSocket]() {
            TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
            bool bExists = false;
            if (UEditorAssetLibrary::DoesAssetExist(PathToCheck)) bExists = true;
            Out->SetBoolField(TEXT("exists"), bExists);
            Out->SetStringField(TEXT("path"), PathToCheck);
            Out->SetBoolField(TEXT("success"), true);
            SendAutomationResponse(RequestingSocket, RequestId, true, bExists ? TEXT("Asset exists") : TEXT("Asset not found"), Out, bExists ? FString() : TEXT("NOT_FOUND"));
        });
        return true;
    }

    if (FN == TEXT("SET_VIEWPORT_CAMERA") || FN == TEXT("SET_VIEWPORT_CAMERA_INFO"))
    {
        const TSharedPtr<FJsonObject>* Params = nullptr; FVector Loc(0,0,0); FRotator Rot(0,0,0);
        if (Payload->TryGetObjectField(TEXT("params"), Params) && Params && (*Params).IsValid()) { ReadVectorField(*Params, TEXT("location"), Loc, Loc); ReadRotatorField(*Params, TEXT("rotation"), Rot, Rot); }
        else { ReadVectorField(Payload, TEXT("location"), Loc, Loc); ReadRotatorField(Payload, TEXT("rotation"), Rot, Rot); }
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, Loc, Rot, RequestingSocket]() {
            if (!GEditor) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE")); return; }
            if (UUnrealEditorSubsystem* UES = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>()) { UES->SetLevelViewportCameraInfo(Loc, Rot); if (ULevelEditorSubsystem* LES = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()) LES->EditorInvalidateViewports(); TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>(); R->SetBoolField(TEXT("success"), true); SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Camera set"), R, FString()); return; }
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("UnrealEditorSubsystem not available"), nullptr, TEXT("NOT_IMPLEMENTED"));
        });
        return true;
    }

    if (FN == TEXT("BUILD_LIGHTING"))
    {
        FString Quality; Payload->TryGetStringField(TEXT("quality"), Quality);
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, Quality, RequestingSocket]() {
            if (!GEditor) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE")); return; }
            if (ULevelEditorSubsystem* LES = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()) { // Map quality strings
                enum class ELightQuality { Preview, Medium, High, Production };
                ELightQuality Q = ELightQuality::Preview; FString QLower = Quality.ToLower(); if (QLower == TEXT("medium")) Q = ELightQuality::Medium; else if (QLower == TEXT("high")) Q = ELightQuality::High; else if (QLower == TEXT("production")) Q = ELightQuality::Production;
                // LevelEditorSubsystem exposes BuildLightMaps with a LightingBuildQuality parameter in later engine versions
                ELightingBuildQuality QualityEnum = ELightingBuildQuality::Quality_Production;
                if (!Quality.IsEmpty())
                {
                    const FString LowerQuality = Quality.ToLower();
                    if (LowerQuality == TEXT("preview")) { QualityEnum = ELightingBuildQuality::Quality_Preview; }
                    else if (LowerQuality == TEXT("medium")) { QualityEnum = ELightingBuildQuality::Quality_Medium; }
                    else if (LowerQuality == TEXT("high")) { QualityEnum = ELightingBuildQuality::Quality_High; }
                }
                LES->BuildLightMaps(QualityEnum, /*bWithReflectionCaptures*/false);
                TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>(); R->SetBoolField(TEXT("requested"), true); SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Build lighting requested"), R, FString()); return; }
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("LevelEditorSubsystem not available"), nullptr, TEXT("NOT_IMPLEMENTED"));
        });
        return true;
    }

    // RESOLVE_OBJECT: return basic object/asset discovery info
    if (FN == TEXT("RESOLVE_OBJECT"))
    {
        FString Path; Payload->TryGetStringField(TEXT("path"), Path);
        if (Path.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("path required"), TEXT("INVALID_ARGUMENT")); return true; }
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, Path, RequestingSocket]() {
            TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
            bool bExists = false; FString ClassName;
            if (UEditorAssetLibrary::DoesAssetExist(Path))
            {
                bExists = true;
                UObject* Obj = UEditorAssetLibrary::LoadAsset(Path);
                if (Obj && Obj->GetClass()) ClassName = Obj->GetClass()->GetPathName();
            }
            else
            {
                UObject* Obj = FindObject<UObject>(nullptr, *Path);
                if (Obj) { bExists = true; if (Obj->GetClass()) ClassName = Obj->GetClass()->GetPathName(); }
            }
            Out->SetBoolField(TEXT("exists"), bExists);
            Out->SetStringField(TEXT("path"), Path);
            Out->SetStringField(TEXT("class"), ClassName);
            Out->SetBoolField(TEXT("success"), true);
            SendAutomationResponse(RequestingSocket, RequestId, true, bExists ? TEXT("Object resolved") : TEXT("Object not found"), Out, bExists ? FString() : TEXT("NOT_FOUND"));
        });
        return true;
    }

    // LIST_ACTOR_COMPONENTS: provide a simple listing of components for a given editor actor
    if (FN == TEXT("LIST_ACTOR_COMPONENTS"))
    {
        FString ActorPath; Payload->TryGetStringField(TEXT("actorPath"), ActorPath);
        if (ActorPath.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("actorPath required"), TEXT("INVALID_ARGUMENT")); return true; }
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, ActorPath, RequestingSocket]() {
            if (!GEditor) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE")); return; }
            UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>(); if (!ActorSS) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("EditorActorSubsystem not available"), nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING")); return; }
            AActor* Found = nullptr;
            for (AActor* A : ActorSS->GetAllLevelActors()) { if (!A) continue; if (A->GetActorLabel().Equals(ActorPath, ESearchCase::IgnoreCase) || A->GetName().Equals(ActorPath, ESearchCase::IgnoreCase) || A->GetPathName().Equals(ActorPath, ESearchCase::IgnoreCase)) { Found = A; break; } }
            if (!Found) { TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>(); Err->SetStringField(TEXT("error"), TEXT("Actor not found")); SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Actor not found"), Err, TEXT("ACTOR_NOT_FOUND")); return; }
            TArray<UActorComponent*> Comps; Found->GetComponents(Comps);
            TArray<TSharedPtr<FJsonValue>> Arr; for (UActorComponent* C : Comps) { if (!C) continue; TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>(); R->SetStringField(TEXT("name"), C->GetName()); R->SetStringField(TEXT("class"), C->GetClass() ? C->GetClass()->GetPathName() : TEXT("")); R->SetStringField(TEXT("path"), C->GetPathName()); Arr.Add(MakeShared<FJsonValueObject>(R)); }
            TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>(); Out->SetArrayField(TEXT("components"), Arr); Out->SetNumberField(TEXT("count"), Arr.Num()); SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Components listed"), Out, FString());
        });
        return true;
    }

    // GET_BLUEPRINT_CDO: best-effort CDO/class info for a Blueprint asset
    if (FN == TEXT("GET_BLUEPRINT_CDO"))
    {
        FString BlueprintPath; Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
        if (BlueprintPath.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("blueprintPath required"), TEXT("INVALID_ARGUMENT")); return true; }
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, BlueprintPath, RequestingSocket]() {
            TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
            UObject* Obj = UEditorAssetLibrary::LoadAsset(BlueprintPath);
            if (!Obj) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint not found"), nullptr, TEXT("NOT_FOUND")); return; }
            // Try to detect UBlueprint or UClass
            UBlueprint* BP = Cast<UBlueprint>(Obj);
            if (BP && BP->GeneratedClass)
            {
                UClass* Gen = BP->GeneratedClass;
                Out->SetStringField(TEXT("blueprintPath"), BlueprintPath);
                Out->SetStringField(TEXT("classPath"), Gen->GetPathName());
                Out->SetStringField(TEXT("className"), Gen->GetName());
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint CDO info"), Out, FString());
                return;
            }
            // Not a UBlueprint instance; try if the asset is a UClass itself
            if (UClass* C = Cast<UClass>(Obj)) { Out->SetStringField(TEXT("classPath"), C->GetPathName()); Out->SetStringField(TEXT("className"), C->GetName()); SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Class info"), Out, FString()); return; }
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint/GeneratedClass not available"), nullptr, TEXT("NOT_IMPLEMENTED"));
        });
        return true;
    }

    // SET_BLUEPRINT_DEFAULT: delegate to blueprint_set_default handler when possible
    if (FN == TEXT("SET_BLUEPRINT_DEFAULT"))
    {
        // Support either a JSON string 'payload' or nested params object
        FString PayloadJson; if (!Payload->TryGetStringField(TEXT("payload"), PayloadJson))
        {
            const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
            if (Payload->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr && (*ParamsPtr).IsValid())
            {
                // Forward directly to blueprint_set_default implementation
                return HandleBlueprintAction(RequestId, TEXT("blueprint_set_default"), *ParamsPtr, RequestingSocket);
            }
            // Try top-level object with fields
            return HandleBlueprintAction(RequestId, TEXT("blueprint_set_default"), Payload, RequestingSocket);
        }
        // If we have a JSON string, attempt to parse
        TSharedPtr<FJsonObject> Parsed = MakeShared<FJsonObject>(); TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadJson);
        if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid JSON payload"), TEXT("INVALID_ARGUMENT")); return true; }
        return HandleBlueprintAction(RequestId, TEXT("blueprint_set_default"), Parsed, RequestingSocket);
    }

    if (FN == TEXT("SAVE_DIRTY_PACKAGES") || FN == TEXT("SAVE_ALL_DIRTY_PACKAGES"))
    {
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, RequestingSocket]() {
            if (!GEditor) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE")); return; }
            bool bOk = false;
#if WITH_EDITOR && __has_include("EditorLoadingAndSavingUtils.h")
            bOk = UEditorLoadingAndSavingUtils::SaveDirtyPackages(true, true, true);
#endif
            TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>(); R->SetBoolField(TEXT("success"), bOk); SendAutomationResponse(RequestingSocket, RequestId, bOk, bOk ? TEXT("Save requested") : TEXT("Save failed"), R, bOk ? FString() : TEXT("SAVE_FAILED"));
        });
        return true;
    }

    // SAVE_ASSET: save an asset by path
    if (FN == TEXT("SAVE_ASSET"))
    {
        FString AssetPath; Payload->TryGetStringField(TEXT("path"), AssetPath);
        if (AssetPath.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("path required"), TEXT("INVALID_ARGUMENT")); return true; }
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, AssetPath, RequestingSocket]() {
            bool bOk = false;
#if WITH_EDITOR
            bOk = UEditorAssetLibrary::SaveLoadedAsset(UEditorAssetLibrary::LoadAsset(AssetPath));
#endif
            TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>(); Out->SetStringField(TEXT("path"), AssetPath); Out->SetBoolField(TEXT("success"), bOk);
            SendAutomationResponse(RequestingSocket, RequestId, bOk, bOk ? TEXT("Asset saved") : TEXT("Save failed"), Out, bOk ? FString() : TEXT("SAVE_FAILED"));
        });
        return true;
    }

    // DELETE_ASSET: delete assets via script-supplied python snippet or path list
    if (FN == TEXT("DELETE_ASSET"))
    {
        const TSharedPtr<FJsonObject>* Params = nullptr;
        if (Payload->TryGetObjectField(TEXT("params"), Params) && Params && (*Params).IsValid())
        {
            // If params contains 'paths' array, delegate to HandleAssetAction
            const TArray<TSharedPtr<FJsonValue>>* PathsArr = nullptr; if ((*Params)->TryGetArrayField(TEXT("paths"), PathsArr) && PathsArr && PathsArr->Num() > 0)
            {
                return HandleAssetAction(RequestId, TEXT("delete_assets"), *Params, RequestingSocket);
            }
        }
        // Fallback: no native handling available for arbitrary script; indicate unimplemented
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("DELETE_ASSET not implemented natively; provide explicit paths or allow Python fallback"), nullptr, TEXT("UNKNOWN_PLUGIN_ACTION"));
        return true;
    }

    // CREATE_ASSET: generic creation helper using factory_class/asset_class hints
    if (FN == TEXT("CREATE_ASSET"))
    {
        const TSharedPtr<FJsonObject>* Params = nullptr; if (!Payload->TryGetObjectField(TEXT("params"), Params) || !(*Params).IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("params object required"), TEXT("INVALID_ARGUMENT")); return true; }
        FString FactoryClassName; (*Params)->TryGetStringField(TEXT("factory_class"), FactoryClassName);
        FString AssetClassName; (*Params)->TryGetStringField(TEXT("asset_class"), AssetClassName);
        FString AssetName; (*Params)->TryGetStringField(TEXT("asset_name"), AssetName);
        FString PackagePath; (*Params)->TryGetStringField(TEXT("package_path"), PackagePath);
        if (AssetName.IsEmpty() || PackagePath.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("asset_name and package_path required"), TEXT("INVALID_ARGUMENT")); return true; }
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, FactoryClassName, AssetClassName, AssetName, PackagePath, RequestingSocket]() {
            // Try to locate a factory class by name and instantiate
            UFactory* FactoryInstance = nullptr;
#if WITH_EDITOR
            UClass* FactoryClass = nullptr;
            if (!FactoryClassName.IsEmpty())
            {
                FactoryClass = ResolveClassByName(FactoryClassName);
                if (!FactoryClass)
                {
                    // Try common script prefix
                    FString Guess = FString::Printf(TEXT("/Script/Engine.%s"), *FactoryClassName);
                    FactoryClass = StaticLoadClass(UFactory::StaticClass(), nullptr, *Guess);
                }
            }
            if (FactoryClass && FactoryClass->IsChildOf(UFactory::StaticClass()))
            {
                FactoryInstance = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
            }
            // Try to resolve asset class
            UClass* AssetClass = nullptr;
            if (!AssetClassName.IsEmpty())
            {
                // If asset_class looks like 'unreal.Material', try to map to '/Script/Engine.Material' style
                if (AssetClassName.StartsWith(TEXT("unreal.")))
                {
                    FString Short = AssetClassName.RightChop(7);
                    FString Guess = FString::Printf(TEXT("/Script/Engine.%s"), *Short);
                    AssetClass = ResolveClassByName(Short);
                    if (!AssetClass) AssetClass = StaticLoadClass(UObject::StaticClass(), nullptr, *Guess);
                }
                else
                {
                    AssetClass = ResolveClassByName(AssetClassName);
                    if (!AssetClass) AssetClass = StaticLoadClass(UObject::StaticClass(), nullptr, *AssetClassName);
                }
            }
            UObject* Created = nullptr;
            if (UClass* FinalClass = AssetClass)
            {
                FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
                Created = AssetTools.Get().CreateAsset(AssetName, PackagePath, FinalClass, FactoryInstance);
            }
            if (!Created)
            {
                TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>(); Err->SetStringField(TEXT("error"), TEXT("Creation failed or unsupported asset type")); SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Create asset failed"), Err, TEXT("CREATE_FAILED")); return; }
            TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>(); Out->SetStringField(TEXT("path"), Created->GetPathName()); Out->SetStringField(TEXT("class"), Created->GetClass() ? Created->GetClass()->GetPathName() : TEXT("")); Out->SetBoolField(TEXT("success"), true); SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Asset created"), Out, FString());
#else
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Create asset requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
#endif
        });
        return true;
    }

    // ADD_COMPONENT_TO_BLUEPRINT: helper that maps to blueprint_modify_scs 'add_component' operation
    if (FN == TEXT("ADD_COMPONENT_TO_BLUEPRINT") || FN == TEXT("ADD_COMPONENT"))
    {
        // Expect either params or payloadBase64 with JSON
        const TSharedPtr<FJsonObject>* Params = nullptr; TSharedPtr<FJsonObject> LocalParams = MakeShared<FJsonObject>();
        if (Payload->TryGetObjectField(TEXT("params"), Params) && Params && (*Params).IsValid()) LocalParams = *Params;
        else if (Payload->HasField(TEXT("payloadBase64")))
        {
            FString Enc; Payload->TryGetStringField(TEXT("payloadBase64"), Enc);
            if (!Enc.IsEmpty())
            {
                TArray<uint8> DecodedBytes;
                if (FBase64::Decode(Enc, DecodedBytes) && DecodedBytes.Num() > 0)
                {
                    // Ensure null-termination for UTF8 conversion
                    DecodedBytes.Add(0);
                    const ANSICHAR* Utf8 = reinterpret_cast<const ANSICHAR*>(DecodedBytes.GetData());
                    FString Decoded = FString(UTF8_TO_TCHAR(Utf8));
                    TSharedPtr<FJsonObject> Parsed = MakeShared<FJsonObject>(); TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Decoded);
                    if (FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid()) LocalParams = Parsed;
                }
            }
        }
        // Build blueprint_modify_scs payload
        TSharedPtr<FJsonObject> SCSPayload = MakeShared<FJsonObject>();
        FString TargetBP; LocalParams->TryGetStringField(TEXT("blueprintPath"), TargetBP);
        if (TargetBP.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("blueprintPath required"), TEXT("INVALID_ARGUMENT")); return true; }
        SCSPayload->SetStringField(TEXT("blueprintPath"), TargetBP);
        // Build operations array
        TArray<TSharedPtr<FJsonValue>> Ops;
        TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>(); Op->SetStringField(TEXT("type"), TEXT("add_component"));
        FString Name; LocalParams->TryGetStringField(TEXT("componentName"), Name); if (!Name.IsEmpty()) Op->SetStringField(TEXT("componentName"), Name);
        FString Class; LocalParams->TryGetStringField(TEXT("componentClass"), Class); if (!Class.IsEmpty()) Op->SetStringField(TEXT("componentClass"), Class);
        FString AttachTo; LocalParams->TryGetStringField(TEXT("attachTo"), AttachTo); if (!AttachTo.IsEmpty()) Op->SetStringField(TEXT("attachTo"), AttachTo);
        Ops.Add(MakeShared<FJsonValueObject>(Op)); SCSPayload->SetArrayField(TEXT("operations"), Ops);
        // Delegate to blueprint_modify_scs
        return HandleBlueprintAction(RequestId, TEXT("blueprint_modify_scs"), SCSPayload, RequestingSocket);
    }

    // PLAY_SOUND helpers
    if (FN == TEXT("PLAY_SOUND_AT_LOCATION") || FN == TEXT("PLAY_SOUND_2D"))
    {
        const TSharedPtr<FJsonObject>* Params = nullptr; if (!Payload->TryGetObjectField(TEXT("params"), Params) || !(*Params).IsValid()) { /* allow top-level path fields */ }
        FString SoundPath; if (!Payload->TryGetStringField(TEXT("path"), SoundPath)) Payload->TryGetStringField(TEXT("soundPath"), SoundPath);
        if (SoundPath.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("soundPath or path required"), TEXT("INVALID_ARGUMENT")); return true; }
        if (FN == TEXT("PLAY_SOUND_AT_LOCATION"))
        {
            float x=0,y=0,z=0; const TSharedPtr<FJsonObject>* LocObj = nullptr; if (Payload->TryGetObjectField(TEXT("params"), LocObj) && LocObj && (*LocObj).IsValid()) { (*LocObj)->TryGetNumberField(TEXT("x"), x); (*LocObj)->TryGetNumberField(TEXT("y"), y); (*LocObj)->TryGetNumberField(TEXT("z"), z); }
            AsyncTask(ENamedThreads::GameThread, [this, RequestId, SoundPath, x, y, z, RequestingSocket]() {
                UWorld* World = nullptr; if (GEditor) { if (UUnrealEditorSubsystem* UES = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>()) { World = UES->GetEditorWorld(); } }
                if (!World) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor world not available"), nullptr, TEXT("NOT_IMPLEMENTED")); return; }
                USoundBase* Snd = Cast<USoundBase>(UEditorAssetLibrary::LoadAsset(SoundPath)); if (!Snd) { TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>(); Err->SetStringField(TEXT("error"), TEXT("Sound asset not found")); SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Sound not found"), Err, TEXT("NOT_FOUND")); return; }
                FVector Loc(x,y,z); UGameplayStatics::SpawnSoundAtLocation(World, Snd, Loc);
                TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>(); Out->SetBoolField(TEXT("success"), true); SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sound played"), Out, FString());
            });
            return true;
        }
        else
        {
            AsyncTask(ENamedThreads::GameThread, [this, RequestId, SoundPath, RequestingSocket]() {
                // 2D playback: spawn at default location in editor world
                UWorld* World = nullptr; if (GEditor) { if (UUnrealEditorSubsystem* UES = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>()) { World = UES->GetEditorWorld(); } }
                if (!World) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor world not available"), nullptr, TEXT("NOT_IMPLEMENTED")); return; }
                USoundBase* Snd = Cast<USoundBase>(UEditorAssetLibrary::LoadAsset(SoundPath)); if (!Snd) { TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>(); Err->SetStringField(TEXT("error"), TEXT("Sound asset not found")); SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Sound not found"), Err, TEXT("NOT_FOUND")); return; }
                FVector Loc(0,0,0); UGameplayStatics::SpawnSoundAtLocation(World, Snd, Loc);
                TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>(); Out->SetBoolField(TEXT("success"), true); SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sound played"), Out, FString());
            });
            return true;
        }
    }

    // ADD_WIDGET_TO_VIEWPORT: best-effort; not always supported in editor context
    if (FN == TEXT("ADD_WIDGET_TO_VIEWPORT"))
    {
        FString WidgetPath; Payload->TryGetStringField(TEXT("widget_path"), WidgetPath);
        int32 z = 0; Payload->TryGetNumberField(TEXT("z_order"), z);
        int32 playerIndex = 0; Payload->TryGetNumberField(TEXT("player_index"), playerIndex);
        // Editor-time widget addition is not supported reliably; return NOT_IMPLEMENTED so server may fallback
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Add widget to viewport not implemented natively in editor context"), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
    }

    // RC_* pass-through: indicate not implemented natively so server may fallback to Python
    if (FN.StartsWith(TEXT("RC_")))
    {
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Remote Control functions are not implemented natively in plugin; allow Python fallback or implement RC handlers"), nullptr, TEXT("UNKNOWN_PLUGIN_ACTION"));
        return true;
    }

    // Map several BLUEPRINT_* editor-function fallbacks to the blueprint_* action handlers
    if (FN == TEXT("CREATE_BLUEPRINT") || FN == TEXT("BLUEPRINT_CREATE"))
    {
        // Expect either 'payload' containing JSON string or nested params
        FString JsonStr; if (Payload->TryGetStringField(TEXT("payload"), JsonStr) && !JsonStr.IsEmpty())
        {
            TSharedPtr<FJsonObject> Parsed; TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
            if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid JSON payload"), TEXT("INVALID_ARGUMENT")); return true; }
            return HandleBlueprintAction(RequestId, TEXT("blueprint_create"), Parsed, RequestingSocket);
        }
        // Try nested params object
        const TSharedPtr<FJsonObject>* ParamsObj = nullptr; if (Payload->TryGetObjectField(TEXT("params"), ParamsObj) && ParamsObj && (*ParamsObj).IsValid()) { return HandleBlueprintAction(RequestId, TEXT("blueprint_create"), *ParamsObj, RequestingSocket); }
        // Fallback: try forwarding full payload
        return HandleBlueprintAction(RequestId, TEXT("blueprint_create"), Payload, RequestingSocket);
    }

    if (FN == TEXT("BLUEPRINT_ADD_VARIABLE") || FN == TEXT("BLUEPRINT_ADD_VAR"))
    {
        // Accept either 'payload' JSON string or params
        FString JsonStr; if (Payload->TryGetStringField(TEXT("payload"), JsonStr) && !JsonStr.IsEmpty()) { TSharedPtr<FJsonObject> Parsed; TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr); if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid JSON payload"), TEXT("INVALID_ARGUMENT")); return true; } return HandleBlueprintAction(RequestId, TEXT("blueprint_add_variable"), Parsed, RequestingSocket); }
        const TSharedPtr<FJsonObject>* ParamsObj = nullptr; if (Payload->TryGetObjectField(TEXT("params"), ParamsObj) && ParamsObj && (*ParamsObj).IsValid()) { return HandleBlueprintAction(RequestId, TEXT("blueprint_add_variable"), *ParamsObj, RequestingSocket); }
        return HandleBlueprintAction(RequestId, TEXT("blueprint_add_variable"), Payload, RequestingSocket);
    }

    if (FN == TEXT("BLUEPRINT_SET_VARIABLE_METADATA") || FN == TEXT("BLUEPRINT_SET_VAR_METADATA"))
    {
        FString JsonStr; if (Payload->TryGetStringField(TEXT("payload"), JsonStr) && !JsonStr.IsEmpty()) { TSharedPtr<FJsonObject> Parsed; TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr); if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid JSON payload"), TEXT("INVALID_ARGUMENT")); return true; } return HandleBlueprintAction(RequestId, TEXT("blueprint_set_variable_metadata"), Parsed, RequestingSocket); }
        const TSharedPtr<FJsonObject>* ParamsObj = nullptr; if (Payload->TryGetObjectField(TEXT("params"), ParamsObj) && ParamsObj && (*ParamsObj).IsValid()) { return HandleBlueprintAction(RequestId, TEXT("blueprint_set_variable_metadata"), *ParamsObj, RequestingSocket); }
        return HandleBlueprintAction(RequestId, TEXT("blueprint_set_variable_metadata"), Payload, RequestingSocket);
    }

    if (FN == TEXT("BLUEPRINT_ADD_CONSTRUCTION_SCRIPT") || FN == TEXT("BLUEPRINT_ADD_CONSTRUCTION"))
    {
        FString JsonStr; if (Payload->TryGetStringField(TEXT("payload"), JsonStr) && !JsonStr.IsEmpty()) { TSharedPtr<FJsonObject> Parsed; TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr); if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid JSON payload"), TEXT("INVALID_ARGUMENT")); return true; } return HandleBlueprintAction(RequestId, TEXT("blueprint_add_construction_script"), Parsed, RequestingSocket); }
        const TSharedPtr<FJsonObject>* ParamsObj = nullptr; if (Payload->TryGetObjectField(TEXT("params"), ParamsObj) && ParamsObj && (*ParamsObj).IsValid()) { return HandleBlueprintAction(RequestId, TEXT("blueprint_add_construction_script"), *ParamsObj, RequestingSocket); }
        return HandleBlueprintAction(RequestId, TEXT("blueprint_add_construction_script"), Payload, RequestingSocket);
    }

    // CREATE_SOUND_CUE: create a SoundCue asset when factory is available
    if (FN == TEXT("CREATE_SOUND_CUE"))
    {
        const TSharedPtr<FJsonObject>* ParamsPtr = nullptr; TSharedPtr<FJsonObject> Params = Payload;
        if (Payload->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr && (*ParamsPtr).IsValid()) Params = *ParamsPtr;
        FString Name; Params->TryGetStringField(TEXT("name"), Name);
        FString Package; Params->TryGetStringField(TEXT("package_path"), Package);
        if (Name.IsEmpty() || Package.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("name and package_path required"), TEXT("INVALID_ARGUMENT")); return true; }
    AsyncTask(ENamedThreads::GameThread, [this, RequestId, Name, Package, RequestingSocket]() {
#if WITH_EDITOR
        UFactory* FactoryInstance = nullptr;
    UClass* FactoryClass = ResolveClassByName(TEXT("SoundCueFactoryNew"));
        if (FactoryClass && FactoryClass->IsChildOf(UFactory::StaticClass()))
        {
        FactoryInstance = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
        }
        FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
        UObject* Created = AssetTools.Get().CreateAsset(Name, Package, USoundCue::StaticClass(), FactoryInstance);
        if (!Created) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to create SoundCue"), nullptr, TEXT("CREATE_FAILED")); return; }
        UEditorAssetLibrary::SaveLoadedAsset(Created);
        TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>(); Out->SetStringField(TEXT("path"), Created->GetPathName()); Out->SetBoolField(TEXT("success"), true); SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SoundCue created"), Out, FString());
#else
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Create sound cue requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
#endif
        });
        return true;
    }

    // Unknown function -> indicate the plugin does not implement it so callers
    // can either fall back to Python (server opt-in) or surface UNKNOWN_PLUGIN_ACTION.
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Unknown editor function or not implemented by plugin"), nullptr, TEXT("UNKNOWN_PLUGIN_ACTION"));
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Editor functions require editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
