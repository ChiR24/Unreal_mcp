#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UCanvasPanelSlot;

namespace McpCassowary
{
class FSolver;
}

// manage_blueprint arrange_widgets: the children of a Canvas Panel placed by relations (A left of B with a 20 gap, the
// same width, 24 in from the right edge, centred) instead of coordinates. The relations, kept to as Cassowary allows
// (McpCassowary: required ones exactly, preferred ones by strength), with every widget weakly staying where it is,
// strongly inside the panel and no narrower than its text, give each widget's box at a reference screen size; its
// anchors follow what pins it, so the layout holds at other sizes, and the relations are checked again at each size
// asked for. Relations and their checks are McpWidgetArrangeRelations.cpp, anchors McpWidgetArrangeAnchors.cpp.
namespace McpWidgetArrange
{
enum class EAnchor : uint8
{
    Start,  // left or top
    Middle, // centred
    End,    // right or bottom
    Stretch // both edges
};

/** A widget's box in Slate units, from the panel's top-left corner. */
struct FArrangeBox
{
    double Left = 0.0;
    double Top = 0.0;
    double Width = 0.0;
    double Height = 0.0;
};

struct FRelation
{
    FString Type;              // leftOf, rightOf, above, below, align*, sameWidth, sameHeight, width, height, inset, center
    TArray<int32> Widgets;     // indices into the arranged widgets; leftOf etc.: [a, b], "a left of b"
    double Value = 0.0;        // gap, size or inset, in Slate units
    bool bHasValue = false;    // a gap given: exactly that far apart (else at least 0 apart)
    FString Edge;              // inset: left, right, top or bottom; center: x, y or both
    double Strength = 0.0;     // McpCassowary strength
    FString Text;              // as the caller wrote it, for replies
};

/** Relations from JSON; Names are the widgets being arranged (a relation naming another adds it to Names). False with
 *  OutError (INVALID_ARGUMENT) for a relation that cannot be read. */
bool ParseRelations(const TArray<TSharedPtr<FJsonValue>>& Json, TArray<FString>& Names, TArray<FRelation>& Out, FString& OutError);

/** Adds the relation to Solver, whose variables are 4 per widget (left, top, width, height) in Names order, on a panel
 *  of Panel size; false when it is required and cannot hold with the ones before it. */
bool AddRelation(McpCassowary::FSolver& Solver, const FRelation& Relation, const FVector2D& Panel);

/** The boxes that keep Relations on a panel of Panel size, each widget otherwise inside the panel and no smaller than
 *  MinSize (strong) and where Current has it (weak); false with OutFailed, the first required relation that cannot
 *  hold with the required ones before it. */
bool Solve(const TArray<FRelation>& Relations, const TArray<FArrangeBox>& Current, const TArray<FVector2D>& MinSize, const FVector2D& Panel,
           TArray<FArrangeBox>& Out, int32& OutFailed);

/** How far Boxes (on a panel of Panel size) are from meeting the relation, in Slate units (0: met). */
double Residual(const FRelation& Relation, const TArray<FArrangeBox>& Boxes, const FVector2D& Panel);

/** Each widget's anchoring per axis (X then Y): its own inset or centre pins, else those of the widgets the relations
 *  tie it to on that axis, else where its box sits on the panel. */
TArray<TPair<EAnchor, EAnchor>> InferAnchors(const TArray<FRelation>& Relations, const TArray<FArrangeBox>& Boxes, const FVector2D& Panel);

/** Slot laid out so the widget's box on a panel of Panel size is Box, held by Anchors. */
void WriteSlot(UCanvasPanelSlot& Slot, const FArrangeBox& Box, const TPair<EAnchor, EAnchor>& Anchors, const FVector2D& Panel);
} // namespace McpWidgetArrange
