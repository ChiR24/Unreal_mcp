// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FileManage/UmgFileTransformation.h"
#include "UmgMcp.h"
#include "PropertyNameMappings.h"

#include "Blueprint/UserWidget.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Components/TextBlock.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "UObject/ObjectMacros.h"
#include "Async/Async.h"
#include "WidgetBlueprintFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"

// Forward declarations
static UWidget* CreateWidgetFromJson(const TSharedPtr<FJsonObject>& WidgetJson, UWidgetTree* WidgetTree, UWidget* ParentWidget);
static void ApplyPropertiesToExistingWidget(const TSharedPtr<FJsonObject>& WidgetJson, UWidget* TargetWidget);
static void MergeChildrenAppendOnly(const TSharedPtr<FJsonObject>& WidgetJson, UWidgetTree* WidgetTree, UWidget* TargetWidget);
static void UpsertWidgetFromJsonAppendOnly(const TSharedPtr<FJsonObject>& WidgetJson, UWidgetTree* WidgetTree, UWidget* TargetWidget);

bool ApplyJsonToUmgAsset_GameThread(const FString& AssetPath, const FString& JsonData, const FString& TargetWidgetName);

TSharedPtr<FJsonObject> UUmgFileTransformation::NormalizeJsonKeysToPascalCase(const TSharedPtr<FJsonObject>& SourceJson)
{
    if (!SourceJson.IsValid())
    {
        return nullptr;
    }
    
    TSharedPtr<FJsonObject> NormalizedJson = MakeShared<FJsonObject>();
    
    for (const auto& Pair : SourceJson->Values)
    {
        FString OriginalKey = Pair.Key;
        FString NormalizedKey;
        
        // Handle dotted keys (e.g. "slot.position")
        if (OriginalKey.Contains(TEXT(".")))
        {
            TArray<FString> Parts;
            OriginalKey.ParseIntoArray(Parts, TEXT("."));
            for (int32 i = 0; i < Parts.Num(); ++i)
            {
                Parts[i] = NormalizePropertyName(Parts[i]);
            }
            NormalizedKey = FString::Join(Parts, TEXT("."));
        }
        else
        {
            NormalizedKey = NormalizePropertyName(OriginalKey);
        }

        if (NormalizedKey != OriginalKey)
        {
            UE_LOG(LogUmgMcp, Verbose, TEXT("NormalizeJsonKeys: '%s' → '%s'"), *OriginalKey, *NormalizedKey);
        }
        
        // Recursively process nested objects and arrays
        TSharedPtr<FJsonValue> Value = Pair.Value;
        if (Value->Type == EJson::Object)
        {
            TSharedPtr<FJsonObject> NestedObj = Value->AsObject();
            TSharedPtr<FJsonObject> NormalizedNestedObj = NormalizeJsonKeysToPascalCase(NestedObj);
            NormalizedJson->SetObjectField(NormalizedKey, NormalizedNestedObj);
        }
        else if (Value->Type == EJson::Array)
        {
            // Process objects within arrays
            TArray<TSharedPtr<FJsonValue>> SourceArray = Value->AsArray();
            TArray<TSharedPtr<FJsonValue>> NormalizedArray;
            
            for (const TSharedPtr<FJsonValue>& ArrayValue : SourceArray)
            {
                if (ArrayValue->Type == EJson::Object)
                {
                    TSharedPtr<FJsonObject> NormalizedArrayObj = NormalizeJsonKeysToPascalCase(ArrayValue->AsObject());
                    NormalizedArray.Add(MakeShared<FJsonValueObject>(NormalizedArrayObj));
                }
                else
                {
                    NormalizedArray.Add(ArrayValue);
                }
            }
            NormalizedJson->SetArrayField(NormalizedKey, NormalizedArray);
        }
        else
        {
            // Primitive types: copy directly
            NormalizedJson->SetField(NormalizedKey, Value);
        }
    }
    
    return NormalizedJson;
}

