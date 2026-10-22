#include "Domains/PCG/Editing/McpPCGEditState.h"
#include "Serialization/ObjectReader.h"
#include "Serialization/ObjectWriter.h"
#include "UObject/UObjectHash.h"
#include "UObject/Package.h"
#include "EdGraph/EdGraph.h"
#include "Domains/PCG/Editor/McpPCGEditor.h"
#include "MCP/Gateway/McpNativeGatewayCanonicalJson.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
FPCGEditState::FRecord::FRecord(UObject* InObject)
    : Object(InObject), Outer(InObject->GetOuter()), Name(InObject->GetFName())
{
    FObjectWriter Writer(Bytes);
    Writer.ArIgnoreClassRef = true;
    Writer.ArIgnoreArchetypeRef = true;
    Writer.ArNoDelta = true;
    InObject->Serialize(Writer);
}

FPCGEditState::FPCGEditState(UPCGGraph* InGraph)
    : Graph(InGraph), Revision(GetPCGGraphRevision(InGraph)), bWasDirty(InGraph->GetOutermost()->IsDirty())
{
    TArray<UObject*> Objects;
    GetObjectsWithOuter(Graph, Objects, true);
    Objects.Insert(Graph, 0);
    bool bAmbiguous = false;
    UObject* EditorGraph = FindExistingPCGEditorGraph(Graph, bAmbiguous);
    EditorRepresentation.Reset(EditorGraph);
    McpCanonicalJsonObject(ReadPCGEditorGraphSnapshot(Graph), EditorFingerprint, true);
    if (UEdGraph* Editor = Cast<UEdGraph>(EditorGraph))
    {
        for (UEdGraphNode* Node : Editor->Nodes) if (Node) EditorNodes.Emplace(Node);
    }
    for (UObject* Object : Objects)
    {
        if (!Object) continue;
        OriginalObjects.Add(Object);
        if (!EditorGraph || (Object != EditorGraph && !Object->IsIn(EditorGraph))) Records.Emplace(Object);
    }
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    Graph->DisableNotificationsForEditor();
#endif
}

FPCGEditState::~FPCGEditState()
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    Graph->EnableNotificationsForEditor();
#endif
}

TArray<UObject*> FPCGEditState::GetObjects() const
{
    TArray<UObject*> Result;
    for (const FRecord& Record : Records) Result.Add(Record.Object.Get());
    if (EditorRepresentation.IsValid()) Result.Add(EditorRepresentation.Get());
    for (const auto& Node : EditorNodes) Result.Add(Node.Get());
    return Result;
}

bool FPCGEditState::Restore()
{
    const TSet<UObject*>& Original = OriginalObjects;
    TArray<UObject*> Current;
    GetObjectsWithOuter(Graph, Current, true);
    bool bRestored = !Revision.IsEmpty();
    for (UObject* Object : Current)
    {
        if (Object && !Original.Contains(Object) && Original.Contains(Object->GetOuter()) &&
            (!EditorRepresentation.IsValid() || (Object != EditorRepresentation.Get() && !Object->IsIn(EditorRepresentation.Get()))))
        {
            bRestored &= Object->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
        }
    }
    for (FRecord& Record : Records)
    {
        if (Record.Object->GetOuter() != Record.Outer || Record.Object->GetFName() != Record.Name)
        {
            bRestored &= Record.Object->Rename(*Record.Name.ToString(), Record.Outer, REN_DontCreateRedirectors | REN_NonTransactional);
        }
    }
    for (int32 Index = Records.Num() - 1; Index >= 0; --Index)
    {
        FRecord& Record = Records[Index];
        FObjectReader Reader(Record.Bytes);
        Reader.ArIgnoreClassRef = true;
        Reader.ArIgnoreArchetypeRef = true;
        Record.Object->Serialize(Reader);
        bRestored &= !Reader.IsError();
    }
    TArray<UEdGraphNode*> OriginalEditorNodes;
    for (const auto& Node : EditorNodes) OriginalEditorNodes.Add(Cast<UEdGraphNode>(Node.Get()));
    bRestored &= RestorePCGEditorGraphState(Graph, Cast<UEdGraph>(EditorRepresentation.Get()), OriginalEditorNodes);
    FString RestoredEditor;
    bRestored &= McpCanonicalJsonObject(ReadPCGEditorGraphSnapshot(Graph), RestoredEditor, true);
    Graph->GetOutermost()->SetDirtyFlag(bWasDirty);
    return bRestored && Revision == GetPCGGraphRevision(Graph) && EditorFingerprint == RestoredEditor;
}

bool ApplyPCGSettingsPayload(UPCGSettings* Settings, const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload, FString& OutError, int32& OutCount)
{
    OutCount = 0;
    const TSharedPtr<FJsonObject>* SettingsObject = nullptr;
    if (Payload->TryGetObjectField(TEXT("settings"), SettingsObject) && SettingsObject && SettingsObject->IsValid())
    {
        if (!ApplySettingsObject(Settings, *SettingsObject, OutError, OutCount)) return false;
    }
    int32 ConvenienceCount = 0;
    if (!ApplyPCGConvenienceSettings(SubAction, Settings, Payload, OutError, ConvenienceCount)) return false;
    OutCount += ConvenienceCount;
    return true;
}

UPCGSettings* PreparePCGSettings(UPCGSettings* Source, const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
    TStrongObjectPtr<UPCGSettings> Staged(Source ? DuplicateObject<UPCGSettings>(Source, GetTransientPackage()) : nullptr);
    if (!Staged.IsValid())
    {
        OutError = TEXT("Could not stage PCG settings.");
        return nullptr;
    }
    Staged->SetFlags(RF_Transient);
    int32 Count = 0;
    return ApplyPCGSettingsPayload(Staged.Get(), SubAction, Payload, OutError, Count) ? Staged.Get() : nullptr;
}
}
#endif
