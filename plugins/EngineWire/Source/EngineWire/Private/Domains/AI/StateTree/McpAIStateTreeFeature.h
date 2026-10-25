#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"

#include "Modules/ModuleManager.h"

#if ENGINE_MINOR_VERSION >= 3
#define MCP_HAS_STATE_TREE 1
#if __has_include("StateTree.h")
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeSchema.h"
#include "StateTreeState.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
#include "StateTreeEditingSubsystem.h"
#endif
#define MCP_STATE_TREE_HEADERS_AVAILABLE 1
#else
#define MCP_STATE_TREE_HEADERS_AVAILABLE 0
#endif
#else
#define MCP_HAS_STATE_TREE 0
#define MCP_STATE_TREE_HEADERS_AVAILABLE 0
#endif

#if MCP_HAS_STATE_TREE && MCP_STATE_TREE_HEADERS_AVAILABLE
// The first state named Name (case ignored) at or under State; null when none.
inline UStateTreeState* McpFindStateTreeState(UStateTreeState* State, const FString& Name)
{
    if (!State)
    {
        return nullptr;
    }
    if (State->Name.ToString().Equals(Name, ESearchCase::IgnoreCase))
    {
        return State;
    }
    for (UStateTreeState* Child : State->Children)
    {
        if (UStateTreeState* Found = McpFindStateTreeState(Child, Name))
        {
            return Found;
        }
    }
    return nullptr;
}

// The first state named Name anywhere in EditorData's subtrees.
inline UStateTreeState* McpFindStateTreeState(const UStateTreeEditorData* EditorData, const FString& Name)
{
    for (UStateTreeState* SubTree : EditorData->SubTrees)
    {
        if (UStateTreeState* Found = McpFindStateTreeState(SubTree, Name))
        {
            return Found;
        }
    }
    return nullptr;
}

// The schema says what runs the tree, and a tree without one never compiles. component (a StateTreeComponent on any
// actor) and ai (a StateTreeAIComponent on an AI controller, UE 5.4+) live in the GameplayStateTree plugin, which this one
// does not link, so they are found by class path: the include this used was never on the path, and every tree was created
// without a schema. Any other value names a schema class (a Mass, Smart Object or project schema).
inline UClass* McpFindStateTreeSchemaClass(const FString& Schema, FString& OutError, FString& OutCode)
{
    const bool bAI = Schema.Equals(TEXT("ai"), ESearchCase::IgnoreCase);
    if (!bAI && !Schema.IsEmpty() && !Schema.Equals(TEXT("component"), ESearchCase::IgnoreCase))
    {
        UClass* Named = ResolveClassByName(Schema);
        if (Named && Named->IsChildOf(UStateTreeSchema::StaticClass()) && !Named->HasAnyClassFlags(CLASS_Abstract))
        {
            return Named;
        }
        OutError = FString::Printf(TEXT("%s is not a State Tree schema class that loads: send component, ai, or a schema class path "
            "(UStateTreeSchema subclasses, such as /Script/MassAIBehavior.MassStateTreeSchema). Nothing was created."), *Schema);
        OutCode = TEXT("INVALID_ARGUMENT");
        return nullptr;
    }
    if (FModuleManager::Get().ModuleExists(TEXT("GameplayStateTreeModule")))
    {
        FModuleManager::Get().LoadModule(TEXT("GameplayStateTreeModule"));
    }
    UClass* SchemaClass = FindObject<UClass>(nullptr, bAI ? TEXT("/Script/GameplayStateTreeModule.StateTreeAIComponentSchema")
                                                         : TEXT("/Script/GameplayStateTreeModule.StateTreeComponentSchema"));
    if (!SchemaClass || !SchemaClass->IsChildOf(UStateTreeSchema::StaticClass()))
    {
        OutError = FString::Printf(TEXT("The %s schema is not available: enable the GameplayStateTree plugin%s. Nothing was created."),
            bAI ? TEXT("AI component") : TEXT("component"), bAI ? TEXT(" (the AI schema needs UE 5.4+)") : TEXT(""));
        OutCode = TEXT("SCHEMA_NOT_AVAILABLE");
        return nullptr;
    }
    return SchemaClass;
}

// The tree compiled from its editor data, as the editor's Compile button does: from 5.5 through the editing subsystem,
// which validates the data first and stamps the hash that marks the tree up to date. An uncompiled tree does not run;
// edits used to reply "recompile the asset in the StateTree editor". OutMessages gets the errors and warnings.
inline bool McpCompileStateTree(UStateTree* Tree, TArray<FString>& OutMessages)
{
    struct FLog : FStateTreeCompilerLog
    {
        const TArray<FStateTreeCompilerLogMessage>& All() const { return Messages; }
    } Log;
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
    const bool bCompiled = UStateTreeEditingSubsystem::CompileStateTree(Tree, Log);
#else
    FStateTreeCompiler Compiler(Log);
    const bool bCompiled = Compiler.Compile(*Tree);
#endif
    for (const FStateTreeCompilerLogMessage& Message : Log.All())
    {
        if (Message.Severity <= EMessageSeverity::Warning)
        {
            OutMessages.Add(Message.Message);
        }
    }
    return bCompiled;
}

// compiled, and compileMessages when the compiler had something to say.
inline void McpCompileStateTreeInto(UStateTree* Tree, const TSharedPtr<FJsonObject>& Result)
{
    TArray<FString> Messages;
    Result->SetBoolField(TEXT("compiled"), McpCompileStateTree(Tree, Messages));
    if (Messages.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> Values;
        for (const FString& Message : Messages)
        {
            Values.Add(MakeShared<FJsonValueString>(Message));
        }
        Result->SetArrayField(TEXT("compileMessages"), Values);
    }
}
#endif
