// =============================================================================
// McpAutomationBridge_PropertyHandlers.cpp
// =============================================================================
// Property and Container Manipulation Handlers for MCP Automation Bridge
//
// HANDLERS IMPLEMENTED:
// ---------------------
// Section 1: Property Access
//   - set_object_property            : Set property value on any UObject
//   - get_object_property            : Get property value from UObject
//   - set_property_by_path           : Set nested property via path
//   - get_property_by_path           : Get nested property via path
//
// Section 2: Array Operations
//   - array_append                   : Append element to array
//   - array_insert                   : Insert element at index
//   - array_remove                   : Remove element at index
//   - array_clear                    : Clear all elements
//   - array_get                      : Get element at index
//   - array_set                      : Set element at index
//   - array_length                   : Get array length
//
// Section 3: Map Operations
//   - map_set                        : Set key-value pair
//   - map_get                        : Get value by key
//   - map_remove                     : Remove by key
//   - map_has                        : Check key existence
//   - map_keys                       : Get all keys
//   - map_clear                      : Clear all entries
//
// Section 4: Set Operations
//   - set_add                        : Add element to set
//   - set_remove                     : Remove element from set
//   - set_contains                   : Check element existence
//   - set_clear                      : Clear all elements
//
// PAYLOAD/RESPONSE FORMATS:
// -------------------------
// set_object_property:
//   Payload: { "objectPath": string, "propertyName": string, "value": any }
//   Response: { "success": bool, "propertyName": string, "value": any }
//
// array_append:
//   Payload: { "objectPath": string, "propertyName": string, "value": any }
//   Response: { "success": bool, "arrayLength": int }
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0-5.7: All handlers supported
// - FProperty reflection APIs stable across versions
// - Container manipulation via standard UE reflection
//
// REFACTORING NOTES:
// ------------------
// - Uses McpHandlerUtils for standardized error responses
// - Uses McpPropertyReflection for property conversion
// - Consistent parameter validation patterns
// - Shared object resolution logic extracted to helpers
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first

#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "McpPropertyReflection.h"

#if WITH_EDITOR
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#endif

// =============================================================================
// Local Helper Functions
// =============================================================================
namespace
{
    /**
     * Add verification data to result based on object type.
     */
    void AddObjectVerification(TSharedPtr<FJsonObject>& Result, UObject* Object)
    {
#if WITH_EDITOR
        if (AActor* AsActor = Cast<AActor>(Object))
        {
            McpHandlerUtils::AddVerification(Result, AsActor);
        }
        else
        {
            McpHandlerUtils::AddVerification(Result, Object);
        }
#endif
    }
}

bool UMcpAutomationBridgeSubsystem::HandleSetObjectProperty(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("set_object_property"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("set_object_property")))
    return false;

  if (!Payload.IsValid())
  {
      SendAutomationError(RequestingSocket, RequestId,
          TEXT("set_object_property payload missing."),
          TEXT("INVALID_PAYLOAD"));
      return true;
  }

  // --- Parameter Validation (using McpHandlerUtils patterns) ---
  FString ObjectPath;
  FString ParamError;
  // objectPath is optional when blueprintPath is provided
  Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);
  ObjectPath.TrimStartAndEndInline();

  FString BlueprintPath;
  Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
  BlueprintPath.TrimStartAndEndInline();

  if (ObjectPath.IsEmpty() && BlueprintPath.IsEmpty())
  {
      SendAutomationError(RequestingSocket, RequestId,
          TEXT("Either objectPath or blueprintPath is required."),
          TEXT("INVALID_OBJECT"));
      return true;
  }

  FString PropertyName;
  if (!McpHandlerUtils::TryGetRequiredString(Payload, TEXT("propertyName"), PropertyName, ParamError))
  {
      SendAutomationError(RequestingSocket, RequestId, ParamError, TEXT("INVALID_PROPERTY"));
      return true;
  }

  const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
  if (!ValueField.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId,
          TEXT("set_object_property payload missing value field."),
          TEXT("INVALID_VALUE"));
      return true;
  }

  // --- Object Resolution ---
  UObject* RootObject = nullptr;

  // Priority 1: blueprintPath → load Blueprint → get CDO
  if (!BlueprintPath.IsEmpty())
  {
      FString NormalizedPath, LoadError;
      UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, NormalizedPath, LoadError);
      if (!Blueprint)
      {
          SendAutomationError(RequestingSocket, RequestId,
              FString::Printf(TEXT("Blueprint not found: %s (%s)"), *BlueprintPath, *LoadError),
              TEXT("BLUEPRINT_NOT_FOUND"));
          return true;
      }

      UClass* GeneratedClass = Blueprint->GeneratedClass;
      if (!GeneratedClass)
      {
          SendAutomationError(RequestingSocket, RequestId,
              TEXT("Blueprint has no GeneratedClass (not compiled?)"),
              TEXT("CDO_NOT_FOUND"));
          return true;
      }

      RootObject = GeneratedClass->GetDefaultObject();
      if (!RootObject)
      {
          SendAutomationError(RequestingSocket, RequestId,
              TEXT("Failed to get Class Default Object"),
              TEXT("CDO_NOT_FOUND"));
          return true;
      }

      ObjectPath = RootObject->GetPathName();
  }
  else
  {
      // Priority 2: objectPath → standard resolution
      FString ResolvedPath;
      RootObject = McpHandlerUtils::ResolveObjectFromPath(ObjectPath, &ResolvedPath);
      if (!RootObject)
      {
          SendAutomationError(RequestingSocket, RequestId,
              FString::Printf(TEXT("Unable to find object at path %s."), *ObjectPath),
              TEXT("OBJECT_NOT_FOUND"));
          return true;
      }
      if (!ResolvedPath.IsEmpty())
      {
          ObjectPath = ResolvedPath;
      }
  }

  // --- Special Actor Property Handling ---
  // Handle properties that require setter methods instead of direct property access
  // CDOs don't support runtime setters — changes won't persist to Blueprint defaults
  const bool bIsClassDefaultObject = RootObject->HasAnyFlags(RF_ClassDefaultObject);
  if (AActor *Actor = Cast<AActor>(RootObject))
  {
    if (bIsClassDefaultObject &&
        (PropertyName.Equals(TEXT("ActorLocation"), ESearchCase::IgnoreCase) ||
         PropertyName.Equals(TEXT("ActorRotation"), ESearchCase::IgnoreCase) ||
         PropertyName.Equals(TEXT("ActorScale"), ESearchCase::IgnoreCase) ||
         PropertyName.Equals(TEXT("ActorScale3D"), ESearchCase::IgnoreCase))) {
      SendAutomationError(RequestingSocket, RequestId,
          TEXT("Cannot modify runtime transform on a Blueprint CDO. Edit defaults on the root component or SCS template instead."),
          TEXT("CDO_TRANSFORM"));
      return true;
    }
    if (!bIsClassDefaultObject &&
        PropertyName.Equals(TEXT("ActorLocation"), ESearchCase::IgnoreCase)) {
          FVector NewLoc = FVector::ZeroVector;
          if (ValueField->Type == EJson::Object)
          {
              McpPropertyReflection::JsonToVector(ValueField->AsObject(), NewLoc);
          }
          else if (ValueField->Type == EJson::Array)
          {
              McpPropertyReflection::JsonArrayToVector(ValueField->AsArray(), NewLoc);
          }
          
          Actor->SetActorLocation(NewLoc);
          
          TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
          ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
          ResultPayload->SetBoolField(TEXT("saved"), true);
          ResultPayload->SetObjectField(TEXT("value"), McpPropertyReflection::VectorToJson(NewLoc));
          AddObjectVerification(ResultPayload, Actor);
          SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor location updated."), ResultPayload);
          return true;
      }
      
      // ActorRotation
      if (PropertyName.Equals(TEXT("ActorRotation"), ESearchCase::IgnoreCase))
      {
          FRotator NewRot = FRotator::ZeroRotator;
          if (ValueField->Type == EJson::Object)
          {
              McpPropertyReflection::JsonToRotator(ValueField->AsObject(), NewRot);
          }
          else if (ValueField->Type == EJson::Array)
          {
              McpPropertyReflection::JsonArrayToRotator(ValueField->AsArray(), NewRot);
          }
          
          Actor->SetActorRotation(NewRot);
          
          TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
          ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
          ResultPayload->SetBoolField(TEXT("saved"), true);
          ResultPayload->SetObjectField(TEXT("value"), McpPropertyReflection::RotatorToJson(NewRot));
          AddObjectVerification(ResultPayload, Actor);
          SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor rotation updated."), ResultPayload);
          return true;
      }
      
      // ActorScale / ActorScale3D
      if (PropertyName.Equals(TEXT("ActorScale"), ESearchCase::IgnoreCase) ||
          PropertyName.Equals(TEXT("ActorScale3D"), ESearchCase::IgnoreCase))
      {
          FVector NewScale = FVector::OneVector;
          if (ValueField->Type == EJson::Object)
          {
              McpPropertyReflection::JsonToVector(ValueField->AsObject(), NewScale);
          }
          else if (ValueField->Type == EJson::Array)
          {
              McpPropertyReflection::JsonArrayToVector(ValueField->AsArray(), NewScale);
          }
          
          Actor->SetActorScale3D(NewScale);
          
          TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
          ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
          ResultPayload->SetBoolField(TEXT("saved"), true);
          ResultPayload->SetObjectField(TEXT("value"), McpPropertyReflection::VectorToJson(NewScale));
          AddObjectVerification(ResultPayload, Actor);
          SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor scale updated."), ResultPayload);
          return true;
      }
      
      // bHidden (visibility) — skip runtime setter for CDOs, let generic path handle it
      if (!bIsClassDefaultObject && PropertyName.Equals(TEXT("bHidden"), ESearchCase::IgnoreCase))
      {
          bool bHidden = McpHandlerUtils::GetOptionalBool(Payload, TEXT("value"), false);
          if (ValueField->Type == EJson::Boolean)
              bHidden = ValueField->AsBool();
          else if (ValueField->Type == EJson::Number)
              bHidden = ValueField->AsNumber() != 0;
          
          Actor->SetActorHiddenInGame(bHidden);
          
          TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
          ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
          ResultPayload->SetBoolField(TEXT("saved"), true);
          ResultPayload->SetBoolField(TEXT("value"), bHidden);
          AddObjectVerification(ResultPayload, Actor);
          SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor visibility updated."), ResultPayload);
          return true;
      }
  }


  void* TargetContainer = nullptr;
  FProperty* Property = nullptr;

  if (PropertyName.Contains(TEXT("."))) {
      // Nested property path (e.g., "MyComponent.PropertyName")
      FString ResolveError;
      Property = ResolveNestedPropertyPath(RootObject, PropertyName, TargetContainer, ResolveError);
      if (!Property || !TargetContainer) {
          SendAutomationError(RequestingSocket, RequestId,
              FString::Printf(TEXT("Failed to resolve nested property path '%s': %s"), *PropertyName, *ResolveError),
              TEXT("PROPERTY_NOT_FOUND"));
          return true;
      }
  }
  else
  {
      // Simple property name - look it up directly
      TargetContainer = RootObject;
      Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
      if (!Property) {
          SendAutomationError(RequestingSocket, RequestId,
              FString::Printf(TEXT("Property '%s' not found on object '%s'."), *PropertyName, *ObjectPath),
              TEXT("PROPERTY_NOT_FOUND"));
          return true;
      }
  }

  // --- Apply Value ---