TSharedPtr<FJsonObject> UUmgFileTransformation::ExportWidgetToJson(UWidget* Widget)
{
    if (!Widget)
    {
        UE_LOG(LogUmgMcp, Warning, TEXT("ExportWidgetToJson: Input Widget is null."));
        return nullptr;
    }

    TSharedPtr<FJsonObject> WidgetJson = MakeShared<FJsonObject>();
    UObject* DefaultWidget = Widget->GetClass()->GetDefaultObject();

    // 1. Add basic information
    WidgetJson->SetStringField(TEXT("widget_name"), Widget->GetName());
    WidgetJson->SetStringField(TEXT("widget_class"), Widget->GetClass()->GetPathName());

    // 2. Iterate over all properties and only add non-default ones
    TSharedPtr<FJsonObject> PropertiesJson = MakeShared<FJsonObject>();
    
    for (TFieldIterator<FProperty> PropIt(Widget->GetClass()); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;

        bool bIsEditorOnly = false;
#if WITH_EDITOR
        bIsEditorOnly = Property->HasAnyPropertyFlags(CPF_EditorOnly);
#endif

        if (Property->HasAnyPropertyFlags(CPF_Edit) && !Property->HasAnyPropertyFlags(CPF_Transient) && !bIsEditorOnly)
        {
            void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Widget);
            void* DefaultValuePtr = Property->ContainerPtrToValuePtr<void>(DefaultWidget);

            if (!Property->Identical(ValuePtr, DefaultValuePtr))
            {
                if (Property->GetFName() == TEXT("Slot"))
                {
                    if (UPanelSlot* SlotObject = Cast<UPanelSlot>(CastField<FObjectProperty>(Property)->GetObjectPropertyValue_InContainer(Widget)))
                    {
                        TSharedPtr<FJsonObject> SlotPropertiesJson = MakeShared<FJsonObject>();
                        UObject* DefaultSlotObject = SlotObject->GetClass()->GetDefaultObject();

                        for (TFieldIterator<FProperty> SlotPropIt(SlotObject->GetClass()); SlotPropIt; ++SlotPropIt)
                        {
                            FProperty* SlotProperty = *SlotPropIt;
                            FName SlotPropertyName = SlotProperty->GetFName();

                            if (SlotPropertyName == TEXT("Content") || SlotPropertyName == TEXT("Parent"))
                            {
                                continue;
                            }

                            if (SlotProperty->HasAnyPropertyFlags(CPF_Edit) && !SlotProperty->HasAnyPropertyFlags(CPF_Transient))
                            {
                                void* SlotValuePtr = SlotProperty->ContainerPtrToValuePtr<void>(SlotObject);
                                void* DefaultSlotValuePtr = SlotProperty->ContainerPtrToValuePtr<void>(DefaultSlotObject);

                                if (!SlotProperty->Identical(SlotValuePtr, DefaultSlotValuePtr))
                                {
                                    TSharedPtr<FJsonValue> SlotPropertyJsonValue = FJsonObjectConverter::UPropertyToJsonValue(SlotProperty, SlotValuePtr);
                                    if (SlotPropertyJsonValue.IsValid())
                                    {
                                        SlotPropertiesJson->SetField(SlotProperty->GetName(), SlotPropertyJsonValue);
                                    }
                                }
                            }
                        }
                        
                        if(SlotPropertiesJson->Values.Num() > 0)
                        {
                           PropertiesJson->SetObjectField(TEXT("Slot"), SlotPropertiesJson);
                        }
                    }
                }
                else
                {
                    TSharedPtr<FJsonValue> PropertyJsonValue = FJsonObjectConverter::UPropertyToJsonValue(Property, ValuePtr);
                    if (PropertyJsonValue.IsValid())
                    {
                        PropertiesJson->SetField(Property->GetName(), PropertyJsonValue);
                    }
                }
            }
        }
    }
    
    if (PropertiesJson->Values.Num() > 0)
    {
        WidgetJson->SetObjectField(TEXT("properties"), PropertiesJson);
    }

    TArray<TSharedPtr<FJsonValue>> ChildrenJsonArray;
    if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
    {
        for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
        {
            if (UWidget* ChildWidget = PanelWidget->GetChildAt(i))
            {
                TSharedPtr<FJsonObject> ChildJson = ExportWidgetToJson(ChildWidget);
                if (ChildJson.IsValid())
                {
                    ChildrenJsonArray.Add(MakeShared<FJsonValueObject>(ChildJson));
                }
            }
        }
    }
    
    if (ChildrenJsonArray.Num() > 0)
    {
        WidgetJson->SetArrayField(TEXT("children"), ChildrenJsonArray);
    }

    return WidgetJson;
}

