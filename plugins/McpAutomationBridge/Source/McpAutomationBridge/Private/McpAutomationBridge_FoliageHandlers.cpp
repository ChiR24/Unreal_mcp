#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

#if WITH_EDITOR
#include "InstancedFoliageActor.h"
#include "FoliageType.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EditorAssetLibrary.h"
#include "UObject/SavePackage.h"

#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#endif

bool UMcpAutomationBridgeSubsystem::HandlePaintFoliage(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("paint_foliage"), ESearchCase::IgnoreCase)) { return false; }

#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("paint_foliage payload missing"), TEXT("INVALID_PAYLOAD")); return true; }
    
    FString FoliageTypePath;
    if (!Payload->TryGetStringField(TEXT("foliageTypePath"), FoliageTypePath))
    {
        // Accept alternate key used by some clients
        Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
    }
    if (FoliageTypePath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("foliageTypePath (or foliageType) required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Accept single 'position' or array of 'locations'
    TArray<FVector> Locations;
    const TArray<TSharedPtr<FJsonValue>>* LocationsArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("locations"), LocationsArray) && LocationsArray && LocationsArray->Num() > 0)
    {
        for (const TSharedPtr<FJsonValue>& Val : *LocationsArray)
        {
            if (Val.IsValid() && Val->Type == EJson::Object)
            {
                const TSharedPtr<FJsonObject>* Obj = nullptr;
                if (Val->TryGetObject(Obj) && Obj)
                {
                    double X = 0, Y = 0, Z = 0;
                    (*Obj)->TryGetNumberField(TEXT("x"), X);
                    (*Obj)->TryGetNumberField(TEXT("y"), Y);
                    (*Obj)->TryGetNumberField(TEXT("z"), Z);
                    Locations.Add(FVector(X, Y, Z));
                }
            }
        }
    }
    else
    {
        // Try a single 'position' object
        const TSharedPtr<FJsonObject>* PosObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("position"), PosObj) && PosObj)
        {
            double X = 0, Y = 0, Z = 0;
            (*PosObj)->TryGetNumberField(TEXT("x"), X);
            (*PosObj)->TryGetNumberField(TEXT("y"), Y);
            (*PosObj)->TryGetNumberField(TEXT("z"), Z);
            Locations.Add(FVector(X, Y, Z));
        }
    }

    if (Locations.Num() == 0)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("locations array or position required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (!GEditor || !GEditor->GetEditorWorldContext().World())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor world not available"), TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();

    UFoliageType* FoliageType = LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
    if (!FoliageType)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load foliage type"), TEXT("LOAD_FAILED"));
        return true;
    }

    AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(World, true);
    if (!IFA)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to get foliage actor"), TEXT("FOLIAGE_ACTOR_FAILED"));
        return true;
    }

    TArray<FVector> PlacedLocations;
    for (const FVector& Location : Locations)
    {
        FFoliageInstance Instance;
        Instance.Location = Location;
        Instance.Rotation = FRotator::ZeroRotator;
        Instance.DrawScale3D = FVector3f(1.0f);
        Instance.ZOffset = 0.0f;

        if (FFoliageInfo* Info = IFA->FindInfo(FoliageType))
        {
            Info->AddInstance(FoliageType, Instance, /*InBaseComponent*/ nullptr);
        }
        else
        {
            IFA->AddFoliageType(FoliageType);
            if (FFoliageInfo* NewInfo = IFA->FindInfo(FoliageType))
            {
                NewInfo->AddInstance(FoliageType, Instance, /*InBaseComponent*/ nullptr);
            }
        }
        PlacedLocations.Add(Location);
    }

    IFA->Modify();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
    Resp->SetNumberField(TEXT("instancesPlaced"), PlacedLocations.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Foliage painted successfully"), Resp, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("paint_foliage requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleRemoveFoliage(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("remove_foliage"), ESearchCase::IgnoreCase)) { return false; }

#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("remove_foliage payload missing"), TEXT("INVALID_PAYLOAD")); return true; }
    
    FString FoliageTypePath;
    Payload->TryGetStringField(TEXT("foliageTypePath"), FoliageTypePath);

    bool bRemoveAll = false;
    Payload->TryGetBoolField(TEXT("removeAll"), bRemoveAll);

    if (!GEditor || !GEditor->GetEditorWorldContext().World())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor world not available"), TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(World);
    if (!IFA)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No foliage actor found"), TEXT("FOLIAGE_ACTOR_NOT_FOUND"));
        return true;
    }

    int32 RemovedCount = 0;

    if (bRemoveAll)
    {
        IFA->ForEachFoliageInfo([&](UFoliageType* Type, FFoliageInfo& Info)
        {
            RemovedCount += Info.Instances.Num();
            Info.Instances.Empty();
            return true;
        });
        IFA->Modify();
    }
    else if (!FoliageTypePath.IsEmpty())
    {
        UFoliageType* FoliageType = LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
        if (FoliageType)
        {
            FFoliageInfo* Info = IFA->FindInfo(FoliageType);
            if (Info)
            {
                RemovedCount = Info->Instances.Num();
                Info->Instances.Empty();
                IFA->Modify();
            }
        }
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("instancesRemoved"), RemovedCount);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Foliage removed successfully"), Resp, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("remove_foliage requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetFoliageInstances(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("get_foliage_instances"), ESearchCase::IgnoreCase)) { return false; }

