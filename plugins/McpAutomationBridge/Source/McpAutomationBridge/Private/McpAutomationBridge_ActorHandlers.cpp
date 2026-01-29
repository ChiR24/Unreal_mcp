#include "McpAutomationBridgeSubsystem.h"
#include "Templates/SharedPointer.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpBridgeWebSocket.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/SceneComponent.h"
#include "Components/ActorComponent.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EngineUtils.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleControlActorBatchTransform(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (Payload.Get() == nullptr) return false;
  const TArray<TSharedPtr<FJsonValue>> *TransformsArray = nullptr;
  if (Payload.Get()->TryGetArrayField(TEXT("transforms"), TransformsArray) == false || !TransformsArray || TransformsArray->Num() == 0) {

    SendAutomationError(Socket, RequestId, TEXT("transforms array required (array of {actorName, location?, rotation?, scale?})"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UWorld *World = GetActiveWorld();
  if (!World) {
    SendAutomationError(Socket, RequestId, TEXT("No active world available"), TEXT("NO_WORLD"));
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> ResultsArray;
  int32 SuccessCount = 0;
  int32 FailCount = 0;

  for (const TSharedPtr<FJsonValue> &Entry : *TransformsArray) {
    if (!Entry.IsValid() || Entry->Type != EJson::Object) continue;
    
    const TSharedPtr<FJsonObject> &TransformSpec = Entry->AsObject();
    FString ActorName;
    TransformSpec->TryGetStringField(TEXT("actorName"), ActorName);
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    
    if (ActorName.IsEmpty()) {
      Result->SetBoolField(TEXT("success"), false);
      Result->SetStringField(TEXT("error"), TEXT("actorName required"));
      FailCount++;
      ResultsArray.Add(MakeShared<FJsonValueObject>(Result));
      continue;
    }

    AActor *Found = FindActorCached(FName(*ActorName));
    if (!Found) {
      Result->SetBoolField(TEXT("success"), false);
      Result->SetStringField(TEXT("error"), TEXT("Actor not found"));
      FailCount++;
      ResultsArray.Add(MakeShared<FJsonValueObject>(Result));
      continue;
    }

    Found->Modify();
    
    // Apply location if specified
    FVector NewLocation = Found->GetActorLocation();
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    if (TransformSpec->TryGetObjectField(TEXT("location"), LocObj) && LocObj && (*LocObj).IsValid()) {
      double X, Y, Z;
      if ((*LocObj)->TryGetNumberField(TEXT("x"), X)) NewLocation.X = X;
      if ((*LocObj)->TryGetNumberField(TEXT("y"), Y)) NewLocation.Y = Y;
      if ((*LocObj)->TryGetNumberField(TEXT("z"), Z)) NewLocation.Z = Z;
      Found->SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
    }
    
    // Apply rotation if specified
    FRotator NewRotation = Found->GetActorRotation();
    const TSharedPtr<FJsonObject> *RotObj = nullptr;
    if (TransformSpec->TryGetObjectField(TEXT("rotation"), RotObj) && RotObj && (*RotObj).IsValid()) {
      double Pitch, Yaw, Roll;
      if ((*RotObj)->TryGetNumberField(TEXT("pitch"), Pitch)) NewRotation.Pitch = Pitch;
      if ((*RotObj)->TryGetNumberField(TEXT("yaw"), Yaw)) NewRotation.Yaw = Yaw;
      if ((*RotObj)->TryGetNumberField(TEXT("roll"), Roll)) NewRotation.Roll = Roll;
      Found->SetActorRotation(NewRotation, ETeleportType::TeleportPhysics);
    }
    
    // Apply scale if specified
    FVector NewScale = Found->GetActorScale3D();
    const TSharedPtr<FJsonObject> *ScaleObj = nullptr;
    if (TransformSpec->TryGetObjectField(TEXT("scale"), ScaleObj) && ScaleObj && (*ScaleObj).IsValid()) {
      double X, Y, Z;
      if ((*ScaleObj)->TryGetNumberField(TEXT("x"), X)) NewScale.X = X;
      if ((*ScaleObj)->TryGetNumberField(TEXT("y"), Y)) NewScale.Y = Y;
      if ((*ScaleObj)->TryGetNumberField(TEXT("z"), Z)) NewScale.Z = Z;
      Found->SetActorScale3D(NewScale);
    }
    
    Found->MarkComponentsRenderStateDirty();
    Found->MarkPackageDirty();
    
    Result->SetBoolField(TEXT("success"), true);
    SuccessCount++;
    ResultsArray.Add(MakeShared<FJsonValueObject>(Result));
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetArrayField(TEXT("results"), ResultsArray);
  Data->SetNumberField(TEXT("successCount"), SuccessCount);
  Data->SetNumberField(TEXT("failCount"), FailCount);
  Data->SetNumberField(TEXT("totalCount"), TransformsArray->Num());
  
  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Batch transformed %d/%d actors"), SuccessCount, TransformsArray->Num());
  SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Batch transformed %d actors"), SuccessCount), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorCloneComponentHierarchy(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SourceActorName, TargetActorName;
  if (!Payload.IsValid()) return false;
  Payload->TryGetStringField(TEXT("sourceActor"), SourceActorName);
  Payload->TryGetStringField(TEXT("targetActor"), TargetActorName);
  
  if (SourceActorName.IsEmpty() || TargetActorName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("sourceActor and targetActor required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UWorld *World = GetActiveWorld();
  if (!World) {
    SendAutomationError(Socket, RequestId, TEXT("No active world available"), TEXT("NO_WORLD"));
    return true;
  }

  AActor *Source = FindActorCached(FName(*SourceActorName));
  AActor *Target = FindActorCached(FName(*TargetActorName));
  
  if (!Source) {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Source actor not found: %s"), *SourceActorName), TEXT("ACTOR_NOT_FOUND"));
    return true;
  }
  if (!Target) {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Target actor not found: %s"), *TargetActorName), TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  // Optional: filter by component name or class
  FString ComponentFilter;
  Payload->TryGetStringField(TEXT("componentFilter"), ComponentFilter);
  
  Target->Modify();
  
  TArray<TSharedPtr<FJsonValue>> ClonedComponents;
  
  for (UActorComponent *SourceComp : Source->GetComponents()) {
    if (!SourceComp) continue;
    
    // Skip if filter is set and doesn't match
    if (!ComponentFilter.IsEmpty() && 
        !SourceComp->GetName().Contains(ComponentFilter, ESearchCase::IgnoreCase) &&
        !SourceComp->GetClass()->GetName().Contains(ComponentFilter, ESearchCase::IgnoreCase)) {
      continue;
    }
    
    // Clone the component
    UClass *CompClass = SourceComp->GetClass();
    FName NewCompName = MakeUniqueObjectName(Target, CompClass, *SourceComp->GetName());
    UActorComponent *NewComp = NewObject<UActorComponent>(Target, CompClass, NewCompName, RF_Transactional);
    
    if (!NewComp) continue;
    
    // Copy properties from source to new component
    UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
    CopyParams.bDoDelta = false;
    UEngine::CopyPropertiesForUnrelatedObjects(SourceComp, NewComp, CopyParams);
    
    Target->AddInstanceComponent(NewComp);
    NewComp->OnComponentCreated();
    
    // Handle SceneComponent attachment
    if (USceneComponent *NewSceneComp = Cast<USceneComponent>(NewComp)) {
      if (Target->GetRootComponent() && !NewSceneComp->GetAttachParent()) {
        NewSceneComp->SetupAttachment(Target->GetRootComponent());
      }
      
      // Copy relative transform from source if it's also a scene component
      if (USceneComponent *SourceSceneComp = Cast<USceneComponent>(SourceComp)) {
        NewSceneComp->SetRelativeTransform(SourceSceneComp->GetRelativeTransform());
      }
    }
    
    NewComp->RegisterComponent();
    NewComp->MarkPackageDirty();
    
    TSharedPtr<FJsonObject> CompEntry = MakeShared<FJsonObject>();
    CompEntry->SetStringField(TEXT("name"), NewComp->GetName());
    CompEntry->SetStringField(TEXT("class"), CompClass->GetName());
    CompEntry->SetStringField(TEXT("sourceName"), SourceComp->GetName());
    ClonedComponents.Add(MakeShared<FJsonValueObject>(CompEntry));
  }
  
  Target->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("sourceActor"), Source->GetActorLabel());
  Data->SetStringField(TEXT("targetActor"), Target->GetActorLabel());
  Data->SetArrayField(TEXT("clonedComponents"), ClonedComponents);
  Data->SetNumberField(TEXT("count"), ClonedComponents.Num());
  
  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Cloned %d components from '%s' to '%s'"), 
         ClonedComponents.Num(), *Source->GetActorLabel(), *Target->GetActorLabel());
  SendAutomationResponse(Socket, RequestId, true, TEXT("Component hierarchy cloned"), Data);
  return true;
#else
  return false;
#endif
}