FString UUmgFileTransformation::ExportUmgAssetToJsonString(const FString& AssetPath, const FString& TargetWidgetName)
{
    FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
    
    UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *PackageName));

    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ExportUmgAssetToJsonString: Failed to load UWidgetBlueprint from path '%s'."), *AssetPath);
        return FString();
    }
    
    if (!WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ExportUmgAssetToJsonString: WidgetTree is null in UWidgetBlueprint '%s'."), *AssetPath);
        return FString();
    }

    UWidget* TargetWidget = nullptr;
    if (TargetWidgetName.IsEmpty() || TargetWidgetName.Equals(TEXT("Root"), ESearchCase::IgnoreCase))
    {
        TargetWidget = WidgetBlueprint->WidgetTree->RootWidget;
        if (!TargetWidget)
        {
            UE_LOG(LogUmgMcp, Warning, TEXT("ExportUmgAssetToJsonString: Root widget not found in UWidgetBlueprint '%s'. This may be an empty UI."), *AssetPath);
            return FString();
        }
    }
    else
    {
        TargetWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*TargetWidgetName));
        if (!TargetWidget)
        {
             UE_LOG(LogUmgMcp, Error, TEXT("ExportUmgAssetToJsonString: Target widget '%s' not found in '%s'."), *TargetWidgetName, *AssetPath);
             return FString();
        }
    }

    TSharedPtr<FJsonObject> RootJsonObject = ExportWidgetToJson(TargetWidget);

    if (!RootJsonObject.IsValid())
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ExportUmgAssetToJsonString: Failed to convert widget '%s' of '%s' to FJsonObject."), *TargetWidget->GetName(), *AssetPath);
        return FString();
    }

    FString JsonString;
    TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonString);
    
    if (FJsonSerializer::Serialize(RootJsonObject.ToSharedRef(), JsonWriter))
    {
        JsonWriter->Close();
        UE_LOG(LogUmgMcp, Log, TEXT("Successfully exported UMG asset '%s' (Target: %s) to JSON."), *AssetPath, *TargetWidget->GetName());
        return JsonString;
    }
    else
    {
        JsonWriter->Close();
        UE_LOG(LogUmgMcp, Error, TEXT("ExportUmgAssetToJsonString: Failed to serialize FJsonObject to string for asset '%s'."), *AssetPath);
        return FString();
    }
}

bool UUmgFileTransformation::ApplyJsonStringToUmgAsset(const FString& AssetPath, const FString& JsonData, const FString& TargetWidgetName)
{
    // Dispatch the task to the game thread asynchronously ("fire and forget").
    // The original implementation used a blocking wait which could cause the editor to freeze or deadlock.
    // We capture parameters by value to ensure they are valid when the task eventually executes.
    FFunctionGraphTask::CreateAndDispatchWhenReady([AssetPath, JsonData, TargetWidgetName]()
    {
        ApplyJsonToUmgAsset_GameThread(AssetPath, JsonData, TargetWidgetName);
    }, TStatId(), nullptr, ENamedThreads::GameThread);

    // Return true to indicate the task was successfully dispatched.
    // The operation itself runs in the background and the result will be visible in the editor.
    return true;
}


