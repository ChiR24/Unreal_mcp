#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "UObject/Package.h"

// The engine's StandardMacros library: the Blueprint behind ForLoop, ForEachLoop, DoOnce, Gate, WhileLoop and the
// other loops and gates the node catalog has no class for.
inline const TCHAR* McpStandardMacrosPackage()
{
    return TEXT("/Engine/EditorBlueprintResources/StandardMacros");
}

// The library, loaded; null when the engine content is missing.
inline UBlueprint* McpStandardMacros()
{
    return LoadObject<UBlueprint>(nullptr, *(FString(McpStandardMacrosPackage()) + TEXT(".StandardMacros")));
}

// The library's macro graph named Name, in any case; null when it has none or does not load.
inline UEdGraph* McpFindStandardMacro(const FString& Name)
{
    if (UBlueprint* Library = McpStandardMacros())
    {
        for (UEdGraph* Graph : Library->MacroGraphs)
        {
            if (Graph && Graph->GetName().Equals(Name, ESearchCase::IgnoreCase))
            {
                return Graph;
            }
        }
    }
    return nullptr;
}

// Whether Graph is one of the library's macros (a project macro of the same name is not).
inline bool McpIsStandardMacro(const UEdGraph* Graph)
{
    return Graph && Graph->GetOutermost()->GetName() == McpStandardMacrosPackage();
}
