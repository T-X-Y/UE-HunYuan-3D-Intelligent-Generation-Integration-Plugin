#include "Import/ModelImportManager.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Editor.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "HunYuanAI.h"
#include "Logging/HunYuanLogging.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/Paths.h"
#include "Engine/StaticMesh.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "AssetImportTask.h"


TSharedPtr<FModelImportManager> FModelImportManager::Instance = nullptr;
FCriticalSection FModelImportManager::InstanceCriticalSection;

TSharedPtr<FModelImportManager> FModelImportManager::Get()
{
    FScopeLock Lock(&InstanceCriticalSection);
    if (!Instance.IsValid())
    {
        Instance = MakeShareable(new FModelImportManager());
    }
    return Instance;
}

void FModelImportManager::Shutdown()
{
    FScopeLock Lock(&InstanceCriticalSection);
    if (Instance.IsValid())
    {
        if (Instance->IsImporting())
        {
            Instance->CancelImport();
        }
        Instance.Reset();
    }
}

FModelImportManager::FModelImportManager()
    : DefaultImportPath(TEXT("/Game/HunyuanImports/"))
    , bIsImporting(false)
    , bCancelRequested(false)
{
}

FModelImportManager::~FModelImportManager()
{
    CancelImport();
    if (ImportThread.IsValid())
    {
        ImportThread->WaitForCompletion();
        ImportThread.Reset();
    }
}

FModelFileGroup FModelImportManager::ScanFolderForModelFiles(const FString& FolderPath)
{
    FModelFileGroup FileGroup;
    FileGroup.FolderPath = FolderPath;
    FileGroup.ModelName = FPaths::GetCleanFilename(FolderPath);

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!PlatformFile.DirectoryExists(*FolderPath))
    {
        UE_LOG(LogHunYuanImport, Error, TEXT("文件夹不存在: %s"), *FolderPath);
        return FileGroup;
    }

    TArray<FString> FoundFiles;
    PlatformFile.FindFiles(FoundFiles, *FolderPath, TEXT(""));

    for (const FString& FilePath : FoundFiles)
    {
        FString Extension = FPaths::GetExtension(FilePath).ToLower();

        if (Extension == TEXT("obj"))
        {
            FileGroup.ObjFilePath = FilePath;
            UE_LOG(LogHunYuanImport, Log, TEXT("找到OBJ文件: %s"), *FPaths::GetCleanFilename(FilePath));
        }
        else if (Extension == TEXT("mtl"))
        {
            FileGroup.MtlFilePath = FilePath;
            UE_LOG(LogHunYuanImport, Log, TEXT("找到MTL文件: %s"), *FPaths::GetCleanFilename(FilePath));
        }
        else if (Extension == TEXT("png") || Extension == TEXT("jpg") || Extension == TEXT("jpeg") || Extension == TEXT("tga"))
        {
            FileGroup.TextureFilePath = FilePath;
            UE_LOG(LogHunYuanImport, Log, TEXT("找到纹理文件: %s"), *FPaths::GetCleanFilename(FilePath));
        }
    }

    return FileGroup;
}