// This function is executed on the game thread to ensure thread safety when dealing with UObjects.
bool ApplyJsonToUmgAsset_GameThread(const FString& AssetPath, const FString& JsonData, const FString& TargetWidgetName)
{
    // 0. Handle default workspace: if AssetPath is empty, use default path
    FString FinalAssetPath = AssetPath;
    if (FinalAssetPath.IsEmpty() || FinalAssetPath.TrimStartAndEnd().IsEmpty())
    {
        FinalAssetPath = TEXT("/Game/UnrealMotionGraphicsMCP.UnrealMotionGraphicsMCP");
        UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: No asset path provided. Using default workspace: '%s'."), *FinalAssetPath);
    }
    
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Starting for asset '%s'."), *FinalAssetPath);

    // 1. Parse the incoming JSON data.
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Parsing JSON data."));
    TSharedPtr<FJsonObject> RootJsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    if (!FJsonSerializer::Deserialize(Reader, RootJsonObject) || !RootJsonObject.IsValid())
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Failed to parse JSON data."));
        return false;
    }
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: JSON data parsed successfully."));

    // 2. Load or create the target Widget Blueprint asset.
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Loading Widget Blueprint from '%s'."), *FinalAssetPath);
    UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *FinalAssetPath));
    
    bool bIsNewlyCreated = false;  // Track if this is a newly created blueprint
    
    if (!WidgetBlueprint)
    {
        UE_LOG(LogUmgMcp, Warning, TEXT("ApplyJsonToUmgAsset_GameThread: Widget Blueprint at '%s' not found. Attempting to create new asset."), *FinalAssetPath);
        
        // Extract package path and asset name from FinalAssetPath
        // FinalAssetPath format: /Game/FolderName/AssetName.AssetName
        FString PackagePath, AssetName;
        if (FinalAssetPath.Split(TEXT("."), &PackagePath, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
        {
            UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Package: '%s', Asset: '%s'"), *PackagePath, *AssetName);
            
            // Create a new package
            UPackage* Package = CreatePackage(*PackagePath);
            if (!Package)
            {
                UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Failed to create package at '%s'."), *PackagePath);
                return false;
            }
            
            // Create the Widget Blueprint using the factory
            UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
            WidgetBlueprint = Cast<UWidgetBlueprint>(Factory->FactoryCreateNew(
                UWidgetBlueprint::StaticClass(),
                Package,
                FName(*AssetName),
                RF_Public | RF_Standalone,
                nullptr,
                GWarn
            ));
            
            if (!WidgetBlueprint)
            {
                UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Failed to create new Widget Blueprint at '%s'."), *FinalAssetPath);
                return false;
            }
            
            bIsNewlyCreated = true;  // Mark as newly created
            
            // Mark package as dirty and notify asset registry
            Package->MarkPackageDirty();
            FAssetRegistryModule::AssetCreated(WidgetBlueprint);
            
            UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: New Widget Blueprint created at '%s'."), *FinalAssetPath);
        }
        else
        {
            UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Invalid asset path format '%s'."), *FinalAssetPath);
            return false;
        }
    }
    else
    {
        UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Widget Blueprint loaded."));
    }

    // 3. Ensure the WidgetTree is valid.
    if (!WidgetBlueprint->WidgetTree)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: WidgetTree is null in UWidgetBlueprint '%s'."), *FinalAssetPath);
        return false;
    }
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: WidgetTree is valid."));

    // 4. Mark the blueprint as modified so the editor knows to save it.
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Marking blueprint as modified."));
    WidgetBlueprint->Modify();
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Blueprint marked as modified."));

    // 5. Clear the existing widget tree OR find target widget
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Preparing to apply JSON. Target: %s"), *TargetWidgetName);

    UWidget* TargetWidget = nullptr;
    bool bReplacingRoot = true;

    if (!TargetWidgetName.IsEmpty() && !TargetWidgetName.Equals(TEXT("Root"), ESearchCase::IgnoreCase))
    {
        TargetWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*TargetWidgetName));
        if (!TargetWidget)
        {
             UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Target widget '%s' to replace not found."), *TargetWidgetName);
             return false;
        }
        bReplacingRoot = false;
    }

    // Delete-aware append mode: delete_widget exists in this API directory, so apply_json_to_umg
    // must never implicitly remove widgets. We only upsert/append; explicit deletions stay on the delete tool.
    const bool bAppendOnlyWrites = true;

    if (bAppendOnlyWrites)
    {
        if (bReplacingRoot)
        {
            if (!WidgetBlueprint->WidgetTree->RootWidget)
            {
                UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: No existing root widget. Creating from JSON (append-only mode)."));
                UWidget* NewRootWidget = CreateWidgetFromJson(RootJsonObject, WidgetBlueprint->WidgetTree, nullptr);
                if (!NewRootWidget)
                {
                    UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Failed to create root widget from JSON for asset '%s'."), *FinalAssetPath);
                    return false;
                }
                WidgetBlueprint->WidgetTree->RootWidget = NewRootWidget;
                UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: New root widget set: %s"), *NewRootWidget->GetName());
            }
            else
            {
                UpsertWidgetFromJsonAppendOnly(RootJsonObject, WidgetBlueprint->WidgetTree, WidgetBlueprint->WidgetTree->RootWidget);
            }
        }
        else
        {
            UpsertWidgetFromJsonAppendOnly(RootJsonObject, WidgetBlueprint->WidgetTree, TargetWidget);
        }
    }

    // 7.5. Verify widget tree integrity before marking as modified
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Verifying widget tree integrity."));
    if (!WidgetBlueprint->WidgetTree->RootWidget)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("ApplyJsonToUmgAsset_GameThread: Root widget became null after assignment!"));
        return false;
    }
    
    // Ensure all widgets are properly owned by the WidgetTree
    TArray<UWidget*> AllWidgets;
    WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Widget tree contains %d widgets."), AllWidgets.Num());

    // 8. Save the blueprint by marking package as dirty
    // Note: We don't call MarkBlueprintAsStructurallyModified because it causes crashes
    // in async context, even for existing blueprints. Instead, we just mark dirty and let
    // the user save manually or the editor will prompt on project close.
    UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Marking package as dirty."));
    
    UPackage* Package = WidgetBlueprint->GetOutermost();
    if (Package)
    {
        Package->MarkPackageDirty();
        FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Package marked dirty: %s"), *PackageFileName);
        
        if (bIsNewlyCreated)
        {
            UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: New asset created. Please save manually (Ctrl+S) or it will be saved on project close."));
        }
        else
        {
            UE_LOG(LogUmgMcp, Log, TEXT("ApplyJsonToUmgAsset_GameThread: Existing asset modified. Changes applied. If editor is open, close and reopen the asset to see changes, or press Compile."));
        }
    }
    
    // 9. Notify the editor that the blueprint has changed structurally
    // This forces the UI to refresh and show the new widgets
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
    
    UE_LOG(LogUmgMcp, Log, TEXT("Successfully applied JSON to UMG asset '%s'."), *FinalAssetPath);

    return true;
}

