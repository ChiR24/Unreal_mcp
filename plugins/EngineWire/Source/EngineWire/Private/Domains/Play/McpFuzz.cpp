// control_editor fuzz_blueprint (see McpFuzz.h): the target, the functions to call, and what is held around the calls.
#include "Domains/Play/McpFuzz.h"

#include "Domains/Play/McpPlaySession.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "EngineUtils.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPAssetLoad.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "GameFramework/Actor.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Templates/UnrealTemplate.h"

namespace McpFuzz
{
namespace FuzzParts
{
constexpr int32 MaxFunctions = 64;

// The actor named, or the first instance of the Blueprint in the running game, and the Blueprint whose own functions are
// the default: blueprintPath's, else the actor's class's.
AActor* FindTarget(const TSharedPtr<FJsonObject>& Payload, UWorld* World, const UBlueprint*& OutBlueprint, FString& OutCode,
                   FString& OutError)
{
    OutCode = TEXT("ACTOR_NOT_FOUND");
    const FString Name = GetJsonStringField(Payload, TEXT("actorName"));
    if (!Name.IsEmpty())
    {
        AActor* Actor = FindActorByNameInWorldForMcp(World, Name, false);
        OutBlueprint = Actor ? UBlueprint::GetBlueprintFromClass(Actor->GetClass()) : nullptr;
        OutError = FString::Printf(TEXT("actorName '%s' names no actor in the running game."), *Name);
        return Actor;
    }
    FString Path;
    OutBlueprint = LoadBlueprintAsset(GetJsonStringField(Payload, TEXT("blueprintPath")), Path, OutError);
    UClass* Class = OutBlueprint ? OutBlueprint->GeneratedClass.Get() : nullptr;
    if (!Class || !Class->IsChildOf(AActor::StaticClass()))
    {
        OutCode = TEXT("BLUEPRINT_NOT_FOUND");
        OutError = Class ? FString::Printf(TEXT("%s is no actor Blueprint: only actors placed in the running game are called."), *Path)
                         : FString::Printf(TEXT("blueprintPath names no compiled Blueprint (%s)."), *OutError);
        return nullptr;
    }
    TActorIterator<AActor> It(World, Class);
    OutError = FString::Printf(TEXT("No instance of %s is in the running game: place one in the level, or name one with actorName."), *Path);
    return It ? *It : nullptr;
}

// The Blueprint's own functions and custom events, in the order written; its construction script is left out.
TArray<FName> OwnFunctions(const UBlueprint* Blueprint)
{
    TArray<FName> Names;
    for (const UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (Graph && Graph->GetFName() != UEdGraphSchema_K2::FN_UserConstructionScript)
        {
            Names.AddUnique(Graph->GetFName());
        }
    }
    for (const UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (!Graph)
        {
            continue;
        }
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            if (const UK2Node_CustomEvent* Event = Cast<UK2Node_CustomEvent>(Node))
            {
                Names.AddUnique(Event->CustomFunctionName);
            }
        }
    }
    return Names;
}

// What a caller passes: a pass-by-reference parameter is an input too; a return value or a plain output is not.
TArray<const FProperty*> InputsOf(const UFunction* Function)
{
    TArray<const FProperty*> Inputs;
    for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
    {
        const bool bOutput = It->HasAnyPropertyFlags(CPF_ReturnParm) ||
                             (It->HasAnyPropertyFlags(CPF_OutParm) && !It->HasAnyPropertyFlags(CPF_ReferenceParm));
        if (!bOutput)
        {
            Inputs.Add(*It);
        }
    }
    return Inputs;
}

// The functions named (a parent's included), else the Blueprint's own that take inputs; every value is chosen up front.
bool PlanCalls(const TSharedPtr<FJsonObject>& Payload, const UBlueprint* Blueprint, UWorld* World, AActor* Target, TArray<FPlan>& Out,
               FString& OutError)
{
    TArray<FName> Names;
    for (const FString& Each : McpHandlerUtils::GetStringArrayField(Payload, TEXT("functions")))
    {
        Names.AddUnique(FName(*Each));
    }
    const bool bNamed = Names.Num() > 0;
    const TArray<FName> Own = OwnFunctions(Blueprint);
    if (!bNamed)
    {
        Names = Own;
    }
    for (const FName& Name : Names)
    {
        UFunction* Function = Target->GetClass()->FindFunctionByName(Name);
        if (!Function && bNamed)
        {
            TArray<FString> Listed;
            for (const FName& Each : Own)
            {
                Listed.Add(Each.ToString());
            }
            OutError = FString::Printf(TEXT("%s has no function or custom event '%s'. Its own: %s."), *Blueprint->GetName(), *Name.ToString(),
                                       Listed.Num() > 0 ? *FString::Join(Listed, TEXT(", ")) : TEXT("none"));
            return false;
        }
        FPlan Plan;
        Plan.Function = Function;
        Plan.Inputs = Function ? InputsOf(Function) : TArray<const FProperty*>();
        if (!Function || (!bNamed && Plan.Inputs.Num() == 0) || Out.Num() >= MaxFunctions)
        {
            continue;
        }
        for (const FProperty* Input : Plan.Inputs)
        {
            Plan.Values.Add(ValuesFor(Input, World, Target));
        }
        Out.Add(MoveTemp(Plan));
    }
    return true;
}

FString Summary(const FJsonObject& Result, int32 Functions)
{
    const int32 Failures = static_cast<int32>(Result.GetNumberField(TEXT("failures")));
    FString Text = FString::Printf(TEXT("Called %d function(s) %d time(s) with edge-case inputs: "), Functions,
                                   static_cast<int32>(Result.GetNumberField(TEXT("calls"))));
    Text += Failures == 0 ? FString(TEXT("no Blueprint error."))
                          : FString::Printf(TEXT("%d call(s) raised Blueprint runtime errors, each listed under its function with the input "
                                                 "that caused it and the node that raised it."), Failures);
    FString Stopped;
    if (Result.TryGetStringField(TEXT("stopped"), Stopped))
    {
        Text += Stopped == TEXT("budget")             ? TEXT(" Stopped at maxCalls: name the functions not reached to call them.")
              : Stopped == TEXT("deadline")           ? TEXT(" Stopped after 20 s: name the functions not reached to call them.")
              : Stopped == TEXT("target_destroyed")   ? TEXT(" Stopped: a call destroyed the actor (stoppedBy).")
                                                      : TEXT(" Stopped: Play is ending after a call (stoppedBy).");
    }
    return Text;
}
} // namespace FuzzParts

