// control_editor explore_level, the reply (see McpExplore.h): what the archive holds, what was found, and how much of the
// nav mesh the pawn reached. Islands are nav points sampled at start that no path from the start reaches and no visited
// cell holds. Coverage: of the cells those samples fall in (flags aside), how many the pawn has stood in.
#include "Domains/Play/McpExplore.h"

#include "Domains/Navigation/McpNavPath.h"
#include "Editor.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpExplore
{
namespace ExploreReportParts
{
constexpr int32 MaxRows = 200;

TSharedPtr<FJsonObject> Row(const FFinding& F)
{
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetStringField(TEXT("kind"), F.Kind);
    Out->SetStringField(TEXT("text"), F.Text);
    Out->SetArrayField(TEXT("where"), McpHandlerUtils::VectorToJsonArray(F.Where));
    Out->SetNumberField(TEXT("cell"), F.Cell);
    Out->SetNumberField(TEXT("t"), FMath::RoundToDouble(F.GameSeconds * 100.0) / 100.0);
    return Out;
}
} // namespace ExploreReportParts

using namespace ExploreReportParts;

TSharedPtr<FJsonObject> Report(const FSession& S, bool bDetail)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    // On the floor plan: a nav point lies at the floor and the pawn's origin a capsule's half height above it, so their
    // height bands rarely agree (coverage read 0.08 on a platform the pawn had crossed).
    TSet<FIntPoint> Stood;
    for (const FCell& Cell : S.Cells)
    {
        Stood.Add(FIntPoint(Cell.Key.X, Cell.Key.Y));
    }
    TSet<FIntPoint> Sampled;
    TArray<TSharedPtr<FJsonValue>> Islands;
    UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
    for (const FVector& Point : S.NavSamples)
    {
        const FIntPoint Key = FloorCell(S, Point);
        Sampled.Add(Key);
        if (Stood.Contains(Key) || !World)
        {
            continue;
        }
        McpNavigationHandlers::FMcpNavPath Path;
        FString Code;
        FString Message;
        const bool bReach = McpNavigationHandlers::FindNavPath(World, S.Start, Point, FVector(100.0, 100.0, 250.0), Path, Code, Message) &&
                            Path.Reason == TEXT("ok");
        if (!bReach && Islands.Num() < 32)
        {
            Islands.Add(MakeShared<FJsonValueArray>(McpHandlerUtils::VectorToJsonArray(Point)));
        }
    }
    int32 Covered = 0;
    for (const FIntPoint& Key : Sampled)
    {
        Covered += Stood.Contains(Key) ? 1 : 0;
    }
    TArray<TSharedPtr<FJsonValue>> Findings;
    for (const FFinding& F : S.Findings)
    {
        Findings.Add(MakeShared<FJsonValueObject>(Row(F)));
    }
    Result->SetNumberField(TEXT("cells"), S.Cells.Num());
    Result->SetNumberField(TEXT("rounds"), S.Rounds);
    Result->SetNumberField(TEXT("coverage"), Sampled.Num() > 0 ? FMath::RoundToDouble(100.0 * Covered / Sampled.Num()) / 100.0 : 0.0);
    Result->SetArrayField(TEXT("findings"), Findings);
    Result->SetArrayField(TEXT("islands"), Islands);
    if (bDetail)
    {
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (int32 Index = 0; Index < S.Cells.Num() && Index < MaxRows; ++Index)
        {
            const FCell& Cell = S.Cells[Index];
            TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
            Out->SetNumberField(TEXT("cell"), Index);
            Out->SetArrayField(TEXT("where"), McpHandlerUtils::VectorToJsonArray(Cell.Where));
            Out->SetNumberField(TEXT("parent"), Cell.Parent);
            Out->SetNumberField(TEXT("chosen"), Cell.Chosen);
            Out->SetNumberField(TEXT("leads"), Cell.Leads);
            if (Cell.Key.Flags != 0)
            {
                Out->SetNumberField(TEXT("flags"), Cell.Key.Flags);
            }
            Rows.Add(MakeShared<FJsonValueObject>(Out));
        }
        Result->SetArrayField(TEXT("archive"), Rows);
    }
    return Result;
}
} // namespace McpExplore
