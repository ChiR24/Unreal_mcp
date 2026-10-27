#include "Domains/ControlActor/Placement/McpPartPlacement.h"

#include "Foundation/BridgeHelpers/Blueprints/McpBPAssetLoad.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SceneComponent.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

// The replies built from a part audit: the warnings an SCS edit carries, and audit_placement's
// Blueprint mode.
namespace McpPartPlacement
{
namespace
{
constexpr int32 MaxReported = 8;
} // namespace

TSharedPtr<FJsonObject> IssueToJson(const FPartIssue& Issue)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("componentName"), Issue.Component);
    Object->SetStringField(TEXT("kind"), Issue.Kind);
    if (!Issue.Other.IsEmpty())
    {
        Object->SetStringField(TEXT("otherComponent"), Issue.Other);
        Object->SetNumberField(TEXT("insideShare"), FMath::RoundToDouble(Issue.InsideShare * 100.0) / 100.0);
    }
    Object->SetNumberField(TEXT("depth"), FMath::RoundToDouble(Issue.Depth * 10.0) / 10.0);
    Object->SetStringField(TEXT("issue"), Issue.Issue);
    return Object;
}

void NoteInheritedScale(AActor* Actor, const TSet<FString>& Focus, TArray<FString>& OutNotes)
{
    TInlineComponentArray<USceneComponent*> Components(Actor);
    for (const USceneComponent* Component : Components)
    {
        const USceneComponent* Parent = Component ? Component->GetAttachParent() : nullptr;
        if (!Parent || !Focus.Contains(Component->GetName()) || Component->IsUsingAbsoluteScale())
        {
            continue;
        }
        // A trigger box of extent 70x70x40 under a base flattened to (1.6, 1.6, 0.1) was 112x112x4 in the world, and
        // its 40 cm offset 4 cm: the reply only said the plate beside it was buried.
        const FVector Inherited = Parent->GetComponentScale();
        if (Inherited.GetAbsMax() <= Inherited.GetAbsMin() * 1.05)
        {
            continue;
        }
        OutNotes.Add(FString::Printf(
            TEXT("'%s' is attached to '%s', which is scaled unevenly (%g, %g, %g), so it takes that scale too: its "
                 "size and offsets are stretched the same way in the world. Attach it to an unscaled part "
                 "(DefaultSceneRoot) to keep its own size."),
            *Component->GetName(), *Parent->GetName(), Inherited.X, Inherited.Y, Inherited.Z));
    }
}

int32 AppendPartWarnings(UBlueprint* Blueprint, const TSet<FString>& Focus, const TSharedPtr<FJsonObject>& Result)
{
    if (!Blueprint || !Result.IsValid())
    {
        return 0;
    }
    const FPartAudit Audit = AuditBlueprintParts(Blueprint, Focus);
    if (Audit.Issues.Num() == 0 && Audit.Notes.Num() == 0)
    {
        return 0;
    }
    TArray<TSharedPtr<FJsonValue>> Entries;
    TArray<TSharedPtr<FJsonValue>> Warnings;
    const TArray<TSharedPtr<FJsonValue>>* Existing = nullptr;
    if (Result->TryGetArrayField(TEXT("warnings"), Existing) && Existing)
    {
        Warnings = *Existing;
    }
    for (int32 Index = 0; Index < Audit.Issues.Num() && Index < MaxReported; ++Index)
    {
        Entries.Add(MakeShared<FJsonValueObject>(IssueToJson(Audit.Issues[Index])));
        Warnings.Add(MakeShared<FJsonValueString>(Audit.Issues[Index].Issue));
    }
    for (const FString& Note : Audit.Notes)
    {
        Warnings.Add(MakeShared<FJsonValueString>(Note));
    }
    if (Entries.Num() > 0)
    {
        Result->SetArrayField(TEXT("partWarnings"), Entries);
    }
    Result->SetArrayField(TEXT("warnings"), Warnings);
    return Audit.Issues.Num();
}

int32 AppendPartWarnings(const FString& BlueprintPath, const TSet<FString>& Focus, const TSharedPtr<FJsonObject>& Result)
{
    FString Normalized;
    FString Error;
    return AppendPartWarnings(LoadBlueprintAsset(BlueprintPath, Normalized, Error), Focus, Result);
}