using namespace FuzzParts;

bool Handle(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    auto Refuse = [&](const TCHAR* Code, const FString& Message)
    {
        Bridge.SendAutomationResponse(Socket, RequestId, false, Message, nullptr, Code);
        return true;
    };
    UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
    if (!World || !McpPlaySession::StartedByMcp(World))
    {
        return Refuse(TEXT("PLAY_NOT_STARTED_BY_MCP"), TEXT("fuzz_blueprint calls into a Play session started through this server "
                      "(control_editor play); none runs, or the running one may be someone's game."));
    }
    const UBlueprint* Blueprint = nullptr;
    FString Code;
    FString Error;
    AActor* Target = FindTarget(Payload, World, Blueprint, Code, Error);
    if (!Target)
    {
        return Refuse(*Code, Error);
    }
    if (!Blueprint)
    {
        return Refuse(TEXT("NOT_A_BLUEPRINT"), FString::Printf(TEXT("'%s' is a %s, a native class: only a Blueprint's functions are called."),
                      *Target->GetActorNameOrLabel(), *Target->GetClass()->GetName()));
    }
    // A breakpoint a call reaches would stop the game in the debugger and bring the Blueprint's editor forward.
    if (FKismetDebugUtilities::BlueprintHasBreakpoints(Blueprint))
    {
        return Refuse(TEXT("BREAKPOINTS_SET"), FString::Printf(TEXT("%s has breakpoints: a call reaching one would stop the game in the "
                      "debugger. Remove them (Debug > Delete All Breakpoints in its editor), then call again."), *Blueprint->GetName()));
    }
    TArray<FPlan> Plans;
    if (!PlanCalls(Payload, Blueprint, World, Target, Plans, Error))
    {
        return Refuse(TEXT("FUNCTION_NOT_FOUND"), Error);
    }
    if (Plans.Num() == 0)
    {
        return Refuse(TEXT("NOTHING_TO_FUZZ"), FString::Printf(TEXT("%s has no function or custom event of its own that takes inputs: "
                      "name the ones to call with functions."), *Blueprint->GetName()));
    }
    const FString TargetName = Target->GetActorNameOrLabel();
    const int32 MaxCalls = FMath::Clamp(static_cast<int32>(GetJsonNumberField(Payload, TEXT("maxCalls"), 200.0)), 1, 500);
    TSharedPtr<FJsonObject> Result;
    {
        // Where Break on Exceptions is on, the first error would stop the game in the debugger: held off for the calls only.
        TGuardValue<bool> NoBreak(GetMutableDefault<UEditorExperimentalSettings>()->bBreakOnExceptions, false);
        Result = CallAll(Plans, World, Target, MaxCalls);
    }
    // Each error is in the reply's rows already: the script log's copies of them say nothing about this call.
    Bridge.ForgetCapturedMessages([](const FString& Line)
    {
        return Line.StartsWith(TEXT("[LogScriptCore]")) || Line.StartsWith(TEXT("[LogScript]")) || Line.Contains(TEXT("Blueprint Runtime Error"));
    });
    Result->SetStringField(TEXT("target"), TargetName);
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetOutermost()->GetName());
    Bridge.SendAutomationResponse(Socket, RequestId, true, Summary(*Result, Plans.Num()), Result);
    return true;
}
} // namespace McpFuzz
