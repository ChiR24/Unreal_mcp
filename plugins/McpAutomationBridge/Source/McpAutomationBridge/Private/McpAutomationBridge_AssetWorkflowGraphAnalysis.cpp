// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Components/ActorComponent.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleFindByTag(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("find_by_tag"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("find_by_tag payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Tag;
  if (!Payload->TryGetStringField(TEXT("tag"), Tag) || Tag.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("tag field is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // CRITICAL: Validate path parameter for security even if not used for actor search
  // This prevents false negatives in security testing and follows defense-in-depth
  FString Path;
  if (Payload->TryGetStringField(TEXT("path"), Path) && !Path.IsEmpty()) {
    FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("Invalid path (traversal/security violation): %s"), *Path),
          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    // Path is valid - could be used for scoping asset search in future
  }

  FName TagName(*Tag);
  TArray<TSharedPtr<FJsonValue>> Results;
  int32 MaxResults = 100;
  Payload->TryGetNumberField(TEXT("maxResults"), MaxResults);
  MaxResults = FMath::Clamp(MaxResults, 1, 1000);

  bool bSearchActors = true;
  bool bSearchComponents = false;
  bool bSearchAssets = false;
  Payload->TryGetBoolField(TEXT("searchActors"), bSearchActors);
  Payload->TryGetBoolField(TEXT("searchComponents"), bSearchComponents);
  Payload->TryGetBoolField(TEXT("searchAssets"), bSearchAssets);

  // Search in world
  if (GEditor && bSearchActors) {
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (World) {
      for (TActorIterator<AActor> It(World); It && Results.Num() < MaxResults; ++It) {
        AActor *Actor = *It;
        if (Actor && Actor->ActorHasTag(TagName)) {
          TSharedPtr<FJsonObject> ResultObj = McpHandlerUtils::CreateResultObject();
          ResultObj->SetStringField(TEXT("type"), TEXT("Actor"));
          ResultObj->SetStringField(TEXT("name"), Actor->GetName());
          ResultObj->SetStringField(TEXT("label"), Actor->GetActorLabel());
          ResultObj->SetStringField(TEXT("path"), Actor->GetPathName());
          ResultObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());

          const FVector Location = Actor->GetActorLocation();
          TSharedPtr<FJsonObject> LocObj = McpHandlerUtils::CreateResultObject();
          LocObj->SetNumberField(TEXT("x"), Location.X);
          LocObj->SetNumberField(TEXT("y"), Location.Y);
          LocObj->SetNumberField(TEXT("z"), Location.Z);
          ResultObj->SetObjectField(TEXT("location"), LocObj);

          Results.Add(MakeShared<FJsonValueObject>(ResultObj));
        }
      }
    }
  }

  // Search for components with tag
  if (bSearchComponents && GEditor && Results.Num() < MaxResults) {
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (World) {
      for (TActorIterator<AActor> It(World); It && Results.Num() < MaxResults; ++It) {
        AActor *Actor = *It;
        if (Actor) {
          TInlineComponentArray<UActorComponent*> Components;
          Actor->GetComponents(Components);
          for (UActorComponent *Component : Components) {
            if (Component && Component->ComponentHasTag(TagName)) {
              TSharedPtr<FJsonObject> ResultObj = McpHandlerUtils::CreateResultObject();
              ResultObj->SetStringField(TEXT("type"), TEXT("Component"));
              ResultObj->SetStringField(TEXT("name"), Component->GetName());
              ResultObj->SetStringField(TEXT("class"), Component->GetClass()->GetName());
              ResultObj->SetStringField(TEXT("owner"), Actor->GetName());
              ResultObj->SetStringField(TEXT("path"), Component->GetPathName());
              Results.Add(MakeShared<FJsonValueObject>(ResultObj));
            }
          }
        }
      }
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("tag"), Tag);
  Resp->SetNumberField(TEXT("count"), Results.Num());
  Resp->SetArrayField(TEXT("results"), Results);

  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Found %d objects with tag '%s'"), Results.Num(), *Tag),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("find_by_tag requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
