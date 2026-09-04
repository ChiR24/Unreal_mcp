// Per-component detail enrichment for get_components entries.
//
// The component list handler stays under the pure-line ceiling by delegating
// the inspection fields to this sibling file. Everything added here is
// additive: existing readers keep reading name/class/path/relative* untouched,
// and the new fields (class identity, attach parent, visibility/active state,
// and a bounded property name/type census) only widen the entry.

#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"

#include "Foundation/Reflection/McpPropertyReflection.h"
#include "UObject/Class.h"

#if WITH_EDITOR
void McpAppendComponentDetailFields(UActorComponent *Component,
                                    TSharedPtr<FJsonObject> &Entry) {
  if (!Component || !Entry.IsValid()) {
    return;
  }

  // Class identity: display name and full path side by side, since the legacy
  // `class` field only carries the path name.
  UClass *ComponentClass = Component->GetClass();
  if (ComponentClass) {
    Entry->SetStringField(TEXT("className"), ComponentClass->GetName());
    Entry->SetStringField(TEXT("classPath"), ComponentClass->GetPathName());
  }

  Entry->SetBoolField(TEXT("isActive"), Component->IsActive());

  if (USceneComponent *SceneComp = Cast<USceneComponent>(Component)) {
    Entry->SetBoolField(TEXT("isSceneComponent"), true);
    Entry->SetBoolField(TEXT("isVisible"), SceneComp->IsVisible());
    if (USceneComponent *AttachParent = SceneComp->GetAttachParent()) {
      Entry->SetStringField(TEXT("attachParent"), AttachParent->GetName());
    }
  } else {
    Entry->SetBoolField(TEXT("isSceneComponent"), false);
  }

  // Bounded property census: name plus the shared reflection type name for at
  // most ten non-deprecated instance properties. Bounded so a component with
  // hundreds of fields cannot flood a component-listing response.
  if (ComponentClass) {
    TArray<TSharedPtr<FJsonValue>> Properties;
    for (TFieldIterator<FProperty> It(ComponentClass);
         It && Properties.Num() < 10; ++It) {
      FProperty *Property = *It;
      if (!Property || Property->HasAnyPropertyFlags(CPF_Deprecated)) {
        continue;
      }
      TSharedPtr<FJsonObject> PropertyObj =
          McpHandlerUtils::CreateResultObject();
      PropertyObj->SetStringField(TEXT("name"), Property->GetName());
      PropertyObj->SetStringField(
          TEXT("type"), McpPropertyReflection::GetPropertyTypeName(Property));
      Properties.Add(MakeShared<FJsonValueObject>(PropertyObj));
    }
    Entry->SetArrayField(TEXT("properties"), Properties);
  }
}
#endif
