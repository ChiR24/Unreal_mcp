// Copyright (c) 2024 EngineWire Contributors
//
// Where each piece of a fix_coplanar pass goes. Moving the smaller piece of every pair moved each piece of a row of
// equal flush pieces (pier sections, floor tiles) the same way, so neighbours stayed in one plane and a pass freed only
// the end of the row: twelve sections were left as a staircase after four passes. The faces that touch in one plane are
// 2-coloured instead, outward from the larger piece of the largest overlap, and only one colour moves, so a row
// alternates and one pass frees it.
//
// A move carries all of a piece's faces, not only the one it parts: the walls of a room meeting at its corners were
// each nudged off one neighbour into the plane of another, and moved back on the next pass, until the passes ran out
// with pairs left and no word on them. Each move is now made on the plan so far and kept only when its pair is parted
// and the piece it moved meets no face it did not meet before; else the other way, then the other piece, then twice as
// far. A pair no move parts is handed back with why.

#include "Domains/ControlActor/Placement/McpCoplanarFaces.h"

namespace McpCoplanar
{
namespace
{
// One side (axis * 2, plus 1 for the + side) of one mover: an actor (its Owner) for a pair between two actors, or one
// part (-1 - its piece) for a pair inside one actor, which moves in its Blueprint.
using FMcpFaceKey = TPair<int32, int32>;

// Two faces in one plane, the lower piece first.
struct FMcpTouch
{
    int32 Piece = 0;
    int32 Face = 0;
    int32 Other = 0;
    int32 OtherFace = 0;