bool FModelImportManager::PrepareImportFiles(const FModelFileGroup& FileGroup, const FString& DestinationPath,
    FString& OutObjDestPath, FString& OutMtlDestPath, FString& OutTextureDestPath)
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    FString ContentPhysicalPath = FPackageName::LongPackageNameToFilename(DestinationPath);
    FString ModelFolder = ContentPhysicalPath / FileGroup.ModelName;

    if (!PlatformFile.DirectoryExists(*ModelFolder))
    {
        if (!PlatformFile.CreateDirectoryTree(*ModelFolder))
        {
            UE_LOG(LogHunYuanImport, Error, TEXT("无法创建模型文件夹: %s"), *ModelFolder);
            return false;
        }
    }

    OutObjDestPath = ModelFolder / FPaths::GetCleanFilename(FileGroup.ObjFilePath);
    if (!PlatformFile.CopyFile(*OutObjDestPath, *FileGroup.ObjFilePath))
    {
        UE_LOG(LogHunYuanImport, Error, TEXT("无法复制OBJ文件: %s"), *FileGroup.ObjFilePath);
        return false;
    }

    if (FileGroup.HasMaterial())
    {
        OutMtlDestPath = ModelFolder / FPaths::GetCleanFilename(FileGroup.MtlFilePath);
        if (!PlatformFile.CopyFile(*OutMtlDestPath, *FileGroup.MtlFilePath))
        {
            UE_LOG(LogHunYuanImport, Warning, TEXT("无法复制MTL文件: %s"), *FileGroup.MtlFilePath);
        }
    }

    if (FileGroup.HasTexture())
    {
        OutTextureDestPath = ModelFolder / FPaths::GetCleanFilename(FileGroup.TextureFilePath);
        if (!PlatformFile.CopyFile(*OutTextureDestPath, *FileGroup.TextureFilePath))
        {
            UE_LOG(LogHunYuanImport, Warning, TEXT("无法复制纹理文件: %s"), *FileGroup.TextureFilePath);
        }
    }

    UE_LOG(LogHunYuanImport, Log, TEXT("文件准备完成 - 目标文件夹: %s"), *ModelFolder);
    return true;
}

bool FModelImportManager::UpdateMtlTexturePath(const FString& MtlFilePath, const FString& TextureFileName, const FString& OutputMtlPath)
{
    if (!FPaths::FileExists(MtlFilePath))
    {
        return false;
    }

    FString MtlContent;
    if (!FFileHelper::LoadFileToString(MtlContent, *MtlFilePath))
    {
        UE_LOG(LogHunYuanImport, Error, TEXT("无法读取MTL文件: %s"), *MtlFilePath);
        return false;
    }

    TArray<FString> Lines;
    MtlContent.ParseIntoArrayLines(Lines);

    bool bModified = false;
    FString NewMtlContent;

    for (FString Line : Lines)
    {
        if (Line.Contains(TEXT("map_")) || Line.Contains(TEXT("bump")) || Line.Contains(TEXT("disp")))
        {
            int32 SpaceIndex;
            if (Line.FindChar(' ', SpaceIndex))
            {
                FString Command = Line.Left(SpaceIndex);
                NewMtlContent += Command + TEXT(" ") + TextureFileName + TEXT("\n");
                bModified = true;
            }
            else
            {
                NewMtlContent += Line + TEXT("\n");
            }
        }
        else
        {
            NewMtlContent += Line + TEXT("\n");
        }
    }

    if (bModified)
    {
        return FFileHelper::SaveStringToFile(NewMtlContent, *OutputMtlPath);
    }

    return true;
}