#if WITH_EDITOR
  RootObject->Modify();
#endif

  FString ConversionError;
  if (!ApplyJsonValueToProperty(TargetContainer, Property, ValueField, ConversionError))
  {
      SendAutomationError(RequestingSocket, RequestId, ConversionError, TEXT("PROPERTY_CONVERSION_FAILED"));
      return true;
  }

  // --- Mark Dirty (optional) ---
  const bool bMarkDirty = McpHandlerUtils::GetOptionalBool(Payload, TEXT("markDirty"), true);
  if (bMarkDirty)
  {
      RootObject->MarkPackageDirty();
  }

#if WITH_EDITOR
  RootObject->PostEditChange();
#endif

  // --- Build Response ---
  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetBoolField(TEXT("saved"), true);
  AddObjectVerification(ResultPayload, RootObject);

  // Include the updated value in response
  if (TSharedPtr<FJsonValue> CurrentValue = ExportPropertyToJsonValue(TargetContainer, Property))
  {
      ResultPayload->SetField(TEXT("value"), CurrentValue);
  }

  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Property value updated."), ResultPayload);
  return true;
}


bool UMcpAutomationBridgeSubsystem::HandleGetObjectProperty(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("get_object_property"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("get_object_property")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("get_object_property payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // --- Parameter Validation ---
  FString ObjectPath;
  Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);
  ObjectPath.TrimStartAndEndInline();

  FString BlueprintPath;
  Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
  BlueprintPath.TrimStartAndEndInline();

  if (ObjectPath.IsEmpty() && BlueprintPath.IsEmpty())
  {
      SendAutomationError(RequestingSocket, RequestId,
          TEXT("Either objectPath or blueprintPath is required."),
          TEXT("INVALID_OBJECT"));
      return true;
  }

  FString PropertyName;
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("get_object_property requires a non-empty propertyName."),
        TEXT("INVALID_PROPERTY"));
    return true;
  }

  // --- Object Resolution ---
  UObject* RootObject = nullptr;

  // Priority 1: blueprintPath → load Blueprint → get CDO
  if (!BlueprintPath.IsEmpty())
  {
      FString NormalizedPath, LoadError;
      UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, NormalizedPath, LoadError);
      if (!Blueprint)
      {
          SendAutomationError(RequestingSocket, RequestId,
              FString::Printf(TEXT("Blueprint not found: %s (%s)"), *BlueprintPath, *LoadError),
              TEXT("BLUEPRINT_NOT_FOUND"));
          return true;
      }

      UClass* GeneratedClass = Blueprint->GeneratedClass;
      if (!GeneratedClass)
      {
          SendAutomationError(RequestingSocket, RequestId,
              TEXT("Blueprint has no GeneratedClass (not compiled?)"),
              TEXT("CDO_NOT_FOUND"));
          return true;
      }

      RootObject = GeneratedClass->GetDefaultObject();
      if (!RootObject)
      {
          SendAutomationError(RequestingSocket, RequestId,
              TEXT("Failed to get Class Default Object"),
              TEXT("CDO_NOT_FOUND"));
          return true;
      }

      ObjectPath = RootObject->GetPathName();
  }
  else
  {
      // Priority 2: objectPath → standard resolution
      FString ResolvedPath;
      RootObject = McpHandlerUtils::ResolveObjectFromPath(ObjectPath, &ResolvedPath);
      if (!RootObject)
      {
          SendAutomationError(
              RequestingSocket, RequestId,
              FString::Printf(TEXT("Unable to find object at path %s."), *ObjectPath),
              TEXT("OBJECT_NOT_FOUND"));
          return true;
      }
      if (!ResolvedPath.IsEmpty())
      {
          ObjectPath = ResolvedPath;
      }
  }

  // Special handling for common AActor properties that are actually functions
  // or require setters — CDOs don't have valid runtime transform data
  const bool bIsCDO = RootObject->HasAnyFlags(RF_ClassDefaultObject);
  if (AActor *Actor = Cast<AActor>(RootObject)) {
    if (bIsCDO && (PropertyName.Equals(TEXT("ActorLocation"), ESearchCase::IgnoreCase) ||
                   PropertyName.Equals(TEXT("ActorRotation"), ESearchCase::IgnoreCase) ||
                   PropertyName.Equals(TEXT("ActorScale"), ESearchCase::IgnoreCase) ||
                   PropertyName.Equals(TEXT("ActorScale3D"), ESearchCase::IgnoreCase))) {
      SendAutomationError(RequestingSocket, RequestId,
          TEXT("Cannot read runtime transform from a Blueprint CDO. Query the SCS template or a spawned instance instead."),
          TEXT("CDO_TRANSFORM"));
      return true;
    }
    if (PropertyName.Equals(TEXT("ActorLocation"), ESearchCase::IgnoreCase)) {
      FVector Loc = Actor->GetActorLocation();
      TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
      ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
      McpHandlerUtils::AddVerification(ResultPayload, Actor);

      TSharedPtr<FJsonObject> ValObj = McpHandlerUtils::CreateResultObject();
      ValObj->SetNumberField(TEXT("x"), Loc.X);
      ValObj->SetNumberField(TEXT("y"), Loc.Y);
      ValObj->SetNumberField(TEXT("z"), Loc.Z);
      ResultPayload->SetField(TEXT("value"),
                              MakeShared<FJsonValueObject>(ValObj));

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Actor location retrieved."), ResultPayload,
                             FString());
      return true;
    } else if (PropertyName.Equals(TEXT("ActorRotation"),
                                   ESearchCase::IgnoreCase)) {
      FRotator Rot = Actor->GetActorRotation();
      TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
      ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
      McpHandlerUtils::AddVerification(ResultPayload, Actor);

      TSharedPtr<FJsonObject> ValObj = McpHandlerUtils::CreateResultObject();
      ValObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
      ValObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
      ValObj->SetNumberField(TEXT("roll"), Rot.Roll);
      ResultPayload->SetField(TEXT("value"),
                              MakeShared<FJsonValueObject>(ValObj));

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Actor rotation retrieved."), ResultPayload,
                             FString());
      return true;
    } else if (PropertyName.Equals(TEXT("ActorScale"),
                                   ESearchCase::IgnoreCase) ||
               PropertyName.Equals(TEXT("ActorScale3D"),
                                   ESearchCase::IgnoreCase)) {
      FVector Scale = Actor->GetActorScale3D();
      TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
      ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
      McpHandlerUtils::AddVerification(ResultPayload, Actor);

      TSharedPtr<FJsonObject> ValObj = McpHandlerUtils::CreateResultObject();
      ValObj->SetNumberField(TEXT("x"), Scale.X);
      ValObj->SetNumberField(TEXT("y"), Scale.Y);
      ValObj->SetNumberField(TEXT("z"), Scale.Z);
      ResultPayload->SetField(TEXT("value"),
                              MakeShared<FJsonValueObject>(ValObj));

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Actor scale retrieved."), ResultPayload,
                             FString());
      return true;
    }
  }

  // Support nested property paths (e.g., "MyComponent.PropertyName")
  McpHandlerUtils::FPropertyResolveResult PropResult = McpHandlerUtils::ResolveProperty(RootObject, PropertyName);
  if (!PropResult.IsValid())
  {
      SendAutomationError(
          RequestingSocket, RequestId,
          PropResult.Error,
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
  }

  const TSharedPtr<FJsonValue> CurrentValue =
      ExportPropertyToJsonValue(PropResult.Container, PropResult.Property);
  if (!CurrentValue.IsValid()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Unable to export property %s."), *PropertyName),
        TEXT("PROPERTY_EXPORT_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetField(TEXT("value"), CurrentValue);
  
  // Add verification based on object type
  if (AActor* AsActor = Cast<AActor>(RootObject)) {
    McpHandlerUtils::AddVerification(ResultPayload, AsActor);
  } else {
    McpHandlerUtils::AddVerification(ResultPayload, RootObject);
  }

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Property value retrieved."), ResultPayload,
                         FString());
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleArrayAppend(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("array_append"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("array_append")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_append payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString ObjectPath;
  if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_append requires objectPath."),
                        TEXT("INVALID_OBJECT"));
    return true;
  }

  FString PropertyName;
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_append requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
  if (!ValueField.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_append requires value field."),
                        TEXT("INVALID_VALUE"));
    return true;
  }

  UObject *RootObject = FindObject<UObject>(nullptr, *ObjectPath);
  if (!RootObject) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Unable to find object at path %s."), *ObjectPath),
        TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FProperty *Property = nullptr;

  if (PropertyName.Contains(TEXT("."))) {
    FString ResolveError;
    Property = ResolveNestedPropertyPath(RootObject, PropertyName,
                                         TargetContainer, ResolveError);
    if (!Property || !TargetContainer) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to resolve property path '%s': %s"),
                          *PropertyName, *ResolveError),
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  } else {
    TargetContainer = RootObject;
    Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Property %s not found."), *PropertyName),
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  }

  FArrayProperty *ArrayProp = CastField<FArrayProperty>(Property);
  if (!ArrayProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not an array."),
                        TEXT("NOT_AN_ARRAY"));
    return true;
  }

