// McpAutomationBridge_LevelStructureBlueprintNodeAliases.cpp — friendly node names for add_level_blueprint_node.
//
// Dogfood #160/#164: callers reach for "EventBeginPlay" or "PrintString"; only raw
// K2Node_* class names used to resolve, and the created event/function nodes carried
// no binding at all. The aliases below map to a node class plus the member to bind.
//
// Dogfood #221 (gameplay build): the function lookup was hard-wired to
// UKismetSystemLibrary, so gameplay-critical calls such as GetPlayerPawn,
// GetAllActorsOfClass, ApplyDamage, SpawnSystemAtLocation or RandomFloatInRange
// were rejected as "could not be bound" even though the caller passed a perfectly
// valid K2 function name. The lookup now searches the common Blueprint function
// libraries (and core actor/controller classes) before failing, so the raw
//   nodeClass: "K2Node_CallFunction", functionName: "<AnyK2Function>"
// path works for gameplay graphs and not just for PrintString/Delay.
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureActions.h"

#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_MacroInstance.h"
#include "Kismet/KismetSystemLibrary.h"

#if WITH_EDITOR
namespace McpLevelStructure
{
namespace
{
// Function libraries probed in order when binding a raw K2Node_CallFunction.
// Paths are resolved through FindObject so this file needs no extra includes and
// stays valid when a given plugin (UMG, AIModule) is not loaded — the lookup just
// skips it.
const TCHAR* const GBlueprintFunctionLibraries[] = {
    TEXT("/Script/Engine.KismetSystemLibrary"),
    TEXT("/Script/Engine.KismetMathLibrary"),
    TEXT("/Script/Engine.KismetStringLibrary"),
    TEXT("/Script/Engine.KismetTextLibrary"),
    TEXT("/Script/Engine.KismetArrayLibrary"),
    TEXT("/Script/Engine.KismetInputLibrary"),
    TEXT("/Script/Engine.KismetMaterialLibrary"),
    TEXT("/Script/Engine.KismetRenderingLibrary"),
    TEXT("/Script/Engine.GameplayStatics"),
    TEXT("/Script/Engine.Actor"),
    TEXT("/Script/Engine.Pawn"),
    TEXT("/Script/Engine.Character"),
    TEXT("/Script/Engine.Controller"),
    TEXT("/Script/Engine.PlayerController"),
    TEXT("/Script/Engine.LevelScriptActor"),
    TEXT("/Script/Engine.BlueprintAsyncActionBase"),
    TEXT("/Script/AIModule.AIBlueprintHelperLibrary"),
    TEXT("/Script/NavigationSystem.NavigationSystemV1"),
    TEXT("/Script/UMG.WidgetBlueprintLibrary"),
    TEXT("/Script/Niagara.NiagaraFunctionLibrary"),
};

UFunction* FindBindableFunctionByName(const FString& FunctionName)
{
    const FName FunctionFName(*FunctionName);
    for (const TCHAR* LibraryPath : GBlueprintFunctionLibraries)
    {
        UClass* Library = FindObject<UClass>(nullptr, LibraryPath);
        if (!Library)
        {
            continue;
        }
        if (UFunction* Function = Library->FindFunctionByName(FunctionFName))
        {
            return Function;
        }
    }
    return nullptr;
}
} // namespace

bool ResolveLevelBlueprintNodeAlias(FString& InOutNodeClass, FString& OutEventName, FString& OutFunctionName)
{
    FString Key = InOutNodeClass.ToLower();
    Key.ReplaceInline(TEXT("_"), TEXT(""));
    Key.ReplaceInline(TEXT(" "), TEXT(""));
    OutEventName.Reset();
    OutFunctionName.Reset();
    struct FAlias { const TCHAR* Key; const TCHAR* NodeClass; const TCHAR* Event; const TCHAR* Function; };
    static const FAlias Aliases[] = {
        {TEXT("eventbeginplay"), TEXT("K2Node_Event"), TEXT("ReceiveBeginPlay"), nullptr},
        {TEXT("beginplay"), TEXT("K2Node_Event"), TEXT("ReceiveBeginPlay"), nullptr},
        {TEXT("receivebeginplay"), TEXT("K2Node_Event"), TEXT("ReceiveBeginPlay"), nullptr},
        {TEXT("eventtick"), TEXT("K2Node_Event"), TEXT("ReceiveTick"), nullptr},
        {TEXT("tick"), TEXT("K2Node_Event"), TEXT("ReceiveTick"), nullptr},
        {TEXT("receivetick"), TEXT("K2Node_Event"), TEXT("ReceiveTick"), nullptr},
        {TEXT("eventendplay"), TEXT("K2Node_Event"), TEXT("ReceiveEndPlay"), nullptr},
        {TEXT("endplay"), TEXT("K2Node_Event"), TEXT("ReceiveEndPlay"), nullptr},
        {TEXT("printstring"), TEXT("K2Node_CallFunction"), nullptr, TEXT("PrintString")},
        {TEXT("print"), TEXT("K2Node_CallFunction"), nullptr, TEXT("PrintString")},
        {TEXT("printtext"), TEXT("K2Node_CallFunction"), nullptr, TEXT("PrintText")},
        {TEXT("delay"), TEXT("K2Node_CallFunction"), nullptr, TEXT("Delay")},
        // Gameplay aliases (dogfood #221) — resolved through the wide library search.
        {TEXT("getplayerpawn"), TEXT("K2Node_CallFunction"), nullptr, TEXT("GetPlayerPawn")},
        {TEXT("getplayercharacter"), TEXT("K2Node_CallFunction"), nullptr, TEXT("GetPlayerCharacter")},
        {TEXT("getallactorsofclass"), TEXT("K2Node_CallFunction"), nullptr, TEXT("GetAllActorsOfClass")},
        {TEXT("getgameplaystatics"), TEXT("K2Node_CallFunction"), nullptr, TEXT("GetGameInstance")},
        {TEXT("applydamage"), TEXT("K2Node_CallFunction"), nullptr, TEXT("ApplyDamage")},
        {TEXT("spawnsound2d"), TEXT("K2Node_CallFunction"), nullptr, TEXT("SpawnSound2D")},
        {TEXT("spawnsoundatlocation"), TEXT("K2Node_CallFunction"), nullptr, TEXT("SpawnSoundAtLocation")},
        {TEXT("spawnsystematlocation"), TEXT("K2Node_CallFunction"), nullptr, TEXT("SpawnSystemAtLocation")},
        {TEXT("spawnemitteratlocation"), TEXT("K2Node_CallFunction"), nullptr, TEXT("SpawnEmitterAtLocation")},
        {TEXT("maketransform"), TEXT("K2Node_CallFunction"), nullptr, TEXT("MakeTransform")},
        {TEXT("makevector"), TEXT("K2Node_CallFunction"), nullptr, TEXT("MakeVector")},
        {TEXT("randomfloatinrange"), TEXT("K2Node_CallFunction"), nullptr, TEXT("RandomFloatInRange")},
        {TEXT("randomintegerinrange"), TEXT("K2Node_CallFunction"), nullptr, TEXT("RandomIntegerInRange")},
        {TEXT("settimer"), TEXT("K2Node_CallFunction"), nullptr, TEXT("K2_SetTimer")},
        {TEXT("k2settimer"), TEXT("K2Node_CallFunction"), nullptr, TEXT("K2_SetTimer")},
        {TEXT("setactorlocation"), TEXT("K2Node_CallFunction"), nullptr, TEXT("K2_SetActorLocation")},
        {TEXT("getactorlocation"), TEXT("K2Node_CallFunction"), nullptr, TEXT("K2_GetActorLocation")},
        {TEXT("setactorrotation"), TEXT("K2Node_CallFunction"), nullptr, TEXT("K2_SetActorRotation")},
        {TEXT("destroyactor"), TEXT("K2Node_CallFunction"), nullptr, TEXT("K2_DestroyActor")},
        {TEXT("setlifespan"), TEXT("K2Node_CallFunction"), nullptr, TEXT("SetLifeSpan")},
        {TEXT("branch"), TEXT("K2Node_IfThenElse"), nullptr, nullptr},
        {TEXT("ifthenelse"), TEXT("K2Node_IfThenElse"), nullptr, nullptr},
        {TEXT("sequence"), TEXT("K2Node_ExecutionSequence"), nullptr, nullptr},
        {TEXT("executionsequence"), TEXT("K2Node_ExecutionSequence"), nullptr, nullptr},
        // Class-name form of the spawn node: bare "SpawnActorFromClass" used to
        // fall through alias resolution and die with "Node class not found",
        // even though the pin-order fix made the node itself safe to create.
        {TEXT("spawnactorfromclass"), TEXT("K2Node_SpawnActorFromClass"), nullptr, nullptr},
        {TEXT("foreachloop"), TEXT("K2Node_MacroInstance"), nullptr, TEXT("ForEachLoop")},
        // Standard macros (K2Node_MacroInstance) — resolved against
        // /Engine/EditorBlueprintResources/StandardMacros by the node handler.
        {TEXT("forloop"), TEXT("K2Node_MacroInstance"), nullptr, TEXT("ForLoop")},
        {TEXT("whileloop"), TEXT("K2Node_MacroInstance"), nullptr, TEXT("WhileLoop")},
        {TEXT("doonce"), TEXT("K2Node_MacroInstance"), nullptr, TEXT("DoOnce")},
        {TEXT("gate"), TEXT("K2Node_MacroInstance"), nullptr, TEXT("Gate")},
        {TEXT("multigate"), TEXT("K2Node_MacroInstance"), nullptr, TEXT("MultiGate")},
        {TEXT("flipflop"), TEXT("K2Node_MacroInstance"), nullptr, TEXT("FlipFlop")},
        {TEXT("isvalid"), TEXT("K2Node_MacroInstance"), nullptr, TEXT("IsValid")},
    };
    for (const FAlias& Alias : Aliases)
    {
        if (Key == Alias.Key)
        {
            InOutNodeClass = Alias.NodeClass;
            OutEventName = Alias.Event ? Alias.Event : TEXT("");
            OutFunctionName = Alias.Function ? Alias.Function : TEXT("");
            return true;
        }
    }
    return false;
}

bool ApplyLevelBlueprintNodeAlias(UK2Node* Node, const FString& EventName, const FString& FunctionName, FString& OutError)
{
    if (!EventName.IsEmpty())
    {
        UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
        if (!EventNode)
        {
            OutError = FString::Printf(TEXT("Alias event %s needs a K2Node_Event node"), *EventName);
            return false;
        }
        EventNode->EventReference.SetExternalMember(FName(*EventName), AActor::StaticClass());
        EventNode->bOverrideFunction = true;
        return true;
    }
    // Macro instances (ForEachLoop, DoOnce, Gate, ...) carry the macro name in the
    // FunctionName slot; bind them to the engine StandardMacros macro graph.
    if (UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))
    {
        if (FunctionName.IsEmpty())
        {
            OutError = TEXT("Macro node needs a macro name (ForEachLoop, DoOnce, Gate, ...).");
            return false;
        }
        UBlueprint* MacroLibrary = LoadObject<UBlueprint>(
            nullptr,
            TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
        if (!MacroLibrary)
        {
            OutError = TEXT("Could not load engine StandardMacros library.");
            return false;
        }
        UEdGraph* MacroGraph = nullptr;
        for (UEdGraph* Graph : MacroLibrary->MacroGraphs)
        {
            if (Graph && Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
            {
                MacroGraph = Graph;
                break;
            }
        }
        if (!MacroGraph)
        {
            OutError = FString::Printf(TEXT("Macro '%s' not found in StandardMacros library."), *FunctionName);
            return false;
        }
        MacroNode->SetMacroGraph(MacroGraph);
        return true;
    }
    if (!FunctionName.IsEmpty())
    {
        UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
        // Dogfood #221: search every common Blueprint library instead of only
        // UKismetSystemLibrary, so gameplay calls (GetPlayerPawn, ApplyDamage,
        // SpawnSystemAtLocation, ...) bind instead of being rejected.
        UFunction* Function = FindBindableFunctionByName(FunctionName);
        if (!CallNode || !Function)
        {
            OutError = FString::Printf(
                TEXT("Function '%s' could not be bound to a K2Node_CallFunction. "
                     "Pass a K2 callable name from a Blueprint library (KismetSystemLibrary, "
                     "KismetMathLibrary, GameplayStatics, Actor, ...) or use an event alias."),
                *FunctionName);
            return false;
        }
        CallNode->SetFromFunction(Function);
        return true;
    }
    return true;
}
}
#endif
