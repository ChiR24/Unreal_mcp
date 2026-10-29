// control_actor rename with actorNames: several actors relabelled in one undo step, numbered by a {n} in newName
// ("Crate_{n}" -> Crate_01, Crate_02, ...) in the order given, from start (default 1) zero-padded to digits (default 2).
// Every name is found and every new label checked with the engine's own label test (FActorEditorUtils::ValidateActorName,
// what SetActorLabel itself checks, in every 5.x) before anything changes, so a refusal renames nothing; a newName without
// {n} for several actors is refused (they would all read the same in the Outliner). Labels only: renameObject stays a
// one-actor option. Actors already labelled as asked make no undo step.

#include "Domains/ControlActor/Relations/McpRelations.h"

#include "ActorEditorUtils.h"
#include "Dom/JsonObject.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "GameFramework/Actor.h"

namespace McpRelations
{
bool HandleRenameActors(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                        TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find)
{
    const auto Refuse = [&](const FString& Message, const TCHAR* Code)
    {
        Bridge->SendAutomationError(Socket, RequestId, Message + TEXT(" Nothing was renamed."), Code);
        return true;
    };
    if (GetJsonBoolField(Payload, TEXT("renameObject"), false))
    {
        return Refuse(TEXT("renameObject renames one actor (actorName); with actorNames only labels change."), TEXT("INVALID_ARGUMENT"));
    }
    const FString Pattern = GetJsonStringField(Payload, TEXT("newName")).TrimStartAndEnd();
    const int32 Start = FMath::Max(0, GetJsonIntField(Payload, TEXT("start"), 1));
    const int32 Digits = FMath::Clamp(GetJsonIntField(Payload, TEXT("digits"), 2), 1, 6);
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
    if (Pattern.IsEmpty() || (Actors.Num() > 1 && !Pattern.Contains(TEXT("{n}"))))
    {
        return Refuse(TEXT("newName needs {n} where the number goes when several actors are renamed (Crate_{n})."), TEXT("INVALID_ARGUMENT"));
    }
    TArray<FString> Labels;
    TArray<UObject*> Changing;
    for (int32 Index = 0; Index < Actors.Num(); ++Index)
    {
        FString Number = FString::FromInt(Start + Index);
        while (Number.Len() < Digits)
        {
            Number = TEXT("0") + Number;
        }
        Labels.Add(Pattern.Replace(TEXT("{n}"), *Number).TrimStartAndEnd());
        FText Why;
        if (!FActorEditorUtils::ValidateActorName(FText::FromString(Labels.Last()), Why))
        {
            return Refuse(FString::Printf(TEXT("The editor refuses the label '%s': %s."), *Labels.Last(), *Why.ToString()), TEXT("INVALID_NAME"));
        }
        if (!Actors[Index]->GetActorLabel().Equals(Labels.Last(), ESearchCase::CaseSensitive))
        {
            Changing.Add(Actors[Index]);
        }
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<FString> OldLabels;
    for (const AActor* Actor : Actors)
    {
        OldLabels.Add(Actor->GetActorLabel());
    }
    // The undo step covers only actors whose label changes: one already as asked would make a step that changes nothing.
    if (Changing.Num() > 0)
    {
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Rename Actors")), EMcpMutationDurability::EditorStateOnly, Changing);
        for (int32 Index = 0; Index < Actors.Num(); ++Index)
        {
            Actors[Index]->SetActorLabel(Labels[Index]);
        }
        Transaction.DescribeInto(Result);
    }
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 Index = 0; Index < Actors.Num(); ++Index)
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("actorName"), McpActorRef(Actors[Index]));
        Row->SetStringField(TEXT("oldLabel"), OldLabels[Index]);
        Row->SetStringField(TEXT("label"), Actors[Index]->GetActorLabel());
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Result->SetArrayField(TEXT("renamed"), Rows);
    Bridge->SendAutomationResponse(Socket, RequestId, true,
        Changing.Num() > 0 ? FString::Printf(TEXT("Relabelled %d of %d actor(s)."), Changing.Num(), Actors.Num())
                           : FString::Printf(TEXT("Nothing changed: the %d actor(s) already carry those labels."), Actors.Num()),
        Result);
    return true;
}
} // namespace McpRelations