bool FModelImportManager::ImportModelFromFolder(const FString& FolderPath, const FString& DestinationPath)
{
    UE_LOG(LogHunYuanImport, Log, TEXT("=== 开始导入模型文件夹 ==="));
    UE_LOG(LogHunYuanImport, Log, TEXT("源文件夹: %s"), *FolderPath);
    UE_LOG(LogHunYuanImport, Log, TEXT("目标路径: %s"), *DestinationPath);

    FModelFileGroup FileGroup = ScanFolderForModelFiles(FolderPath);

    if (!FileGroup.IsValid())
    {
        UE_LOG(LogHunYuanImport, Error, TEXT("文件夹中未找到有效的OBJ文件: %s"), *FolderPath);
        OnModelImported.Broadcast(false, FString());
        return false;
    }

    if (!GEditor)
    {
        UE_LOG(LogHunYuanImport, Error, TEXT("GEditor 为空，不在编辑器模式！"));
        OnModelImported.Broadcast(false, FString());
        return false;
    }

    FString TargetPath = DestinationPath;
    if (!TargetPath.StartsWith(TEXT("/Game/")))
    {
        TargetPath = TEXT("/Game/") + TargetPath;
    }

    if (!TargetPath.EndsWith(TEXT("/")))
    {
        TargetPath += TEXT("/");
    }

    FString ModelTargetPath = TargetPath + SanitizePackageName(FileGroup.ModelName) + TEXT("/");

    UE_LOG(LogHunYuanImport, Log, TEXT("目标包路径: %s"), *ModelTargetPath);

    if (!EnsureDirectoryExists(ModelTargetPath))
    {
        UE_LOG(LogHunYuanImport, Error, TEXT("无法创建目录: %s"), *ModelTargetPath);
        OnModelImported.Broadcast(false, FString());
        return false;
    }

    FString ObjDestPath, MtlDestPath, TextureDestPath;
    if (!PrepareImportFiles(FileGroup, ModelTargetPath, ObjDestPath, MtlDestPath, TextureDestPath))
    {
        UE_LOG(LogHunYuanImport, Error, TEXT("文件准备失败"));
        OnModelImported.Broadcast(false, FString());
        return false;
    }

    if (FileGroup.HasTexture() && FileGroup.HasMaterial())
    {
        FString TextureFileName = FPaths::GetCleanFilename(FileGroup.TextureFilePath);
        UpdateMtlTexturePath(MtlDestPath, TextureFileName, MtlDestPath);
        UE_LOG(LogHunYuanImport, Log, TEXT("已更新MTL纹理路径"));
    }

    FString ImportFilePath = ModelTargetPath + FPaths::GetCleanFilename(FileGroup.ObjFilePath);

    TArray<UObject*> ImportedAssets;
    EModelImportResult Result = ImportModelInternal(ImportFilePath, ModelTargetPath, ImportedAssets);

    bool bSuccess = (Result == EModelImportResult::Success);

    if (bSuccess)
    {
        UE_LOG(LogHunYuanImport, Log, TEXT("成功导入模型: %s，导入资产数量: %d"),
            *FileGroup.ModelName, ImportedAssets.Num());

        if (ImportedAssets.Num() > 0)
        {
            SelectInContentBrowser(ImportedAssets);
        }

        OnModelImported.Broadcast(true, ModelTargetPath + FileGroup.ModelName);
    }
    else
    {
        UE_LOG(LogHunYuanImport, Error, TEXT("导入模型失败: %s"), *FileGroup.ModelName);
        OnModelImported.Broadcast(false, FString());
    }

    return bSuccess;
}

int32 FModelImportManager::ImportModelsFromFolders(const TArray<FString>& FolderPaths, const FString& DestinationPath)
{
    if (FolderPaths.Num() == 0) return 0;

    UE_LOG(LogHunYuanImport, Log, TEXT("=== 开始批量导入 %d 个模型文件夹 ==="), FolderPaths.Num());

    int32 SuccessCount = 0;
    FString TargetPath = DestinationPath;

    if (!TargetPath.StartsWith(TEXT("/Game/")))
    {
        TargetPath = TEXT("/Game/") + TargetPath;
    }

    if (!TargetPath.EndsWith(TEXT("/")))
    {
        TargetPath += TEXT("/");
    }

    if (!EnsureDirectoryExists(TargetPath))
    {
        UE_LOG(LogHunYuanImport, Error, TEXT("无法创建主目录: %s"), *TargetPath);
        return 0;
    }

    for (int32 i = 0; i < FolderPaths.Num(); i++)
    {
        {
            FScopeLock Lock(&AsyncCriticalSection);
            if (bCancelRequested)
            {
                UE_LOG(LogHunYuanImport, Warning, TEXT("导入已取消，已导入 %d/%d 个模型"), SuccessCount, FolderPaths.Num());
                OnImportCompleted.Broadcast(SuccessCount);
                return SuccessCount;
            }
        }

        const FString& FolderPath = FolderPaths[i];
        FString FolderName = FPaths::GetCleanFilename(FolderPath);

        OnModelImportProgress.Broadcast(i + 1, FolderPaths.Num(), FolderName);

        if (ImportModelFromFolder(FolderPath, TargetPath))
        {
            SuccessCount++;
        }
    }

    UE_LOG(LogHunYuanImport, Log, TEXT("批量导入完成: %d/%d 个模型成功"), SuccessCount, FolderPaths.Num());
    OnImportCompleted.Broadcast(SuccessCount);

    return SuccessCount;
}

