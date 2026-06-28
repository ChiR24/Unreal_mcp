// McpAutomationBridge_GameplayTagHandlers.cpp
// Gameplay Tag operations: add/remove/list/query tags in the project dictionary
// and manage gameplay tags on actors.

#include "McpAutomationBridgeSubsystem.h"
#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameplayTagsManager.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsModule.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMcpGameplayTagHandlers, Log, All);

// ============================================================================
// Helper Functions
// ============================================================================
// Uses consolidated JSON helpers from McpAutomationBridgeHelpers.h:
//   - GetJsonStringField(Obj, Field, Default)
//   - GetJsonBoolField(Obj, Field, Default)

#if WITH_EDITOR

// ---------------------------------------------------------------------------
// Get the editor world
// ---------------------------------------------------------------------------
static UWorld* GetEditorWorld()
{
    if (GEditor)
    {
        return GEditor->GetEditorWorldContext().World();
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Find an actor by label or name (matches existing codebase convention)
// ---------------------------------------------------------------------------
static AActor* FindActorByLabelOrName(UWorld* World, const FString& ActorName)
{
    if (!World || ActorName.IsEmpty())
    {
        return nullptr;
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor)
        {
            if (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase))
            {
                return Actor;
            }
            if (Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase))
            {
                return Actor;
            }
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Validate a tag string: must be non-empty, dot-separated identifiers,
// no path traversal or special characters.
// ---------------------------------------------------------------------------
static bool ValidateTagName(const FString& TagName, FString& OutError)
{
    if (TagName.IsEmpty())
    {
        OutError = TEXT("tag name is required");
        return false;
    }

    if (TagName.Contains(TEXT("..")) || TagName.Contains(TEXT("/")) || TagName.Contains(TEXT("\\")))
    {
        OutError = TEXT("tag name must not contain path separators or traversal sequences");
        return false;
    }

    if (TagName.Contains(TEXT(":")) || TagName.Contains(TEXT(" ")))
    {
        OutError = TEXT("tag name must not contain colons or spaces");
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Find a FProperty of type FGameplayTagContainer on an actor, by property name
// ---------------------------------------------------------------------------
static FGameplayTagContainer* FindTagContainerProperty(AActor* Actor, const FString& PropertyName, FString& OutError)
{
    if (!Actor)
    {
        OutError = TEXT("Actor is null");
        return nullptr;
    }

    FProperty* Prop = Actor->GetClass()->FindPropertyByName(FName(*PropertyName));
    if (!Prop)
    {
        OutError = FString::Printf(TEXT("Property '%s' not found on actor '%s'"), *PropertyName, *Actor->GetActorLabel());
        return nullptr;
    }

    FStructProperty* StructProp = CastField<FStructProperty>(Prop);
    if (!StructProp || StructProp->Struct != FGameplayTagContainer::StaticStruct())
    {
        OutError = FString::Printf(TEXT("Property '%s' is not a FGameplayTagContainer"), *PropertyName);
        return nullptr;
    }

    void* ValuePtr = StructProp->ContainerPtrToValuePtr<void>(Actor);
    return static_cast<FGameplayTagContainer*>(ValuePtr);
}

// ---------------------------------------------------------------------------
// Recursively build a JSON tree from the gameplay tag tree
// ---------------------------------------------------------------------------
static TSharedPtr<FJsonObject> BuildTagHierarchyJson(const TSharedPtr<FGameplayTagNode>& Node)
{
    TSharedPtr<FJsonObject> NodeJson = MakeShareable(new FJsonObject());
    if (!Node.IsValid())
    {
        return NodeJson;
    }

    FString TagName = Node->GetCompleteTagName().ToString();
    NodeJson->SetStringField(TEXT("tag"), TagName);

    TArray<TSharedPtr<FGameplayTagNode>> ChildNodes = Node->GetChildTagNodes();
    TArray<TSharedPtr<FJsonValue>> ChildrenArray;
    for (const TSharedPtr<FGameplayTagNode>& Child : ChildNodes)
    {
        if (Child.IsValid())
        {
            TSharedPtr<FJsonObject> ChildJson = BuildTagHierarchyJson(Child);
            ChildrenArray.Add(MakeShareable(new FJsonValueObject(ChildJson)));
        }
    }
    NodeJson->SetArrayField(TEXT("children"), ChildrenArray);

    return NodeJson;
}

// ============================================================================
// SubAction: add_tag
// Add a new gameplay tag to the project's tag dictionary.
// Params: tag (string, dot-separated e.g. "Character.State.Dead"),
//         comment (string, optional description)
// ============================================================================
static bool HandleAddTag(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString TagName = GetJsonStringField(Payload, TEXT("tag"), TEXT(""));
    FString Comment = GetJsonStringField(Payload, TEXT("comment"), TEXT(""));

    FString ValidationError;
    if (!ValidateTagName(TagName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            ValidationError, nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

    // Check if tag already exists
    FGameplayTag ExistingTag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
    if (ExistingTag.IsValid())
    {
        TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
        ResultJson->SetStringField(TEXT("tag"), TagName);
        ResultJson->SetBoolField(TEXT("alreadyExists"), true);

        Subsystem->SendAutomationResponse(Socket, RequestId, true,
            FString::Printf(TEXT("Tag already exists: %s"), *TagName), ResultJson);
        return true;
    }

    // Add the tag via the native tag registration API
    Manager.AddNativeGameplayTag(FName(*TagName), Comment);

    // Force the tag manager to reconstruct the tree so the new tag is queryable
    // immediately. DoneAddingNativeTags will finalize the pending native tags.
    Manager.DoneAddingNativeTags();

    // Verify the tag was added
    FGameplayTag NewTag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("tag"), TagName);
    ResultJson->SetBoolField(TEXT("added"), NewTag.IsValid());
    if (!Comment.IsEmpty())
    {
        ResultJson->SetStringField(TEXT("comment"), Comment);
    }

    if (NewTag.IsValid())
    {
        UE_LOG(LogMcpGameplayTagHandlers, Log, TEXT("Added gameplay tag: %s"), *TagName);
        Subsystem->SendAutomationResponse(Socket, RequestId, true,
            FString::Printf(TEXT("Added gameplay tag: %s"), *TagName), ResultJson);
    }
    else
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to add gameplay tag: %s"), *TagName), ResultJson, TEXT("ADD_FAILED"));
    }

    return true;
}

// ============================================================================
// SubAction: remove_tag
// Remove a tag from the project dictionary.
// Params: tag (string)
// Note: Removing native tags at runtime is limited. This removes from the
// manager's tag map but may not persist across editor restarts without
// editing the GameplayTags .ini or DataTable source.
// ============================================================================
static bool HandleRemoveTag(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString TagName = GetJsonStringField(Payload, TEXT("tag"), TEXT(""));

    FString ValidationError;
    if (!ValidateTagName(TagName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            ValidationError, nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
    if (!Tag.IsValid())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Tag does not exist: %s"), *TagName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // Use IGameplayTagsModule to remove the tag from the editor tag sources
    IGameplayTagsModule& TagsModule = IGameplayTagsModule::Get();

    // Attempt removal via the GameplayTagsManager destructive API.
    // DestroyGameplayTag is available for editor-added tags (ini-sourced).
    UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

    // The safest approach is to remove from the default tag list ini.
    // UGameplayTagsManager provides DeleteTagRedirect and related methods,
    // but direct removal of a tag from the runtime tree is not officially
    // supported. We can at minimum remove it from the ini-based tag list.
    TSharedPtr<FGameplayTagNode> TagNode = Manager.FindTagNode(Tag);
    if (!TagNode.IsValid())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Tag node not found in tree: %s"), *TagName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // Check if this tag has children - warn but still allow removal
    TArray<TSharedPtr<FGameplayTagNode>> ChildNodes = TagNode->GetChildTagNodes();
    bool bHasChildren = ChildNodes.Num() > 0;

    // Remove from the default GameplayTags ini source
    // The tag source file is typically DefaultGameplayTags.ini
    FString ConfigFileName = FPaths::ProjectConfigDir() / TEXT("DefaultGameplayTags.ini");

    bool bRemovedFromIni = false;
    if (FPaths::FileExists(ConfigFileName))
    {
        // Load the config file and remove the tag entry
        FString FileContents;
        if (FFileHelper::LoadFileToString(FileContents, *ConfigFileName))
        {
            // Tags are stored as +GameplayTagList=(Tag="TagName",DevComment="Comment")
            FString SearchPattern = FString::Printf(TEXT("+GameplayTagList=(Tag=\"%s\""), *TagName);
            if (FileContents.Contains(SearchPattern))
            {
                // Remove the line containing this tag
                TArray<FString> Lines;
                FileContents.ParseIntoArrayLines(Lines);
                FString NewContents;
                for (const FString& Line : Lines)
                {
                    if (!Line.Contains(SearchPattern))
                    {
                        NewContents += Line + TEXT("\n");
                    }
                }
                bRemovedFromIni = FFileHelper::SaveStringToFile(NewContents, *ConfigFileName);
            }
        }
    }

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("tag"), TagName);
    ResultJson->SetBoolField(TEXT("removedFromIni"), bRemovedFromIni);
    ResultJson->SetBoolField(TEXT("hadChildren"), bHasChildren);
    if (bHasChildren)
    {
        ResultJson->SetNumberField(TEXT("childCount"), ChildNodes.Num());
    }

    UE_LOG(LogMcpGameplayTagHandlers, Log, TEXT("Removed gameplay tag: %s (ini=%s)"),
        *TagName, bRemovedFromIni ? TEXT("yes") : TEXT("no"));

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Removed gameplay tag: %s (note: restart editor to fully unregister)"), *TagName), ResultJson);

    return true;
}

// ============================================================================
// SubAction: list_tags
// List all registered tags, optionally filtered by a parent prefix.
// Params: filter (string, optional - e.g. "Character" to list all under Character.*)
// ============================================================================
static bool HandleListTags(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Filter = GetJsonStringField(Payload, TEXT("filter"), TEXT(""));

    UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

    FGameplayTagContainer AllTags;
    Manager.RequestAllGameplayTags(AllTags, true);

    TArray<TSharedPtr<FJsonValue>> TagArray;
    for (const FGameplayTag& Tag : AllTags)
    {
        FString TagStr = Tag.GetTagName().ToString();

        // Apply filter if provided
        if (!Filter.IsEmpty())
        {
            // Match tags that start with the filter prefix (with or without trailing dot)
            FString Prefix = Filter;
            if (!Prefix.EndsWith(TEXT(".")))
            {
                Prefix += TEXT(".");
            }
            if (!TagStr.StartsWith(Prefix, ESearchCase::IgnoreCase) &&
                !TagStr.Equals(Filter, ESearchCase::IgnoreCase))
            {
                continue;
            }
        }

        TagArray.Add(MakeShareable(new FJsonValueString(TagStr)));
    }

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetArrayField(TEXT("tags"), TagArray);
    ResultJson->SetNumberField(TEXT("count"), TagArray.Num());
    if (!Filter.IsEmpty())
    {
        ResultJson->SetStringField(TEXT("filter"), Filter);
    }

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Found %d gameplay tags"), TagArray.Num()), ResultJson);

    return true;
}

// ============================================================================
// SubAction: get_tag_children
// Get direct children of a tag.
// Params: tag (string - the parent tag)
// ============================================================================
static bool HandleGetTagChildren(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString TagName = GetJsonStringField(Payload, TEXT("tag"), TEXT(""));

    FString ValidationError;
    if (!ValidateTagName(TagName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            ValidationError, nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    FGameplayTag ParentTag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
    if (!ParentTag.IsValid())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Tag does not exist: %s"), *TagName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
    TSharedPtr<FGameplayTagNode> TagNode = Manager.FindTagNode(ParentTag);

    TArray<TSharedPtr<FJsonValue>> ChildArray;
    if (TagNode.IsValid())
    {
        TArray<TSharedPtr<FGameplayTagNode>> ChildNodes = TagNode->GetChildTagNodes();
        for (const TSharedPtr<FGameplayTagNode>& Child : ChildNodes)
        {
            if (Child.IsValid())
            {
                TSharedPtr<FJsonObject> ChildObj = MakeShareable(new FJsonObject());
                ChildObj->SetStringField(TEXT("tag"), Child->GetCompleteTagName().ToString());
                ChildObj->SetStringField(TEXT("shortName"), Child->GetSimpleTagName().ToString());
                ChildObj->SetNumberField(TEXT("childCount"), Child->GetChildTagNodes().Num());
                ChildArray.Add(MakeShareable(new FJsonValueObject(ChildObj)));
            }
        }
    }

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("parentTag"), TagName);
    ResultJson->SetArrayField(TEXT("children"), ChildArray);
    ResultJson->SetNumberField(TEXT("count"), ChildArray.Num());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Tag '%s' has %d direct children"), *TagName, ChildArray.Num()), ResultJson);

    return true;
}

// ============================================================================
// SubAction: has_tag
// Check if a tag exists in the project dictionary.
// Params: tag (string)
// ============================================================================
static bool HandleHasTag(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString TagName = GetJsonStringField(Payload, TEXT("tag"), TEXT(""));

    FString ValidationError;
    if (!ValidateTagName(TagName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            ValidationError, nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("tag"), TagName);
    ResultJson->SetBoolField(TEXT("exists"), Tag.IsValid());

    if (Tag.IsValid())
    {
        // Also report child count
        UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
        TSharedPtr<FGameplayTagNode> TagNode = Manager.FindTagNode(Tag);
        if (TagNode.IsValid())
        {
            ResultJson->SetNumberField(TEXT("childCount"), TagNode->GetChildTagNodes().Num());
        }
    }

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Tag '%s' %s"), *TagName, Tag.IsValid() ? TEXT("exists") : TEXT("does not exist")),
        ResultJson);

    return true;
}

// ============================================================================
// SubAction: add_tag_to_actor
// Add a gameplay tag to an actor. If propertyName is specified, looks for a
// FGameplayTagContainer property on the actor. Otherwise, adds to the actor's
// built-in Tags array as a string representation.
// Params: actorName (string), tag (string), propertyName (string, optional)
// ============================================================================
static bool HandleAddTagToActor(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"), TEXT(""));
    FString TagName = GetJsonStringField(Payload, TEXT("tag"), TEXT(""));
    FString PropertyName = GetJsonStringField(Payload, TEXT("propertyName"), TEXT(""));

    if (ActorName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    FString ValidationError;
    if (!ValidateTagName(TagName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            ValidationError, nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    AActor* Actor = FindActorByLabelOrName(World, ActorName);
    if (!Actor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // Verify the tag exists in the project dictionary
    FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
    if (!Tag.IsValid())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Tag does not exist in project dictionary: %s. Add it first with add_tag."), *TagName),
            nullptr, TEXT("TAG_NOT_FOUND"));
        return true;
    }

    if (!PropertyName.IsEmpty())
    {
        // Add to a FGameplayTagContainer property on the actor
        FString PropError;
        FGameplayTagContainer* Container = FindTagContainerProperty(Actor, PropertyName, PropError);
        if (!Container)
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                PropError, nullptr, TEXT("PROPERTY_ERROR"));
            return true;
        }

        bool bAlreadyHad = Container->HasTag(Tag);
        if (!bAlreadyHad)
        {
            Container->AddTag(Tag);
            Actor->MarkPackageDirty();
        }

        TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
        ResultJson->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
        ResultJson->SetStringField(TEXT("tag"), TagName);
        ResultJson->SetStringField(TEXT("propertyName"), PropertyName);
        ResultJson->SetBoolField(TEXT("alreadyHadTag"), bAlreadyHad);
        ResultJson->SetBoolField(TEXT("added"), !bAlreadyHad);

        Subsystem->SendAutomationResponse(Socket, RequestId, true,
            FString::Printf(TEXT("Tag '%s' %s actor '%s' property '%s'"),
                *TagName, bAlreadyHad ? TEXT("already on") : TEXT("added to"),
                *Actor->GetActorLabel(), *PropertyName),
            ResultJson);
    }
    else
    {
        // Add to the actor's built-in Tags array (TArray<FName>)
        FName TagFName(*TagName);
        bool bAlreadyHad = Actor->Tags.Contains(TagFName);
        if (!bAlreadyHad)
        {
            Actor->Tags.Add(TagFName);
            Actor->MarkPackageDirty();
        }

        TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
        ResultJson->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
        ResultJson->SetStringField(TEXT("tag"), TagName);
        ResultJson->SetBoolField(TEXT("alreadyHadTag"), bAlreadyHad);
        ResultJson->SetBoolField(TEXT("added"), !bAlreadyHad);
        ResultJson->SetNumberField(TEXT("totalTags"), Actor->Tags.Num());

        Subsystem->SendAutomationResponse(Socket, RequestId, true,
            FString::Printf(TEXT("Tag '%s' %s actor '%s' Tags array"),
                *TagName, bAlreadyHad ? TEXT("already on") : TEXT("added to"),
                *Actor->GetActorLabel()),
            ResultJson);
    }

    return true;
}

// ============================================================================
// SubAction: remove_tag_from_actor
// Remove a gameplay tag from an actor.
// Params: actorName (string), tag (string), propertyName (string, optional)
// ============================================================================
static bool HandleRemoveTagFromActor(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"), TEXT(""));
    FString TagName = GetJsonStringField(Payload, TEXT("tag"), TEXT(""));
    FString PropertyName = GetJsonStringField(Payload, TEXT("propertyName"), TEXT(""));

    if (ActorName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    FString ValidationError;
    if (!ValidateTagName(TagName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            ValidationError, nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    AActor* Actor = FindActorByLabelOrName(World, ActorName);
    if (!Actor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    if (!PropertyName.IsEmpty())
    {
        // Remove from a FGameplayTagContainer property
        FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
        if (!Tag.IsValid())
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Tag does not exist: %s"), *TagName), nullptr, TEXT("TAG_NOT_FOUND"));
            return true;
        }

        FString PropError;
        FGameplayTagContainer* Container = FindTagContainerProperty(Actor, PropertyName, PropError);
        if (!Container)
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                PropError, nullptr, TEXT("PROPERTY_ERROR"));
            return true;
        }

        bool bHadTag = Container->HasTag(Tag);
        if (bHadTag)
        {
            Container->RemoveTag(Tag);
            Actor->MarkPackageDirty();
        }

        TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
        ResultJson->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
        ResultJson->SetStringField(TEXT("tag"), TagName);
        ResultJson->SetStringField(TEXT("propertyName"), PropertyName);
        ResultJson->SetBoolField(TEXT("removed"), bHadTag);

        Subsystem->SendAutomationResponse(Socket, RequestId, true,
            FString::Printf(TEXT("Tag '%s' %s actor '%s' property '%s'"),
                *TagName, bHadTag ? TEXT("removed from") : TEXT("was not on"),
                *Actor->GetActorLabel(), *PropertyName),
            ResultJson);
    }
    else
    {
        // Remove from the actor's built-in Tags array
        FName TagFName(*TagName);
        bool bHadTag = Actor->Tags.Contains(TagFName);
        if (bHadTag)
        {
            Actor->Tags.Remove(TagFName);
            Actor->MarkPackageDirty();
        }

        TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
        ResultJson->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
        ResultJson->SetStringField(TEXT("tag"), TagName);
        ResultJson->SetBoolField(TEXT("removed"), bHadTag);
        ResultJson->SetNumberField(TEXT("totalTags"), Actor->Tags.Num());

        Subsystem->SendAutomationResponse(Socket, RequestId, true,
            FString::Printf(TEXT("Tag '%s' %s actor '%s' Tags array"),
                *TagName, bHadTag ? TEXT("removed from") : TEXT("was not on"),
                *Actor->GetActorLabel()),
            ResultJson);
    }

    return true;
}

// ============================================================================
// SubAction: get_actor_tags
// Get all gameplay tags on an actor.
// Params: actorName (string), propertyName (string, optional)
// If propertyName is given, reads from that FGameplayTagContainer property.
// Otherwise returns the actor's built-in Tags array.
// ============================================================================
static bool HandleGetActorTags(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"), TEXT(""));
    FString PropertyName = GetJsonStringField(Payload, TEXT("propertyName"), TEXT(""));

    if (ActorName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    AActor* Actor = FindActorByLabelOrName(World, ActorName);
    if (!Actor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> TagArray;

    if (!PropertyName.IsEmpty())
    {
        // Read from FGameplayTagContainer property
        FString PropError;
        FGameplayTagContainer* Container = FindTagContainerProperty(Actor, PropertyName, PropError);
        if (!Container)
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                PropError, nullptr, TEXT("PROPERTY_ERROR"));
            return true;
        }

        for (const FGameplayTag& Tag : *Container)
        {
            TagArray.Add(MakeShareable(new FJsonValueString(Tag.GetTagName().ToString())));
        }
    }
    else
    {
        // Read from the actor's built-in Tags array (TArray<FName>)
        for (const FName& TagFName : Actor->Tags)
        {
            TagArray.Add(MakeShareable(new FJsonValueString(TagFName.ToString())));
        }
    }

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
    ResultJson->SetArrayField(TEXT("tags"), TagArray);
    ResultJson->SetNumberField(TEXT("count"), TagArray.Num());
    if (!PropertyName.IsEmpty())
    {
        ResultJson->SetStringField(TEXT("propertyName"), PropertyName);
        ResultJson->SetStringField(TEXT("source"), TEXT("GameplayTagContainer"));
    }
    else
    {
        ResultJson->SetStringField(TEXT("source"), TEXT("ActorTags"));
    }

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Actor '%s' has %d tags"), *Actor->GetActorLabel(), TagArray.Num()),
        ResultJson);

    return true;
}

// ============================================================================
// SubAction: get_tag_hierarchy
// Get the full tag tree as a nested JSON structure.
// Params: root (string, optional - if provided, returns hierarchy under that tag)
// ============================================================================
static bool HandleGetTagHierarchy(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString RootFilter = GetJsonStringField(Payload, TEXT("root"), TEXT(""));

    UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

    // Get the root-level tag nodes from the manager
    const FGameplayTagContainer& AllTagsContainer = Manager.RequestGameplayTagParents(
        FGameplayTag());

    // To get the actual tree structure, we access the tag node tree.
    // The manager's tag tree root contains all top-level nodes.
    TArray<TSharedPtr<FJsonValue>> RootNodes;

    if (!RootFilter.IsEmpty())
    {
        // Start from a specific tag
        FGameplayTag RootTag = FGameplayTag::RequestGameplayTag(FName(*RootFilter), false);
        if (!RootTag.IsValid())
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Root tag does not exist: %s"), *RootFilter), nullptr, TEXT("NOT_FOUND"));
            return true;
        }

        TSharedPtr<FGameplayTagNode> RootNode = Manager.FindTagNode(RootTag);
        if (RootNode.IsValid())
        {
            TSharedPtr<FJsonObject> RootJson = BuildTagHierarchyJson(RootNode);
            RootNodes.Add(MakeShareable(new FJsonValueObject(RootJson)));
        }
    }
    else
    {
        // Get all root-level tags by iterating top-level nodes
        // We collect all tags and find the ones with no parent (single-segment names)
        FGameplayTagContainer AllTags;
        Manager.RequestAllGameplayTags(AllTags, true);

        // Build a set of all tag names for quick lookup
        TSet<FString> AllTagNames;
        for (const FGameplayTag& Tag : AllTags)
        {
            AllTagNames.Add(Tag.GetTagName().ToString());
        }

        // Find root tags: tags whose parent (everything before the last dot) doesn't exist
        // or tags that have no dot (single segment)
        TSet<FString> ProcessedRoots;
        for (const FGameplayTag& Tag : AllTags)
        {
            FString TagStr = Tag.GetTagName().ToString();

            // Get the first segment (root)
            FString RootSegment;
            int32 DotIndex;
            if (TagStr.FindChar(TEXT('.'), DotIndex))
            {
                RootSegment = TagStr.Left(DotIndex);
            }
            else
            {
                RootSegment = TagStr;
            }

            if (!ProcessedRoots.Contains(RootSegment))
            {
                ProcessedRoots.Add(RootSegment);
                FGameplayTag RootTag = FGameplayTag::RequestGameplayTag(FName(*RootSegment), false);
                if (RootTag.IsValid())
                {
                    TSharedPtr<FGameplayTagNode> Node = Manager.FindTagNode(RootTag);
                    if (Node.IsValid())
                    {
                        TSharedPtr<FJsonObject> NodeJson = BuildTagHierarchyJson(Node);
                        RootNodes.Add(MakeShareable(new FJsonValueObject(NodeJson)));
                    }
                }
            }
        }
    }

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetArrayField(TEXT("hierarchy"), RootNodes);
    ResultJson->SetNumberField(TEXT("rootCount"), RootNodes.Num());
    if (!RootFilter.IsEmpty())
    {
        ResultJson->SetStringField(TEXT("root"), RootFilter);
    }

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Tag hierarchy: %d root nodes"), RootNodes.Num()), ResultJson);

    return true;
}

#endif // WITH_EDITOR

// ============================================================================
// Main Dispatcher
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleGameplayTags(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"), TEXT(""));

    UE_LOG(LogMcpGameplayTagHandlers, Verbose, TEXT("HandleGameplayTags: SubAction=%s"), *SubAction);

    // Project tag dictionary operations
    if (SubAction == TEXT("add_tag"))
    {
        return HandleAddTag(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("remove_tag"))
    {
        return HandleRemoveTag(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("list_tags"))
    {
        return HandleListTags(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("get_tag_children"))
    {
        return HandleGetTagChildren(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("has_tag"))
    {
        return HandleHasTag(this, RequestId, Payload, Socket);
    }

    // Actor tag operations
    if (SubAction == TEXT("add_tag_to_actor"))
    {
        return HandleAddTagToActor(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("remove_tag_from_actor"))
    {
        return HandleRemoveTagFromActor(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("get_actor_tags"))
    {
        return HandleGetActorTags(this, RequestId, Payload, Socket);
    }

    // Hierarchy operations
    if (SubAction == TEXT("get_tag_hierarchy"))
    {
        return HandleGetTagHierarchy(this, RequestId, Payload, Socket);
    }

    // Unknown subAction
    SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("Unknown subAction for manage_gameplay_tags: '%s'"), *SubAction),
        nullptr, TEXT("UNKNOWN_SUBACTION"));
    return true;

#else
    SendAutomationResponse(Socket, RequestId, false,
        TEXT("Gameplay tag operations require editor mode (WITH_EDITOR)"),
        nullptr, TEXT("EDITOR_ONLY"));
    return true;
#endif
}
