// find_text: where a piece of text appears. search_assets matches asset names only, so "where does
// this string still show?" meant opening every widget, graph and table by hand. Under packagePaths
// it reads Blueprint graph pin literals, the assets picked on pins and comments, Blueprint variable and component defaults,
// widget tree properties (text block text, tooltips), DataTable rows and String Table entries; with
// includeLevel (default true) also every actor and component of the open level (TextRender text,
// strings set on a placed instance).
#include "Domains/AssetQuery/McpAutomationBridge_AssetQueryHandlersPrivate.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "EngineUtils.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Misc/PackageName.h"
#include "WidgetBlueprint.h"

namespace McpAssetQueryHandlers
{
namespace
{
struct FMcpFindTextScan
{
    FString Query;
    ESearchCase::Type Case = ESearchCase::IgnoreCase;
    int32 Limit = 50;
    int32 Total = 0;
    FString NodeId;
    TArray<TSharedPtr<FJsonValue>> Matches;

    void Hit(const FString& Asset, const FString& Where, const FString& Field, const FString& Text)
    {
        if (!Text.Contains(Query, Case) || ++Total > Limit)
        {
            return;
        }
        TSharedPtr<FJsonObject> Match = MakeShared<FJsonObject>();
        Match->SetStringField(TEXT("asset"), Asset);
        Match->SetStringField(TEXT("where"), Where);
        Match->SetStringField(TEXT("field"), Field);
        Match->SetStringField(TEXT("text"), Text);
        if (!NodeId.IsEmpty())
        {
            Match->SetStringField(TEXT("nodeId"), NodeId);
        }
        Matches.Add(MakeShared<FJsonValueObject>(Match));
    }

    // A reference to another asset counts by its path (a component's Sound set to MS_Music),
    // not by what it holds; links inside the same package and native classes are skipped.
    void Reference(const FString& Asset, const FString& Where, const FString& Field, const FString& Path)
    {
        if (!Path.IsEmpty() && !Path.StartsWith(TEXT("/Script/")) && FPackageName::ObjectPathToPackageName(Path) != FPackageName::ObjectPathToPackageName(Asset)) { Hit(Asset, Where, Field, Path); }
    }

    // Every string, text and name reachable through Struct's properties: nested structs and arrays
    // are followed; an object reference is matched by its path, never followed.
    void Properties(const UStruct* Struct, const void* Container, const FString& Asset, const FString& Where,
                    const FString& Prefix, int32 Depth = 0)
    {
        // Engine plumbing every actor carries: a search for "a" in one level returned 2,435 hits of
        // GameNetDriver, BlockAll and sprite categories.
        static const TSet<FName> Plumbing = {TEXT("NetDriverName"), TEXT("BodyInstance"), TEXT("SpriteInfo"), TEXT("FolderPath")};
        for (TFieldIterator<FProperty> It(Struct); It && Depth < 8; ++It)
        {
            for (int32 Index = 0; Index < It->ArrayDim && !Plumbing.Contains(It->GetFName()); ++Index)
            {
                Value(*It, It->ContainerPtrToValuePtr<void>(Container, Index), Asset, Where, Prefix + It->GetName(), Depth);
            }
        }
    }

    void Value(const FProperty* Property, const void* Data, const FString& Asset, const FString& Where,
               const FString& Field, int32 Depth)
    {
        if (const FStrProperty* Str = CastField<FStrProperty>(Property))
        {
            Hit(Asset, Where, Field, Str->GetPropertyValue(Data));
        }
        else if (const FTextProperty* Text = CastField<FTextProperty>(Property))
        {
            Hit(Asset, Where, Field, Text->GetPropertyValue(Data).ToString());
        }
        else if (const FNameProperty* Name = CastField<FNameProperty>(Property))
        {
            Hit(Asset, Where, Field, Name->GetPropertyValue(Data).ToString());
        }
        else if (const FObjectPropertyBase* Ref = CastField<FObjectPropertyBase>(Property))
        {
            const FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property);
            const UObject* Target = Soft ? nullptr : Ref->GetObjectPropertyValue(Data);
            Reference(Asset, Where, Field, Soft ? Soft->GetPropertyValue(Data).ToString() : (Target ? Target->GetPathName() : FString()));
        }
        else if (const FStructProperty* Nested = CastField<FStructProperty>(Property))
        {
            Properties(Nested->Struct, Data, Asset, Where, Field + TEXT("."), Depth + 1);
        }
        else if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
        {
            FScriptArrayHelper Helper(Array, Data);
            for (int32 Index = 0; Index < Helper.Num(); ++Index)
            {
                Value(Array->Inner, Helper.GetRawPtr(Index), Asset, Where, FString::Printf(TEXT("%s[%d]"), *Field, Index), Depth + 1);
            }
        }
    }

    void Object(const UObject* Object, const FString& Asset, const FString& Where)
    {
        Properties(Object->GetClass(), Object, Asset, Where, FString());
    }