int32 AppendMeshUserWarnings(UStaticMesh* Mesh, const TSharedPtr<FJsonObject>& Result)
{
    if (!Mesh || !Result.IsValid())
    {
        return 0;
    }
    TArray<FName> Referencers;
    FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetReferencers(
        Mesh->GetOutermost()->GetFName(), Referencers);
    Referencers.Sort(FNameLexicalLess());
    TArray<TSharedPtr<FJsonValue>> Entries;
    TArray<TSharedPtr<FJsonValue>> Warnings;
    const TArray<TSharedPtr<FJsonValue>>* Existing = nullptr;
    if (Result->TryGetArrayField(TEXT("warnings"), Existing) && Existing)
    {
        Warnings = *Existing;
    }
    int32 Found = 0;
    int32 Audited = 0;
    // ponytail: loaded Blueprints only (no package is loaded to answer this), at most 8 of them; a Blueprint
    // that was never opened this session is checked by audit_placement blueprintPath.
    for (const FName& Package : Referencers)
    {
        const FString PackageName = Package.ToString();
        UBlueprint* Blueprint = FindObject<UBlueprint>(
            nullptr, *(PackageName + TEXT(".") + FPackageName::GetShortName(PackageName)));
        if (!Blueprint || Audited++ >= 8)
        {
            continue;
        }
        const FPartAudit Audit = AuditBlueprintPartsUsingMesh(Blueprint, Mesh);
        for (const FPartIssue& Issue : Audit.Issues)
        {
            if (Found++ < MaxReported)
            {
                TSharedPtr<FJsonObject> Entry = IssueToJson(Issue);
                Entry->SetStringField(TEXT("blueprintPath"), PackageName);
                Entries.Add(MakeShared<FJsonValueObject>(Entry));
                Warnings.Add(MakeShared<FJsonValueString>(
                    FString::Printf(TEXT("%s: %s"), *FPackageName::GetShortName(PackageName), *Issue.Issue)));
            }
        }
    }
    if (Entries.Num() > 0)
    {
        Result->SetArrayField(TEXT("partWarnings"), Entries);
        Result->SetArrayField(TEXT("warnings"), Warnings);
    }
    return Found;
}

TSharedPtr<FJsonObject> BuildBlueprintAuditReply(const FString& BlueprintPath, double Tolerance, int32 Limit,
                                                 FString& OutError)
{
    FString Normalized;
    UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, Normalized, OutError);
    if (!Blueprint)
    {
        return nullptr;
    }
    const FPartAudit Audit = AuditBlueprintParts(Blueprint, TSet<FString>(), Tolerance);
    if (!Audit.Error.IsEmpty())
    {
        OutError = Audit.Error;
        return nullptr;
    }
    TMap<FString, int32> KindCounts;
    TArray<TSharedPtr<FJsonValue>> Problems;
    for (int32 Index = 0; Index < Audit.Issues.Num(); ++Index)
    {
        KindCounts.FindOrAdd(Audit.Issues[Index].Kind) += 1;
        if (Index < Limit)
        {
            TSharedPtr<FJsonObject> Entry = IssueToJson(Audit.Issues[Index]);
            Entry->SetNumberField(TEXT("severity"), FMath::RoundToDouble(Audit.Issues[Index].Depth * 10.0) / 10.0);
            Problems.Add(MakeShared<FJsonValueObject>(Entry));
        }
    }
    TSharedPtr<FJsonObject> Kinds = MakeShared<FJsonObject>();
    for (const TPair<FString, int32>& Pair : KindCounts)
    {
        Kinds->SetNumberField(Pair.Key, Pair.Value);
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("blueprintPath"), Normalized);
    Data->SetNumberField(TEXT("examined"), Audit.Examined);
    Data->SetNumberField(TEXT("flagged"), Audit.Issues.Num());
    Data->SetNumberField(TEXT("returned"), Problems.Num());
    Data->SetObjectField(TEXT("byKind"), Kinds);
    Data->SetArrayField(TEXT("problems"), Problems);
    if (Audit.bHasGround)
    {
        Data->SetNumberField(TEXT("groundZ"), FMath::RoundToDouble(Audit.GroundZ * 10.0) / 10.0);
    }
    return Data;
}
} // namespace McpPartPlacement