#if WITH_EDITOR
  RootObject->Modify();
#endif

  FScriptArrayHelper Helper(
      ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(TargetContainer));
  const int32 NewIndex = Helper.AddValue();
  void *ElemPtr = Helper.GetRawPtr(NewIndex);
  FProperty *Inner = ArrayProp->Inner;

  FString ConversionError;
  if (!ApplyJsonValueToProperty(TargetContainer, Inner, ValueField,
                                ConversionError)) {
    // Try direct assignment to element memory
    bool bSuccess = false;
    if (FStrProperty *StrInner = CastField<FStrProperty>(Inner)) {
      *reinterpret_cast<FString *>(ElemPtr) =
          (ValueField->Type == EJson::String)
              ? ValueField->AsString()
              : FString::Printf(TEXT("%g"), ValueField->AsNumber());
      bSuccess = true;
    } else if (FIntProperty *IntInner = CastField<FIntProperty>(Inner)) {
      *reinterpret_cast<int32 *>(ElemPtr) =
          (ValueField->Type == EJson::Number)
              ? (int32)ValueField->AsNumber()
              : FCString::Atoi(*ValueField->AsString());
      bSuccess = true;
    } else if (FFloatProperty *FloatInner = CastField<FFloatProperty>(Inner)) {
      *reinterpret_cast<float *>(ElemPtr) =
          (ValueField->Type == EJson::Number)
              ? (float)ValueField->AsNumber()
              : (float)FCString::Atod(*ValueField->AsString());
      bSuccess = true;
    } else if (FBoolProperty *BoolInner = CastField<FBoolProperty>(Inner)) {
      *reinterpret_cast<uint8 *>(ElemPtr) =
          (ValueField->Type == EJson::Boolean)
              ? (ValueField->AsBool() ? 1 : 0)
              : (ValueField->AsNumber() != 0.0 ? 1 : 0);
      bSuccess = true;
    }

    if (!bSuccess) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to append value: %s"), *ConversionError),
          TEXT("CONVERSION_FAILED"));
      return true;
    }
  }

#if WITH_EDITOR
  RootObject->PostEditChange();
#endif

TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetNumberField(TEXT("newIndex"), NewIndex);
  ResultPayload->SetNumberField(TEXT("newSize"), Helper.Num());
  
  // Add verification based on object type
  if (AActor* AsActor = Cast<AActor>(RootObject)) {
    McpHandlerUtils::AddVerification(ResultPayload, AsActor);
  } else {
    McpHandlerUtils::AddVerification(ResultPayload, RootObject);
  }

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Array element appended."), ResultPayload,
                         FString());
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleArrayRemove(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("array_remove"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("array_remove")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_remove requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_remove requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  int32 Index = -1;
  if (!Payload->TryGetNumberField(TEXT("index"), Index) || Index < 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_remove requires valid index."),
                        TEXT("INVALID_INDEX"));
    return true;
  }

  UObject *RootObject = FindObject<UObject>(nullptr, *ObjectPath);
  if (!RootObject) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
        TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FProperty *Property = nullptr;
  if (PropertyName.Contains(TEXT("."))) {
    FString ResolveError;
    Property = ResolveNestedPropertyPath(RootObject, PropertyName,
                                         TargetContainer, ResolveError);
    if (!Property || !TargetContainer) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to resolve property: %s"),
                          *ResolveError),
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  } else {
    TargetContainer = RootObject;
    Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Property not found."),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  }

  FArrayProperty *ArrayProp = CastField<FArrayProperty>(Property);
  if (!ArrayProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not an array."),
                        TEXT("NOT_AN_ARRAY"));
    return true;
  }

#if WITH_EDITOR
  RootObject->Modify();
#endif

  FScriptArrayHelper Helper(
      ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(TargetContainer));
  if (Index >= Helper.Num()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Index %d out of range (size: %d)"), Index,
                        Helper.Num()),
        TEXT("INDEX_OUT_OF_RANGE"));
    return true;
  }

  Helper.RemoveValues(Index, 1);

#if WITH_EDITOR
  RootObject->PostEditChange();
#endif

TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetNumberField(TEXT("removedIndex"), Index);
  ResultPayload->SetNumberField(TEXT("newSize"), Helper.Num());
  
  // Add verification based on object type
  if (AActor* AsActor = Cast<AActor>(RootObject)) {
    McpHandlerUtils::AddVerification(ResultPayload, AsActor);
  } else {
    McpHandlerUtils::AddVerification(ResultPayload, RootObject);
  }

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Array element removed."), ResultPayload,
                         FString());
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleArrayClear(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("array_clear"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("array_clear")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_clear requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_clear requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  UObject *RootObject = FindObject<UObject>(nullptr, *ObjectPath);
  if (!RootObject) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
        TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FProperty *Property = nullptr;
  if (PropertyName.Contains(TEXT("."))) {
    FString ResolveError;
    Property = ResolveNestedPropertyPath(RootObject, PropertyName,
                                         TargetContainer, ResolveError);
    if (!Property || !TargetContainer) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to resolve property: %s"),
                          *ResolveError),
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  } else {
    TargetContainer = RootObject;
    Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Property not found."),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  }

  FArrayProperty *ArrayProp = CastField<FArrayProperty>(Property);
  if (!ArrayProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not an array."),
                        TEXT("NOT_AN_ARRAY"));
    return true;
  }

#if WITH_EDITOR
  RootObject->Modify();
#endif

  FScriptArrayHelper Helper(
      ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(TargetContainer));
  const int32 PrevSize = Helper.Num();
  Helper.EmptyValues();

#if WITH_EDITOR
  RootObject->PostEditChange();
#endif

TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetNumberField(TEXT("previousSize"), PrevSize);
  ResultPayload->SetNumberField(TEXT("newSize"), 0);
  
  // Add verification based on object type
  if (AActor* AsActor = Cast<AActor>(RootObject)) {
    McpHandlerUtils::AddVerification(ResultPayload, AsActor);
  } else {
    McpHandlerUtils::AddVerification(ResultPayload, RootObject);
  }

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Array cleared."), ResultPayload, FString());
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleArrayInsert(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("array_insert"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("array_insert")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_insert requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_insert requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  int32 Index = -1;
  if (!Payload->TryGetNumberField(TEXT("index"), Index) || Index < 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_insert requires valid index."),
                        TEXT("INVALID_INDEX"));
    return true;
  }

  const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
  if (!ValueField.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_insert requires value field."),
                        TEXT("INVALID_VALUE"));
    return true;
  }

  UObject *RootObject = FindObject<UObject>(nullptr, *ObjectPath);
  if (!RootObject) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
        TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FProperty *Property = nullptr;
  if (PropertyName.Contains(TEXT("."))) {
    FString ResolveError;
    Property = ResolveNestedPropertyPath(RootObject, PropertyName,
                                         TargetContainer, ResolveError);
    if (!Property || !TargetContainer) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to resolve property: %s"),
                          *ResolveError),
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  } else {
    TargetContainer = RootObject;
    Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Property not found."),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  }

  FArrayProperty *ArrayProp = CastField<FArrayProperty>(Property);
  if (!ArrayProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not an array."),
                        TEXT("NOT_AN_ARRAY"));
    return true;
  }

#if WITH_EDITOR
  RootObject->Modify();
#endif

  FScriptArrayHelper Helper(
      ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(TargetContainer));
  if (Index > Helper.Num()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Index %d out of range (size: %d)"), Index,
                        Helper.Num()),
        TEXT("INDEX_OUT_OF_RANGE"));
    return true;
  }

  Helper.InsertValues(Index, 1);
  void *ElemPtr = Helper.GetRawPtr(Index);
  FProperty *Inner = ArrayProp->Inner;

  // Try to set the value using helper
  bool bSuccess = false;
  if (FStrProperty *StrInner = CastField<FStrProperty>(Inner)) {
    *reinterpret_cast<FString *>(ElemPtr) =
        (ValueField->Type == EJson::String)
            ? ValueField->AsString()
            : FString::Printf(TEXT("%g"), ValueField->AsNumber());
    bSuccess = true;
  } else if (FIntProperty *IntInner = CastField<FIntProperty>(Inner)) {
    *reinterpret_cast<int32 *>(ElemPtr) =
        (ValueField->Type == EJson::Number)
            ? (int32)ValueField->AsNumber()
            : FCString::Atoi(*ValueField->AsString());
    bSuccess = true;
  } else if (FFloatProperty *FloatInner = CastField<FFloatProperty>(Inner)) {
    *reinterpret_cast<float *>(ElemPtr) =
        (ValueField->Type == EJson::Number)
            ? (float)ValueField->AsNumber()
            : (float)FCString::Atod(*ValueField->AsString());
    bSuccess = true;
  } else if (FBoolProperty *BoolInner = CastField<FBoolProperty>(Inner)) {
    *reinterpret_cast<uint8 *>(ElemPtr) =
        (ValueField->Type == EJson::Boolean)
            ? (ValueField->AsBool() ? 1 : 0)
            : (ValueField->AsNumber() != 0.0 ? 1 : 0);
    bSuccess = true;
  }

  if (!bSuccess) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to insert value: unsupported type"),
                        TEXT("CONVERSION_FAILED"));
    return true;
  }

