// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

// A Pratt parser for the assertions a call states about the editor (execute's options.expect and options.require):
//   Lamp on Table, "Point Light 2" inside Room, Chair facing Table, Lamp near Table within 50,
//   Cube.location.z in 0..10, Lamp.LightComponent.Intensity > 1000, Door exists, /Game/BP_Door compiles,
//   noWarnings, not Lamp hidden, A on B and C on D, (A on B or A on C).
// Names hold letters, digits, _ - and / (an asset path); quotes hold anything else. Keywords match in any case.
// Precedence, loosest first: or, and, not, then a relation, comparison or predicate over operands.
namespace McpPratt
{
/** One node: an operator over its operands, or a leaf (Op empty): a name or dotted path (Path, one segment per part), or
 *  a number. */
struct FNode
{
    FString Op;
    TArray<FString> Path;
    double Number = 0.0;
    bool bNumber = false;
    TArray<TSharedPtr<FNode>> Args;
};

/** Parses Source; false with OutError naming what was expected and the character it stopped at. */
bool Parse(const FString& Source, TSharedPtr<FNode>& OutRoot, FString& OutError);

/** The node written back as text, fully parenthesized (for tests and replies). */
FString Print(const TSharedPtr<FNode>& Node);
} // namespace McpPratt