    bool operator==(const FMcpTouch& Rhs) const
    {
        return Piece == Rhs.Piece && Face == Rhs.Face && Other == Rhs.Other && OtherFace == Rhs.OtherFace;
    }
};

FMcpTouch McpTouch(int32 Piece, int32 Face, int32 Other, int32 OtherFace)
{
    return Piece < Other ? FMcpTouch{Piece, Face, Other, OtherFace} : FMcpTouch{Other, OtherFace, Piece, Face};
}

int32 McpCoplanarWorldAxis(const FVector& Normal)
{
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        if (FMath::Abs(Normal[Axis]) > 0.99)
        {
            return Axis;
        }
    }
    return INDEX_NONE;
}

// What moves Piece of Pair: its actor, or the part alone when both pieces belong to one actor.
int32 McpMover(const TArray<FMcpCoplanarPiece>& Pieces, const FMcpCoplanarPair& Pair, int32 Piece)
{
    return Pieces[Pair.Piece].Owner == Pieces[Pair.Other].Owner ? -1 - Piece : Pieces[Piece].Owner;
}

// True for each face that moves. Every run of faces touching in one plane is coloured from the larger piece of its
// largest overlap, which stays; an odd ring keeps one pair alike, which the check on each move then parts.
TMap<FMcpFaceKey, bool> McpCoplanarColour(const TArray<FMcpCoplanarPiece>& Pieces, const TArray<FMcpCoplanarPair>& Pairs,
                                          const TArray<int32>& Sides)
{
    TMap<FMcpFaceKey, TArray<int32>> Touching;
    for (int32 Index = 0; Index < Pairs.Num(); ++Index)
    {
        if (Sides[Index] != INDEX_NONE)
        {
            Touching.FindOrAdd(FMcpFaceKey(McpMover(Pieces, Pairs[Index], Pairs[Index].Piece), Sides[Index])).Add(Index);
            Touching.FindOrAdd(FMcpFaceKey(McpMover(Pieces, Pairs[Index], Pairs[Index].Other), Sides[Index])).Add(Index);
        }
    }
    TMap<FMcpFaceKey, bool> Moves;
    for (int32 Index = 0; Index < Pairs.Num(); ++Index)
    {
        const FMcpFaceKey Start(McpMover(Pieces, Pairs[Index], Pairs[Index].Other), Sides[Index]);
        if (Sides[Index] == INDEX_NONE || Moves.Contains(Start))
        {
            continue;
        }
        Moves.Add(Start, false);
        TArray<FMcpFaceKey> Queue = {Start};
        for (int32 Head = 0; Head < Queue.Num(); ++Head)
        {
            const FMcpFaceKey Face = Queue[Head];
            const bool bMoves = Moves.FindRef(Face);
            for (const int32 Pair : Touching.FindRef(Face))
            {
                const int32 Mine = McpMover(Pieces, Pairs[Pair], Pairs[Pair].Piece);
                const FMcpFaceKey Next(Mine == Face.Key ? McpMover(Pieces, Pairs[Pair], Pairs[Pair].Other) : Mine, Face.Value);
                if (!Moves.Contains(Next))
                {
                    Moves.Add(Next, !bMoves);
                    Queue.Add(Next);
                }
            }
        }
    }
    return Moves;
}

// Whether Face of Piece and OtherFace of Other lie in one plane where the plan so far puts them.
bool McpInOnePlane(const TArray<FMcpCoplanarPiece>& Pieces, const TArray<FVector>& Offsets, int32 Piece, int32 Face,
                   int32 Other, int32 OtherFace)
{
    FMcpCoplanarFace Moved = Pieces[Other].Faces[OtherFace];
    Moved.Center += Offsets[Other] - Offsets[Piece];
    double Gap = 0.0;
    double U = 0.0;
    double V = 0.0;
    return FacesInOnePlane(Pieces[Piece].Faces[Face], Moved, Gap, U, V);
}

// Every face of Members in one plane with a face of a piece outside them, where the plan so far puts them.
// ponytail: every piece's box is tested per move; sweep along X as FindPairs does if a level makes the fix slow.
TArray<FMcpTouch> McpTouches(const TArray<FMcpCoplanarPiece>& Pieces, const TArray<FVector>& Offsets,
                             const TArray<int32>& Members)
{
    TArray<FMcpTouch> Touches;
    for (const int32 Member : Members)
    {
        const FBox Reach = Pieces[Member].Bounds.ShiftBy(Offsets[Member]).ExpandBy(McpCoplanarPlaneTolerance + 1.0);
        for (int32 Other = 0; Other < Pieces.Num(); ++Other)
        {
            if (Members.Contains(Other) || !Reach.Intersect(Pieces[Other].Bounds.ShiftBy(Offsets[Other])))
            {
                continue;
            }
            for (int32 Face = 0; Face < Pieces[Member].Faces.Num(); ++Face)
            {
                for (int32 OtherFace = 0; OtherFace < Pieces[Other].Faces.Num(); ++OtherFace)
                {
                    if (McpInOnePlane(Pieces, Offsets, Member, Face, Other, OtherFace))
                    {
                        Touches.Add(McpTouch(Member, Face, Other, OtherFace));
                    }
                }
            }
        }
    }
    return Touches;
}

// Why no move parted Pair: both parts pinned, every move too short to read as two planes, or every move putting a piece
// in another plane.
FString McpLeftWhy(const TArray<FMcpCoplanarPiece>& Pieces, const FMcpCoplanarPair& Pair, bool bParts, bool bParted,
                   double Distance)
{
    const FString Mine = bParts ? Pieces[Pair.Piece].PinnedWhy : FString();
    const FString Theirs = bParts ? Pieces[Pair.Other].PinnedWhy : FString();
    if (!Mine.IsEmpty() && !Theirs.IsEmpty())
    {
        return Mine == Theirs ? Mine : Mine + TEXT("; ") + Theirs;
    }
    if (!bParted)
    {
        return FString::Printf(TEXT("a move of %.1f units leaves the faces closer than %.1f, one plane to the depth buffer: "
                                    "give a distance of 1 or more"), 2.0 * Distance, McpCoplanarPlaneTolerance);
    }
    const FString Pinned = Mine.IsEmpty() ? Theirs : Mine;
    return FString::Printf(TEXT("every move of %s, %.1f or %.1f units either way, puts a piece in the plane of another face "
                                "it touches: move one by hand"),
                           Pinned.IsEmpty() ? TEXT("either piece") : *(TEXT("the other piece (") + Pinned + TEXT(")")),
                           Distance, 2.0 * Distance);
}
} // namespace