#if WITH_EDITOR
  RootObject->PostEditChange();
#endif

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetNumberField(TEXT("insertedAt"), Index);
  ResultPayload->SetNumberField(TEXT("newSize"), Helper.Num());

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Array element inserted."), ResultPayload,
                         FString());
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleArrayGetElement(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("array_get_element"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("array_get")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_get_element requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_get_element requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  int32 Index = -1;
  if (!Payload->TryGetNumberField(TEXT("index"), Index) || Index < 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_get_element requires valid index."),
                        TEXT("INVALID_INDEX"));
    return true;
  }

  UObject *RootObject = FindObject<UObject>(nullptr, *ObjectPath);
  if (!RootObject) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
        TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FProperty *Property = nullptr;
  if (PropertyName.Contains(TEXT("."))) {
    FString ResolveError;
    Property = ResolveNestedPropertyPath(RootObject, PropertyName,
                                         TargetContainer, ResolveError);
    if (!Property || !TargetContainer) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to resolve property: %s"),
                          *ResolveError),
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  } else {
    TargetContainer = RootObject;
    Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Property not found."),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  }

  FArrayProperty *ArrayProp = CastField<FArrayProperty>(Property);
  if (!ArrayProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not an array."),
                        TEXT("NOT_AN_ARRAY"));
    return true;
  }

  FScriptArrayHelper Helper(
      ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(TargetContainer));
  if (Index >= Helper.Num()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Index %d out of range (size: %d)"), Index,
                        Helper.Num()),
        TEXT("INDEX_OUT_OF_RANGE"));
    return true;
  }

  void *ElemPtr = Helper.GetRawPtr(Index);
  FProperty *Inner = ArrayProp->Inner;

  // Export the element value
  TSharedPtr<FJsonValue> ElemValue;
  if (FStrProperty *StrInner = CastField<FStrProperty>(Inner)) {
    ElemValue =
        MakeShared<FJsonValueString>(*reinterpret_cast<FString *>(ElemPtr));
  } else if (FIntProperty *IntInner = CastField<FIntProperty>(Inner)) {
    ElemValue = MakeShared<FJsonValueNumber>(
        (double)*reinterpret_cast<int32 *>(ElemPtr));
  } else if (FFloatProperty *FloatInner = CastField<FFloatProperty>(Inner)) {
    ElemValue = MakeShared<FJsonValueNumber>(
        (double)*reinterpret_cast<float *>(ElemPtr));
  } else if (FBoolProperty *BoolInner = CastField<FBoolProperty>(Inner)) {
    ElemValue = MakeShared<FJsonValueBoolean>(
        (*reinterpret_cast<uint8 *>(ElemPtr)) != 0);
  } else {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Unsupported array element type."),
                        TEXT("UNSUPPORTED_TYPE"));
    return true;
  }

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetNumberField(TEXT("index"), Index);
  ResultPayload->SetField(TEXT("value"), ElemValue);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Array element retrieved."), ResultPayload,
                         FString());
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleArraySetElement(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("array_set_element"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("array_set")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_set_element requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_set_element requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  int32 Index = -1;
  if (!Payload->TryGetNumberField(TEXT("index"), Index) || Index < 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_set_element requires valid index."),
                        TEXT("INVALID_INDEX"));
    return true;
  }

  const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
  if (!ValueField.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_set_element requires value field."),
                        TEXT("INVALID_VALUE"));
    return true;
  }

  UObject *RootObject = FindObject<UObject>(nullptr, *ObjectPath);
  if (!RootObject) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
        TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FProperty *Property = nullptr;
  if (PropertyName.Contains(TEXT("."))) {
    FString ResolveError;
    Property = ResolveNestedPropertyPath(RootObject, PropertyName,
                                         TargetContainer, ResolveError);
    if (!Property || !TargetContainer) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to resolve property: %s"),
                          *ResolveError),
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  } else {
    TargetContainer = RootObject;
    Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Property not found."),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  }

  FArrayProperty *ArrayProp = CastField<FArrayProperty>(Property);
  if (!ArrayProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not an array."),
                        TEXT("NOT_AN_ARRAY"));
    return true;
  }

#if WITH_EDITOR
  RootObject->Modify();
#endif

  FScriptArrayHelper Helper(
      ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(TargetContainer));
  if (Index >= Helper.Num()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Index %d out of range (size: %d)"), Index,
                        Helper.Num()),
        TEXT("INDEX_OUT_OF_RANGE"));
    return true;
  }

  void *ElemPtr = Helper.GetRawPtr(Index);
  FProperty *Inner = ArrayProp->Inner;

  // Set the element value
  bool bSuccess = false;
  if (FStrProperty *StrInner = CastField<FStrProperty>(Inner)) {
    *reinterpret_cast<FString *>(ElemPtr) =
        (ValueField->Type == EJson::String)
            ? ValueField->AsString()
            : FString::Printf(TEXT("%g"), ValueField->AsNumber());
    bSuccess = true;
  } else if (FIntProperty *IntInner = CastField<FIntProperty>(Inner)) {
    *reinterpret_cast<int32 *>(ElemPtr) =
        (ValueField->Type == EJson::Number)
            ? (int32)ValueField->AsNumber()
            : FCString::Atoi(*ValueField->AsString());
    bSuccess = true;
  } else if (FFloatProperty *FloatInner = CastField<FFloatProperty>(Inner)) {
    *reinterpret_cast<float *>(ElemPtr) =
        (ValueField->Type == EJson::Number)
            ? (float)ValueField->AsNumber()
            : (float)FCString::Atod(*ValueField->AsString());
    bSuccess = true;
  } else if (FBoolProperty *BoolInner = CastField<FBoolProperty>(Inner)) {
    *reinterpret_cast<uint8 *>(ElemPtr) =
        (ValueField->Type == EJson::Boolean)
            ? (ValueField->AsBool() ? 1 : 0)
            : (ValueField->AsNumber() != 0.0 ? 1 : 0);
    bSuccess = true;
  }

  if (!bSuccess) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Unsupported array element type."),
                        TEXT("UNSUPPORTED_TYPE"));
    return true;
  }

#if WITH_EDITOR
  RootObject->PostEditChange();
#endif

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetNumberField(TEXT("index"), Index);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Array element updated."), ResultPayload,
                         FString());
  return true;
}

// Map operation handlers
bool UMcpAutomationBridgeSubsystem::HandleMapSetValue(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("map_set_value"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("map_set")))
    return false;

  FString ObjectPath, PropertyName, Key;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_set_value requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_set_value requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("key"), Key)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_set_value requires key."),
                        TEXT("INVALID_KEY"));
    return true;
  }

  const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
  if (!ValueField.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_set_value requires value field."),
                        TEXT("INVALID_VALUE"));
    return true;
  }

  UObject *RootObject = FindObject<UObject>(nullptr, *ObjectPath);
  if (!RootObject) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
        TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FProperty *Property = nullptr;
  if (PropertyName.Contains(TEXT("."))) {
    FString ResolveError;
    Property = ResolveNestedPropertyPath(RootObject, PropertyName,
                                         TargetContainer, ResolveError);
    if (!Property || !TargetContainer) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to resolve property: %s"),
                          *ResolveError),
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  } else {
    TargetContainer = RootObject;
    Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Property not found."),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  }

  FMapProperty *MapProp = CastField<FMapProperty>(Property);
  if (!MapProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not a map."), TEXT("NOT_A_MAP"));
    return true;
  }

#if WITH_EDITOR
  RootObject->Modify();
#endif

  FScriptMapHelper Helper(
      MapProp, MapProp->ContainerPtrToValuePtr<void>(TargetContainer));
  FProperty *KeyProp = MapProp->KeyProp;
  FProperty *ValueProp = MapProp->ValueProp;

  // Create key and value in temporary memory
  void *TempKey =
      FMemory::Malloc(KeyProp->GetSize(), KeyProp->GetMinAlignment());
  void *TempValue =
      FMemory::Malloc(ValueProp->GetSize(), ValueProp->GetMinAlignment());
  KeyProp->InitializeValue(TempKey);
  ValueProp->InitializeValue(TempValue);

  bool bSuccess = false;
  // Set key
  if (FStrProperty *StrKey = CastField<FStrProperty>(KeyProp)) {
    *reinterpret_cast<FString *>(TempKey) = Key;
    bSuccess = true;
  } else if (FNameProperty *NameKey = CastField<FNameProperty>(KeyProp)) {
    *reinterpret_cast<FName *>(TempKey) = FName(*Key);
    bSuccess = true;
  } else if (FIntProperty *IntKey = CastField<FIntProperty>(KeyProp)) {
    *reinterpret_cast<int32 *>(TempKey) = FCString::Atoi(*Key);
    bSuccess = true;
  }

  if (!bSuccess) {
    KeyProp->DestroyValue(TempKey);
    ValueProp->DestroyValue(TempValue);
    FMemory::Free(TempKey);
    FMemory::Free(TempValue);
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Unsupported map key type."),
                        TEXT("UNSUPPORTED_KEY_TYPE"));
    return true;
  }

  // Set value
  bSuccess = false;
  if (FStrProperty *StrVal = CastField<FStrProperty>(ValueProp)) {
    *reinterpret_cast<FString *>(TempValue) =
        (ValueField->Type == EJson::String)
            ? ValueField->AsString()
            : FString::Printf(TEXT("%g"), ValueField->AsNumber());
    bSuccess = true;
  } else if (FIntProperty *IntVal = CastField<FIntProperty>(ValueProp)) {
    *reinterpret_cast<int32 *>(TempValue) =
        (ValueField->Type == EJson::Number)
            ? (int32)ValueField->AsNumber()
            : FCString::Atoi(*ValueField->AsString());
    bSuccess = true;
  } else if (FFloatProperty *FloatVal = CastField<FFloatProperty>(ValueProp)) {
    *reinterpret_cast<float *>(TempValue) =
        (ValueField->Type == EJson::Number)
            ? (float)ValueField->AsNumber()
            : (float)FCString::Atod(*ValueField->AsString());
    bSuccess = true;
  } else if (FBoolProperty *BoolVal = CastField<FBoolProperty>(ValueProp)) {
    *reinterpret_cast<uint8 *>(TempValue) =
        (ValueField->Type == EJson::Boolean)
            ? (ValueField->AsBool() ? 1 : 0)
            : (ValueField->AsNumber() != 0.0 ? 1 : 0);
    bSuccess = true;
  }

  if (!bSuccess) {
    KeyProp->DestroyValue(TempKey);
    ValueProp->DestroyValue(TempValue);
    FMemory::Free(TempKey);
    FMemory::Free(TempValue);
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Unsupported map value type."),
                        TEXT("UNSUPPORTED_VALUE_TYPE"));
    return true;
  }

  // Add to map
  Helper.AddPair(TempKey, TempValue);

  KeyProp->DestroyValue(TempKey);
  ValueProp->DestroyValue(TempValue);
  FMemory::Free(TempKey);
  FMemory::Free(TempValue);

