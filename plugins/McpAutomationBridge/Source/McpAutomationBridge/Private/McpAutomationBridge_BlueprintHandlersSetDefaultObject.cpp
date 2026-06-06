#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "JsonObjectConverter.h"
#include "McpHandlerUtils.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleBlueprintSetDefaultObject(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("blueprint_set_default")) ||
      ActionMatchesPattern(TEXT("set_default")) ||
      ActionMatchesPattern(TEXT("setdefault")) ||
      AlphaNumLower.Contains(TEXT("blueprintsetdefault")) ||
      AlphaNumLower.Contains(TEXT("setdefault"))) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Entered blueprint_set_default handler: RequestId=%s"),
           *RequestId);
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_set_default requires a blueprint path."), nullptr,
          TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }
    FString PropertyName;
    LocalPayload->TryGetStringField(TEXT("propertyName"), PropertyName);
    if (PropertyName.IsEmpty()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("propertyName required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }
    const TSharedPtr<FJsonValue> Value =
        LocalPayload->TryGetField(TEXT("value"));
    if (!Value.IsValid()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("value required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

#if WITH_EDITOR
    FString Normalized;
    FString LoadErr;
    UBlueprint *BP = LoadBlueprintAsset(Path, Normalized, LoadErr);

    if (!BP) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("error"), LoadErr);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, LoadErr,
                             Result, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    const FString RegistryKey = Normalized.IsEmpty() ? Path : Normalized;

    // Get the CDO (Class Default Object) from the generated class
    UClass *GeneratedClass = BP->GeneratedClass;
    if (!GeneratedClass) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("error"),
                             TEXT("Blueprint has no generated class"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No generated class"), Result,
                             TEXT("NO_GENERATED_CLASS"));
      return true;
    }

    UObject *CDO = GeneratedClass->GetDefaultObject();
    if (!CDO) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("error"), TEXT("Failed to get CDO"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("No CDO"),
                             Result, TEXT("NO_CDO"));
      return true;
    }

    // Find the property by name (supports nested via dot notation)
    FProperty *TargetProperty =
        FindFProperty<FProperty>(GeneratedClass, FName(*PropertyName));
    if (!TargetProperty) {
      // Try nested property path (e.g., "LightComponent.Intensity",
      // "RootComponent.bHiddenInGame")
      const int32 DotIdx = PropertyName.Find(TEXT("."));
      if (DotIdx != INDEX_NONE) {
        const FString ComponentName = PropertyName.Left(DotIdx);
        const FString NestedProp = PropertyName.Mid(DotIdx + 1);

        // Search in generated class and all parent classes for the component
        // property
        UClass *SearchClass = GeneratedClass;
        FProperty *CompProp = nullptr;
        while (SearchClass && !CompProp) {
          CompProp =
              FindFProperty<FProperty>(SearchClass, FName(*ComponentName));
          if (!CompProp) {
            SearchClass = SearchClass->GetSuperClass();
          }
        }

        if (CompProp && CompProp->IsA<FObjectProperty>()) {
          FObjectProperty *ObjProp = CastField<FObjectProperty>(CompProp);
          void *CompPtr = ObjProp->GetPropertyValuePtr_InContainer(CDO);
          UObject *CompObj = ObjProp->GetObjectPropertyValue(CompPtr);
          if (CompObj) {
            TargetProperty = FindFProperty<FProperty>(CompObj->GetClass(),
                                                      FName(*NestedProp));
            if (TargetProperty) {
              CDO = CompObj; // Update CDO to point to component
            }
          }
        }
      }
    }

    if (!TargetProperty) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("propertyName"), PropertyName);
      Result->SetStringField(TEXT("blueprintPath"), Path);
      Result->SetStringField(TEXT("error"),
                             TEXT("Property not found on generated class"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Property not found on blueprint"), Result,
                             TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }

    // Special handling for Class/SoftClass properties to resolve string paths
    // to UClass*
    if (TargetProperty->IsA<FClassProperty>() ||
        TargetProperty->IsA<FSoftClassProperty>()) {
      FString ClassPath;
      if (Value->TryGetString(ClassPath)) {
        UClass *ClassToSet = nullptr;
        if (!ClassPath.IsEmpty()) {
          ClassToSet = LoadObject<UClass>(nullptr, *ClassPath);
          if (!ClassToSet) {
            ClassToSet = FindObject<UClass>(nullptr, *ClassPath);
          }
        }

        // proceeded if we found the class OR if the intention was to clear it
        // (empty path)
        if (ClassToSet || ClassPath.IsEmpty()) {
          CDO->Modify();
          BP->Modify();

          if (FClassProperty *ClassProp =
                  CastField<FClassProperty>(TargetProperty)) {
            ClassProp->SetPropertyValue_InContainer(CDO, ClassToSet);
          } else if (FSoftClassProperty *SoftClassProp =
                         CastField<FSoftClassProperty>(TargetProperty)) {
            SoftClassProp->SetPropertyValue_InContainer(
                CDO, FSoftObjectPtr(ClassToSet));
          }

          FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
          McpSafeCompileBlueprint(BP);
          bool bSaved = SaveLoadedAssetThrottled(BP);

          TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
          Result->SetBoolField(TEXT("success"), true);
          Result->SetStringField(TEXT("propertyName"), PropertyName);
          Result->SetStringField(TEXT("blueprintPath"), Path);
          Result->SetBoolField(TEXT("saved"), bSaved);
          // Add verification data for the blueprint asset
          McpHandlerUtils::AddVerification(Result, BP);
          Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                 TEXT("Blueprint default class property set"),
                                 Result, FString());
          return true;
        }
      }
    }

    // Convert JSON value to property value using the existing JSON
    // serialization system
    TSharedPtr<FJsonObject> TempObj = McpHandlerUtils::CreateResultObject();
    TempObj->SetField(TEXT("temp"), Value);

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(TempObj.ToSharedRef(), Writer);

    // Use FJsonObjectConverter to deserialize the value
    TSharedPtr<FJsonObject> ValueWrapObj = McpHandlerUtils::CreateResultObject();
    ValueWrapObj->SetField(TargetProperty->GetName(), Value);

    CDO->Modify();
    BP->Modify();

    // Attempt to set the property value
    bool bSuccess = FJsonObjectConverter::JsonAttributesToUStruct(
        ValueWrapObj->Values, GeneratedClass, CDO, 0, 0);

    if (bSuccess) {
      FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
      McpSafeCompileBlueprint(BP);

      // Save the blueprint to persist changes
      bool bSaved = SaveLoadedAssetThrottled(BP);

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetBoolField(TEXT("success"), true);
      Result->SetStringField(TEXT("propertyName"), PropertyName);
      Result->SetStringField(TEXT("blueprintPath"), Path);
      Result->SetBoolField(TEXT("saved"), bSaved);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Blueprint default property set"), Result,
                             FString());
    } else {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetBoolField(TEXT("success"), false);
      Result->SetStringField(TEXT("error"),
                             TEXT("Failed to set property value"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Property set failed"), Result,
                             TEXT("SET_FAILED"));
    }
    return true;
#else
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("blueprint_set_default requires editor build"),
                           nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  // Compile a Blueprint asset (editor builds only). Returns whether
  // compilation (and optional save) succeeded.
  return false;
}
#endif
} // namespace McpBlueprintHandlers
