// Per-component detail enrichment for get_components entries.
//
// The component list handler stays under the pure-line ceiling by delegating
// the inspection fields to this sibling file. Everything added here is
// additive: existing readers keep reading name/class/path/relative* untouched,
// and the new fields (class identity, attach parent, visibility/active state)
// only widen the entry.

#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"

#include "UObject/Class.h"

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
  // A world actor's mesh and materials took an inspect_object call per actor to find.
  // No property list rides along: the first ten names a class declares, with no values, were most of a
  // 36-component pawn's 30 KB answer; get_component_property and get_component_details read properties.
  McpHandlerUtils::AddMeshAssetFields(Component, Entry);
}