// 修正后的异步导入函数
void FModelImportManager::ImportModelsAsync(const TArray<FString>& FolderPaths, const FString& DestinationPath)
{
    if (bIsImporting)
    {
        UE_LOG(LogHunYuanImport, Warning, TEXT("已有导入任务正在进行中"));
        return;
    }

    FScopeLock Lock(&AsyncCriticalSection);
    bIsImporting = true;
    bCancelRequested = false;

    // 创建异步任务类（内部类）
    class FImportAsyncTask : public FRunnable
    {
    public:
        FImportAsyncTask(FModelImportManager* InManager, TArray<FString> InFolderPaths, FString InDestinationPath)
            : Manager(InManager)
            , FolderPaths(MoveTemp(InFolderPaths))
            , DestinationPath(MoveTemp(InDestinationPath))
        {
        }

        virtual uint32 Run() override
        {
            if (Manager)
            {
                Manager->ImportModelsFromFolders(FolderPaths, DestinationPath);
            }
            return 0;
        }

        virtual void Stop() override {}
        virtual void Exit() override {}

    private:
        FModelImportManager* Manager;
        TArray<FString> FolderPaths;
        FString DestinationPath;
    };

    // FRunnableThread::Create 返回原始指针，需要转换为 TSharedPtr
    FRunnableThread* NewThread = FRunnableThread::Create(
        new FImportAsyncTask(this, FolderPaths, DestinationPath),
        TEXT("ModelImportThread"),
        0,
        TPri_Normal
    );

    // 将原始指针包装为 TSharedPtr
    ImportThread = TSharedPtr<FRunnableThread>(NewThread);
}

// 核心导入函数
EModelImportResult FModelImportManager::ImportModelInternal(const FString& FilePath, const FString& DestinationPath, TArray<UObject*>& OutImportedAssets)
{
    OutImportedAssets.Empty();

    if (!GEditor)
    {
        return EModelImportResult::EditorNotAvailable;
    }

    if (!FPaths::FileExists(FilePath))
    {
        UE_LOG(LogHunYuanImport, Error, TEXT("导入文件不存在: %s"), *FilePath);
        return EModelImportResult::FileNotFound;
    }

    FString Extension = FPaths::GetExtension(FilePath).ToLower();
    if (!IsSupportedFormat(FilePath))
    {
        UE_LOG(LogHunYuanImport, Error, TEXT("不支持的格式: %s"), *Extension);
        return EModelImportResult::UnsupportedFormat;
    }

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

    UAssetImportTask* ImportTask = NewObject<UAssetImportTask>();
    ImportTask->Filename = FilePath;
    ImportTask->DestinationPath = DestinationPath;
    ImportTask->bAutomated = true;
    ImportTask->bReplaceExisting = true;
    ImportTask->bSave = true;
    ImportTask->Factory = nullptr;

    UE_LOG(LogHunYuanImport, Log, TEXT("开始导入 - 文件: %s, 目标: %s"),
        *FPaths::GetCleanFilename(FilePath), *DestinationPath);

    TArray<UAssetImportTask*> ImportTasks;
    ImportTasks.Add(ImportTask);
    AssetToolsModule.Get().ImportAssetTasks(ImportTasks);

    if (ImportTask->ImportedObjectPaths.Num() > 0)
    {
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

        for (const FString& ObjectPath : ImportTask->ImportedObjectPaths)
        {
            FSoftObjectPath SoftPath(ObjectPath);
            UObject* Asset = SoftPath.TryLoad();

            if (Asset)
            {
                OutImportedAssets.Add(Asset);
                UE_LOG(LogHunYuanImport, Log, TEXT("导入成功: %s"), *Asset->GetName());
            }
        }

        return EModelImportResult::Success;
    }

    UE_LOG(LogHunYuanImport, Error, TEXT("导入任务失败: %s"), *FilePath);
    return EModelImportResult::ImportFailed;
}