#if WITH_EDITOR
  RootObject->PostEditChange();
#endif

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetStringField(TEXT("key"), Key);
  ResultPayload->SetNumberField(TEXT("mapSize"), Helper.Num());

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Map value set."), ResultPayload, FString());
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleMapGetValue(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("map_get_value"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("map_get")))
    return false;

  FString ObjectPath, PropertyName, Key;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_get_value requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_get_value requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("key"), Key)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_get_value requires key."),
                        TEXT("INVALID_KEY"));
    return true;
  }

  UObject *RootObject = FindObject<UObject>(nullptr, *ObjectPath);
  if (!RootObject) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
        TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FProperty *Property = nullptr;
  if (PropertyName.Contains(TEXT("."))) {
    FString ResolveError;
    Property = ResolveNestedPropertyPath(RootObject, PropertyName,
                                         TargetContainer, ResolveError);
    if (!Property || !TargetContainer) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to resolve property: %s"),
                          *ResolveError),
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  } else {
    TargetContainer = RootObject;
    Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Property not found."),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  }

  FMapProperty *MapProp = CastField<FMapProperty>(Property);
  if (!MapProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not a map."), TEXT("NOT_A_MAP"));
    return true;
  }

  FScriptMapHelper Helper(
      MapProp, MapProp->ContainerPtrToValuePtr<void>(TargetContainer));
  FProperty *KeyProp = MapProp->KeyProp;
  FProperty *ValueProp = MapProp->ValueProp;

  // Find the key
  for (int32 i = 0; i < Helper.Num(); ++i) {
    if (!Helper.IsValidIndex(i))
      continue;

    const uint8 *KeyPtr = Helper.GetKeyPtr(i);
    FString KeyStr;

    if (FStrProperty *StrKey = CastField<FStrProperty>(KeyProp)) {
      KeyStr = *reinterpret_cast<const FString *>(KeyPtr);
    } else if (FNameProperty *NameKey = CastField<FNameProperty>(KeyProp)) {
      KeyStr = reinterpret_cast<const FName *>(KeyPtr)->ToString();
    } else if (FIntProperty *IntKey = CastField<FIntProperty>(KeyProp)) {
      KeyStr = FString::FromInt(*reinterpret_cast<const int32 *>(KeyPtr));
    }

    if (KeyStr.Equals(Key)) {
      const uint8 *ValuePtr = Helper.GetValuePtr(i);
      TSharedPtr<FJsonValue> ValueJson;

      if (FStrProperty *StrVal = CastField<FStrProperty>(ValueProp)) {
        ValueJson = MakeShared<FJsonValueString>(
            *reinterpret_cast<const FString *>(ValuePtr));
      } else if (FIntProperty *IntVal = CastField<FIntProperty>(ValueProp)) {
        ValueJson = MakeShared<FJsonValueNumber>(
            (double)*reinterpret_cast<const int32 *>(ValuePtr));
      } else if (FFloatProperty *FloatVal =
                     CastField<FFloatProperty>(ValueProp)) {
        ValueJson = MakeShared<FJsonValueNumber>(
            (double)*reinterpret_cast<const float *>(ValuePtr));
      } else if (FBoolProperty *BoolVal = CastField<FBoolProperty>(ValueProp)) {
        ValueJson = MakeShared<FJsonValueBoolean>(
            (*reinterpret_cast<const uint8 *>(ValuePtr)) != 0);
      } else {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Unsupported map value type."),
                            TEXT("UNSUPPORTED_VALUE_TYPE"));
        return true;
      }

      TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
      ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
      ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
      ResultPayload->SetStringField(TEXT("key"), Key);
      ResultPayload->SetField(TEXT("value"), ValueJson);

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Map value retrieved."), ResultPayload,
                             FString());
      return true;
    }
  }

  SendAutomationError(RequestingSocket, RequestId,
                      FString::Printf(TEXT("Key '%s' not found in map."), *Key),
                      TEXT("KEY_NOT_FOUND"));
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleMapRemoveKey(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("map_remove_key"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("map_remove")))
    return false;

  FString ObjectPath, PropertyName, Key;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_remove_key requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_remove_key requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("key"), Key)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_remove_key requires key."),
                        TEXT("INVALID_KEY"));
    return true;
  }

  UObject *RootObject = FindObject<UObject>(nullptr, *ObjectPath);
  if (!RootObject) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
        TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FProperty *Property = nullptr;
  if (PropertyName.Contains(TEXT("."))) {
    FString ResolveError;
    Property = ResolveNestedPropertyPath(RootObject, PropertyName,
                                         TargetContainer, ResolveError);
    if (!Property || !TargetContainer) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to resolve property: %s"),
                          *ResolveError),
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  } else {
    TargetContainer = RootObject;
    Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Property not found."),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  }

  FMapProperty *MapProp = CastField<FMapProperty>(Property);
  if (!MapProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not a map."), TEXT("NOT_A_MAP"));
    return true;
  }

#if WITH_EDITOR
  RootObject->Modify();
#endif

  FScriptMapHelper Helper(
      MapProp, MapProp->ContainerPtrToValuePtr<void>(TargetContainer));
  FProperty *KeyProp = MapProp->KeyProp;

  // Find and remove the key
  for (int32 i = 0; i < Helper.Num(); ++i) {
    if (!Helper.IsValidIndex(i))
      continue;

    const uint8 *KeyPtr = Helper.GetKeyPtr(i);
    FString KeyStr;

    if (FStrProperty *StrKey = CastField<FStrProperty>(KeyProp)) {
      KeyStr = *reinterpret_cast<const FString *>(KeyPtr);
    } else if (FNameProperty *NameKey = CastField<FNameProperty>(KeyProp)) {
      KeyStr = reinterpret_cast<const FName *>(KeyPtr)->ToString();
    } else if (FIntProperty *IntKey = CastField<FIntProperty>(KeyProp)) {
      KeyStr = FString::FromInt(*reinterpret_cast<const int32 *>(KeyPtr));
    }

    if (KeyStr.Equals(Key)) {
      Helper.RemoveAt(i);

#if WITH_EDITOR
      RootObject->PostEditChange();
#endif

      TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
      ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
      ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
      ResultPayload->SetStringField(TEXT("key"), Key);
      ResultPayload->SetNumberField(TEXT("mapSize"), Helper.Num());

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Map key removed."), ResultPayload,
                             FString());
      return true;
    }
  }

  SendAutomationError(RequestingSocket, RequestId,
                      FString::Printf(TEXT("Key '%s' not found in map."), *Key),
                      TEXT("KEY_NOT_FOUND"));
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleMapHasKey(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("map_has_key"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("map_has")))
    return false;

  FString ObjectPath, PropertyName, Key;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_has_key requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_has_key requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("key"), Key)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_has_key requires key."), TEXT("INVALID_KEY"));
    return true;
  }

  UObject *RootObject = FindObject<UObject>(nullptr, *ObjectPath);
  if (!RootObject) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
        TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FProperty *Property = nullptr;
  if (PropertyName.Contains(TEXT("."))) {
    FString ResolveError;
    Property = ResolveNestedPropertyPath(RootObject, PropertyName,
                                         TargetContainer, ResolveError);
    if (!Property || !TargetContainer) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to resolve property: %s"),
                          *ResolveError),
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  } else {
    TargetContainer = RootObject;
    Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Property not found."),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  }

  FMapProperty *MapProp = CastField<FMapProperty>(Property);
  if (!MapProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not a map."), TEXT("NOT_A_MAP"));
    return true;
  }

  FScriptMapHelper Helper(
      MapProp, MapProp->ContainerPtrToValuePtr<void>(TargetContainer));
  FProperty *KeyProp = MapProp->KeyProp;

  // Check if key exists
  bool bHasKey = false;
  for (int32 i = 0; i < Helper.Num(); ++i) {
    if (!Helper.IsValidIndex(i))
      continue;

    const uint8 *KeyPtr = Helper.GetKeyPtr(i);
    FString KeyStr;

    if (FStrProperty *StrKey = CastField<FStrProperty>(KeyProp)) {
      KeyStr = *reinterpret_cast<const FString *>(KeyPtr);
    } else if (FNameProperty *NameKey = CastField<FNameProperty>(KeyProp)) {
      KeyStr = reinterpret_cast<const FName *>(KeyPtr)->ToString();
    } else if (FIntProperty *IntKey = CastField<FIntProperty>(KeyProp)) {
      KeyStr = FString::FromInt(*reinterpret_cast<const int32 *>(KeyPtr));
    }

    if (KeyStr.Equals(Key)) {
      bHasKey = true;
      break;
    }
  }

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetStringField(TEXT("key"), Key);
  ResultPayload->SetBoolField(TEXT("hasKey"), bHasKey);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         bHasKey ? TEXT("Key exists in map.")
                                 : TEXT("Key does not exist in map."),
                         ResultPayload, FString());
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleMapGetKeys(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("map_get_keys"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("map_get_keys")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_get_keys requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_get_keys requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  UObject *RootObject = FindObject<UObject>(nullptr, *ObjectPath);
  if (!RootObject) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
        TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FProperty *Property = nullptr;
  if (PropertyName.Contains(TEXT("."))) {
    FString ResolveError;
    Property = ResolveNestedPropertyPath(RootObject, PropertyName,
                                         TargetContainer, ResolveError);
    if (!Property || !TargetContainer) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to resolve property: %s"),
                          *ResolveError),
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  } else {
    TargetContainer = RootObject;
    Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Property not found."),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  }

  FMapProperty *MapProp = CastField<FMapProperty>(Property);
  if (!MapProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not a map."), TEXT("NOT_A_MAP"));
    return true;
  }

  FScriptMapHelper Helper(
      MapProp, MapProp->ContainerPtrToValuePtr<void>(TargetContainer));
  FProperty *KeyProp = MapProp->KeyProp;

  // Collect all keys
  TArray<TSharedPtr<FJsonValue>> KeysArray;
  for (int32 i = 0; i < Helper.Num(); ++i) {
    if (!Helper.IsValidIndex(i))
      continue;

    const uint8 *KeyPtr = Helper.GetKeyPtr(i);

    if (FStrProperty *StrKey = CastField<FStrProperty>(KeyProp)) {
      KeysArray.Add(MakeShared<FJsonValueString>(
          *reinterpret_cast<const FString *>(KeyPtr)));
    } else if (FNameProperty *NameKey = CastField<FNameProperty>(KeyProp)) {
      KeysArray.Add(MakeShared<FJsonValueString>(
          reinterpret_cast<const FName *>(KeyPtr)->ToString()));
    } else if (FIntProperty *IntKey = CastField<FIntProperty>(KeyProp)) {
      KeysArray.Add(MakeShared<FJsonValueNumber>(
          (double)*reinterpret_cast<const int32 *>(KeyPtr)));
    }
  }

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetArrayField(TEXT("keys"), KeysArray);
  ResultPayload->SetNumberField(TEXT("keyCount"), KeysArray.Num());

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Map keys retrieved."), ResultPayload, FString());
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleMapClear(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("map_clear"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("map_clear")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_clear requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_clear requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  UObject *RootObject = FindObject<UObject>(nullptr, *ObjectPath);
  if (!RootObject) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
        TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FProperty *Property = nullptr;
  if (PropertyName.Contains(TEXT("."))) {
    FString ResolveError;
    Property = ResolveNestedPropertyPath(RootObject, PropertyName,
                                         TargetContainer, ResolveError);
    if (!Property || !TargetContainer) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to resolve property: %s"),
                          *ResolveError),
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  } else {
    TargetContainer = RootObject;
    Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Property not found."),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  }

  FMapProperty *MapProp = CastField<FMapProperty>(Property);
  if (!MapProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not a map."), TEXT("NOT_A_MAP"));
    return true;
  }

#if WITH_EDITOR
  RootObject->Modify();
#endif

  FScriptMapHelper Helper(
      MapProp, MapProp->ContainerPtrToValuePtr<void>(TargetContainer));
  const int32 PrevSize = Helper.Num();
  Helper.EmptyValues();

#if WITH_EDITOR
  RootObject->PostEditChange();
#endif

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetNumberField(TEXT("previousSize"), PrevSize);
  ResultPayload->SetNumberField(TEXT("newSize"), 0);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Map cleared."), ResultPayload, FString());
  return true;
}

// Set operation handlers
bool UMcpAutomationBridgeSubsystem::HandleSetAdd(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("set_add"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("set_add")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_add requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_add requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
  if (!ValueField.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_add requires value field."),
                        TEXT("INVALID_VALUE"));
    return true;
  }

  UObject *RootObject = FindObject<UObject>(nullptr, *ObjectPath);
  if (!RootObject) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
        TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FProperty *Property = nullptr;
  if (PropertyName.Contains(TEXT("."))) {
    FString ResolveError;
    Property = ResolveNestedPropertyPath(RootObject, PropertyName,
                                         TargetContainer, ResolveError);
    if (!Property || !TargetContainer) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to resolve property: %s"),
                          *ResolveError),
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  } else {
    TargetContainer = RootObject;
    Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Property not found."),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  }

  FSetProperty *SetProp = CastField<FSetProperty>(Property);
  if (!SetProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not a set."), TEXT("NOT_A_SET"));
    return true;
  }

#if WITH_EDITOR
  RootObject->Modify();
#endif

  FScriptSetHelper Helper(
      SetProp, SetProp->ContainerPtrToValuePtr<void>(TargetContainer));
  FProperty *ElemProp = SetProp->ElementProp;

  // Create element in temporary memory
  void *TempElem =
      FMemory::Malloc(ElemProp->GetSize(), ElemProp->GetMinAlignment());
  ElemProp->InitializeValue(TempElem);

  bool bSuccess = false;
  if (FStrProperty *StrElem = CastField<FStrProperty>(ElemProp)) {
    *reinterpret_cast<FString *>(TempElem) =
        (ValueField->Type == EJson::String)
            ? ValueField->AsString()
            : FString::Printf(TEXT("%g"), ValueField->AsNumber());
    bSuccess = true;
  } else if (FIntProperty *IntElem = CastField<FIntProperty>(ElemProp)) {
    *reinterpret_cast<int32 *>(TempElem) =
        (ValueField->Type == EJson::Number)
            ? (int32)ValueField->AsNumber()
            : FCString::Atoi(*ValueField->AsString());
    bSuccess = true;
  } else if (FFloatProperty *FloatElem = CastField<FFloatProperty>(ElemProp)) {
    *reinterpret_cast<float *>(TempElem) =
        (ValueField->Type == EJson::Number)
            ? (float)ValueField->AsNumber()
            : (float)FCString::Atod(*ValueField->AsString());
    bSuccess = true;
  } else if (FNameProperty *NameElem = CastField<FNameProperty>(ElemProp)) {
    *reinterpret_cast<FName *>(TempElem) = (ValueField->Type == EJson::String)
                                               ? FName(*ValueField->AsString())
                                               : NAME_None;
    bSuccess = true;
  }

  if (!bSuccess) {
    ElemProp->DestroyValue(TempElem);
    FMemory::Free(TempElem);
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Unsupported set element type."),
                        TEXT("UNSUPPORTED_TYPE"));
    return true;
  }

  Helper.AddElement(TempElem);

  ElemProp->DestroyValue(TempElem);
  FMemory::Free(TempElem);

#if WITH_EDITOR
  RootObject->PostEditChange();
#endif

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetNumberField(TEXT("setSize"), Helper.Num());

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Element added to set."), ResultPayload,
                         FString());
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleSetRemove(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("set_remove"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("set_remove")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_remove requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_remove requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
  if (!ValueField.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_remove requires value field."),
                        TEXT("INVALID_VALUE"));
    return true;
  }

  UObject *RootObject = FindObject<UObject>(nullptr, *ObjectPath);
  if (!RootObject) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
        TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FProperty *Property = nullptr;
  if (PropertyName.Contains(TEXT("."))) {
    FString ResolveError;
    Property = ResolveNestedPropertyPath(RootObject, PropertyName,
                                         TargetContainer, ResolveError);
    if (!Property || !TargetContainer) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to resolve property: %s"),
                          *ResolveError),
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  } else {
    TargetContainer = RootObject;
    Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Property not found."),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  }

  FSetProperty *SetProp = CastField<FSetProperty>(Property);
  if (!SetProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not a set."), TEXT("NOT_A_SET"));
    return true;
  }

#if WITH_EDITOR
  RootObject->Modify();
#endif

  FScriptSetHelper Helper(
      SetProp, SetProp->ContainerPtrToValuePtr<void>(TargetContainer));
  FProperty *ElemProp = SetProp->ElementProp;

  // Find and remove the element
  for (int32 i = 0; i < Helper.Num(); ++i) {
    if (!Helper.IsValidIndex(i))
      continue;

    const uint8 *ElemPtr = Helper.GetElementPtr(i);
    bool bMatch = false;

    if (FStrProperty *StrElem = CastField<FStrProperty>(ElemProp)) {
      const FString &ElemValue = *reinterpret_cast<const FString *>(ElemPtr);
      const FString SearchValue =
          (ValueField->Type == EJson::String)
              ? ValueField->AsString()
              : FString::Printf(TEXT("%g"), ValueField->AsNumber());
      bMatch = ElemValue.Equals(SearchValue);
    } else if (FIntProperty *IntElem = CastField<FIntProperty>(ElemProp)) {
      const int32 ElemValue = *reinterpret_cast<const int32 *>(ElemPtr);
      const int32 SearchValue = (ValueField->Type == EJson::Number)
                                    ? (int32)ValueField->AsNumber()
                                    : FCString::Atoi(*ValueField->AsString());
      bMatch = (ElemValue == SearchValue);
    } else if (FFloatProperty *FloatElem =
                   CastField<FFloatProperty>(ElemProp)) {
      const float ElemValue = *reinterpret_cast<const float *>(ElemPtr);
      const float SearchValue =
          (ValueField->Type == EJson::Number)
              ? (float)ValueField->AsNumber()
              : (float)FCString::Atod(*ValueField->AsString());
      bMatch = FMath::IsNearlyEqual(ElemValue, SearchValue);
    }

    if (bMatch) {
      Helper.RemoveAt(i);

#if WITH_EDITOR
      RootObject->PostEditChange();
#endif

      TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
      ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
      ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
      ResultPayload->SetNumberField(TEXT("setSize"), Helper.Num());

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Element removed from set."), ResultPayload,
                             FString());
      return true;
    }
  }

  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Element not found in set."),
                      TEXT("ELEMENT_NOT_FOUND"));
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleSetContains(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("set_contains"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("set_contains")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_contains requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_contains requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
  if (!ValueField.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_contains requires value field."),
                        TEXT("INVALID_VALUE"));
    return true;
  }

  UObject *RootObject = FindObject<UObject>(nullptr, *ObjectPath);
  if (!RootObject) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
        TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FProperty *Property = nullptr;
  if (PropertyName.Contains(TEXT("."))) {
    FString ResolveError;
    Property = ResolveNestedPropertyPath(RootObject, PropertyName,
                                         TargetContainer, ResolveError);
    if (!Property || !TargetContainer) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to resolve property: %s"),
                          *ResolveError),
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  } else {
    TargetContainer = RootObject;
    Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Property not found."),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  }

  FSetProperty *SetProp = CastField<FSetProperty>(Property);
  if (!SetProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not a set."), TEXT("NOT_A_SET"));
    return true;
  }

  FScriptSetHelper Helper(
      SetProp, SetProp->ContainerPtrToValuePtr<void>(TargetContainer));
  FProperty *ElemProp = SetProp->ElementProp;

  // Check if element exists
  bool bContains = false;
  for (int32 i = 0; i < Helper.Num(); ++i) {
    if (!Helper.IsValidIndex(i))
      continue;

    const uint8 *ElemPtr = Helper.GetElementPtr(i);

    if (FStrProperty *StrElem = CastField<FStrProperty>(ElemProp)) {
      const FString &ElemValue = *reinterpret_cast<const FString *>(ElemPtr);
      const FString SearchValue =
          (ValueField->Type == EJson::String)
              ? ValueField->AsString()
              : FString::Printf(TEXT("%g"), ValueField->AsNumber());
      if (ElemValue.Equals(SearchValue)) {
        bContains = true;
        break;
      }
    } else if (FIntProperty *IntElem = CastField<FIntProperty>(ElemProp)) {
      const int32 ElemValue = *reinterpret_cast<const int32 *>(ElemPtr);
      const int32 SearchValue = (ValueField->Type == EJson::Number)
                                    ? (int32)ValueField->AsNumber()
                                    : FCString::Atoi(*ValueField->AsString());
      if (ElemValue == SearchValue) {
        bContains = true;
        break;
      }
    } else if (FFloatProperty *FloatElem =
                   CastField<FFloatProperty>(ElemProp)) {
      const float ElemValue = *reinterpret_cast<const float *>(ElemPtr);
      const float SearchValue =
          (ValueField->Type == EJson::Number)
              ? (float)ValueField->AsNumber()
              : (float)FCString::Atod(*ValueField->AsString());
      if (FMath::IsNearlyEqual(ElemValue, SearchValue)) {
        bContains = true;
        break;
      }
    }
  }

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetBoolField(TEXT("contains"), bContains);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         bContains ? TEXT("Element exists in set.")
                                   : TEXT("Element does not exist in set."),
                         ResultPayload, FString());
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleSetClear(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("set_clear"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("set_clear")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_clear requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_clear requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  UObject *RootObject = FindObject<UObject>(nullptr, *ObjectPath);
  if (!RootObject) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
        TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FProperty *Property = nullptr;
  if (PropertyName.Contains(TEXT("."))) {
    FString ResolveError;
    Property = ResolveNestedPropertyPath(RootObject, PropertyName,
                                         TargetContainer, ResolveError);
    if (!Property || !TargetContainer) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to resolve property: %s"),
                          *ResolveError),
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  } else {
    TargetContainer = RootObject;
    Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Property not found."),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  }

  FSetProperty *SetProp = CastField<FSetProperty>(Property);
  if (!SetProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not a set."), TEXT("NOT_A_SET"));
    return true;
  }

#if WITH_EDITOR
  RootObject->Modify();
#endif

  FScriptSetHelper Helper(
      SetProp, SetProp->ContainerPtrToValuePtr<void>(TargetContainer));
  const int32 PrevSize = Helper.Num();
  Helper.EmptyElements();

#if WITH_EDITOR
  RootObject->PostEditChange();
#endif

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetNumberField(TEXT("previousSize"), PrevSize);
  ResultPayload->SetNumberField(TEXT("newSize"), 0);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Set cleared."), ResultPayload, FString());
  return true;
}

// Asset dependency graph traversal
bool UMcpAutomationBridgeSubsystem::HandleGetAssetReferences(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("get_asset_references"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("get_asset_references")))
    return false;

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("get_asset_references payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
      AssetPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("get_asset_references requires assetPath."),
                        TEXT("INVALID_ASSET"));
    return true;
  }

  // Get asset registry
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  // Find the asset
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
#else
  // UE 5.0: GetAssetByObjectPath takes FName
  FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FName(*AssetPath));
