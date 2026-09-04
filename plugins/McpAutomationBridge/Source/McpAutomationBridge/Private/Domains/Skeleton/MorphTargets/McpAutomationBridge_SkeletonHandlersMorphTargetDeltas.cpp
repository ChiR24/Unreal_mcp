#include "Domains/Skeleton/MorphTargets/McpAutomationBridge_SkeletonHandlersMorphTargetDeltas.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshModel.h"

#include <initializer_list>

namespace McpSkeletonHandlers
{
namespace
{
const TCHAR* const AcceptedDeltaShapes =
    TEXT("Each deltas item must be {vertexIndex|index: number, positionDelta|delta|position: [x,y,z] or {x,y,z}, normalDelta|tangentDelta (optional): [x,y,z] or {x,y,z}}");

// Reads [x,y,z] or {x,y,z}; false when the value is neither.
bool ReadVector3f(const TSharedPtr<FJsonValue>& Value, FVector3f& Out)
{
    if (!Value.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
    if (Value->TryGetArray(Array) && Array && Array->Num() >= 3)
    {
        double Components[3] = {0.0, 0.0, 0.0};
        for (int32 Index = 0; Index < 3; ++Index)
        {
            if (!(*Array)[Index].IsValid() || !(*Array)[Index]->TryGetNumber(Components[Index]))
            {
                return false;
            }
        }
        Out = FVector3f(static_cast<float>(Components[0]), static_cast<float>(Components[1]), static_cast<float>(Components[2]));
        return true;
    }

    const TSharedPtr<FJsonObject>* Object = nullptr;
    if (Value->TryGetObject(Object) && Object && Object->IsValid())
    {
        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        const bool bHasX = (*Object)->TryGetNumberField(TEXT("x"), X);
        const bool bHasY = (*Object)->TryGetNumberField(TEXT("y"), Y);
        const bool bHasZ = (*Object)->TryGetNumberField(TEXT("z"), Z);
        if (!bHasX && !bHasY && !bHasZ)
        {
            return false;
        }
        Out = FVector3f(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
        return true;
    }
    return false;
}

// First present field among the aliases, or null.
TSharedPtr<FJsonValue> FindField(const FJsonObject& Item, std::initializer_list<const TCHAR*> Names)
{
    for (const TCHAR* Name : Names)
    {
        TSharedPtr<FJsonValue> Field = Item.TryGetField(Name);
        if (Field.IsValid())
        {
            return Field;
        }
    }
    return nullptr;
}

bool FailItem(int32 ItemIndex, const TCHAR* Problem, FString& OutError, FString& OutErrorCode)
{
    OutError = FString::Printf(TEXT("deltas[%d] %s. %s"), ItemIndex, Problem, AcceptedDeltaShapes);
    OutErrorCode = TEXT("INVALID_ARGUMENT");
    return false;
}
} // namespace

bool ParseMorphTargetDeltas(const TSharedPtr<FJsonObject>& Payload, TArray<FMorphTargetDelta>& OutDeltas, FString& OutError, FString& OutErrorCode)
{
    OutDeltas.Reset();
    const TArray<TSharedPtr<FJsonValue>>* DeltasArray = nullptr;
    if (!Payload.IsValid() || !Payload->TryGetArrayField(TEXT("deltas"), DeltasArray) || !DeltasArray)
    {
        OutError = FString::Printf(TEXT("deltas array is required. %s"), AcceptedDeltaShapes);
        OutErrorCode = TEXT("MISSING_PARAM");
        return false;
    }

    OutDeltas.Reserve(DeltasArray->Num());
    for (int32 ItemIndex = 0; ItemIndex < DeltasArray->Num(); ++ItemIndex)
    {
        const TSharedPtr<FJsonValue>& ItemValue = (*DeltasArray)[ItemIndex];
        const TSharedPtr<FJsonObject>* ItemObject = nullptr;
        if (!ItemValue.IsValid() || !ItemValue->TryGetObject(ItemObject) || !ItemObject || !ItemObject->IsValid())
        {
            return FailItem(ItemIndex, TEXT("is not an object"), OutError, OutErrorCode);
        }
        const FJsonObject& Item = **ItemObject;

        double VertexIndex = -1.0;
        const TSharedPtr<FJsonValue> IndexField = FindField(Item, {TEXT("vertexIndex"), TEXT("index")});
        if (!IndexField.IsValid() || !IndexField->TryGetNumber(VertexIndex) || VertexIndex < 0.0)
        {
            return FailItem(ItemIndex, TEXT("needs a non-negative vertexIndex (alias: index)"), OutError, OutErrorCode);
        }

        FMorphTargetDelta Delta;
        Delta.SourceIdx = static_cast<uint32>(VertexIndex);
        Delta.PositionDelta = FVector3f::ZeroVector;
        Delta.TangentZDelta = FVector3f::ZeroVector;
        if (!ReadVector3f(FindField(Item, {TEXT("positionDelta"), TEXT("delta"), TEXT("position")}), Delta.PositionDelta))
        {
            return FailItem(ItemIndex, TEXT("needs positionDelta (aliases: delta, position) as [x,y,z] or {x,y,z}"), OutError, OutErrorCode);
        }
        const TSharedPtr<FJsonValue> NormalField = FindField(Item, {TEXT("normalDelta"), TEXT("tangentDelta"), TEXT("normal")});
        if (NormalField.IsValid() && !ReadVector3f(NormalField, Delta.TangentZDelta))
        {
            return FailItem(ItemIndex, TEXT("has a normalDelta that is not [x,y,z] or {x,y,z}"), OutError, OutErrorCode);
        }
        OutDeltas.Add(Delta);
    }

    if (OutDeltas.Num() == 0)
    {
        OutError = FString::Printf(TEXT("deltas array is empty. %s"), AcceptedDeltaShapes);
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return false;
    }
    return true;
}

TArray<FSkelMeshSection> GetMorphTargetLodSections(USkeletalMesh* Mesh, int32 LODIndex)
{
    TArray<FSkelMeshSection> Sections;
    const FSkeletalMeshModel* Model = Mesh ? Mesh->GetImportedModel() : nullptr;
    if (Model && Model->LODModels.IsValidIndex(LODIndex))
    {
        Sections = Model->LODModels[LODIndex].Sections;
    }
    return Sections;
}
}
