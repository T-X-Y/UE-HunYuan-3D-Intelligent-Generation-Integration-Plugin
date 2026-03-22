// SKGRagSearchWindow.cpp
#include "UI/SKGRagSearchWindow.h"
#include "UI/SKnowledgeGraphSearchWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Framework/Docking/TabManager.h"

#define LOCTEXT_NAMESPACE "KGRagSearchWindow"

TWeakPtr<SWindow> SKGRagSearchWindow::Instance;

void SKGRagSearchWindow::OpenWindow()
{
    if (Instance.IsValid())
    {
        Instance.Pin()->BringToFront();
        return;
    }

    TSharedRef<SWindow> Window = SNew(SKGRagSearchWindow);
    Instance = Window;

    FSlateApplication::Get().AddWindow(Window);
}

void SKGRagSearchWindow::CloseWindow()
{
    if (Instance.IsValid())
    {
        Instance.Pin()->RequestDestroyWindow();
        Instance.Reset();
    }
}

TSharedPtr<SWindow> SKGRagSearchWindow::GetWindow()
{
    return Instance.Pin();
}

void SKGRagSearchWindow::Construct(const FArguments& InArgs)
{
    // 创建搜索组件
    SearchWidget = SNew(SKnowledgeGraphSearchWidget);

    SWindow::Construct(SWindow::FArguments()
        .Title(LOCTEXT("WindowTitle", "知识图谱搜索"))
        .ClientSize(FVector2D(600, 500))
        .SizingRule(ESizingRule::UserSized)
        .SupportsMinimize(true)
        .SupportsMaximize(true)
        [
            SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
                .Padding(0)
                [
                    SearchWidget.ToSharedRef()
                ]
        ]
    );
}

#undef LOCTEXT_NAMESPACE