#endif
  if (!AssetData.IsValid()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Asset not found: %s"), *AssetPath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  // Get dependencies (what this asset references)
  TArray<FAssetIdentifier> Dependencies;
  AssetRegistry.GetDependencies(FAssetIdentifier(AssetData.PackageName),
                                Dependencies);

  // Convert to JSON array
  TArray<TSharedPtr<FJsonValue>> ReferencesArray;
  for (const FAssetIdentifier &Dep : Dependencies) {
    TSharedPtr<FJsonObject> RefObj = McpHandlerUtils::CreateResultObject();
    RefObj->SetStringField(TEXT("packageName"), Dep.PackageName.ToString());
    if (!Dep.ObjectName.IsNone()) {
      RefObj->SetStringField(TEXT("objectName"), Dep.ObjectName.ToString());
    }
    ReferencesArray.Add(MakeShared<FJsonValueObject>(RefObj));
  }

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("assetPath"), AssetPath);
  ResultPayload->SetStringField(TEXT("packageName"),
                                AssetData.PackageName.ToString());
  ResultPayload->SetArrayField(TEXT("references"), ReferencesArray);
  ResultPayload->SetNumberField(TEXT("referenceCount"), ReferencesArray.Num());

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Asset references retrieved."), ResultPayload,
                         FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("get_asset_references requires editor build."),
                      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetAssetDependencies(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("get_asset_dependencies"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("get_asset_dependencies")))
    return false;

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("get_asset_dependencies payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
      AssetPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("get_asset_dependencies requires assetPath."),
                        TEXT("INVALID_ASSET"));
    return true;
  }

  // Get asset registry
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  // Find the asset
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
#else
  // UE 5.0: GetAssetByObjectPath takes FName
  FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FName(*AssetPath));
