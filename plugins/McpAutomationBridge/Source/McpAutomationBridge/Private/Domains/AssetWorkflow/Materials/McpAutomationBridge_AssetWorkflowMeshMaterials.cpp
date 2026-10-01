// Copyright (c) 2024 MCP Automation Bridge Contributors
//
// process_asset process=mesh_materials. A mesh imported with every slot on the grid material shows it in every
// placement, and a component override fixes one placement at a time. This writes the materials into the mesh asset
// itself. Every entry is checked first and the valid ones are applied in ONE rebuild of the mesh; the reply names what
// each slot holds afterwards and each entry it refused, so a typo in one slot never reads as success.

#include "Domains/AssetWorkflow/Materials/McpAutomationBridge_AssetWorkflowMeshMaterials.h"
#include "Domains/AssetWorkflow/Materials/McpAutomationBridge_AssetWorkflowMeshMaterialSlots.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"

#include "PhysicsEngine/BodySetup.h"
#include "SkeletalMeshTypes.h"
#include "StaticMeshCompiler.h"
#include "UObject/UnrealType.h"

namespace McpMeshMaterials
{
namespace
{
// One edit of the mesh, as the details panel makes it: the render data is rebuilt once, however many slots changed.
void ApplyToStaticMesh(UStaticMesh* Mesh, const TArray<FAssignment>& Assignments)
{
    FProperty* Property = UStaticMesh::StaticClass()->FindPropertyByName(UStaticMesh::GetStaticMaterialsName());
    Mesh->Modify();
    Mesh->PreEditChange(Property);
    for (const FAssignment& Assignment : Assignments)
    {
        FStaticMaterial& Slot = Mesh->GetStaticMaterials()[Assignment.Index];
        Slot.MaterialInterface = Assignment.Material;
        if (Slot.MaterialSlotName == NAME_None) Slot.MaterialSlotName = Assignment.Material->GetFName();
    }
    FPropertyChangedEvent Changed(Property);
    Mesh->PostEditChangeProperty(Changed);
    // The build can run on a worker: finish it before the package is saved.
    TArray<UStaticMesh*> Building;
    Building.Add(Mesh);
    FStaticMeshCompilingManager::Get().FinishCompilation(Building);
    if (UBodySetup* BodySetup = Mesh->GetBodySetup()) BodySetup->CreatePhysicsMeshes();
}

void ApplyToSkeletalMesh(USkeletalMesh* Mesh, const TArray<FAssignment>& Assignments)
{
    // Rebuilds the render data and re-registers the components when the scope ends.
    FScopedSkeletalMeshPostEditChange ScopedPostEditChange(Mesh);
    Mesh->Modify();
    for (const FAssignment& Assignment : Assignments)
    {
        Mesh->GetMaterials()[Assignment.Index].MaterialInterface = Assignment.Material;
    }
}
} // namespace

bool HandleSetMeshMaterials(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    auto Fail = [&](const FString& Message, const TCHAR* Code)
    {
        Bridge->SendAutomationResponse(Socket, RequestId, false, Message, nullptr, Code);
        return true;
    };

    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    if (AssetPath.IsEmpty()) return Fail(TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
    const FString SafePath = SanitizeProjectRelativePath(AssetPath);
    if (SafePath.IsEmpty()) return Fail(McpPathRefusalMessage(TEXT("assetPath"), AssetPath), TEXT("SECURITY_VIOLATION"));
    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    if (!Payload->TryGetArrayField(TEXT("materials"), Entries) || Entries == nullptr || Entries->Num() == 0)
    {
        return Fail(TEXT("materials required: [{slot, materialPath}, ...]"), TEXT("INVALID_ARGUMENT"));
    }
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);

    UObject* Asset = McpLoadAsset(SafePath);
    FMeshSlots Slots;
    Slots.Static = Cast<UStaticMesh>(Asset);
    Slots.Skeletal = Cast<USkeletalMesh>(Asset);
    if (!Asset) return Fail(FString::Printf(TEXT("No mesh asset at %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
    if (!Slots.Static && !Slots.Skeletal)
    {
        return Fail(FString::Printf(TEXT("%s is a %s, not a static or skeletal mesh"), *AssetPath, *Asset->GetClass()->GetName()),
                    TEXT("TYPE_MISMATCH"));
    }

    // Check every entry before anything changes.
    TArray<FAssignment> Assignments;
    TArray<TSharedPtr<FJsonValue>> Refused;
    TArray<FString> RefusalLines;
    for (int32 Position = 0; Position < Entries->Num(); ++Position)
    {
        const TSharedPtr<FJsonObject>* EntryPtr = nullptr;
        const TSharedPtr<FJsonValue>& Raw = (*Entries)[Position];
        TSharedPtr<FJsonValue> SlotValue;
        FString MaterialText;
        FString Reason;
        FString Code;
        int32 Index = INDEX_NONE;
        UMaterialInterface* Material = nullptr;
        if (!Raw.IsValid() || !Raw->TryGetObject(EntryPtr) || EntryPtr == nullptr || !EntryPtr->IsValid())
        {
            Reason = TEXT("an entry must be an object {slot, materialPath}");
            Code = TEXT("INVALID_ENTRY");
        }
        else
        {
            SlotValue = (*EntryPtr)->TryGetField(TEXT("slot"));
            (*EntryPtr)->TryGetStringField(TEXT("materialPath"), MaterialText);
            if (ResolveSlot(Slots, SlotValue, Index, Reason, Code)) Material = LoadMaterial(MaterialText, Reason, Code);
        }
        if (Material)
        {
            FAssignment Assignment;
            Assignment.Index = Index;
            Assignment.Material = Material;
            Assignments.Add(Assignment);
            continue;
        }
        TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
        Entry->SetNumberField(TEXT("index"), Position);
        if (SlotValue.IsValid()) Entry->SetField(TEXT("slot"), SlotValue);
        Entry->SetStringField(TEXT("materialPath"), MaterialText);
        Entry->SetStringField(TEXT("code"), Code);
        Entry->SetStringField(TEXT("reason"), Reason);
        Refused.Add(MakeShared<FJsonValueObject>(Entry));
        RefusalLines.Add(FString::Printf(TEXT("entry %d: %s"), Position, *Reason));
    }

    TArray<UMaterialInterface*> Before;
    for (int32 Index = 0; Index < Slots.Num(); ++Index) Before.Add(Slots.Material(Index));
    bool bAnyChange = false;
    for (const FAssignment& Assignment : Assignments)
    {
        bAnyChange = bAnyChange || Before[Assignment.Index] != Assignment.Material;
    }
    if (bAnyChange)
    {
        if (Slots.Static) ApplyToStaticMesh(Slots.Static, Assignments);
        else ApplyToSkeletalMesh(Slots.Skeletal, Assignments);
        Asset->MarkPackageDirty();
    }
    TSet<int32> Changed;
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
    {
        if (Slots.Material(Index) != Before[Index]) Changed.Add(Index);
    }
    // A save request also writes an earlier unsaved change; a clean package is skipped. Engine content is never written.
    FString SaveSkippedReason;
    if (!bSave) SaveSkippedReason = TEXT("save was false");
    else if (Asset->GetOutermost()->GetName().StartsWith(TEXT("/Engine/"))) SaveSkippedReason = TEXT("engine content is not saved");
    const bool bSaved = SaveSkippedReason.IsEmpty() && SaveLoadedAssetThrottled(Asset);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("assetPath"), SafePath);
    Resp->SetStringField(TEXT("assetType"), Slots.Static ? TEXT("StaticMesh") : TEXT("SkeletalMesh"));
    Resp->SetArrayField(TEXT("materialSlots"), Slots.Table(Changed));
    Resp->SetNumberField(TEXT("applied"), Assignments.Num());
    Resp->SetNumberField(TEXT("changed"), Changed.Num());
    Resp->SetArrayField(TEXT("refused"), Refused);
    Resp->SetBoolField(TEXT("saved"), bSaved);
    if (!SaveSkippedReason.IsEmpty()) Resp->SetStringField(TEXT("saveSkippedReason"), SaveSkippedReason);
    McpHandlerUtils::AddVerification(Resp, Asset);

    if (Refused.Num() > 0)
    {
        constexpr int32 MaxQuoted = 3;
        FString Quoted;
        for (int32 Line = 0; Line < RefusalLines.Num() && Line < MaxQuoted; ++Line)
        {
            if (Line > 0) Quoted += TEXT("; ");
            Quoted += RefusalLines[Line];
        }
        if (RefusalLines.Num() > MaxQuoted) Quoted += FString::Printf(TEXT("; %d more (see refused)"), RefusalLines.Num() - MaxQuoted);
        Bridge->SendAutomationResponse(Socket, RequestId, false,
                                       FString::Printf(TEXT("Set %d of %d material slot(s) on %s; refused %s"), Assignments.Num(),
                                                       Entries->Num(), *SafePath, *Quoted),
                                       Resp, Assignments.Num() > 0 ? TEXT("MATERIAL_SLOTS_PARTIAL") : TEXT("MATERIAL_SLOTS_REFUSED"));
        return true;
    }
    if (SaveSkippedReason.IsEmpty() && !bSaved)
    {
        Bridge->SendAutomationResponse(Socket, RequestId, false,
                                       FString::Printf(TEXT("Materials were set on %s but the mesh could not be saved"), *SafePath),
                                       Resp, TEXT("SAVE_FAILED"));
        return true;
    }
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                                   Changed.Num() > 0 ? FString::Printf(TEXT("Set %d material slot(s) on %s"), Changed.Num(), *SafePath)
                                                     : FString::Printf(TEXT("Every requested slot of %s already held its material"), *SafePath),
                                   Resp);
    return true;
}
} // namespace McpMeshMaterials
