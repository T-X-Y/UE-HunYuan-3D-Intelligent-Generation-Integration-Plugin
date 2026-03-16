#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "HunYuanAIStyle.h"

class FHunYuanAICommands : public TCommands<FHunYuanAICommands>
{
public:
    FHunYuanAICommands()
        : TCommands<FHunYuanAICommands>(
            TEXT("HunYuanAI"),
            NSLOCTEXT("Contexts", "HunYuanAI", "HunYuan AI Plugin"),
            NAME_None,
            FHunYuanAIStyle::GetStyleSetName())
    {
    }

    virtual void RegisterCommands() override;

public:
    TSharedPtr<FUICommandInfo> OpenPluginWindow;
};