#endif
  if (!AssetData.IsValid()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Asset not found: %s"), *AssetPath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  // Get referencers (what references this asset)
  TArray<FAssetIdentifier> Referencers;
  AssetRegistry.GetReferencers(FAssetIdentifier(AssetData.PackageName),
                               Referencers);

  // Convert to JSON array
  TArray<TSharedPtr<FJsonValue>> DependenciesArray;
  for (const FAssetIdentifier &Ref : Referencers) {
    TSharedPtr<FJsonObject> DepObj = McpHandlerUtils::CreateResultObject();
    DepObj->SetStringField(TEXT("packageName"), Ref.PackageName.ToString());
    if (!Ref.ObjectName.IsNone()) {
      DepObj->SetStringField(TEXT("objectName"), Ref.ObjectName.ToString());
    }
    DependenciesArray.Add(MakeShared<FJsonValueObject>(DepObj));
  }

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("assetPath"), AssetPath);
  ResultPayload->SetStringField(TEXT("packageName"),
                                AssetData.PackageName.ToString());
  ResultPayload->SetArrayField(TEXT("dependencies"), DependenciesArray);
  ResultPayload->SetNumberField(TEXT("dependencyCount"),
                                DependenciesArray.Num());

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Asset dependencies retrieved."), ResultPayload,
                         FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("get_asset_dependencies requires editor build."),
                      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// =============================================================================
// inspect_cdo - Blueprint Class Default Object Inspection
// =============================================================================

#if WITH_EDITOR
namespace
{

// Builds a JSON summary for a single component template.
// Summary mode: name, class, source, transform, and key asset fields.
// Detailed mode or propertyNames filter: adds full/selective property export.
TSharedPtr<FJsonObject> BuildComponentSummary(
    UActorComponent* Template,
    const FString& DisplayName,
    const FString& Source,
    bool bDetailed,
    const TArray<FName>& PropertyFilter)
{
    TSharedPtr<FJsonObject> CompObj = McpHandlerUtils::CreateResultObject();
    CompObj->SetStringField(TEXT("name"), DisplayName);
    CompObj->SetStringField(TEXT("class"), Template->GetClass()->GetName());
    CompObj->SetStringField(TEXT("source"), Source);

    // Transform via existing repo helpers
    if (USceneComponent* SceneComp = Cast<USceneComponent>(Template))
    {
        TSharedPtr<FJsonObject> TransformObj = McpHandlerUtils::CreateResultObject();
        TransformObj->SetObjectField(TEXT("location"),
            McpHandlerUtils::VectorToJson(SceneComp->GetRelativeLocation()));
        TransformObj->SetObjectField(TEXT("rotation"),
            McpHandlerUtils::RotatorToJson(SceneComp->GetRelativeRotation()));
        TransformObj->SetObjectField(TEXT("scale"),
            McpHandlerUtils::VectorToJson(SceneComp->GetRelativeScale3D()));
        CompObj->SetObjectField(TEXT("transform"), TransformObj);
    }

    // Key asset fields for common mesh component types
    if (USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Template))
    {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        USkeletalMesh* Mesh = SkelComp->GetSkeletalMeshAsset();
#else
        USkeletalMesh* Mesh = SkelComp->SkeletalMesh;
#endif
        CompObj->SetStringField(TEXT("skeletalMesh"),
            Mesh ? Mesh->GetPathName() : TEXT("None"));
        CompObj->SetStringField(TEXT("animClass"),
            SkelComp->AnimClass ? SkelComp->AnimClass->GetPathName() : TEXT("None"));
    }

