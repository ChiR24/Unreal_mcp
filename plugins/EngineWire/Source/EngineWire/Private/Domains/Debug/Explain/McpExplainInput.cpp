// explain_input: why pressing a key does nothing. The chain an Enhanced Input press goes through: a mapping context maps
// the action (to the key), something adds that context to the player, a Blueprint handles the action, and during Play
// a pawn is possessed, the game is not paused and no widget holds keyboard focus. Outside Play, "added" is read from
// the game mode's player controller and pawn Blueprints (an Add Mapping Context node); during Play from the player.
#include "Domains/Debug/Explain/McpExplain.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Domains/ControlEditor/McpEditorSupport.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "GameFramework/GameModeBase.h"
#include "Misc/PackageName.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "InputAction.h"
#include "Misc/ConfigCacheIni.h"
#include "InputMappingContext.h"
#include "K2Node_CallFunction.h"

namespace McpExplain
{
namespace
{
constexpr int32 MaxContexts = 200;

// The mapping contexts under /Game that map Action (to Key, when one is given); with no Action, every one.
TArray<UInputMappingContext*> ContextsMapping(const UInputAction* Action, const FKey& Key)
{
    FARFilter Filter;
    Filter.PackagePaths.Add(TEXT("/Game"));
    Filter.bRecursivePaths = true;
#if ENGINE_MINOR_VERSION >= 1
    Filter.ClassPaths.Add(UInputMappingContext::StaticClass()->GetClassPathName());
#else
    Filter.ClassNames.Add(UInputMappingContext::StaticClass()->GetFName());
#endif
    TArray<FAssetData> Assets;
    FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssets(Filter, Assets);
    TArray<UInputMappingContext*> Out;
    for (int32 Index = 0; Index < Assets.Num() && Index < MaxContexts; ++Index)
    {
        UInputMappingContext* Context = Cast<UInputMappingContext>(Assets[Index].GetAsset());
        if (Context && !Action)
        {
            Out.Add(Context);
            continue;
        }
        for (const FEnhancedActionKeyMapping& Mapping : Context ? Context->GetMappings() : TArray<FEnhancedActionKeyMapping>())
        {
            if (Mapping.Action == Action && (!Key.IsValid() || Mapping.Key == Key))
            {
                Out.AddUnique(Context);
            }
        }
    }
    return Out;
}

bool AddsContext(const UBlueprint* Blueprint, const TArray<UInputMappingContext*>& Contexts)
{
    return AnyNode(Blueprint, [&Contexts](const UEdGraphNode* Node) {
        const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
        const UEdGraphPin* Pin = Call && Call->FunctionReference.GetMemberName() == TEXT("AddMappingContext") ? Call->FindPin(TEXT("MappingContext")) : nullptr;
        // A context wired in from a variable cannot be read here: counted as adding one of them.
        return Pin && (Pin->LinkedTo.Num() > 0 || Contexts.Contains(Cast<UInputMappingContext>(Pin->DefaultObject)));
    });
}

// An Add Mapping Context node naming this very context (not one fed from a variable).
bool AddsExactly(const UBlueprint* Blueprint, const UInputMappingContext* Context)
{
    return AnyNode(Blueprint, [Context](const UEdGraphNode* Node) {
        const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
        const UEdGraphPin* Pin = Call && Call->FunctionReference.GetMemberName() == TEXT("AddMappingContext") ? Call->FindPin(TEXT("MappingContext")) : nullptr;
        return Pin && Pin->DefaultObject == Context;
    });
}

bool HandlesAction(const UBlueprint* Blueprint, const UInputAction* Action)
{
    return AnyNode(Blueprint, [Action](const UEdGraphNode* Node) {
        const FObjectProperty* Property = Node->GetClass()->GetName() == TEXT("K2Node_EnhancedInputAction")
            ? FindFProperty<FObjectProperty>(Node->GetClass(), TEXT("InputAction")) : nullptr;
        return Property && Property->GetObjectPropertyValue_InContainer(Node) == Action;
    });
}

FString Names(const TArray<UBlueprint*>& Blueprints)
{
    TArray<FString> Out;
    for (const UBlueprint* Blueprint : Blueprints)
    {
        Out.Add(Blueprint->GetName());
    }
    return Out.Num() > 0 ? FString::Join(Out, TEXT(", ")) : FString(TEXT("none (the game mode names no Blueprint player controller or pawn)"));
}

void CheckPlay(FTree& Tree, UWorld* World, const TArray<UInputMappingContext*>& Contexts)
{
    APlayerController* Player = World->GetFirstPlayerController();
    ULocalPlayer* Local = Player ? Player->GetLocalPlayer() : nullptr;
    UEnhancedInputLocalPlayerSubsystem* Input = Local ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Local) : nullptr;
    const UInputMappingContext* Active = nullptr;
    for (const UInputMappingContext* Context : Contexts)
    {
        Active = Active ? Active : (Input && Input->HasMappingContext(Context) ? Context : nullptr);
    }
    if (Active)
    {
        Tree.Pass(TEXT("contextAdded"), FString::Printf(TEXT("The player has '%s' added."), *Active->GetName()));
    }
    else if (Contexts.Num() > 0)
    {
        TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
        Params->SetStringField(TEXT("setting"), TEXT("enable_mapping"));
        Params->SetStringField(TEXT("contextPath"), Contexts[0]->GetPathName());
        Tree.Fail(TEXT("contextAdded"), TEXT("CONTEXT_NOT_ADDED"), FString::Printf(TEXT("No mapping context that maps it is added to the player: every key of '%s' is dead until an Add Mapping Context node adds it."), *Contexts[0]->GetName()),
                  MakeNextCall(TEXT("manage_networking"), TEXT("configure_input"), Params));
    }
    if (!Player || !Player->GetPawn())
    {
        Tree.Fail(TEXT("possessed"), TEXT("NO_PAWN"), TEXT("The player controller possesses no pawn."));
    }
    if (World->IsPaused())
    {
        TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
        Params->SetStringField(TEXT("control"), TEXT("resume"));
        Params->SetBoolField(TEXT("unpauseGame"), true);
        Tree.Fail(TEXT("paused"), TEXT("GAME_PAUSED"), TEXT("The game is paused, so nothing ticks."), MakeNextCall(TEXT("control_editor"), TEXT("play"), Params));
    }
    const FString Focused = FocusedPieWidgetForMcp();
    if (!Focused.IsEmpty())
    {
        Tree.Fail(TEXT("focus"), TEXT("WIDGET_HAS_FOCUS"), FString::Printf(TEXT("%s holds keyboard focus, so keys reach it before the game."), *Focused));
    }
}
} // namespace

