#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"

#include "McpAutomationBridgeSubsystem.h"

#include "Core/Module/McpAutomationBridgeGlobals.h"

#include "Editor.h"
#include "Kismet2/KismetEditorUtilities.h"

#if MCP_HAS_CONTROLRIG_FACTORY
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
#include "ControlRigBlueprintLegacy.h"
#else
#include "ControlRigBlueprint.h"
#endif
#include "ControlRigBlueprintGeneratedClass.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#include "ControlRigBlueprintFactory.h"
#endif
#endif

bool UMcpAutomationBridgeSubsystem::ExecuteEditorCommands(
    const TArray<FString>& Commands,
    FString& OutErrorMessage)
{
    check(IsInGameThread());

    if (!GEditor)
    {
        OutErrorMessage = TEXT("Editor not available");
        return false;
    }

    UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
    if (!EditorWorld)
    {
        OutErrorMessage = TEXT("Editor world context not available");
        return false;
    }

    for (const FString& Command : Commands)
    {
        const FString TrimmedCommand = Command.TrimStartAndEnd();
        if (TrimmedCommand.IsEmpty())
        {
            continue;
        }

        if (McpContainsUnsafeCommandSeparator(TrimmedCommand))
        {
            OutErrorMessage = FString::Printf(
                TEXT("Rejected unsafe editor command: %s"),
                *TrimmedCommand);
            UE_LOG(
                LogMcpAutomationBridgeSubsystem,
                Warning,
                TEXT("ExecuteEditorCommands: %s"),
                *OutErrorMessage);
            return false;
        }

        if (!GEditor->Exec(EditorWorld, *TrimmedCommand))
        {
            OutErrorMessage = FString::Printf(
                TEXT("Failed to execute command: %s"),
                *TrimmedCommand);
            UE_LOG(
                LogMcpAutomationBridgeSubsystem,
                Warning,
                TEXT("ExecuteEditorCommands: %s"),
                *OutErrorMessage);
            return false;
        }

        UE_LOG(
            LogMcpAutomationBridgeSubsystem,
            Verbose,
            TEXT("ExecuteEditorCommands: Executed '%s'"),
            *TrimmedCommand);
    }

    return true;
}

#if MCP_HAS_CONTROLRIG_FACTORY
UBlueprint* UMcpAutomationBridgeSubsystem::CreateControlRigBlueprint(
    const FString& AssetName,
    const FString& PackagePath,
    USkeleton* TargetSkeleton,
    FString& OutError)
{
    if (AssetName.IsEmpty())
    {
        OutError = TEXT("Asset name cannot be empty");
        return nullptr;
    }

    if (PackagePath.IsEmpty())
    {
        OutError = TEXT("Package path cannot be empty");
        return nullptr;
    }

    // Call the canonicalizer WHOLE rather than replaying its steps here. It
    // normalizes separators BEFORE mapping the /Content alias; doing it the
    // other way round left "\Content\TeamA\Thing" invisible to the alias map,
    // so this executor rooted it at /Game/Content/... while the pre-queue gate
    // had already resolved it to /Game/TeamA/... and admitted it. Guard and
    // executor now cannot disagree, because they run the same function.
    const FString NormalizedPath = McpCanonicalizeContentPath(PackagePath, /*bAssumeGameRoot=*/true);
    if (NormalizedPath.IsEmpty())
    {
        OutError = FString::Printf(
            TEXT("Package path is not a valid content path: %s"), *PackagePath);
        return nullptr;
    }

    const FString FullPackageName = NormalizedPath / AssetName;
    const FString FullObjectPath = FullPackageName + TEXT(".") + AssetName;
    // An object already at the path, in memory or on disk: reuse a Control Rig, refuse anything else.
    UObject* Existing = FindObject<UObject>(nullptr, *FullObjectPath);
    if (!Existing && FPackageName::DoesPackageExist(FullPackageName))
    {
        Existing = LoadObject<UObject>(nullptr, *FullObjectPath, nullptr, LOAD_NoWarn);
    }
    if (Existing)
    {
        if (Existing->IsA<UControlRigBlueprint>())
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
                TEXT("Control Rig Blueprint already exists, reusing: %s"), *FullObjectPath);
            return Cast<UBlueprint>(Existing);
        }
        OutError = FString::Printf(
            TEXT("Asset exists at path but is not a ControlRigBlueprint (is %s). "
                 "Cannot create ControlRigBlueprint at this path."),
            *Existing->GetClass()->GetName());
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("%s"), *OutError);
        return nullptr;
    }

    UPackage* Package = CreatePackage(*FullPackageName);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPackageName);
        return nullptr;
    }
    Package->FullyLoad();

    UControlRigBlueprint* NewBlueprint = Cast<UControlRigBlueprint>(
        FKismetEditorUtilities::CreateBlueprint(
            UControlRig::StaticClass(),
            Package,
            *AssetName,
            BPTYPE_Normal,
            UControlRigBlueprint::StaticClass(),
            UControlRigBlueprintGeneratedClass::StaticClass(),
            NAME_None));
    if (!NewBlueprint)
    {
        OutError = TEXT("Factory failed to create Control Rig Blueprint");
        return nullptr;
    }

    if (TargetSkeleton)
    {
        USkeletalMesh* PreviewMesh = TargetSkeleton->GetPreviewMesh();
        if (PreviewMesh)
        {
            NewBlueprint->SetPreviewMesh(PreviewMesh);
        }
    }

    FAssetRegistryModule::AssetCreated(NewBlueprint);
    NewBlueprint->MarkPackageDirty();
    McpSafeAssetSave(NewBlueprint);

    UE_LOG(
        LogMcpAutomationBridgeSubsystem,
        Log,
        TEXT("Created Control Rig Blueprint: %s"),
        *FullPackageName);
    return NewBlueprint;
}
#endif
