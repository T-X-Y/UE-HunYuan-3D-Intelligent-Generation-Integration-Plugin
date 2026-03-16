#include "HunYuanAICommands.h"

#define LOCTEXT_NAMESPACE "FHunYuanAIModule"

void FHunYuanAICommands::RegisterCommands()
{
    UI_COMMAND(OpenPluginWindow, "HunYuanAI", "Bring up HunYuanAI window",
        EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE