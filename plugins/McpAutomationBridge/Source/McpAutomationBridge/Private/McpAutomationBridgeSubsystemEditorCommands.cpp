#if WITH_EDITOR
#include "McpAutomationBridgeHelpers.h"
#endif

#include "McpAutomationBridgeSubsystem.h"

#include "McpAutomationBridgeGlobals.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

#if MCP_HAS_CONTROLRIG_FACTORY
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
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
#if WITH_EDITOR
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
#else
    OutErrorMessage = TEXT("Editor commands only available in editor builds");
    return false;
#endif
}

#if MCP_HAS_CONTROLRIG_FACTORY
UBlueprint* UMcpAutomationBridgeSubsystem::CreateControlRigBlueprint(
    const FString& AssetName,
    const FString& PackagePath,
    USkeleton* TargetSkeleton,
    FString& OutError)
{
#if WITH_EDITOR
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

    FString NormalizedPath = PackagePath;
    NormalizedPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
    NormalizedPath.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (!NormalizedPath.StartsWith(TEXT("/Game")))
    {
        NormalizedPath = TEXT("/Game") / NormalizedPath;
    }
    while (NormalizedPath.EndsWith(TEXT("/")))
    {
        NormalizedPath.LeftChopInline(1);
    }

    const FString FullPackageName = NormalizedPath / AssetName;
    const FString FullObjectPath = FullPackageName + TEXT(".") + AssetName;
    if (UEditorAssetLibrary::DoesAssetExist(FullObjectPath))
    {
        UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(FullObjectPath);
        if (ExistingAsset)
        {
            if (ExistingAsset->IsA<UControlRigBlueprint>())
            {
                UE_LOG(
                    LogMcpAutomationBridgeSubsystem,
                    Log,
                    TEXT("Control Rig Blueprint already exists, reusing: %s"),
                    *FullObjectPath);
                return Cast<UBlueprint>(ExistingAsset);
            }

            OutError = FString::Printf(
                TEXT("Asset exists at path but is not a ControlRigBlueprint (is %s). "
                     "Cannot create ControlRigBlueprint at this path."),
                *ExistingAsset->GetClass()->GetName());
            UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("%s"), *OutError);
            return nullptr;
        }
    }

    UPackage* ExistingPackage = FindPackage(nullptr, *FullPackageName);
    if (ExistingPackage)
    {
        UObject* ExistingObject = FindObject<UObject>(ExistingPackage, *AssetName);
        if (ExistingObject)
        {
            if (ExistingObject->IsA<UControlRigBlueprint>())
            {
                UE_LOG(
                    LogMcpAutomationBridgeSubsystem,
                    Log,
                    TEXT("Control Rig Blueprint already exists in memory, reusing: %s"),
                    *FullObjectPath);
                return Cast<UBlueprint>(ExistingObject);
            }

            OutError = FString::Printf(
                TEXT("In-memory object exists at path but is not a ControlRigBlueprint (is %s). "
                     "Cannot create ControlRigBlueprint at this path."),
                *ExistingObject->GetClass()->GetName());
            UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("%s"), *OutError);
            return nullptr;
        }
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
#else
    OutError = TEXT("Control Rig creation only available in editor builds");
    return nullptr;
#endif
}
#endif
