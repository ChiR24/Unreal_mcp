#include "Domains/LevelStructure/McpLevelStructureEditorWorld.h"

#include "Domains/Level/World/McpLevelWorldAccess.h"
#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Safety/McpSafeOperationsLevelSave.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "Engine/LevelStreamingDynamic.h"
#include "EngineWireLog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Engine/World.h"

namespace LevelStructureHelpers
{

ULevel* ResolveTargetLevelForBlueprintRequest(
    UWorld* World, const TSharedPtr<FJsonObject>& Payload, FString& OutError, bool bAllowTransient)
{
    OutError.Reset();
    const FString Requested = GetJsonStringField(Payload, TEXT("levelPath"));
    // "/Game/Maps/Foo.Foo", "/Game/Maps/Foo.umap" and "/Game/Maps/Foo" name one package.
    const FString NormalizedRequested = FPackageName::ObjectPathToPackageName(Requested.TrimStartAndEnd());

    ULevel* Target = nullptr;
    if (!NormalizedRequested.IsEmpty())
    {
        // Match the requested package against every loaded level (persistent
        // and streaming sublevels) so an explicit levelPath is honoured instead
        // of silently editing whatever level happens to be open.
        for (ULevel* Level : World->GetLevels())
        {
            if (Level && Level->GetOutermost()->GetName().Equals(NormalizedRequested, ESearchCase::IgnoreCase))
            {
                Target = Level;
                break;
            }
        }
        if (!Target)
        {
            OutError = FString::Printf(
                TEXT("Level '%s' is not open in the editor (currently editing '%s'). "
                     "Open it first with manage_level load, or omit levelPath to target the "
                     "level that is open."),
                *Requested, *World->GetOutermost()->GetName());
            return nullptr;
        }
    }
    else
    {
        // No explicit target: the level blueprint belongs to the persistent
        // level, not to whatever sublevel is selected in the Levels panel.
        Target = World->PersistentLevel;
        if (!Target)
        {
            OutError = TEXT("No persistent level available");
            return nullptr;
        }
    }

    // A transient (/Temp/) level cannot host a durable level blueprint: edits
    // report success, then vanish when the throwaway world is replaced. Refuse
    // a MUTATING request instead of writing into a level that will never be
    // saved. Opening or reading the blueprint writes nothing, so the open path
    // passes bAllowTransient and keeps working on an unsaved map.
    if (!bAllowTransient)
    {
        const FString TargetPackage = Target->GetOutermost()->GetName();
        if (TargetPackage.StartsWith(TEXT("/Temp/")))
        {
            OutError = FString::Printf(
                TEXT("The open level is a transient '/Temp/' level ('%s'), which has no durable "
                     "level blueprint. Open a saved level with manage_level load before editing its "
                     "level blueprint."),
                *TargetPackage);
            return nullptr;
        }
    }

    return Target;
}

UClass* ResolveLevelStreamingClass(const FString& StreamingMethod)
{
    if (StreamingMethod.Equals(TEXT("Blueprint"), ESearchCase::IgnoreCase))
    {
        return ULevelStreamingDynamic::StaticClass();
    }
    if (StreamingMethod.Equals(TEXT("AlwaysLoaded"), ESearchCase::IgnoreCase))
    {
        return ULevelStreamingAlwaysLoaded::StaticClass();
    }
    return nullptr;
}

void SendLevelEditResult(
    UEngineWireSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
    const TSharedPtr<FJsonObject>& Payload, ULevel* Level, const FString& Message, TSharedPtr<FJsonObject> Result,
    bool bSaveByDefault)
{
    if (!Result.IsValid())
    {
        Result = McpHandlerUtils::CreateResultObject();
    }
    if (GetJsonBoolField(Payload, TEXT("save"), bSaveByDefault))
    {
        const FString PackageName = Level ? Level->GetOutermost()->GetName() : FString();
        if (PackageName.IsEmpty() || PackageName.StartsWith(TEXT("/Temp/")))
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                TEXT("The change was made, but save was requested on an unsaved level; save the level to a /Game path with manage_level first."),
                Result, TEXT("SAVE_FAILED"));
            return;
        }
        if (!McpSafeLevelSave(Level, PackageName))
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("The change was made, but saving level %s failed; it exists only in this editor session."), *PackageName),
                Result, TEXT("SAVE_FAILED"));
            return;
        }
        Result->SetBoolField(TEXT("saved"), true);
        Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, Result);
        return;
    }
    // Not saving is the default on purpose: saving a level also writes every other unsaved change
    // the user has in it. The reply says the edit is still only in memory instead of implying it stuck.
    Result->SetBoolField(TEXT("saved"), false);
    // The note is a sentence of its own; a message without a closing stop ran straight into it.
    Subsystem->SendAutomationResponse(Socket, RequestId, true, (Message.EndsWith(TEXT(".")) ? Message : Message + TEXT(".")) +
        TEXT(" The level is not saved yet; pass save: true, or save it with manage_level save."), Result);
}