    void Blueprint(const UBlueprint* Blueprint)
    {
        const FString Asset = Blueprint->GetPathName();
        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        for (const UEdGraph* Graph : Graphs)
        {
            if (!Graph)
            {
                continue;
            }
            for (const UEdGraphNode* Node : Graph->Nodes)
            {
                if (!Node)
                {
                    continue;
                }
                // The node id is what set_pin_default_value and delete_node take.
                NodeId = Node->NodeGuid.ToString();
                const FString Where = Graph->GetName() + TEXT(": ") + Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
                Hit(Asset, Where, TEXT("comment"), Node->NodeComment);
                for (const UEdGraphPin* Pin : Node->Pins)
                {
                    if (Pin && Pin->LinkedTo.Num() == 0)
                    {
                        Hit(Asset, Where, Pin->PinName.ToString(), Pin->DefaultValue);
                        Hit(Asset, Where, Pin->PinName.ToString(), Pin->DefaultTextValue.ToString());
                        // An asset picked on an object or class pin (PlaySound2D's Sound) is DefaultObject, not DefaultValue.
                        Reference(Asset, Where, Pin->PinName.ToString(), Pin->DefaultObject ? Pin->DefaultObject->GetPathName() : FString());
                    }
                }
            }
        }
        NodeId.Reset();
        if (Blueprint->SimpleConstructionScript)
        {
            for (const USCS_Node* ScsNode : Blueprint->SimpleConstructionScript->GetAllNodes())
            {
                if (ScsNode && ScsNode->ComponentTemplate)
                {
                    Object(ScsNode->ComponentTemplate, Asset, ScsNode->GetVariableName().ToString());
                }
            }
        }
        if (const UObject* Defaults = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject(false) : nullptr)
        {
            Object(Defaults, Asset, TEXT("class defaults"));
        }
        const UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Blueprint);
        if (WidgetBlueprint && WidgetBlueprint->WidgetTree)
        {
            WidgetBlueprint->WidgetTree->ForEachWidget([this, &Asset](UWidget* Widget)
            {
                if (Widget)
                {
                    Object(Widget, Asset, Widget->GetName());
                }
            });
        }
    }

    void Table(const UDataTable* Table)
    {
        const FString Asset = Table->GetPathName();
        const UScriptStruct* RowStruct = Table->GetRowStruct();
        for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
        {
            Hit(Asset, Row.Key.ToString(), TEXT("rowName"), Row.Key.ToString());
            if (RowStruct && Row.Value)
            {
                Properties(RowStruct, Row.Value, Asset, Row.Key.ToString(), FString());
            }
        }
    }

    void Strings(const UStringTable* Table)
    {
        const FString Asset = Table->GetPathName();
        auto Visit = [this, &Asset](const FString& Key, const FString& Source)
        {
            Hit(Asset, Key, TEXT("source"), Source);
            return true;
        };
        // Editor builds overload this with a DevNotes variant; the explicit signature picks the plain one.
        Table->GetStringTable()->EnumerateSourceStrings(TFunctionRef<bool(const FString&, const FString&)>(Visit));
    }

    void Level(UWorld* World)
    {
        const FString Asset = World->GetPackage()->GetName();
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            const FString Label = It->GetActorLabel();
            Object(*It, Asset, Label);
            for (const UActorComponent* Component : It->GetComponents())
            {
                if (Component)
                {
                    Object(Component, Asset, Label + TEXT(".") + Component->GetName());
                }
            }
        }
    }
};
}

bool HandleFindText(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FMcpFindTextScan Scan;
    Payload->TryGetStringField(TEXT("searchText"), Scan.Query);
    if (Scan.Query.IsEmpty())
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("find_text needs searchText, the text to look for."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    Scan.Case = GetJsonBoolField(Payload, TEXT("caseSensitive"), false) ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase;
    Scan.Limit = FMath::Clamp(static_cast<int32>(GetJsonNumberField(Payload, TEXT("limit"), 50)), 1, 500);

    FARFilter Filter;
    const TArray<TSharedPtr<FJsonValue>>* Paths = nullptr;
    if (Payload->TryGetArrayField(TEXT("packagePaths"), Paths) && Paths)
    {
        for (const TSharedPtr<FJsonValue>& PathValue : *Paths)
        {
            const FString RawPath = PathValue.IsValid() ? PathValue->AsString() : FString();
            const FString SanitizedPath = SanitizeProjectRelativePath(RawPath);
            if (SanitizedPath.IsEmpty())
            {
                Bridge->SendAutomationError(Socket, RequestId, McpPathRefusalMessage(TEXT("package path"), RawPath), TEXT("INVALID_PATH"));
                return true;
            }
            Filter.PackagePaths.Add(FName(*SanitizedPath));
        }
    }
    if (Filter.PackagePaths.Num() == 0)
    {
        Filter.PackagePaths.Add(FName(TEXT("/Game")));
    }
    Filter.bRecursivePaths = true;
    Filter.bRecursiveClasses = true;
    for (const UClass* Class : {UBlueprint::StaticClass(), UDataTable::StaticClass(), UStringTable::StaticClass()})
    {
#if ENGINE_MINOR_VERSION >= 1
        Filter.ClassPaths.Add(Class->GetClassPathName());
#else
        Filter.ClassNames.Add(Class->GetFName());
#endif
    }
    TArray<FAssetData> Assets;
    FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssets(Filter, Assets);

    for (const FAssetData& AssetData : Assets)
    {
        UObject* Asset = AssetData.GetAsset();
        if (const UBlueprint* Blueprint = Cast<UBlueprint>(Asset)) { Scan.Blueprint(Blueprint); }
        else if (const UDataTable* Table = Cast<UDataTable>(Asset)) { Scan.Table(Table); }
        else if (const UStringTable* Strings = Cast<UStringTable>(Asset)) { Scan.Strings(Strings); }
    }
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (World && GetJsonBoolField(Payload, TEXT("includeLevel"), true))
    {
        Scan.Level(World);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("matches"), Scan.Matches);
    Result->SetNumberField(TEXT("matchCount"), Scan.Total);
    Result->SetNumberField(TEXT("scannedAssets"), Assets.Num());
    Result->SetBoolField(TEXT("truncated"), Scan.Total > Scan.Matches.Num());
    Bridge->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("%d match(es) for '%s' in %d asset(s)%s"),
        Scan.Total, *Scan.Query, Assets.Num(), Scan.Total > Scan.Matches.Num() ? TEXT("; raise limit for the rest") : TEXT("")), Result);
    return true;
}
}