FMcpCoplanarSettle SettleCoplanar(const TArray<FMcpCoplanarPiece>& Pieces, const TArray<FMcpCoplanarPair>& Pairs,
                                  double Distance)
{
    FMcpCoplanarSettle Settle;
    Settle.Offsets.Init(FVector::ZeroVector, Pieces.Num());
    TArray<int32> Sides;
    for (const FMcpCoplanarPair& Pair : Pairs)
    {
        const int32 Axis = McpCoplanarWorldAxis(Pair.Normal);
        Sides.Add(Axis == INDEX_NONE ? INDEX_NONE : Axis * 2 + (Pair.Normal[Axis] > 0.0 ? 1 : 0));
    }
    const TMap<FMcpFaceKey, bool> Moves = McpCoplanarColour(Pieces, Pairs, Sides);
    // A piece laid across several neighbours (a carpet over floor tiles, a sticker over two panels) lies inside their
    // faces together though inside none alone, and went back under them: a face's overlaps are summed over its pairs.
    TMap<FMcpFaceKey, double> Covered;
    for (int32 Index = 0; Index < Pairs.Num(); ++Index)
    {
        if (Sides[Index] != INDEX_NONE)
        {
            Covered.FindOrAdd(FMcpFaceKey(McpMover(Pieces, Pairs[Index], Pairs[Index].Piece), Sides[Index])) +=
                Pairs[Index].OverlapU * Pairs[Index].OverlapV;
        }
    }
    TMap<int32, TArray<int32>> ActorPieces;
    for (int32 Piece = 0; Piece < Pieces.Num(); ++Piece)
    {
        ActorPieces.FindOrAdd(Pieces[Piece].Owner).Add(Piece);
    }
    for (int32 Index = 0; Index < Pairs.Num(); ++Index)
    {
        const FMcpCoplanarPair& Pair = Pairs[Index];
        if (!McpInOnePlane(Pieces, Settle.Offsets, Pair.Piece, Pair.Face, Pair.Other, Pair.OtherFace))
        {
            continue; // a move made for an earlier pair parted it too
        }
        // The smaller piece comes forward when it lies wholly inside the other face and goes back when it is only
        // partly inside; when the colouring keeps it, the other piece moves the opposite way instead.
        const FMcpFaceKey Mine(McpMover(Pieces, Pair, Pair.Piece), Sides[Index]);
        const FMcpFaceKey Theirs(McpMover(Pieces, Pair, Pair.Other), Sides[Index]);
        const double Overlap = Sides[Index] != INDEX_NONE ? Covered.FindRef(Mine) : Pair.OverlapU * Pair.OverlapV;
        const bool bApplied = Pair.FaceArea > 0.0 && Overlap >= 0.95 * Pair.FaceArea;
        double Shift = bApplied ? Distance : -Distance; // along the face's own direction
        int32 First = Pair.Piece;
        int32 Second = Pair.Other;
        if (Sides[Index] != INDEX_NONE && !Moves.FindRef(Mine) && Moves.FindRef(Theirs))
        {
            Swap(First, Second);
            Shift = -Shift;
        }
        const bool bParts = Pieces[Pair.Piece].Owner == Pieces[Pair.Other].Owner;
        const FMcpTouch Target = McpTouch(Pair.Piece, Pair.Face, Pair.Other, Pair.OtherFace);
        bool bKept = false;
        bool bParted = false;
        // The preferred move, then the other way, then the other piece both ways; then all four twice as far.
        for (int32 Try = 0; Try < 8 && !bKept; ++Try)
        {
            const int32 Piece = Try % 4 < 2 ? First : Second;
            if (bParts && !Pieces[Piece].PinnedWhy.IsEmpty())
            {
                continue;
            }
            const double Along = (Try % 4 < 2 ? Shift : -Shift) * (Try % 2 ? -1.0 : 1.0) * (Try < 4 ? 1.0 : 2.0);
            const FVector Delta = Pair.Normal * Along;
            TArray<int32> Members = bParts ? Pieces[Piece].Carried : ActorPieces.FindRef(Pieces[Piece].Owner);
            if (bParts)
            {
                Members.Add(Piece);
            }
            const TArray<FMcpTouch> Before = McpTouches(Pieces, Settle.Offsets, Members);
            for (const int32 Member : Members)
            {
                Settle.Offsets[Member] += Delta;
            }
            const TArray<FMcpTouch> After = McpTouches(Pieces, Settle.Offsets, Members);
            bParted |= !After.Contains(Target);
            bKept = !After.Contains(Target) &&
                    !After.ContainsByPredicate([&Before](const FMcpTouch& Touch) { return !Before.Contains(Touch); });
            if (bKept)
            {
                Settle.Moves.Add({Index, Piece, Delta});
                continue;
            }
            for (const int32 Member : Members)
            {
                Settle.Offsets[Member] -= Delta;
            }
        }
        if (!bKept)
        {
            Settle.Left.Emplace(Index, McpLeftWhy(Pieces, Pair, bParts, bParted, Distance));
        }
    }
    return Settle;
}
} // namespace McpCoplanar