#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("get_foliage_instances payload missing"), TEXT("INVALID_PAYLOAD")); return true; }

    FString FoliageTypePath;
    Payload->TryGetStringField(TEXT("foliageTypePath"), FoliageTypePath);

    if (!GEditor || !GEditor->GetEditorWorldContext().World())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor world not available"), TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(World);
    if (!IFA)
    {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetArrayField(TEXT("instances"), TArray<TSharedPtr<FJsonValue>>());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("No foliage actor found"), Resp, FString());
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> InstancesArray;

    if (!FoliageTypePath.IsEmpty())
    {
        UFoliageType* FoliageType = LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
        if (FoliageType)
        {
            FFoliageInfo* Info = IFA->FindInfo(FoliageType);
            if (Info)
            {
                for (const FFoliageInstance& Inst : Info->Instances)
                {
                    TSharedPtr<FJsonObject> InstObj = MakeShared<FJsonObject>();
                    InstObj->SetNumberField(TEXT("x"), Inst.Location.X);
                    InstObj->SetNumberField(TEXT("y"), Inst.Location.Y);
                    InstObj->SetNumberField(TEXT("z"), Inst.Location.Z);
                    InstObj->SetNumberField(TEXT("pitch"), Inst.Rotation.Pitch);
                    InstObj->SetNumberField(TEXT("yaw"), Inst.Rotation.Yaw);
                    InstObj->SetNumberField(TEXT("roll"), Inst.Rotation.Roll);
                    InstancesArray.Add(MakeShared<FJsonValueObject>(InstObj));
                }
            }
        }
    }
    else
    {
        IFA->ForEachFoliageInfo([&](UFoliageType* Type, FFoliageInfo& Info)
        {
            for (const FFoliageInstance& Inst : Info.Instances)
            {
                TSharedPtr<FJsonObject> InstObj = MakeShared<FJsonObject>();
                InstObj->SetStringField(TEXT("foliageType"), Type->GetPathName());
                InstObj->SetNumberField(TEXT("x"), Inst.Location.X);
                InstObj->SetNumberField(TEXT("y"), Inst.Location.Y);
                InstObj->SetNumberField(TEXT("z"), Inst.Location.Z);
                InstancesArray.Add(MakeShared<FJsonValueObject>(InstObj));
            }
            return true;
        });
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetArrayField(TEXT("instances"), InstancesArray);
    Resp->SetNumberField(TEXT("count"), InstancesArray.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Foliage instances retrieved"), Resp, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("get_foliage_instances requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleAddFoliageType(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("add_foliage_type"), ESearchCase::IgnoreCase)) { return false; }

#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("add_foliage_type payload missing"), TEXT("INVALID_PAYLOAD")); return true; }
    
    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("name required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString MeshPath;
    if (!Payload->TryGetStringField(TEXT("meshPath"), MeshPath) || MeshPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("meshPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    double Density = 100.0;
    Payload->TryGetNumberField(TEXT("density"), Density);

    double MinScale = 1.0, MaxScale = 1.0;
    Payload->TryGetNumberField(TEXT("minScale"), MinScale);
    Payload->TryGetNumberField(TEXT("maxScale"), MaxScale);

    bool AlignToNormal = true;
    Payload->TryGetBoolField(TEXT("alignToNormal"), AlignToNormal);

    bool RandomYaw = true;
    Payload->TryGetBoolField(TEXT("randomYaw"), RandomYaw);

    UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
    if (!StaticMesh)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load static mesh"), TEXT("LOAD_FAILED"));
        return true;
    }

    FString PackagePath = TEXT("/Game/Foliage");
    FString AssetName = Name;
    FString FullPackagePath = FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);

    UPackage* Package = CreatePackage(*FullPackagePath);
    if (!Package)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_CREATION_FAILED"));
        return true;
    }

    UFoliageType_InstancedStaticMesh* FoliageType = NewObject<UFoliageType_InstancedStaticMesh>(Package, FName(*AssetName), RF_Public | RF_Standalone);
    if (!FoliageType)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create foliage type"), TEXT("CREATION_FAILED"));
        return true;
    }

    FoliageType->SetStaticMesh(StaticMesh);
    FoliageType->Density = static_cast<float>(Density);
    FoliageType->Scaling = EFoliageScaling::Uniform;
    FoliageType->ScaleX.Min = static_cast<float>(MinScale);
    FoliageType->ScaleX.Max = static_cast<float>(MaxScale);
    FoliageType->ScaleY.Min = static_cast<float>(MinScale);
    FoliageType->ScaleY.Max = static_cast<float>(MaxScale);
    FoliageType->ScaleZ.Min = static_cast<float>(MinScale);
    FoliageType->ScaleZ.Max = static_cast<float>(MaxScale);
    FoliageType->AlignToNormal = AlignToNormal;
    FoliageType->RandomYaw = RandomYaw;
    FoliageType->ReapplyDensity = true;

    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(FoliageType);

    FString PackageFileName = FPackageName::LongPackageNameToFilename(FullPackagePath, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.Error = GError;
    SaveArgs.SaveFlags = SAVE_NoError;
    bool bSaved = UPackage::SavePackage(Package, FoliageType, *PackageFileName, SaveArgs);

    if (!bSaved)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save foliage type asset"), TEXT("SAVE_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("created"), true);
    Resp->SetBoolField(TEXT("exists_after"), true);
    Resp->SetStringField(TEXT("asset_path"), FoliageType->GetPathName());
    Resp->SetStringField(TEXT("used_mesh"), MeshPath);
    Resp->SetStringField(TEXT("method"), TEXT("native_asset_creation"));

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Foliage type created successfully"), Resp, FString());

    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("add_foliage_type requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleAddFoliageInstances(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("add_foliage_instances"), ESearchCase::IgnoreCase)) { return false; }

#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("add_foliage_instances payload missing"), TEXT("INVALID_PAYLOAD")); return true; }

    FString FoliageTypePath;
    if (!Payload->TryGetStringField(TEXT("foliageTypePath"), FoliageTypePath))
    {
        Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
    }
    if (FoliageTypePath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("foliageType or foliageTypePath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Parse transforms -> locations (optional rotation/scale ignored for now)
    TArray<FVector> Locations;
    const TArray<TSharedPtr<FJsonValue>>* Transforms = nullptr;
    if (Payload->TryGetArrayField(TEXT("transforms"), Transforms) && Transforms)
    {
        for (const TSharedPtr<FJsonValue>& V : *Transforms)
        {
            if (!V.IsValid() || V->Type != EJson::Object) continue;
            const TSharedPtr<FJsonObject>* TObj = nullptr; if (!V->TryGetObject(TObj) || !TObj) continue;
            const TSharedPtr<FJsonObject>* LocObj = nullptr;
            if ((*TObj)->TryGetObjectField(TEXT("location"), LocObj) && LocObj)
            {
                double X=0,Y=0,Z=0; (*LocObj)->TryGetNumberField(TEXT("x"), X); (*LocObj)->TryGetNumberField(TEXT("y"), Y); (*LocObj)->TryGetNumberField(TEXT("z"), Z);
                Locations.Add(FVector(X, Y, Z));
            }
            else
            {
                // Accept location as array [x,y,z]
                const TArray<TSharedPtr<FJsonValue>>* LocArr = nullptr;
                if ((*TObj)->TryGetArrayField(TEXT("location"), LocArr) && LocArr && LocArr->Num() >= 3)
                {
                    double X = (*LocArr)[0]->AsNumber();
                    double Y = (*LocArr)[1]->AsNumber();
                    double Z = (*LocArr)[2]->AsNumber();
                    Locations.Add(FVector(X, Y, Z));
                }
            }
        }
    }

    if (Locations.Num() == 0)
    {
        // Fallback to 'locations' if provided
        const TArray<TSharedPtr<FJsonValue>>* LocationsArray = nullptr;
        if (Payload->TryGetArrayField(TEXT("locations"), LocationsArray) && LocationsArray)
        {
            for (const TSharedPtr<FJsonValue>& Val : *LocationsArray)
            {
                if (Val.IsValid() && Val->Type == EJson::Object)
                {
                    const TSharedPtr<FJsonObject>* Obj = nullptr; if (Val->TryGetObject(Obj) && Obj)
                    {
                        double X=0,Y=0,Z=0; (*Obj)->TryGetNumberField(TEXT("x"), X); (*Obj)->TryGetNumberField(TEXT("y"), Y); (*Obj)->TryGetNumberField(TEXT("z"), Z);
                        Locations.Add(FVector(X, Y, Z));
                    }
                }
            }
        }
    }

    if (!GEditor || !GEditor->GetEditorWorldContext().World())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor world not available"), TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    UFoliageType* FoliageType = LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
    if (!FoliageType)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load foliage type"), TEXT("LOAD_FAILED"));
        return true;
    }

    AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(World, true);
    if (!IFA)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to get foliage actor"), TEXT("FOLIAGE_ACTOR_FAILED"));
        return true;
    }

    int32 Added = 0;
    for (const FVector& Location : Locations)
    {
        FFoliageInstance Instance;
        Instance.Location = Location;
        Instance.Rotation = FRotator::ZeroRotator;
        Instance.DrawScale3D = FVector3f(1.0f);

        if (FFoliageInfo* Info = IFA->FindInfo(FoliageType))
        {
            Info->AddInstance(FoliageType, Instance, nullptr);
        }
        else
        {
            IFA->AddFoliageType(FoliageType);
            if (FFoliageInfo* NewInfo = IFA->FindInfo(FoliageType))
            {
                NewInfo->AddInstance(FoliageType, Instance, nullptr);
            }
        }
        ++Added;
    }
    IFA->Modify();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("instances_count"), Added);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Foliage instances added"), Resp, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("add_foliage_instances requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
