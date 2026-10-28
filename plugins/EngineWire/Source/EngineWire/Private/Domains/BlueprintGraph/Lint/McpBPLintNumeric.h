#pragma once

#include "CoreMinimal.h"
#include "Domains/BlueprintGraph/Lint/Interp/McpBPLintInterp.h"

// The checks the numeric lint rules (McpBPLintNumeric.cpp) gather from each run the interpreter found (McpBPLintChecks.cpp).
namespace McpBPLint
{
/** What every run of a node says of one check: whether its problem may happen in some run, and whether in every one. */
struct FVerdict
{
    bool bMay = false;
    bool bMust = true;
    FString Detail;     // from the first run where it may happen
    FString MustDetail; // from a run where it must
};

using FVerdicts = TMap<const UEdGraphNode*, TMap<FString, FVerdict>>;

void Note(FVerdicts& Verdicts, const UEdGraphNode* Node, const TCHAR* Rule, bool bMay, bool bMust, const FString& Detail);
/** Divisors, array indexes and Sets of a ranged variable, judged in the state the run starts in. */
void CheckValues(const FRun& Run, const FMembers& Members, FVerdicts& Verdicts);
/** Branches decided before they run, and While Loops whose body never makes their condition false. */
void CheckDecisions(const FAnalysis& Analysis, FVerdicts& Verdicts);
} // namespace McpBPLint