TArray<UBlueprint*> PlayerBlueprints(UWorld* World)
{
    TArray<UClass*> Classes;
    TSubclassOf<AGameModeBase> Mode = World && World->GetWorldSettings() ? World->GetWorldSettings()->DefaultGameMode : nullptr;
    FString ProjectMode;
    if (!Mode && GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GlobalDefaultGameMode"), ProjectMode, GEngineIni))
    {
        Mode = LoadClass<AGameModeBase>(nullptr, *ProjectMode);
    }
    if (const AGameModeBase* Defaults = Mode ? Mode->GetDefaultObject<AGameModeBase>() : nullptr)
    {
        Classes.Add(Defaults->PlayerControllerClass);
        Classes.Add(Defaults->DefaultPawnClass);
        Classes.Add(Defaults->HUDClass);
    }
    if (const APlayerController* Player = World && World->IsPlayInEditor() ? World->GetFirstPlayerController() : nullptr)
    {
        Classes.Add(Player->GetClass());
        Classes.Add(Player->GetPawn() ? Player->GetPawn()->GetClass() : nullptr);
    }
    TArray<UBlueprint*> Out;
    for (UClass* Class : Classes)
    {
        if (UBlueprint* Blueprint = Class ? UBlueprint::GetBlueprintFromClass(Class) : nullptr)
        {
            Out.AddUnique(Blueprint);
        }
    }
    // The level's own Blueprint (of the editor level: a Play world's copy has none of its own).
    UWorld* EditorWorld = McpHandlerUtils::GetEditorWorld();
    if (EditorWorld && EditorWorld->PersistentLevel)
    {
        if (UBlueprint* LevelScript = EditorWorld->PersistentLevel->GetLevelScriptBlueprint(true))
        {
            Out.AddUnique(LevelScript);
        }
    }
    return Out;
}

bool ExplainInput(UEngineWireSubsystem& Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                  TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActionPath = GetJsonStringField(Payload, TEXT("actionPath"));
    const FKey Key(*GetJsonStringField(Payload, TEXT("key")));
    UInputAction* Action = ActionPath.IsEmpty() ? nullptr : LoadObject<UInputAction>(nullptr, *ActionPath);
    if (!Action)
    {
        Self.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No Input Action at '%s': actionPath names the action the key should fire."), *ActionPath), TEXT("ACTION_NOT_FOUND"));
        return true;
    }
    UWorld* World = GEditor && GEditor->PlayWorld ? GEditor->PlayWorld.Get() : McpHandlerUtils::GetEditorWorld();
    FTree Tree;
    const TArray<UInputMappingContext*> Contexts = ContextsMapping(Action, Key);
    const TArray<UBlueprint*> Blueprints = PlayerBlueprints(World);
    if (Contexts.Num() > 0)
    {
        Tree.Pass(TEXT("mapped"), FString::Printf(TEXT("'%s' maps it%s."), *Contexts[0]->GetName(), Key.IsValid() ? *FString::Printf(TEXT(" to %s"), *Key.ToString()) : TEXT("")));
    }
    else
    {
        // The fix maps it in a context that already maps the action to another key, else in any context, one a player
        // Blueprint adds first (a mapping in a context nobody adds stays dead); with none under /Game, the fix makes one
        // beside the action. Without a key asked about, SpaceBar stands in for the key.
        const TArray<UInputMappingContext*> Others = ContextsMapping(Action, FKey());
        TArray<UInputMappingContext*> Target = Others.Num() > 0 ? Others : ContextsMapping(nullptr, FKey());
        Target.StableSort([&Blueprints](const UInputMappingContext& A, const UInputMappingContext& B) {
            const auto Added = [&Blueprints](const UInputMappingContext* Context) {
                return Blueprints.ContainsByPredicate([Context](const UBlueprint* Blueprint) { return AddsExactly(Blueprint, Context); });
            };
            return Added(&A) && !Added(&B);
        });
        TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
        Params->SetStringField(TEXT("setting"), Target.Num() > 0 ? TEXT("add_mapping") : TEXT("create_mapping_context"));
        if (Target.Num() > 0)
        {
            Params->SetStringField(TEXT("contextPath"), Target[0]->GetPathName());
            Params->SetStringField(TEXT("actionPath"), Action->GetPathName());
            Params->SetStringField(TEXT("key"), Key.IsValid() ? Key.ToString() : TEXT("SpaceBar"));
        }
        else
        {
            Params->SetStringField(TEXT("name"), TEXT("IMC_") + Action->GetName().Replace(TEXT("IA_"), TEXT("")));
            Params->SetStringField(TEXT("path"), FPackageName::GetLongPackagePath(Action->GetOutermost()->GetName()));
        }
        Tree.Fail(TEXT("mapped"), TEXT("NOT_MAPPED"),
                  FString::Printf(TEXT("No mapping context under /Game maps '%s'%s."), *Action->GetName(), Key.IsValid() ? *FString::Printf(TEXT(" to %s"), *Key.ToString()) : TEXT("")),
                  MakeNextCall(TEXT("manage_networking"), TEXT("configure_input"), Params));
    }
    const bool bAdds = Blueprints.ContainsByPredicate([&Contexts](const UBlueprint* Blueprint) { return AddsContext(Blueprint, Contexts); });
    const bool bHandles = Blueprints.ContainsByPredicate([Action](const UBlueprint* Blueprint) { return HandlesAction(Blueprint, Action); });
    if (World && World->IsPlayInEditor())
    {
        CheckPlay(Tree, World, Contexts);
    }
    else if (bAdds)
    {
        Tree.Pass(TEXT("contextAdded"), FString::Printf(TEXT("An Add Mapping Context node is in %s."), *Names(Blueprints)));
    }
    else if (Contexts.Num() > 0)
    {
        Tree.Fail(TEXT("contextAdded"), TEXT("CONTEXT_NOT_ADDED"),
                  FString::Printf(TEXT("No Add Mapping Context node adds '%s' in the player Blueprints (%s): its keys stay dead."), *Contexts[0]->GetName(), *Names(Blueprints)));
    }
    if (bHandles)
    {
        Tree.Pass(TEXT("handled"), FString::Printf(TEXT("An event for '%s' is in %s."), *Action->GetName(), *Names(Blueprints)));
    }
    else
    {
        // The fix puts the action's event in the pawn's Blueprint (where a press usually acts), else the first one.
        const UBlueprint* const* Pawn = Blueprints.FindByPredicate([](const UBlueprint* Blueprint) {
            return Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(APawn::StaticClass());
        });
        const UBlueprint* Home = Pawn ? *Pawn : (Blueprints.Num() > 0 ? Blueprints[0] : nullptr);
        TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
        if (Home)
        {
            Params->SetStringField(TEXT("blueprintPath"), Home->GetPathName());
            Params->SetStringField(TEXT("graphName"), TEXT("EventGraph"));
            Params->SetStringField(TEXT("nodeType"), TEXT("K2Node_EnhancedInputAction"));
            Params->SetStringField(TEXT("actionPath"), Action->GetPathName());
        }
        Tree.Fail(TEXT("handled"), TEXT("NOT_HANDLED"),
                  FString::Printf(TEXT("No Blueprint of the player (%s) has an event for '%s', so firing it runs nothing."), *Names(Blueprints), *Action->GetName()),
                  Home ? MakeNextCall(TEXT("manage_blueprint"), TEXT("create_node"), Params) : TSharedPtr<FJsonObject>());
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Tree.WriteTo(Result);
    Self.SendAutomationResponse(Socket, RequestId, true, Tree.Summary(FString::Printf(TEXT("Why '%s' does nothing"), *Action->GetName())), Result);
    return true;
}
} // namespace McpExplain
