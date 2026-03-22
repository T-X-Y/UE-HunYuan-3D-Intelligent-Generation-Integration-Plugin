// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Modules/ModuleManager.h"
#include "API/HunYuanAPI.h"

class FToolBarBuilder;
class FMenuBuilder;

class FHunYuanAIModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /** This function will be bound to Command */
    void PluginButtonClicked();

    /** Get module instance */
    static FHunYuanAIModule& Get();

    /** Get API instance */
    TSharedPtr<FHunYuanAPI> GetAPI() const { return API; }

    // Tick 函数
    bool Tick(float DeltaTime);

private:
    void RegisterMenus();
    TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

    TSharedPtr<class FUICommandList> PluginCommands;
    TSharedPtr<FHunYuanAPI> API;  // 添加 API 成员

    // Tick 委托句柄
    FTSTicker::FDelegateHandle TickDelegateHandle;
};