void FModelImportManager::SelectInContentBrowser(const TArray<UObject*>& Assets)
{
    if (!GEditor || Assets.Num() == 0) return;

    FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

    TArray<FAssetData> AssetDataList;
    for (UObject* Asset : Assets)
    {
        if (Asset)
        {
            AssetDataList.Add(FAssetData(Asset));
        }
    }

    if (AssetDataList.Num() > 0)
    {
        ContentBrowserModule.Get().SyncBrowserToAssets(AssetDataList);
    }
}

bool FModelImportManager::IsSupportedFormat(const FString& FilePath)
{
    FString Extension = FPaths::GetExtension(FilePath).ToLower();

    TSet<FString> SupportedFormats = {
        TEXT("obj"), TEXT("fbx"), TEXT("gltf"), TEXT("glb"),
        TEXT("stl"), TEXT("3ds"), TEXT("dae"), TEXT("abc")
    };

    return SupportedFormats.Contains(Extension);
}

TArray<FString> FModelImportManager::GetSupportedFormats()
{
    return { TEXT("obj"), TEXT("fbx"), TEXT("gltf"), TEXT("glb"),
             TEXT("stl"), TEXT("3ds"), TEXT("dae"), TEXT("abc") };
}

void FModelImportManager::SetDefaultImportPath(const FString& NewPath)
{
    FScopeLock Lock(&ImportCriticalSection);
    DefaultImportPath = NewPath;
    if (!DefaultImportPath.StartsWith(TEXT("/Game/")))
    {
        DefaultImportPath = TEXT("/Game/") + DefaultImportPath;
    }
}

FString FModelImportManager::GetDefaultImportPath() const
{
    FScopeLock Lock(&ImportCriticalSection);
    return DefaultImportPath;
}

FString FModelImportManager::SanitizePackageName(const FString& InPackageName)
{
    FString PackageName = InPackageName;

    PackageName = PackageName.Replace(TEXT(" "), TEXT("_"))
        .Replace(TEXT("-"), TEXT("_"))
        .Replace(TEXT("."), TEXT("_"))
        .Replace(TEXT(","), TEXT("_"))
        .Replace(TEXT("("), TEXT("_"))
        .Replace(TEXT(")"), TEXT("_"))
        .Replace(TEXT("["), TEXT("_"))
        .Replace(TEXT("]"), TEXT("_"))
        .Replace(TEXT("{"), TEXT("_"))
        .Replace(TEXT("}"), TEXT("_"))
        .Replace(TEXT("&"), TEXT("_"))
        .Replace(TEXT("*"), TEXT("_"))
        .Replace(TEXT("#"), TEXT("_"))
        .Replace(TEXT("%"), TEXT("_"))
        .Replace(TEXT("@"), TEXT("_"));

    return PackageName;
}

bool FModelImportManager::EnsureDirectoryExists(const FString& PackagePath)
{
    FString ContentPath = FPackageName::LongPackageNameToFilename(PackagePath);

    if (!ContentPath.EndsWith(TEXT("/")) && !ContentPath.EndsWith(TEXT("\\")))
    {
        ContentPath += TEXT("/");
    }

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!PlatformFile.DirectoryExists(*ContentPath))
    {
        return PlatformFile.CreateDirectoryTree(*ContentPath);
    }

    return true;
}

void FModelImportManager::CancelImport()
{
    FScopeLock Lock(&AsyncCriticalSection);
    bCancelRequested = true;
}

bool FModelImportManager::IsMaterialBound(const FString& ModelPath) const
{
    return ImportedModels.Contains(ModelPath);
}