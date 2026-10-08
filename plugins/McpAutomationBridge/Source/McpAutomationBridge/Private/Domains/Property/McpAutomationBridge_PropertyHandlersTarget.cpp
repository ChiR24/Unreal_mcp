#include "Domains/Property/McpAutomationBridge_PropertyHandlersTarget.h"

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Safety/McpSafeReflectionTarget.h"

#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"

namespace McpPropertyTarget
{
bool IsSupersededTarget(const UObject* Object)
{
  const UClass* Class = Object->GetClass();
  const FString ClassName = Class->GetName();
  if (Class->HasAnyClassFlags(CLASS_NewerVersionExists) || ClassName.StartsWith(TEXT("REINST_")) ||
      ClassName.StartsWith(TEXT("SKEL_")) || ClassName.StartsWith(TEXT("TRASHCLASS_"))) {
    return true;
  }
  // A running game keeps its objects in the transient package (GameInstance, a widget); outside it, a Blueprint object
  // there is what a compile moved aside.
  return UBlueprint::GetBlueprintFromClass(Class) && Object->GetOutermost() == GetTransientPackage() &&
         !(GEditor && GEditor->PlayWorld);
}

void KeepDeclaredDefault(UBlueprint* Blueprint, UObject* DefaultObject, const FString& PropertyPath)
{
  const int32 Dot = PropertyPath.Find(TEXT("."));
  const FString Head = Dot == INDEX_NONE ? PropertyPath : PropertyPath.Left(Dot);
  const FProperty* Variable = DefaultObject->GetClass()->FindPropertyByName(FName(*Head));
  const int32 Index = Variable ? FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, Variable->GetFName()) : INDEX_NONE;
  if (Index == INDEX_NONE) {
    return;
  }
  FString Text;
  MCP_PROPERTY_EXPORT_TEXT(Variable, Text, Variable->ContainerPtrToValuePtr<void>(DefaultObject), nullptr, nullptr, PPF_None);
  Blueprint->NewVariables[Index].DefaultValue = Text;
}

bool FindOnCurrentDefault(UBlueprint* Blueprint, const FString& PropertyPath, UObject*& OutObject,
                          FProperty*& OutProperty, void*& OutContainer, FName SubobjectName)
{
  UClass* Current = Blueprint->GeneratedClass;
  UObject* Fresh = Current ? Current->GetDefaultObject() : nullptr;
  if (Fresh && !SubobjectName.IsNone()) {
    Fresh = Fresh->GetDefaultSubobjectByName(SubobjectName);
  }
  void* Container = nullptr;
  FString ResolvedPath, Error;
  FProperty* Found = Fresh ? McpResolvePropertyPath(Fresh, PropertyPath, Container, ResolvedPath, Error) : nullptr;
  if (!Found || !Container) {
    return false;
  }
  OutObject = Fresh;
  OutProperty = Found;
  OutContainer = Container;
  return true;
}

