#include "Foundation/GraphLayout/McpLayered.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Foundation/GraphLayout/McpGraphNodeExtent.h"

namespace McpLayered
{
float LayoutNodes(const TArray<UEdGraphNode*>& Nodes)
{
    if (Nodes.Num() == 0)
    {
        return 0.0f;
    }
    TMap<UEdGraphNode*, int32> Index;
    TArray<FBox> Boxes;
    for (UEdGraphNode* Node : Nodes)
    {
        Index.Add(Node, Boxes.Num());
        FBox& Box = Boxes.AddDefaulted_GetRef();
        McpGraphLayout::EstimateNodeExtent(*Node, Box.Width, Box.Height);
    }
    TArray<FEdge> Edges;
    for (int32 From = 0; From < Nodes.Num(); ++From)
    {
        for (int32 PinAt = 0; PinAt < Nodes[From]->Pins.Num(); ++PinAt)
        {
            const UEdGraphPin* Pin = Nodes[From]->Pins[PinAt];
            if (!Pin || Pin->Direction != EGPD_Output)
            {
                continue;
            }
            const bool bExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
            for (const UEdGraphPin* Linked : Pin->LinkedTo)
            {
                if (const int32* To = Linked ? Index.Find(Linked->GetOwningNode()) : nullptr)
                {
                    Edges.Add({From, *To, (bExec ? 0 : 1000) + PinAt, bExec});
                }
            }
        }
    }
    const TArray<FVector2D> At = Layout(Boxes, Edges, FVector2D(Nodes[0]->NodePosX, Nodes[0]->NodePosY));
    float Bottom = Nodes[0]->NodePosY + Boxes[0].Height;
    for (int32 Node = 1; Node < Nodes.Num(); ++Node)
    {
        Nodes[Node]->NodePosX = FMath::RoundToInt(At[Node].X);
        Nodes[Node]->NodePosY = FMath::RoundToInt(At[Node].Y);
        Bottom = FMath::Max(Bottom, static_cast<float>(At[Node].Y) + Boxes[Node].Height);
    }
    return Bottom;
}
} // namespace McpLayered
