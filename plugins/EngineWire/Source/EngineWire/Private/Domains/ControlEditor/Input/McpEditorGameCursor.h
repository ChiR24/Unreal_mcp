#pragma once

#include "CoreMinimal.h"

// simulate_input mouse_move while a game runs. The game reads its cursor in its own viewport's pixels (PlayerController
// Get Mouse Position, Get Hit Result Under Cursor); a Slate event at desktop coordinates never reached it, so the game
// read no mouse at all and nothing aimed or picked by cursor could be tested. The real cursor is never moved.
// Returns false when no game runs, leaving the editor path to the caller; otherwise fills the reply flags and Message.
bool McpSimulateGameMouseMove(double X, double Y, bool &bSuccess, bool &bRoutedToPIE, bool &bHandledByPIE,
                              FString &Message);