bool ResolvePropertyTarget(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId,
                           const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                           FPropertyTarget& Out)
{
  if (!Payload.IsValid()) {
    Bridge.SendAutomationError(Socket, RequestId, TEXT("Property request payload missing."), TEXT("INVALID_PAYLOAD"));
    return false;
  }
  Payload->TryGetStringField(TEXT("objectPath"), Out.ObjectPath);
  Out.ObjectPath.TrimStartAndEndInline();
  Payload->TryGetStringField(TEXT("blueprintPath"), Out.BlueprintPath);
  Out.BlueprintPath.TrimStartAndEndInline();
  if (Out.ObjectPath.IsEmpty() && Out.BlueprintPath.IsEmpty()) {
    Bridge.SendAutomationError(Socket, RequestId,
                               TEXT("A target is required: objectPath, actorName, name or blueprintPath."),
                               TEXT("INVALID_OBJECT"));
    return false;
  }
  Payload->TryGetStringField(TEXT("propertyName"), Out.PropertyName);
  Out.PropertyName.TrimStartAndEndInline();
  if (Out.PropertyName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("propertyPath"), Out.PropertyName);
    Out.PropertyName.TrimStartAndEndInline();
  }
  if (Out.PropertyName.IsEmpty()) {
    Bridge.SendAutomationError(Socket, RequestId, TEXT("propertyName or propertyPath is required."),
                               TEXT("INVALID_PROPERTY"));
    return false;
  }

  if (!Out.BlueprintPath.IsEmpty()) {
    FString NormalizedPath, LoadError;
    Out.Blueprint = LoadBlueprintAsset(Out.BlueprintPath, NormalizedPath, LoadError);
    if (!Out.Blueprint) {
      Bridge.SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("Blueprint not found: %s (%s)"), *Out.BlueprintPath, *LoadError),
          TEXT("BLUEPRINT_NOT_FOUND"));
      return false;
    }
    UClass* GeneratedClass = Out.Blueprint->GeneratedClass;
    Out.RootObject = GeneratedClass ? GeneratedClass->GetDefaultObject() : nullptr;
    if (!Out.RootObject) {
      Bridge.SendAutomationError(Socket, RequestId,
          GeneratedClass ? TEXT("Failed to get Class Default Object") : TEXT("Blueprint has no GeneratedClass (not compiled?)"),
          TEXT("CDO_NOT_FOUND"));
      return false;
    }
    Out.ObjectPath = Out.RootObject->GetPathName();
  } else {
    FString ResolvedPath;
    Out.RootObject = McpHandlerUtils::ResolveObjectFromPath(Out.ObjectPath, &ResolvedPath);
    if (!Out.RootObject) {
      Bridge.SendAutomationError(Socket, RequestId,
          McpHandlerUtils::DescribeObjectNotFound(Out.ObjectPath), TEXT("OBJECT_NOT_FOUND"));
      return false;
    }
    if (!ResolvedPath.IsEmpty()) {
      Out.ObjectPath = ResolvedPath;
    }
    // The Blueprint asset's own path (/Game/X/BP_Foo) means its class defaults unless the property is the asset's: a
    // read of "CharacterMovement.MaxWalkSpeed" there failed as "not found in scope 'Blueprint'".
    FString Head = Out.PropertyName, Rest;
    Out.PropertyName.Split(TEXT("."), &Head, &Rest);
    UBlueprint* AsBlueprint = Cast<UBlueprint>(Out.RootObject);
    if (AsBlueprint && AsBlueprint->GeneratedClass && !FindFProperty<FProperty>(AsBlueprint->GetClass(), FName(*Head))) {
      Out.RootObject = AsBlueprint->GeneratedClass->GetDefaultObject();
      Out.ObjectPath = Out.RootObject->GetPathName();
    }
    // A caller naming a Blueprint CDO directly (...Default__BP_Foo_C) lands here
    // rather than in the blueprintPath branch. Recover the owning Blueprint so a
    // write is compiled into the class defaults (otherwise every newly spawned
    // instance kept the stale default) and "Component.Property" resolves.
    if (Out.RootObject->HasAnyFlags(RF_ClassDefaultObject)) {
      Out.Blueprint = UBlueprint::GetBlueprintFromClass(Out.RootObject->GetClass());
      // A compile replaces the class and its default object and leaves the old pair in /Engine/Transient as REINST_<Class>,
      // still found by name: the Blueprint's current default object is the one a caller means.
      UClass* Current = Out.Blueprint ? static_cast<UClass*>(Out.Blueprint->GeneratedClass) : nullptr;
      if (Current && Current != Out.RootObject->GetClass()) {
        Out.RootObject = Current->GetDefaultObject();
        Out.ObjectPath = Out.RootObject->GetPathName();
      }
    }
  }

  // Refuse /Script targets before ANY property is read or written. Reads would
  // let a read-only principal export the plugin's own CapabilityToken (the
  // receipt redactor cannot see it under the generic key `value`); writes would
  // let a write-scoped principal set bRequireCapabilityToken=false on the
  // plugin's settings CDO and persist it through PostEditChange().
  if (!McpSafeReflectionTarget::IsAddressable(Out.RootObject)) {
    Bridge.SendAutomationError(Socket, RequestId, McpSafeReflectionTarget::DenyMessage(),
                               McpSafeReflectionTarget::DenyCode());
    return false;
  }
  return true;
}

FString ValueText(FProperty* Property, void* Container)
{
  FString Text;
  MCP_PROPERTY_EXPORT_TEXT(Property, Text, Property->ContainerPtrToValuePtr<void>(Container), nullptr, nullptr, PPF_None);
  return Text;
}