static void ApplyPropertiesToExistingWidget(const TSharedPtr<FJsonObject>& WidgetJson, UWidget* TargetWidget)
{
    if (!WidgetJson.IsValid() || !TargetWidget)
    {
        return;
    }

    const TSharedPtr<FJsonObject>* PropertiesJsonObjPtr;
    TSharedPtr<FJsonObject> WidgetProps = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> SlotProps = nullptr;

    if (WidgetJson->TryGetObjectField(TEXT("properties"), PropertiesJsonObjPtr))
    {
        TSharedPtr<FJsonObject> SourceProps = *PropertiesJsonObjPtr;
        for (auto& Pair : SourceProps->Values)
        {
            if (Pair.Key == TEXT("Slot"))
            {
                const TSharedPtr<FJsonObject>* SlotObjPtr;
                if (Pair.Value->TryGetObject(SlotObjPtr))
                {
                    SlotProps = *SlotObjPtr;
                }
            }
            else
            {
                WidgetProps->SetField(Pair.Key, Pair.Value);
            }
        }
    }

    if (WidgetProps->Values.Num() > 0)
    {
        TargetWidget->Modify();
        if (!FJsonObjectConverter::JsonObjectToUStruct(WidgetProps.ToSharedRef(), TargetWidget->GetClass(), TargetWidget, 0, 0))
        {
            UE_LOG(LogUmgMcp, Warning, TEXT("ApplyPropertiesToExistingWidget: Failed to apply properties to '%s'."), *TargetWidget->GetName());
        }
    }

    if (SlotProps.IsValid() && TargetWidget->Slot)
    {
        TargetWidget->Slot->Modify();
        TSharedPtr<FJsonObject> NormalizedSlotProps = UUmgFileTransformation::NormalizeJsonKeysToPascalCase(SlotProps);
        if (!FJsonObjectConverter::JsonObjectToUStruct(NormalizedSlotProps.ToSharedRef(), TargetWidget->Slot->GetClass(), TargetWidget->Slot, 0, 0))
        {
            UE_LOG(LogUmgMcp, Warning, TEXT("ApplyPropertiesToExistingWidget: Failed to apply Slot properties to '%s'."), *TargetWidget->GetName());
        }
    }
}

