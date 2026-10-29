// control_actor randomize_transforms: placed actors given seeded random turns, tilts, sizes and nudges around where
// they stand, as scattered rocks, trees and props are dressed so that no two look set by hand: yaw turned by up to yaw
// degrees either way, pitch and roll by up to tilt, scale multiplied by a factor between scale[0] and scale[1], and the
// location moved by up to offset cm along X and Y. Each actor draws its six numbers in a fixed order, so the same seed
// and the same actors in the same order give the same result, and a range left at 0 changes nothing else. Every name is
// found before anything changes; one undo step.

#include "Domains/ControlActor/Relations/McpRelations.h"

#include "Dom/JsonObject.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"

namespace McpRelations
{
bool HandleRandomizeTransforms(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                               TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find)
{
    const auto Refuse = [&](const FString& Message, const TCHAR* Code)
    {
        Bridge->SendAutomationError(Socket, RequestId, Message + TEXT(" Nothing was changed."), Code);
        return true;
    };
    const double Yaw = FMath::Clamp(GetJsonNumberField(Payload, TEXT("yaw"), 0.0), 0.0, 180.0);
    const double Tilt = FMath::Clamp(GetJsonNumberField(Payload, TEXT("tilt"), 0.0), 0.0, 90.0);
    const double Offset = FMath::Max(0.0, GetJsonNumberField(Payload, TEXT("offset"), 0.0));
    double ScaleMin = 1.0;
    double ScaleMax = 1.0;
    const TArray<TSharedPtr<FJsonValue>>* Scale = nullptr;
    if (Payload->TryGetArrayField(TEXT("scale"), Scale) && Scale->Num() == 2)
    {
        ScaleMin = (*Scale)[0]->AsNumber();
        ScaleMax = (*Scale)[1]->AsNumber();
    }
    if (ScaleMin <= 0.0 || ScaleMax < ScaleMin)
    {
        return Refuse(TEXT("scale is [min, max] with 0 < min <= max."), TEXT("INVALID_ARGUMENT"));
    }
    if (Yaw == 0.0 && Tilt == 0.0 && Offset == 0.0 && ScaleMin == 1.0 && ScaleMax == 1.0)
    {
        return Refuse(TEXT("Nothing to randomize: give yaw, tilt, scale or offset."), TEXT("INVALID_ARGUMENT"));
    }
    TArray<AActor*> Actors;
    for (const FString& Name : McpHandlerUtils::GetStringArrayField(Payload, TEXT("actorNames")))
    {
        AActor* Actor = Find(Name);
        if (!Actor)
        {
            return Refuse(FString::Printf(TEXT("No actor is labelled or named %s."), *Name), TEXT("ACTOR_NOT_FOUND"));
        }
        Actors.AddUnique(Actor);
    }
    FRandomStream Stream(GetJsonIntField(Payload, TEXT("seed"), 1));
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> Rows;
    {
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Randomize Transforms")), EMcpMutationDurability::EditorStateOnly,
            TArray<UObject*>(Actors));
        for (AActor* Actor : Actors)
        {
            // Six draws per actor in a fixed order: a range at 0 still draws, so it moves no other number.
            const double TurnYaw = Stream.FRandRange(-Yaw, Yaw);
            const double TurnPitch = Stream.FRandRange(-Tilt, Tilt);
            const double TurnRoll = Stream.FRandRange(-Tilt, Tilt);
            const double Factor = Stream.FRandRange(ScaleMin, ScaleMax);
            const FVector Nudge(Stream.FRandRange(-Offset, Offset), Stream.FRandRange(-Offset, Offset), 0.0);
            Actor->Modify();
            const FRotator Rotation = Actor->GetActorRotation() + FRotator(TurnPitch, TurnYaw, TurnRoll);
            Actor->SetActorLocationAndRotation(Actor->GetActorLocation() + Nudge, Rotation.GetNormalized(), false, nullptr, ETeleportType::TeleportPhysics);
            Actor->SetActorScale3D(Actor->GetActorScale3D() * Factor);
            Actor->PostEditMove(true);
            TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("actor"), Actor->GetActorLabel());
            Row->SetArrayField(TEXT("location"), McpHandlerUtils::VectorToJsonArray(Actor->GetActorLocation()));
            Row->SetArrayField(TEXT("rotation"), McpHandlerUtils::RotatorToJsonArray(Actor->GetActorRotation()));
            Row->SetArrayField(TEXT("scale"), McpHandlerUtils::VectorToJsonArray(Actor->GetActorScale3D()));
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        Transaction.DescribeInto(Result);
    }
    Result->SetArrayField(TEXT("actors"), Rows);
    Bridge->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Randomized %d actor(s)."), Actors.Num()), Result);
    return true;
}
} // namespace McpRelations
