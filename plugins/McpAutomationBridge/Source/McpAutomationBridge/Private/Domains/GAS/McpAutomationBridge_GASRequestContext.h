#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Safety/McpSafeOperations.h"

class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;

namespace McpGASHandlers
{
struct FGASRequestContext
{
    UMcpAutomationBridgeSubsystem* Subsystem;
    FString RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket;
    TSharedPtr<FJsonObject> Payload;
    FString Name;
    FString Path;
    FString BlueprintPath;
    FString AssetPath;
};

// blueprintPath's Blueprint and its CDO as T; replies NOT_FOUND or INVALID_TYPE ("Not a <TypeLabel>
// blueprint") itself and returns null on either.
template <typename T>
T* LoadGASBlueprintCDO(const FGASRequestContext& Context, UBlueprint*& OutBlueprint, const TCHAR* TypeLabel)
{
    OutBlueprint = LoadObject<UBlueprint>(nullptr, *Context.BlueprintPath);
    if (!OutBlueprint || !OutBlueprint->GeneratedClass)
    {
        Context.Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
            FString::Printf(TEXT("Blueprint not found: %s"), *Context.BlueprintPath), TEXT("NOT_FOUND"));
        return nullptr;
    }
    T* CDO = Cast<T>(OutBlueprint->GeneratedClass->GetDefaultObject());
    if (!CDO)
    {
        Context.Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
            FString::Printf(TEXT("Not a %s blueprint"), TypeLabel), TEXT("INVALID_TYPE"));
    }
    return CDO;
}

// " (the Blueprint failed to compile - ...)" when it did not; empty when it did. Suffix for a refusal.
inline const TCHAR* GASCompileFailureNote(bool bCompiled)
{
    return bCompiled ? TEXT("") : TEXT(" (the Blueprint failed to compile - it may have unrelated graph errors)");
}

// Saves a change already verified on the compiled class; on failure replies SAVE_FAILED ("<What> verified
// on the compiled class but the asset could NOT be written ...") and returns false.
inline bool SaveVerifiedGASBlueprint(const FGASRequestContext& Context, UBlueprint* Blueprint, const TCHAR* What)
{
    if (McpSafeOperations::McpSafeAssetSave(Blueprint))
    {
        return true;
    }
    Context.Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
        FString::Printf(TEXT("%s verified on the compiled class but the asset could NOT be written to disk (file may be read-only or held by source control). The change exists only in this editor session."), What),
        TEXT("SAVE_FAILED"));
    return false;
}

// A CDO or component-template edit is lost on editor restart unless the Blueprint is compiled and saved.
// Replies COMPILE_FAILED or SAVE_FAILED and returns false when either step fails.
inline bool CommitGASBlueprintEdit(const FGASRequestContext& Context, UBlueprint* Blueprint, const TCHAR* What)
{
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    if (!McpSafeCompileBlueprint(Blueprint))
    {
        Context.Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
            FString::Printf(TEXT("%s applied, but the Blueprint failed to compile (it may have unrelated graph errors), so the asset was NOT saved."), What),
            TEXT("COMPILE_FAILED"));
        return false;
    }
    if (McpSafeOperations::McpSafeAssetSave(Blueprint))
    {
        return true;
    }
    Context.Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
        FString::Printf(TEXT("%s applied, but the asset could NOT be written to disk (file may be read-only or held by source control). The change exists only in this editor session."), What),
        TEXT("SAVE_FAILED"));
    return false;
}

// A class a Blueprint recompile left behind (REINST_/SKEL_/TRASHCLASS_ or superseded): it carries the
// real class's property names but the game never loads it.
inline bool IsCompileDebrisClass(const UClass* Class)
{
    const FString ClassName = Class->GetName();
    return Class->HasAnyClassFlags(CLASS_NewerVersionExists) || ClassName.StartsWith(TEXT("REINST_")) ||
           ClassName.StartsWith(TEXT("SKEL_")) || ClassName.StartsWith(TEXT("TRASHCLASS_"));
}
}