ULevelStreaming* FindOrAddStreamingLevelOrReply(UEngineWireSubsystem* Subsystem, const FString& RequestId,
                                                TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld* World, const FString& LevelName,
                                                bool bAddFromDisk)
{
    // Package name, object path or short name, case-insensitively (create_sublevel reported the
    // level under /Game/.../Sub but the configurators could not find it). A short name two sublevels share
    // configured whichever came first.
    const FString WantedPackage = FPackageName::ObjectPathToPackageName(LevelName);
    const TArray<ULevelStreaming*> Matches = McpLevelHandlers::FindStreamingLevels(World, WantedPackage);
    if (Matches.Num() == 1)
    {
        return Matches[0];
    }
    if (Matches.Num() > 1)
    {
        Subsystem->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("%s names %d streaming levels; pass the "
            "package path of the one to configure."), *LevelName, Matches.Num()), TEXT("AMBIGUOUS_LEVEL"));
        return nullptr;
    }
    // A sublevel that exists on disk but is not streamed by this world yet.
    TArray<FString> Candidates;
    if (LevelName.StartsWith(TEXT("/Game/")))
    {
        Candidates.Add(LevelName);
    }
    Candidates.Add(FPaths::GetPath(World->GetOutermost()->GetName()) / LevelName);
    Candidates.Add(FString(TEXT("/Game/")) / LevelName);
    for (const FString& Candidate : Candidates)
    {
        if (bAddFromDisk && !Candidate.EndsWith(TEXT(".umap")) && FPackageName::DoesPackageExist(Candidate))
        {
            ULevelStreamingDynamic* NewStreamingLevel = NewObject<ULevelStreamingDynamic>(World, ULevelStreamingDynamic::StaticClass());
            NewStreamingLevel->SetWorldAssetByPackageName(FName(*Candidate));
            NewStreamingLevel->LevelTransform = FTransform::Identity;
            NewStreamingLevel->SetShouldBeVisible(true);
            NewStreamingLevel->SetShouldBeLoaded(true);
            World->AddStreamingLevel(NewStreamingLevel);
            UE_LOG(LogEngineWireSubsystem, Log, TEXT("Created streaming reference for existing level: %s"), *Candidate);
            return NewStreamingLevel;
        }
    }
    Subsystem->SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("Streaming level not found: %s"), *LevelName), TEXT("LEVEL_NOT_FOUND"));
    return nullptr;
}

bool ResolveLevelBlueprintGraph(
    UEngineWireSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
    const TSharedPtr<FJsonObject>& Payload, bool bCreate, ULevel*& OutLevel, ULevelScriptBlueprint*& OutBlueprint,
    UEdGraph*& OutGraph)
{
    OutBlueprint = nullptr;
    OutGraph = nullptr;
    UWorld* World = GetEditorWorld();
    FString LevelError = TEXT("No editor world available");
    OutLevel = World ? ResolveTargetLevelForBlueprintRequest(World, Payload, LevelError) : nullptr;
    if (!OutLevel)
    {
        Subsystem->SendAutomationError(Socket, RequestId, LevelError, TEXT("LEVEL_NOT_OPEN"));
        return false;
    }
    const FString LevelName = OutLevel->GetOutermost()->GetName();
    OutBlueprint = OutLevel->GetLevelScriptBlueprint(/*bDontCreate=*/!bCreate);
    if (!OutBlueprint)
    {
        if (bCreate)
        {
            Subsystem->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Failed to create the level Blueprint of %s."), *LevelName), TEXT("CREATE_FAILED"));
        }
        else
        {
            Subsystem->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Level %s has no level Blueprint yet; add a node first (edit_level_blueprint edit add_node)."), *LevelName),
                TEXT("NOT_FOUND"));
        }
        return false;
    }
    OutGraph = FBlueprintEditorUtils::FindEventGraph(OutBlueprint);
    if (!OutGraph)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("The level Blueprint of %s has no event graph."), *LevelName), TEXT("GRAPH_NOT_FOUND"));
        return false;
    }
    return true;
}

bool IsLevelBlueprintNodeNamed(UEdGraphNode* Node, const FString& Wanted)
{
    if (!Node || Wanted.IsEmpty())
    {
        return false;
    }
    // A call's full title is two lines ("Set Light Color\nTarget is Light Component"); its first line is what the
    // node shows, and its list title is the bare function name (SetLightColor).
    const FString FullTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
    FString Header;
    if (!FullTitle.Split(TEXT("\n"), &Header, nullptr))
    {
        Header = FullTitle;
    }
    return Node->GetName().Equals(Wanted, ESearchCase::IgnoreCase) || Node->NodeGuid.ToString().Equals(Wanted, ESearchCase::IgnoreCase) ||
           FullTitle.Equals(Wanted, ESearchCase::IgnoreCase) || Header.Equals(Wanted, ESearchCase::IgnoreCase) ||
           Node->GetNodeTitle(ENodeTitleType::ListView).ToString().Equals(Wanted, ESearchCase::IgnoreCase);
}

UEdGraphNode* FindLevelBlueprintNode(UEdGraph* Graph, const FString& Wanted)
{
    // An actor reference's title is its actor's label, and that actor's bound event carries the label too
    // ("On Actor Begin Overlap (TriggerBox_Center)"), so a part match alone picked whichever came last.
    UEdGraphNode* Partial = nullptr;
    if (!Graph || Wanted.IsEmpty())
    {
        return nullptr;
    }
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (IsLevelBlueprintNodeNamed(Node, Wanted))
        {
            return Node;
        }
        if (Node && (Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Contains(Wanted) || Node->GetName().Contains(Wanted)))
        {
            Partial = Node;
        }
    }
    return Partial;
}

} // namespace LevelStructureHelpers