namespace
{
// The component a placed actor's dotted path names first, by the actor property holding it (StaticMeshComponent,
// CharacterMovement) or by its own name (StaticMeshComponent0), with the rest of the path; null otherwise.
UActorComponent* PathComponent(UObject* Root, const FString& Path, FString& OutRest)
{
  AActor* Actor = Cast<AActor>(Root);
  FString Head;
  if (!Actor || !Path.Split(TEXT("."), &Head, &OutRest)) {
    return nullptr;
  }
  if (const FObjectProperty* Held = FindFProperty<FObjectProperty>(Actor->GetClass(), FName(*Head))) {
    return Cast<UActorComponent>(Held->GetObjectPropertyValue_InContainer(Actor));
  }
  TArray<UActorComponent*> Components;
  Actor->GetComponents(Components);
  UActorComponent* const* Named = Components.FindByPredicate([&Head](const UActorComponent* Each) {
    return Each && Each->GetName().Equals(Head, ESearchCase::IgnoreCase);
  });
  return Named ? *Named : nullptr;
}
}

bool WriteValue(UObject* Root, const FString& Path, FProperty* Property, void* Container,
                const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
  FString Key = Path;
  UActorComponent* Component = Cast<UActorComponent>(Root);
  if (!Component) {
    Component = PathComponent(Root, Path, Key);
  }
  if (!Component || !McpIsComponentSetterKey(Component, Key)) {
    return ApplyJsonValueToProperty(Container, Property, Value, OutError);
  }
  TSharedPtr<FJsonObject> Bag = MakeShared<FJsonObject>();
  Bag->SetField(Key, Value);
  TArray<FString> Applied, Warnings;
  McpApplyComponentProperties(Component, Bag, Applied, Warnings);
  OutError = FString::Join(Warnings, TEXT("; "));
  return Applied.Num() > 0;
}

void NotifyOwners(UObject* Edited)
{
  // A component's actor is left to the caller's own change handling: only instanced subobjects are walked up.
  for (UObject* Owner = Edited && !Edited->IsA<UActorComponent>() ? Edited->GetOuter() : nullptr;
       Owner && !Owner->IsA<UPackage>(); Edited = Owner, Owner = Owner->GetOuter()) {
    FObjectProperty* Holder = nullptr;
    for (TFieldIterator<FObjectProperty> It(Owner->GetClass()); It && !Holder; ++It) {
      if (It->HasAnyPropertyFlags(CPF_InstancedReference) && It->GetObjectPropertyValue_InContainer(Owner) == Edited) {
        Holder = *It;
      }
    }
    if (!Holder) {
      return;
    }
    FPropertyChangedEvent Changed(Holder, EPropertyChangeType::ValueSet);
    Owner->PostEditChangeProperty(Changed);
  }
}

// `saved` used to be hard-coded true while nothing reached disk: the package was only marked dirty, so an InputAction's
// bTriggerWhenPaused set through set_property was gone after the next editor restart.
bool SaveAfterWrite(UObject* Target, UBlueprint* Blueprint, bool bMarkDirty, FString& OutSkipReason)
{
  UPackage* OwningPackage = (Blueprint ? static_cast<UObject*>(Blueprint) : Target)->GetOutermost();
  if (!bMarkDirty) {
    OutSkipReason = TEXT("markDirty was false");
  } else if (OwningPackage->HasAnyPackageFlags(PKG_PlayInEditor) || OwningPackage == GetTransientPackage()) {
    // Checked before ContainsMap: a PIE package holds a map too, and "saved with its level" promised a save that
    // stopping PIE throws away.
    OutSkipReason = TEXT("a running-game or transient object has nothing to save; the change lasts until PIE stops");
  } else if (OwningPackage->ContainsMap()) {
    OutSkipReason = TEXT("level content is saved with its level");
  } else if (OwningPackage->GetName().StartsWith(TEXT("/Engine/"))) {
    OutSkipReason = TEXT("engine content is not saved");
  } else if (McpSafeAssetSave(OwningPackage)) {
    return true;
  } else {
    OutSkipReason = TEXT("the package could not be saved; the change is only in memory");
  }
  return false;
}
}
