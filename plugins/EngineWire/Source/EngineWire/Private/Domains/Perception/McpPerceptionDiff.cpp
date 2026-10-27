// Copyright (c) 2024 EngineWire Contributors
//
// describe_view's saveAs and compareWith: frames kept under a name until the editor closes, each with the renderer's
// own noise, measured by a second capture of the unchanged state (temporal anti-aliasing and denoisers never draw the
// same frame twice), and the camera it was taken from, which a comparison is taken from again.

#include "Domains/Perception/McpPerceptionDiff.h"

#include "Domains/Environment/Inspection/McpEnvInspectViewIds.h"
#include "Domains/Environment/McpEnvShared.h"
#include "Domains/Perception/McpPerception.h"
#include "EditorViewportClient.h"

namespace McpPerception
{
namespace
{
using Diff::FSavedView;

constexpr int32 SavedViewLimit = 8; // saving a ninth name forgets the oldest

TMap<FString, FSavedView>& SavedViews()
{
    static TMap<FString, FSavedView> Views; // the game thread's, as every handler runs there
    return Views;
}
} // namespace

bool PrepareViewDiff(const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FJsonObject>& OutCamera, FString& OutError, FString& OutCode)
{
    OutCamera = Payload;
    const FString Name = GetJsonStringField(Payload, TEXT("compareWith"));
    if (Name.IsEmpty())
    {
        return true;
    }
    const FSavedView* Saved = SavedViews().Find(Name);
    if (!Saved)
    {
        TArray<FString> Names;
        SavedViews().GetKeys(Names);
        OutError = FString::Printf(TEXT("No frame is saved as '%s' (saved: %s); describe_view saveAs keeps one until the editor closes."),
                                   *Name, Names.Num() > 0 ? *FString::Join(Names, TEXT(", ")) : TEXT("none"));
        OutCode = TEXT("VIEW_BASELINE_NOT_FOUND");
        return false;
    }
    if (!ExtractVectorField(Payload, TEXT("location"), Saved->Location).Equals(Saved->Location, 0.5) ||
        !ExtractRotatorField(Payload, TEXT("rotation"), Saved->Rotation).Equals(Saved->Rotation, 0.05f))
    {
        OutError = FString::Printf(TEXT("'%s' was taken from location (%.0f, %.0f, %.0f), rotation (pitch %.1f, yaw %.1f, roll "
                                        "%.1f); leave location and rotation out to compare from there."), *Name, Saved->Location.X,
                                   Saved->Location.Y, Saved->Location.Z, Saved->Rotation.Pitch, Saved->Rotation.Yaw, Saved->Rotation.Roll);
        OutCode = TEXT("CAMERA_MOVED");
        return false;
    }
    OutCamera = MakeShared<FJsonObject>(); // the capture reads only these two
    OutCamera->SetObjectField(TEXT("location"), McpHandlerUtils::VectorToJson(Saved->Location));
    OutCamera->SetObjectField(TEXT("rotation"), McpHandlerUtils::RotatorToJson(Saved->Rotation));
    return true;
}

bool DiffView(const TSharedPtr<FJsonObject>& Payload, const McpEnvironmentHandlers::FMcpViewCapture& View,
              TSharedPtr<FJsonObject>& OutChange, FString& OutError, FString& OutCode)
{
    Diff::FViewMeasure Now = Diff::MeasureFrame(View);
    const FString CompareName = GetJsonStringField(Payload, TEXT("compareWith"));
    if (const FSavedView* Saved = CompareName.IsEmpty() ? nullptr : SavedViews().Find(CompareName))
    {
        if (Saved->Frame.Size != Now.Size)
        {
            OutError = FString::Printf(TEXT("The viewport is %d x %d pixels; '%s' was saved at %d x %d. Save a frame at this size."),
                                       Now.Size.X, Now.Size.Y, *CompareName, Saved->Frame.Size.X, Saved->Frame.Size.Y);
            OutCode = TEXT("VIEW_SIZE_CHANGED");
            return false;
        }
        if (!FMath::IsNearlyEqual(Saved->Fov, View.Client->ViewFOV, 0.01f))
        {
            OutError = FString::Printf(TEXT("The viewport's field of view is %.1f degrees; '%s' was taken at %.1f."),
                                       View.Client->ViewFOV, *CompareName, Saved->Fov);
            OutCode = TEXT("CAMERA_MOVED");
            return false;
        }
        OutChange = Diff::CompareFrames(CompareName, *Saved, Now);
    }
    const FString SaveName = GetJsonStringField(Payload, TEXT("saveAs"));
    if (SaveName.IsEmpty())
    {
        return true;
    }
    McpEnvironmentHandlers::FMcpViewCapture Again; // the unchanged state drawn once more: what the renderer varies alone
    if (!McpEnvironmentHandlers::McpCaptureView(MakeShared<FJsonObject>(), TEXT("describe_view"), true, Again, OutError, OutCode))
    {
        return false;
    }
    static uint64 Saves = 0;
    FSavedView Entry;
    Diff::BlockChange(Now, Diff::MeasureFrame(Again), Entry.NoiseLoss, Entry.NoiseColour);
    Entry.Frame = MoveTemp(Now);
    Entry.Location = View.Client->GetViewLocation();
    Entry.Rotation = View.Client->GetViewRotation();
    Entry.Fov = View.Client->ViewFOV;
    Entry.Order = ++Saves;
    TMap<FString, FSavedView>& Views = SavedViews();
    if (!Views.Contains(SaveName) && Views.Num() >= SavedViewLimit)
    {
        FString Oldest;
        uint64 OldestOrder = MAX_uint64;
        for (const TPair<FString, FSavedView>& Pair : Views)
        {
            Oldest = Pair.Value.Order < OldestOrder ? Pair.Key : Oldest;
            OldestOrder = FMath::Min(OldestOrder, Pair.Value.Order);
        }
        Views.Remove(Oldest);
    }
    Views.Add(SaveName, MoveTemp(Entry));
    return true;
}
} // namespace McpPerception