    if (UStaticMeshComponent* StaticComp = Cast<UStaticMeshComponent>(Template))
    {
        CompObj->SetStringField(TEXT("staticMesh"),
            StaticComp->GetStaticMesh()
                ? StaticComp->GetStaticMesh()->GetPathName()
                : TEXT("None"));
    }

    // Full/selective property export only when requested
    if (PropertyFilter.Num() > 0)
    {
        TSharedPtr<FJsonObject> Props =
            McpPropertyReflection::ExportPropertiesToJson(Template, PropertyFilter);
        if (Props.IsValid())
        {
            CompObj->SetObjectField(TEXT("properties"), Props);
        }
    }
    else if (bDetailed)
    {
        TSharedPtr<FJsonObject> Props =
            McpPropertyReflection::ExportObjectToJson(Template, false);
        if (Props.IsValid())
        {
            CompObj->SetObjectField(TEXT("properties"), Props);
        }
    }

    return CompObj;
}

// Builds a set of SCS variable names for source classification.
// Returns a map: variable name -> source label ("SCS" or "SCS_Inherited").
TMap<FString, FString> BuildScsSourceMap(UBlueprint* Blueprint)
{
    TMap<FString, FString> SourceMap;
    for (UBlueprint* Bp = Blueprint; Bp != nullptr;)
    {
        if (Bp->SimpleConstructionScript)
        {
            for (USCS_Node* Node : Bp->SimpleConstructionScript->GetAllNodes())
            {
                if (!Node) continue;
                const FString VarName = Node->GetVariableName().ToString();
                if (!SourceMap.Contains(VarName))
                {
                    SourceMap.Add(VarName,
                        (Bp == Blueprint) ? TEXT("SCS") : TEXT("SCS_Inherited"));
                }
            }
        }
        UClass* ParentClass = Bp->ParentClass;
        Bp = ParentClass ? Cast<UBlueprint>(ParentClass->ClassGeneratedBy) : nullptr;
    }
    return SourceMap;
}

// Finds a component by name: first on CDO (native), then SCS templates (BP-added).
UActorComponent* FindCdoComponent(
    UBlueprint* Blueprint,
    UObject* CDO,
    const FString& ComponentName)
{
    // Search native CDO components first (effective overrides)
    if (AActor* DefaultActor = Cast<AActor>(CDO))
    {
        TInlineComponentArray<UActorComponent*> Components;
        DefaultActor->GetComponents(Components);
        for (UActorComponent* Comp : Components)
        {
            if (Comp && Comp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
            {
                return Comp;
            }
        }
    }

    // Search SCS node templates (BP-added components)
    for (UBlueprint* Bp = Blueprint; Bp != nullptr;)
    {
        if (Bp->SimpleConstructionScript)
        {
            for (USCS_Node* Node : Bp->SimpleConstructionScript->GetAllNodes())
            {
                if (Node && Node->ComponentTemplate &&
                    Node->GetVariableName().ToString().Equals(ComponentName, ESearchCase::IgnoreCase))
                {
                    return Node->ComponentTemplate;
                }
            }
        }
        UClass* ParentClass = Bp->ParentClass;
        Bp = ParentClass ? Cast<UBlueprint>(ParentClass->ClassGeneratedBy) : nullptr;
    }
    return nullptr;
}

} // anonymous namespace
#endif // WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleInspectCdoAction(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("inspect_cdo: payload missing"),
                            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
    BlueprintPath.TrimStartAndEndInline();
    if (BlueprintPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("blueprintPath is required for inspect_cdo"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString NormalizedPath, LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(
        BlueprintPath, NormalizedPath, LoadError);
    if (!Blueprint)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Blueprint not found: %s (%s)"),
                                            *BlueprintPath, *LoadError),
                            TEXT("BLUEPRINT_NOT_FOUND"));
        return true;
    }

    UClass* GeneratedClass = Blueprint->GeneratedClass;
    if (!GeneratedClass)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Blueprint has no GeneratedClass (not compiled?)"),
                            TEXT("CDO_NOT_FOUND"));
        return true;
    }

    UObject* CDO = GeneratedClass->GetDefaultObject();
    if (!CDO)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Failed to get Class Default Object"),
                            TEXT("CDO_NOT_FOUND"));
        return true;
    }

    // Parse optional params
    FString ComponentNameFilter;
    Payload->TryGetStringField(TEXT("componentName"), ComponentNameFilter);
    ComponentNameFilter.TrimStartAndEndInline();
    bool bDetailed = false;
    Payload->TryGetBoolField(TEXT("detailed"), bDetailed);

    TArray<FName> PropertyNameFilter;
    const TArray<TSharedPtr<FJsonValue>>* PropNamesArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("propertyNames"), PropNamesArr) && PropNamesArr)
    {
        for (const auto& Val : *PropNamesArr)
        {
            FString S;
            if (Val->TryGetString(S))
            {
                PropertyNameFilter.Add(FName(*S));
            }
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("blueprintPath"), NormalizedPath);
    Resp->SetStringField(TEXT("className"), GeneratedClass->GetName());
    Resp->SetStringField(TEXT("classPath"), GeneratedClass->GetPathName());
    Resp->SetStringField(TEXT("parentClass"),
        GeneratedClass->GetSuperClass()
            ? GeneratedClass->GetSuperClass()->GetName()
            : TEXT("None"));

    // --- Component filter mode: single component dump ---
    if (!ComponentNameFilter.IsEmpty())
    {
        UActorComponent* FoundComp = FindCdoComponent(Blueprint, CDO, ComponentNameFilter);
        if (!FoundComp)
        {
            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Component not found: %s"),
                                                *ComponentNameFilter),
                                TEXT("COMPONENT_NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> CompJson;
        if (PropertyNameFilter.Num() > 0)
        {
            CompJson = McpPropertyReflection::ExportPropertiesToJson(
                FoundComp, PropertyNameFilter);
        }
        else
        {
            CompJson = McpPropertyReflection::ExportObjectToJson(
                FoundComp, false);
        }

        // Return both the lookup name and the internal object name
        Resp->SetStringField(TEXT("componentName"), ComponentNameFilter);
        Resp->SetStringField(TEXT("templateObjectName"), FoundComp->GetName());
        Resp->SetStringField(TEXT("componentClass"),
            FoundComp->GetClass()->GetName());
        if (CompJson.IsValid())
        {
            Resp->SetObjectField(TEXT("properties"), CompJson);
        }
        Resp->SetBoolField(TEXT("success"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("CDO component inspected"), Resp, FString());
        return true;
    }

    // --- CDO properties (only when detailed or propertyNames given) ---
    if (PropertyNameFilter.Num() > 0)
    {
        TSharedPtr<FJsonObject> CdoProps =
            McpPropertyReflection::ExportPropertiesToJson(CDO, PropertyNameFilter);
        if (CdoProps.IsValid())
        {
            Resp->SetObjectField(TEXT("cdoProperties"), CdoProps);
        }
    }
    else if (bDetailed)
    {
        TSharedPtr<FJsonObject> CdoProps =
            McpPropertyReflection::ExportObjectToJson(CDO, false);
        if (CdoProps.IsValid())
        {
            Resp->SetObjectField(TEXT("cdoProperties"), CdoProps);
        }
    }

    // --- Components: hybrid CDO + SCS ---
    // Native components (C++ constructor) live on the CDO with effective overrides.
    // SCS components (Blueprint-added) only exist as templates on SCS nodes.
    TArray<TSharedPtr<FJsonValue>> ComponentsArray;
    TSet<FString> SeenNames;
    TMap<FString, FString> ScsSourceMap = BuildScsSourceMap(Blueprint);

    // 1) Native CDO components (effective override values)
    if (AActor* DefaultActor = Cast<AActor>(CDO))
    {
        TInlineComponentArray<UActorComponent*> CdoComponents;
        DefaultActor->GetComponents(CdoComponents);
        for (UActorComponent* Comp : CdoComponents)
        {
            if (!Comp) continue;
            const FString CompName = Comp->GetName();
            SeenNames.Add(CompName);

            const FString Source = ScsSourceMap.Contains(CompName)
                ? TEXT("Native_Override") : TEXT("Native");

            ComponentsArray.Add(MakeShared<FJsonValueObject>(
                BuildComponentSummary(Comp, CompName, Source,
                                      bDetailed, PropertyNameFilter)));
        }
    }

    // 2) SCS components (Blueprint-added) from SCS node templates.
    //    Walk full parent chain for inherited BP components.
    for (UBlueprint* Bp = Blueprint; Bp != nullptr;)
    {
        if (Bp->SimpleConstructionScript)
        {
            for (USCS_Node* Node : Bp->SimpleConstructionScript->GetAllNodes())
            {
                if (!Node || !Node->ComponentTemplate) continue;
                const FString VarName = Node->GetVariableName().ToString();
                if (SeenNames.Contains(VarName)) continue;
                SeenNames.Add(VarName);

                const FString Source = (Bp == Blueprint)
                    ? TEXT("SCS") : TEXT("SCS_Inherited");

                TSharedPtr<FJsonObject> CompObj = BuildComponentSummary(
                    Node->ComponentTemplate, VarName, Source,
                    bDetailed, PropertyNameFilter);

                // Add parent attachment info from SCS node
                if (Node->ParentComponentOrVariableName != NAME_None)
                {
                    CompObj->SetStringField(TEXT("attachParent"),
                        Node->ParentComponentOrVariableName.ToString());
                }

                ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
            }
        }
        UClass* ParentClass = Bp->ParentClass;
        Bp = ParentClass ? Cast<UBlueprint>(ParentClass->ClassGeneratedBy) : nullptr;
    }

    Resp->SetArrayField(TEXT("components"), ComponentsArray);
    Resp->SetNumberField(TEXT("componentCount"), ComponentsArray.Num());
    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("CDO inspection completed"), Resp, FString());
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("inspect_cdo requires editor build."),
                        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
