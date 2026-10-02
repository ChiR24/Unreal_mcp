#pragma once

#include "CoreMinimal.h"
#include "Domains/Geometry/Primitives/McpAutomationBridge_GeometrySdfField.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

// create_sdf shape copies. Both eyes, a row of stitches or a ring of rivets used to mean listing
// every copy by hand (or computing the list outside the editor); a shape now says it once with
// repeat (a row or a ring) and mirror (reflected across the mesh's own planes).
namespace McpGeometrySdf
{
inline FVector3d ReadCopyVec(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
{
    const TSharedPtr<FJsonObject>* V = nullptr;
    FVector3d Out = FVector3d::ZeroVector;
    if (Obj->TryGetObjectField(Field, V) && V && V->IsValid())
    {
        (*V)->TryGetNumberField(TEXT("x"), Out.X);
        (*V)->TryGetNumberField(TEXT("y"), Out.Y);
        (*V)->TryGetNumberField(TEXT("z"), Out.Z);
    }
    return Out;
}

// repeat {count, offset}: a row, each copy offset further. repeat {count, axis, angle, pivot}: a
// ring turned about the axis through pivot; a full 360 spaces the copies evenly, an arc puts the
// last copy at its end. count includes the shape itself.
inline bool ExpandRepeat(const TSharedPtr<FJsonObject>& Obj, const FShape& S, TArray<FShape>& Out, FString& Error)
{
    const TSharedPtr<FJsonObject>* Repeat = nullptr;
    if (!Obj->TryGetObjectField(TEXT("repeat"), Repeat) || !Repeat || !Repeat->IsValid())
    {
        Out.Add(S);
        return true;
    }
    const int32 Count = GetJsonIntField(*Repeat, TEXT("count"), 0);
    if (Count < 2 || Count > 256)
    {
        Error = TEXT("repeat.count must be 2 to 256 (the shape itself included).");
        return false;
    }
    const FString Axis = GetJsonStringField(*Repeat, TEXT("axis")).ToLower();
    if (Axis.IsEmpty())
    {
        const FVector3d Offset = ReadCopyVec(*Repeat, TEXT("offset"));
        if (Offset.IsNearlyZero())
        {
            Error = TEXT("repeat needs offset {x,y,z} for a row, or axis x|y|z for a ring.");
            return false;
        }
        for (int32 I = 0; I < Count; ++I)
        {
            FShape Copy = S;
            Copy.Frame.AddToTranslation(Offset * I);
            Out.Add(Copy);
        }
        return true;
    }
    const int32 AxisIndex = Axis == TEXT("x") ? 0 : Axis == TEXT("y") ? 1 : Axis == TEXT("z") ? 2 : -1;
    if (AxisIndex < 0)
    {
        Error = TEXT("repeat.axis must be x, y or z.");
        return false;
    }
    FVector3d AxisVector = FVector3d::ZeroVector;
    AxisVector[AxisIndex] = 1.0;
    const double Angle = GetJsonNumberField(*Repeat, TEXT("angle"), 360.0);
    const FVector3d Pivot = ReadCopyVec(*Repeat, TEXT("pivot"));
    const double Step = FMath::IsNearlyEqual(FMath::Abs(Angle), 360.0) ? Angle / Count : Angle / (Count - 1);
    for (int32 I = 0; I < Count; ++I)
    {
        const FQuat Turn(AxisVector, FMath::DegreesToRadians(Step * I));
        FShape Copy = S;
        Copy.Frame.SetLocation(Pivot + Turn.RotateVector(S.Frame.GetLocation() - Pivot));
        Copy.Frame.SetRotation(Turn * S.Frame.GetRotation());
        Out.Add(Copy);
    }
    return true;
}

// The shape reflected across the mesh's plane normal to Axis. A reflection is not a rotation, but
// every shape type is symmetric about its own local X, so flipping that axis as well leaves a
// proper rotation that draws the mirrored shape (row vectors: Flip * Rot * Mirror).
inline FShape Reflect(const FShape& S, int32 Axis)
{
    FVector MirrorScale(1.0);
    MirrorScale[Axis] = -1.0;
    const FMatrix Rotated = FScaleMatrix(FVector(-1.0, 1.0, 1.0)) * FQuatRotationMatrix(S.Frame.GetRotation()) *
                            FScaleMatrix(MirrorScale);
    FVector Location = S.Frame.GetLocation();
    Location[Axis] = -Location[Axis];
    FShape Out = S;
    Out.Frame = FTransform(Rotated.ToQuat(), Location);
    return Out;
}

// mirror ["x", "y", "z"]: each listed axis doubles the copies so far, so ["x","y"] gives four.
inline bool ExpandMirror(const TSharedPtr<FJsonObject>& Obj, TArray<FShape>& Copies, FString& Error)
{
    const TArray<TSharedPtr<FJsonValue>>* Axes = nullptr;
    if (!Obj->TryGetArrayField(TEXT("mirror"), Axes) || !Axes)
    {
        return true;
    }
    uint8 Seen = 0;
    for (const TSharedPtr<FJsonValue>& Value : *Axes)
    {
        const FString Axis = Value.IsValid() ? Value->AsString().ToLower() : FString();
        const int32 AxisIndex = Axis == TEXT("x") ? 0 : Axis == TEXT("y") ? 1 : Axis == TEXT("z") ? 2 : -1;
        if (AxisIndex < 0 || (Seen & (1 << AxisIndex)))
        {
            Error = TEXT("mirror lists each of x, y and z at most once.");
            return false;
        }
        Seen |= 1 << AxisIndex;
        const int32 Existing = Copies.Num();
        for (int32 I = 0; I < Existing; ++I)
        {
            Copies.Add(Reflect(Copies[I], AxisIndex));
        }
    }
    return true;
}
} // namespace McpGeometrySdf
