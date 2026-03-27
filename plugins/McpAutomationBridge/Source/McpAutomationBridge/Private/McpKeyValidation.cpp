// McpKeyValidation.cpp
// Key name validation without FKey dependency.
// Uses a hardcoded static set of all standard EKeys names (UE 5.x).
// Zero includes beyond CoreMinimal — no InputCoreTypes.h required.

#include "CoreMinimal.h"
#include "Containers/Set.h"

namespace McpKeyValidation
{

static const TSet<FName>& GetRegisteredKeyNames()
{
  static TSet<FName> Keys = {
    // Mouse
    FName("LeftMouseButton"), FName("RightMouseButton"), FName("MiddleMouseButton"),
    FName("ThumbMouseButton"), FName("ThumbMouseButton2"),
    FName("MouseX"), FName("MouseY"), FName("MouseWheelAxis"),
    FName("MouseScrollUp"), FName("MouseScrollDown"),
    // Letters
    FName("A"), FName("B"), FName("C"), FName("D"), FName("E"), FName("F"),
    FName("G"), FName("H"), FName("I"), FName("J"), FName("K"), FName("L"),
    FName("M"), FName("N"), FName("O"), FName("P"), FName("Q"), FName("R"),
    FName("S"), FName("T"), FName("U"), FName("V"), FName("W"), FName("X"),
    FName("Y"), FName("Z"),
    // Digits
    FName("Zero"), FName("One"), FName("Two"), FName("Three"), FName("Four"),
    FName("Five"), FName("Six"), FName("Seven"), FName("Eight"), FName("Nine"),
    // Numpad
    FName("NumPadZero"), FName("NumPadOne"), FName("NumPadTwo"),
    FName("NumPadThree"), FName("NumPadFour"), FName("NumPadFive"),
    FName("NumPadSix"), FName("NumPadSeven"), FName("NumPadEight"), FName("NumPadNine"),
    FName("Multiply"), FName("Add"), FName("Subtract"), FName("Decimal"), FName("Divide"),
    // Function keys
    FName("F1"), FName("F2"), FName("F3"), FName("F4"), FName("F5"), FName("F6"),
    FName("F7"), FName("F8"), FName("F9"), FName("F10"), FName("F11"), FName("F12"),
    // Control keys
    FName("BackSpace"), FName("Tab"), FName("Enter"), FName("Pause"),
    FName("CapsLock"), FName("Escape"), FName("SpaceBar"),
    FName("PageUp"), FName("PageDown"), FName("End"), FName("Home"),
    FName("Left"), FName("Up"), FName("Right"), FName("Down"),
    FName("Insert"), FName("Delete"),
    // Modifiers
    FName("LeftShift"), FName("RightShift"),
    FName("LeftControl"), FName("RightControl"),
    FName("LeftAlt"), FName("RightAlt"),
    FName("LeftCommand"), FName("RightCommand"),
    FName("LeftWindows"), FName("RightWindows"),
    // Punctuation
    FName("Semicolon"), FName("Equals"), FName("Comma"), FName("Underscore"),
    FName("Hyphen"), FName("Period"), FName("Slash"), FName("Tilde"),
    FName("LeftBracket"), FName("Backslash"), FName("RightBracket"),
    FName("Apostrophe"), FName("Quote"),
    FName("LeftParantheses"), FName("RightParantheses"),
    FName("Ampersand"), FName("Asterix"), FName("Caret"),
    FName("Dollar"), FName("Exclamation"), FName("Colon"),
    // Misc keyboard
    FName("NumLock"), FName("ScrollLock"), FName("PrintScreen"),
    // Gamepad face
    FName("Gamepad_FaceButton_Bottom"), FName("Gamepad_FaceButton_Right"),
    FName("Gamepad_FaceButton_Left"), FName("Gamepad_FaceButton_Top"),
    // Gamepad shoulder / trigger
    FName("Gamepad_LeftShoulder"), FName("Gamepad_RightShoulder"),
    FName("Gamepad_LeftTrigger"), FName("Gamepad_RightTrigger"),
    FName("Gamepad_LeftTriggerAxis"), FName("Gamepad_RightTriggerAxis"),
    // Gamepad thumbsticks
    FName("Gamepad_LeftThumbstick"), FName("Gamepad_RightThumbstick"),
    FName("Gamepad_LeftStick_Up"), FName("Gamepad_LeftStick_Down"),
    FName("Gamepad_LeftStick_Right"), FName("Gamepad_LeftStick_Left"),
    FName("Gamepad_RightStick_Up"), FName("Gamepad_RightStick_Down"),
    FName("Gamepad_RightStick_Right"), FName("Gamepad_RightStick_Left"),
    FName("Gamepad_LeftX"), FName("Gamepad_LeftY"),
    FName("Gamepad_RightX"), FName("Gamepad_RightY"),
    // Gamepad dpad / special
    FName("Gamepad_DPad_Up"), FName("Gamepad_DPad_Down"),
    FName("Gamepad_DPad_Right"), FName("Gamepad_DPad_Left"),
    FName("Gamepad_Special_Left"), FName("Gamepad_Special_Right"),
    FName("Gamepad_Special_Left_X"), FName("Gamepad_Special_Left_Y"),
    FName("Gamepad_Back"), FName("Gamepad_Start"),
    // Touch
    FName("Touch1"), FName("Touch2"), FName("Touch3"), FName("Touch4"),
    FName("Touch5"), FName("Touch6"), FName("Touch7"), FName("Touch8"),
    FName("Touch9"), FName("Touch10"),
    FName("Gesture_Pinch"), FName("Gesture_Flick"), FName("Gesture_Rotate"),
    // Android
    FName("Android_Back"), FName("Android_Volume_Up"),
    FName("Android_Volume_Down"), FName("Android_Menu"),
    // Virtual
    FName("AnyKey"), FName("Invalid"),
  };
  return Keys;
}

bool IsKeyNameRegistered(const FString& KeyName)
{
  if (KeyName.IsEmpty()) return false;
  return GetRegisteredKeyNames().Contains(FName(*KeyName));
}

} // namespace McpKeyValidation
