// McpViewSettle.cpp — a viewport capture waits for the frame to stop changing before it is read.
//
// Nanite cluster pages and texture mips a fresh view asks for stream in over the next engine ticks, from async reads
// that no amount of drawing inside one call completes; a capture taken at once drew a just-built or just-framed
// Nanite mesh as blobs of its coarsest clusters, which read as a broken mesh. The capture compares the frame with
// the one it recorded on its previous pass and takes the picture again a beat later until two in a row match.
#include "Domains/ControlEditor/McpEditorScreenshotSupport.h"

#include "Misc/Parse.h"

namespace
{
constexpr int32 SettleBlock = 32;
constexpr int32 MaxSettleRounds = 5;

// The frame as the mean colour of each 32 px block, two hex digits a block: block means, so anti-aliasing jitter
// along an edge is not change.
FString FrameSignature(const TArray<FColor>& Pixels, const FIntPoint Size)
{
    FString Signature;
    for (int32 BlockY = 0; BlockY + SettleBlock <= Size.Y; BlockY += SettleBlock)
    {
        for (int32 BlockX = 0; BlockX + SettleBlock <= Size.X; BlockX += SettleBlock)
        {
            int32 Sum = 0;
            for (int32 Y = BlockY; Y < BlockY + SettleBlock; Y += 8)
            {
                for (int32 X = BlockX; X < BlockX + SettleBlock; X += 8)
                {
                    const FColor& Pixel = Pixels[Y * Size.X + X];
                    Sum += Pixel.R + Pixel.G + Pixel.B;
                }
            }
            Signature += FString::Printf(TEXT("%02x"), Sum / 48);
        }
    }
    return Signature;
}

// Under 0.5% of the blocks moved their mean by more than 6 of 255.
bool SignaturesMatch(const FString& A, const FString& B)
{
    if (A.Len() != B.Len() || A.IsEmpty()) return false;
    int32 Changed = 0;
    for (int32 At = 0; At + 1 < A.Len(); At += 2)
    {
        const int32 Left = FParse::HexDigit(A[At]) * 16 + FParse::HexDigit(A[At + 1]);
        const int32 Right = FParse::HexDigit(B[At]) * 16 + FParse::HexDigit(B[At + 1]);
        Changed += FMath::Abs(Left - Right) > 6 ? 1 : 0;
    }
    return Changed * 200 <= A.Len() / 2;
}
} // namespace

bool ViewFrameSettledForMcp(FViewport* Viewport, const TSharedPtr<FJsonObject>& Payload)
{
    TArray<FColor> Pixels;
    const FIntPoint Size = Viewport ? Viewport->GetSizeXY() : FIntPoint::ZeroValue;
    if (!Viewport || !Viewport->ReadPixels(Pixels, FReadSurfaceDataFlags(RCM_UNorm)) || Pixels.Num() < Size.X * Size.Y) return true;
    const FString Signature = FrameSignature(Pixels, Size);
    double Round = 0.0;
    Payload->TryGetNumberField(TEXT("_settleRound"), Round);
    if (SignaturesMatch(Signature, GetJsonStringField(Payload, TEXT("_settleFrame"))) || Round >= MaxSettleRounds) return true;
    Payload->SetStringField(TEXT("_settleFrame"), Signature);
    Payload->SetNumberField(TEXT("_settleRound"), Round + 1.0);
    return false;
}