static void MergeChildrenAppendOnly(const TSharedPtr<FJsonObject>& WidgetJson, UWidgetTree* WidgetTree, UWidget* TargetWidget)
{
    if (!WidgetJson.IsValid() || !WidgetTree || !TargetWidget)
    {
        return;
    }

    UPanelWidget* ParentPanel = Cast<UPanelWidget>(TargetWidget);
    if (!ParentPanel)
    {
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* ChildrenJsonArray;
    if (!WidgetJson->TryGetArrayField(TEXT("children"), ChildrenJsonArray))
    {
        return;
    }

    TMap<FString, UWidget*> ExistingChildren;
    for (int32 Index = 0; Index < ParentPanel->GetChildrenCount(); ++Index)
    {
        if (UWidget* Child = ParentPanel->GetChildAt(Index))
        {
            ExistingChildren.Add(Child->GetName(), Child);
        }
    }

    int32 InsertIndex = 0;
    for (const TSharedPtr<FJsonValue>& ChildValue : *ChildrenJsonArray)
    {
        const TSharedPtr<FJsonObject>* ChildObject;
        if (!ChildValue->TryGetObject(ChildObject))
        {
            continue;
        }

        FString ChildName;
        (*ChildObject)->TryGetStringField(TEXT("widget_name"), ChildName);

        if (UWidget** ExistingPtr = ChildName.IsEmpty() ? nullptr : ExistingChildren.Find(ChildName))
        {
            UpsertWidgetFromJsonAppendOnly(*ChildObject, WidgetTree, *ExistingPtr);
            const int32 CurrentIndex = ParentPanel->GetChildIndex(*ExistingPtr);
            InsertIndex = FMath::Max(InsertIndex, CurrentIndex + 1);
            ExistingChildren.Remove(ChildName);
        }
        else
        {
            UWidget* NewChild = CreateWidgetFromJson(*ChildObject, WidgetTree, TargetWidget);
            if (NewChild)
            {
                const int32 CurrentIndex = ParentPanel->GetChildIndex(NewChild);
                if (CurrentIndex != InsertIndex && CurrentIndex != INDEX_NONE)
                {
                    ParentPanel->InsertChildAt(InsertIndex, NewChild);
                }
                InsertIndex = ParentPanel->GetChildIndex(NewChild) + 1;
            }
        }
    }
}

static void UpsertWidgetFromJsonAppendOnly(const TSharedPtr<FJsonObject>& WidgetJson, UWidgetTree* WidgetTree, UWidget* TargetWidget)
{
    if (!WidgetJson.IsValid() || !WidgetTree || !TargetWidget)
    {
        return;
    }

    FString DeclaredName;
    WidgetJson->TryGetStringField(TEXT("widget_name"), DeclaredName);
    if (!DeclaredName.IsEmpty() && DeclaredName != TargetWidget->GetName())
    {
        UE_LOG(LogUmgMcp, Warning, TEXT("UpsertWidgetFromJsonAppendOnly: JSON widget '%s' mapped to existing widget '%s'. Keeping existing instance to avoid implicit deletion."), *DeclaredName, *TargetWidget->GetName());
    }

    FString DeclaredClass;
    WidgetJson->TryGetStringField(TEXT("widget_class"), DeclaredClass);
    if (!DeclaredClass.IsEmpty() && DeclaredClass != TargetWidget->GetClass()->GetPathName())
    {
        UE_LOG(LogUmgMcp, Warning, TEXT("UpsertWidgetFromJsonAppendOnly: Class mismatch for widget '%s' (JSON: %s, Existing: %s). Preserving existing class."), *TargetWidget->GetName(), *DeclaredClass, *TargetWidget->GetClass()->GetPathName());
    }

    ApplyPropertiesToExistingWidget(WidgetJson, TargetWidget);
    MergeChildrenAppendOnly(WidgetJson, WidgetTree, TargetWidget);
}

static UWidget* CreateWidgetFromJson(const TSharedPtr<FJsonObject>& WidgetJson, UWidgetTree* WidgetTree, UWidget* ParentWidget)
{
    FString ParentName = ParentWidget ? ParentWidget->GetName() : TEXT("None");
    // UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Starting for Parent: %s"), *ParentName);

    if (!WidgetJson.IsValid() || !WidgetTree)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("CreateWidgetFromJson: Invalid WidgetJson or WidgetTree."));
        return nullptr;
    }

    FString WidgetClassPath, WidgetName;
    if (!WidgetJson->TryGetStringField(TEXT("widget_class"), WidgetClassPath) || !WidgetJson->TryGetStringField(TEXT("widget_name"), WidgetName))
    {
        UE_LOG(LogUmgMcp, Error, TEXT("CreateWidgetFromJson: JSON is missing widget_class or widget_name."));
        return nullptr;
    }

    UClass* WidgetClass = StaticLoadClass(UWidget::StaticClass(), nullptr, *WidgetClassPath);
    if (!WidgetClass)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("CreateWidgetFromJson: Failed to find widget class '%s'."), *WidgetClassPath);
        return nullptr;
    }

    UWidget* NewWidget = NewObject<UWidget>(WidgetTree, WidgetClass, FName(*WidgetName));
    if (!NewWidget)
    {
        UE_LOG(LogUmgMcp, Error, TEXT("CreateWidgetFromJson: Failed to create widget of class '%s'."), *WidgetClassPath);
        return nullptr;
    }

    // 1. Add to parent to create the Slot
    UPanelSlot* NewSlot = nullptr;
    if (ParentWidget)
    {
        UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
        if (ParentPanel)
        {
            NewSlot = ParentPanel->AddChild(NewWidget);
            if (!NewSlot)
            {
                UE_LOG(LogUmgMcp, Warning, TEXT("CreateWidgetFromJson: AddChild returned null slot for '%s' in '%s'."), *WidgetName, *ParentName);
            }
        }
        else
        {
            UE_LOG(LogUmgMcp, Warning, TEXT("CreateWidgetFromJson: Parent '%s' is not a UPanelWidget, cannot add child '%s'."), *ParentName, *WidgetName);
        }
    }

    // 2. Prepare Property JSONs
    const TSharedPtr<FJsonObject>* PropertiesJsonObjPtr;
    TSharedPtr<FJsonObject> WidgetProps = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> SlotProps = nullptr;

    if (WidgetJson->TryGetObjectField(TEXT("properties"), PropertiesJsonObjPtr))
    {
        // Copy properties to a new object so we can remove 'Slot' without modifying source
        TSharedPtr<FJsonObject> SourceProps = *PropertiesJsonObjPtr;
        for (auto& Pair : SourceProps->Values)
        {
            if (Pair.Key == TEXT("Slot"))
            {
                const TSharedPtr<FJsonObject>* SlotObjPtr;
                if (Pair.Value->TryGetObject(SlotObjPtr))
                {
                    SlotProps = *SlotObjPtr;
                }
            }
            else
            {
                WidgetProps->SetField(Pair.Key, Pair.Value);
            }
        }
    }

    // 3. Apply Widget Properties using JsonObjectToUStruct (Safe & Robust)
    if (WidgetProps->Values.Num() > 0)
    {
        if (!FJsonObjectConverter::JsonObjectToUStruct(WidgetProps.ToSharedRef(), NewWidget->GetClass(), NewWidget, 0, 0))
        {
             UE_LOG(LogUmgMcp, Warning, TEXT("CreateWidgetFromJson: Issues applying properties to '%s'."), *WidgetName);
        }
    }

    // 4. Apply Slot Properties
    if (SlotProps.IsValid())
    {
        // Normalize JSON keys from camelCase to PascalCase to match C++ UPROPERTY names
        UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Processing Slot properties for widget '%s'"), *WidgetName);
        TSharedPtr<FJsonObject> NormalizedSlotProps = UUmgFileTransformation::NormalizeJsonKeysToPascalCase(SlotProps);
        
        // Log the normalized Slot JSON for debugging
        FString SlotPropsString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SlotPropsString);
        FJsonSerializer::Serialize(NormalizedSlotProps.ToSharedRef(), Writer);
        UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Normalized Slot JSON for '%s': %s"), *WidgetName, *SlotPropsString);
        
        if (NewSlot)
        {
            UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Applying Slot properties to NewSlot (class: %s)"), *NewSlot->GetClass()->GetName());
            
            if (!FJsonObjectConverter::JsonObjectToUStruct(NormalizedSlotProps.ToSharedRef(), NewSlot->GetClass(), NewSlot, 0, 0))
            {
                UE_LOG(LogUmgMcp, Warning, TEXT("CreateWidgetFromJson: Issues applying Slot properties to '%s'."), *WidgetName);
            }
            else
            {
                UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Successfully applied Slot properties to '%s'"), *WidgetName);
            }
        }
        else if (NewWidget->Slot)
        {
             // Fallback to NewWidget->Slot if AddChild didn't return one but it exists (e.g. RootWidget might not have slot, but this branch is for children)
             UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Applying Slot properties to NewWidget->Slot (class: %s)"), *NewWidget->Slot->GetClass()->GetName());
             
             if (!FJsonObjectConverter::JsonObjectToUStruct(NormalizedSlotProps.ToSharedRef(), NewWidget->Slot->GetClass(), NewWidget->Slot, 0, 0))
             {
                 UE_LOG(LogUmgMcp, Warning, TEXT("CreateWidgetFromJson: Issues applying Slot properties to '%s' (fallback)."), *WidgetName);
             }
             else
             {
                 UE_LOG(LogUmgMcp, Log, TEXT("CreateWidgetFromJson: Successfully applied Slot properties to '%s' (fallback)"), *WidgetName);
             }
        }
        else
        {
            UE_LOG(LogUmgMcp, Warning, TEXT("CreateWidgetFromJson: Slot properties specified but no valid Slot found for '%s'."), *WidgetName);
        }
    }

    // 5. Process Children
    const TArray<TSharedPtr<FJsonValue>>* ChildrenJsonArray;
    if (WidgetJson->TryGetArrayField(TEXT("children"), ChildrenJsonArray))
    {
        for (const TSharedPtr<FJsonValue>& ChildValue : *ChildrenJsonArray)
        {
            const TSharedPtr<FJsonObject>* ChildObject;
            if (ChildValue->TryGetObject(ChildObject))
            {
                CreateWidgetFromJson(*ChildObject, WidgetTree, NewWidget);
            }
        }
    }

    return NewWidget;
}
