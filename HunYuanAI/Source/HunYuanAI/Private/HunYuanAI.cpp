// Copyright Epic Games, Inc. All Rights Reserved.

#include "HunYuanAI.h"
#include "HunYuanAIStyle.h"
#include "HunYuanAICommands.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "UI/SAIChatWindow.h"
#include "Containers/Ticker.h"
#include "Logging/HunYuanLogging.h"  // 添加日志

static const FName HunYuanAITabName("HunYuanAI");

#define LOCTEXT_NAMESPACE "FHunYuanAIModule"

void FHunYuanAIModule::StartupModule()
{
    // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

    UE_LOG(LogHunYuanAI, Log, TEXT("HunYuanAI module starting up..."));

    // 创建API实例
    API = FHunYuanAPI::Get();

    // 检查 API 是否就绪
    if (API.IsValid())
    {
        UE_LOG(LogHunYuanAI, Log, TEXT("HunYuanAPI initialized successfully"));
    }
    else
    {
        UE_LOG(LogHunYuanAI, Error, TEXT("Failed to initialize HunYuanAPI"));
    }

    // 注册 Tick 委托
    FTSTicker& Ticker = FTSTicker::GetCoreTicker();
    TickDelegateHandle = Ticker.AddTicker(
        FTickerDelegate::CreateRaw(this, &FHunYuanAIModule::Tick),
        0.0f  // 每帧调用
    );
    UE_LOG(LogHunYuanAI, Log, TEXT("Tick delegate registered"));

    FHunYuanAIStyle::Initialize();
    FHunYuanAIStyle::ReloadTextures();

    FHunYuanAICommands::Register();

    PluginCommands = MakeShareable(new FUICommandList);

    PluginCommands->MapAction(
        FHunYuanAICommands::Get().OpenPluginWindow,
        FExecuteAction::CreateRaw(this, &FHunYuanAIModule::PluginButtonClicked),
        FCanExecuteAction());

    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FHunYuanAIModule::RegisterMenus));

    // 注册选项卡生成器
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        HunYuanAITabName,
        FOnSpawnTab::CreateRaw(this, &FHunYuanAIModule::OnSpawnPluginTab))
        .SetDisplayName(LOCTEXT("HunYuanAITabTitle", "HunYuan AI"))
        .SetTooltipText(LOCTEXT("HunYuanAITabTooltip", "Open HunYuan AI Chat Window"))
        .SetIcon(FSlateIcon(FHunYuanAIStyle::GetStyleSetName(), "HunYuanAI.TabIcon"))
        .SetMenuType(ETabSpawnerMenuType::Enabled);  // 改为 Enabled 而不是 Hidden
}

bool FHunYuanAIModule::Tick(float DeltaTime)
{
    // 确保 API 实例存在并调用其 Tick
    if (API.IsValid())
    {
        API->Tick();
    }
    return true;  // 返回 true 继续接收 Tick
}

void FHunYuanAIModule::ShutdownModule()
{
    // This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
    // we call this function before unloading the module.

    UE_LOG(LogHunYuanAI, Log, TEXT("HunYuanAI module shutting down..."));

    // 移除 Tick 委托
    if (TickDelegateHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
        TickDelegateHandle.Reset();
    }

    // 注销选项卡生成器
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(HunYuanAITabName);

    // 清理 API
    if (API.IsValid())
    {
        API->CancelAllRequests();
        FHunYuanAPI::Shutdown();
        API.Reset();
    }

    UToolMenus::UnRegisterStartupCallback(this);

    UToolMenus::UnregisterOwner(this);

    FHunYuanAIStyle::Shutdown();

    FHunYuanAICommands::Unregister();
}

void FHunYuanAIModule::PluginButtonClicked()
{
    // 打开插件主窗口
    FGlobalTabmanager::Get()->TryInvokeTab(HunYuanAITabName);
}

void FHunYuanAIModule::RegisterMenus()
{
    // Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
    FToolMenuOwnerScoped OwnerScoped(this);

    {
        UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
        {
            FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
            Section.AddMenuEntryWithCommandList(FHunYuanAICommands::Get().OpenPluginWindow, PluginCommands);
        }
    }

    {
        UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
        {
            FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
            {
                FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FHunYuanAICommands::Get().OpenPluginWindow));
                Entry.SetCommandList(PluginCommands);
            }
        }
    }
}

TSharedRef<SDockTab> FHunYuanAIModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
    UE_LOG(LogHunYuanAI, Log, TEXT("Spawning HunYuanAI plugin tab"));

    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SAIChatWindow)  // 返回实际窗口
        ];
}

FHunYuanAIModule& FHunYuanAIModule::Get()
{
    return FModuleManager::LoadModuleChecked<FHunYuanAIModule>("HunYuanAI");
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHunYuanAIModule, HunYuanAI)