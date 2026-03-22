//Import/ModelImportManager.cpp
// === 核心模块 ===
#include "Import/ModelImportManager.h"

// === Asset系统 ===
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetImportTask.h"
#include "EditorFramework/AssetImportData.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/SavePackage.h"

// === Interchange导入系统 ===
#include "InterchangeManager.h"
#include "InterchangeAssetImportData.h"
#include "InterchangePipelineBase.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "InterchangeFactoryBase.h"
#include "InterchangeTranslatorBase.h"
#include "InterchangeImportModule.h"
#include "InterchangeStaticMeshFactoryNode.h"
#include "InterchangeMaterialFactoryNode.h"

// === FBX导入相关 ===
#include "Factories/Factory.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxTextureImportData.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/MaterialFactoryNew.h"

// === 引擎基本类型 ===
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Engine/Texture2D.h"

// === 编辑器模块 ===
#include "Editor.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

// === UI通知 ===
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

// === 文件系统 ===
#include "HAL/PlatformFileManager.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// === 项目特定 ===
#include "HunYuanAI.h"
#include "Logging/HunYuanLogging.h"


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

    // 直接使用 DestinationPath 对应的物理路径
    FString ContentPhysicalPath = FPackageName::LongPackageNameToFilename(DestinationPath);

    UE_LOG(LogHunYuanImport, Log, TEXT("准备导入文件到: %s"), *ContentPhysicalPath);

    // 确保文件夹存在
    if (!PlatformFile.DirectoryExists(*ContentPhysicalPath))
    {
        if (!PlatformFile.CreateDirectoryTree(*ContentPhysicalPath))
        {
            UE_LOG(LogHunYuanImport, Error, TEXT("无法创建模型文件夹: %s"), *ContentPhysicalPath);
            return false;
        }
    }

    // 直接复制文件到目标文件夹，不创建任何子文件夹
    OutObjDestPath = ContentPhysicalPath / FPaths::GetCleanFilename(FileGroup.ObjFilePath);
    if (!PlatformFile.CopyFile(*OutObjDestPath, *FileGroup.ObjFilePath))
    {
        UE_LOG(LogHunYuanImport, Error, TEXT("无法复制OBJ文件: %s"), *FileGroup.ObjFilePath);
        return false;
    }

    // 复制并处理 MTL 文件
    if (FileGroup.HasMaterial())
    {
        OutMtlDestPath = ContentPhysicalPath / FPaths::GetCleanFilename(FileGroup.MtlFilePath);
        if (!PlatformFile.CopyFile(*OutMtlDestPath, *FileGroup.MtlFilePath))
        {
            UE_LOG(LogHunYuanImport, Warning, TEXT("无法复制MTL文件: %s"), *FileGroup.MtlFilePath);
        }
    }

    // 复制纹理文件
    if (FileGroup.HasTexture())
    {
        OutTextureDestPath = ContentPhysicalPath / FPaths::GetCleanFilename(FileGroup.TextureFilePath);
        if (!PlatformFile.CopyFile(*OutTextureDestPath, *FileGroup.TextureFilePath))
        {
            UE_LOG(LogHunYuanImport, Warning, TEXT("无法复制纹理文件: %s"), *FileGroup.TextureFilePath);
        }
    }

    UE_LOG(LogHunYuanImport, Log, TEXT("文件准备完成 - 目标文件夹: %s"), *ContentPhysicalPath);
    return true;
}

bool FModelImportManager::UpdateMtlTexturePath(const FString& MtlFilePath, const FString& TextureFileName, const FString& OutputMtlPath)
{
    if (!FPaths::FileExists(MtlFilePath))
    {
        UE_LOG(LogHunYuanImport, Error, TEXT("MTL文件不存在: %s"), *MtlFilePath);
        return false;
    }

    // 读取 MTL 文件内容
    FString MtlContent;
    if (!FFileHelper::LoadFileToString(MtlContent, *MtlFilePath))
    {
        UE_LOG(LogHunYuanImport, Error, TEXT("无法读取MTL文件: %s"), *MtlFilePath);
        return false;
    }

    UE_LOG(LogHunYuanImport, Verbose, TEXT("原始 MTL 内容:\n%s"), *MtlContent);

    TArray<FString> Lines;
    MtlContent.ParseIntoArrayLines(Lines);

    bool bModified = false;
    TArray<FString> NewLines;

    for (const FString& Line : Lines)
    {
        FString TrimmedLine = Line.TrimStartAndEnd();

        // 跳过空行
        if (TrimmedLine.IsEmpty())
        {
            NewLines.Add(Line);
            continue;
        }

        // 检查是否是以 map_Kd 开头的纹理行
        if (TrimmedLine.StartsWith(TEXT("map_Kd"), ESearchCase::IgnoreCase))
        {
            // 提取命令和后面的部分
            int32 SpaceIndex = TrimmedLine.Find(TEXT(" "));
            if (SpaceIndex != INDEX_NONE)
            {
                FString Command = TrimmedLine.Left(SpaceIndex);
                FString OldPath = TrimmedLine.RightChop(SpaceIndex + 1).TrimStart();

                // 获取旧的文件名（仅用于日志）
                FString OldFileName = FPaths::GetCleanFilename(OldPath);

                UE_LOG(LogHunYuanImport, Log, TEXT("MTL 纹理替换: %s -> %s"), *OldFileName, *TextureFileName);

                // 只替换文件名部分，保持命令不变
                FString NewLine = Command + TEXT(" ") + TextureFileName;
                NewLines.Add(NewLine);
                bModified = true;
            }
            else
            {
                // 格式不对，保持原样
                NewLines.Add(Line);
            }
        }
        else
        {
            // 非纹理行，保持原样
            NewLines.Add(Line);
        }
    }

    if (bModified)
    {
        // 重新组合文件内容
        FString NewContent = FString::Join(NewLines, TEXT("\n"));

        UE_LOG(LogHunYuanImport, Verbose, TEXT("修改后的 MTL 内容:\n%s"), *NewContent);

        // 保存到输出路径
        return FFileHelper::SaveStringToFile(NewContent, *OutputMtlPath);
    }
    else
    {
        UE_LOG(LogHunYuanImport, Log, TEXT("MTL 文件无需修改，直接复制"));

        // 如果没有修改，直接复制原文件
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        return PlatformFile.CopyFile(*OutputMtlPath, *MtlFilePath);
    }
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

    // 直接使用传入的目标路径，不拼接模型名
    FString ModelTargetPath = TargetPath;

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

    // 更新 MTL 纹理路径
    if (FileGroup.HasTexture() && FileGroup.HasMaterial())
    {
        FString TextureFileName = FPaths::GetCleanFilename(FileGroup.TextureFilePath);
        UpdateMtlTexturePath(MtlDestPath, TextureFileName, MtlDestPath);
        UE_LOG(LogHunYuanImport, Log, TEXT("已更新MTL纹理路径"));
    }

    // 使用复制后的 OBJ 文件路径，而不是未定义的变量
    FString ImportFilePath = ObjDestPath;

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

            // ===== 关键：用完立即释放引用 =====
            // 让引擎自己管理这些资产，我们不保存任何引用
            ImportedAssets.Empty();
        }

        // 构建正确的资产路径用于回调
        FString AssetPath = ModelTargetPath + SanitizePackageName(FileGroup.ModelName);
        OnModelImported.Broadcast(true, AssetPath);
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

// 异步导入函数
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
    // 确保在游戏线程
    if (!IsInGameThread())
    {
        UE_LOG(LogHunYuanImport, Error, TEXT("ImportModelInternal must be called from game thread only!"));
        return EModelImportResult::ImportFailed;
    }

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

    // 判断是OBJ还是其他格式
    if (Extension == TEXT("obj"))
    {
        // OBJ格式：需要特殊处理材质和网格体
        // 确保目标路径包含模型ID（因为OBJ可能有多个文件）
        FString ObjDestinationPath = DestinationPath;
        if (!ObjDestinationPath.EndsWith(TEXT("/")))
        {
            ObjDestinationPath += TEXT("/");
        }

        TArray<UObject*> MaterialAssets;
        ImportMaterialsWithFbxFactory(FilePath, ObjDestinationPath, MaterialAssets);

        TArray<UObject*> MeshAssets;
        ImportMeshWithInterchange(FilePath, ObjDestinationPath, MeshAssets);

        // 合并所有资产
        OutImportedAssets.Append(MaterialAssets);
        OutImportedAssets.Append(MeshAssets);
    }
    else
    {
        // GLB/FBX等其他格式：直接使用Interchange完整导入
        // 注意：DestinationPath 应该是基础路径，如 "/Game/HunyuanImports/"
        // Interchange会自动在基础路径下创建 [文件名] 文件夹
        return ImportModelWithInterchange(FilePath, DestinationPath, OutImportedAssets);
    }

    return EModelImportResult::Success;
}

// 使用Interchange完整导入（适用于GLB/FBX）
EModelImportResult FModelImportManager::ImportModelWithInterchange(const FString& FilePath, const FString& BasePath, TArray<UObject*>& OutImportedAssets)
{
    UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();

    // 从 FilePath 获取文件名（不含扩展名）
    FString FileName = FPaths::GetBaseFilename(FilePath);

    // 构建完整路径：基础路径 + 模型名文件夹
    FString PackageBasePath = FPaths::Combine(BasePath, FileName);
    PackageBasePath = PackageBasePath / TEXT("");  // 确保以斜杠结尾

    UE_LOG(LogHunYuanImport, Log, TEXT("Interchange完整导入到路径: %s"), *PackageBasePath);

    // 创建SourceData
    UInterchangeSourceData* SourceData = InterchangeManager.CreateSourceData(FilePath);
    if (!SourceData)
    {
        UE_LOG(LogHunYuanImport, Error, TEXT("无法创建SourceData: %s"), *FilePath);
        return EModelImportResult::ImportFailed;
    }

    // 创建导入参数
    FImportAssetParameters ImportParams;
    ImportParams.bIsAutomated = true;

    // 执行导入 - 使用基础路径，Interchange会自动创建子文件夹
    TArray<UObject*> ImportedAssets;
    bool bSuccess = InterchangeManager.ImportAsset(
        PackageBasePath,        // 基础路径，如 "/Game/HunyuanImports/"
        SourceData,
        ImportParams,
        ImportedAssets
    );

    if (bSuccess)
    {
        OutImportedAssets.Append(ImportedAssets);

        // 记录导入的资产
        for (UObject* Asset : ImportedAssets)
        {
            UE_LOG(LogHunYuanImport, Log, TEXT("Interchange导入资产: %s (%s)"),
                *Asset->GetName(), *Asset->GetClass()->GetName());
        }

        return EModelImportResult::Success;
    }

    return EModelImportResult::ImportFailed;
}

bool FModelImportManager::ImportMaterialsWithFbxFactory(const FString& FilePath, const FString& DestinationPath, TArray<UObject*>& OutMaterials)
{
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

    // ===== 确保使用正确的路径 =====
    FString TargetPath = DestinationPath;
    if (TargetPath.EndsWith(TEXT("/")))
    {
        TargetPath = TargetPath.LeftChop(1);
    }

    UFbxFactory* FbxFactory = NewObject<UFbxFactory>();
    FbxFactory->AddToRoot();

    UFbxImportUI* ImportUI = NewObject<UFbxImportUI>();
    ImportUI->bIsObjImport = (FPaths::GetExtension(FilePath).ToLower() == TEXT("obj"));
    ImportUI->bImportMaterials = true;      // 只导入材质
    ImportUI->bImportTextures = true;       // 导入纹理
    ImportUI->bImportAsSkeletal = false;    // 不导入骨骼
    ImportUI->bCreatePhysicsAsset = false;
    ImportUI->bImportAnimations = false;
    ImportUI->bImportMesh = false;          // 不导入网格体！

    // 材质搜索设置
    ImportUI->TextureImportData = NewObject<UFbxTextureImportData>();
    ImportUI->TextureImportData->MaterialSearchLocation = EMaterialSearchLocation::Local;

    FbxFactory->ImportUI = ImportUI;

    UAssetImportTask* ImportTask = NewObject<UAssetImportTask>();
    ImportTask->Filename = FilePath;
    ImportTask->DestinationPath = TargetPath;
    ImportTask->bAutomated = true;
    ImportTask->bReplaceExisting = true;
    ImportTask->bSave = true;
    ImportTask->bAsync = false;
    ImportTask->Factory = FbxFactory;

    TArray<UAssetImportTask*> ImportTasks;
    ImportTasks.Add(ImportTask);

    AssetToolsModule.Get().ImportAssetTasks(ImportTasks);
    FbxFactory->RemoveFromRoot();

    // 收集材质资产
    for (const FString& ObjectPath : ImportTask->ImportedObjectPaths)
    {
        FSoftObjectPath SoftPath(ObjectPath);
        UObject* Asset = SoftPath.TryLoad();
        if (Asset && (Asset->IsA<UMaterialInterface>() || Asset->IsA<UTexture>()))
        {
            OutMaterials.Add(Asset);
        }
    }

    return OutMaterials.Num() > 0;
}

bool FModelImportManager::ImportMeshWithInterchange(const FString& FilePath, const FString& DestinationPath, TArray<UObject*>& OutMeshes)
{
    // 获取Interchange管理器
    UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();

    // ===== 修复：使用 DestinationPath 直接作为包路径，不再拼接文件名 =====
    // 因为 DestinationPath 已经是 /Game/HunyuanImports/模型ID/ 这样的路径
    FString PackagePath = DestinationPath;

    // 移除末尾的斜杠（如果有）
    if (PackagePath.EndsWith(TEXT("/")))
    {
        PackagePath = PackagePath.LeftChop(1);
    }

    UE_LOG(LogHunYuanImport, Log, TEXT("Interchange导入网格体到路径: %s"), *PackagePath);

    // 创建SourceData
    UInterchangeSourceData* SourceData = InterchangeManager.CreateSourceData(FilePath);
    if (!SourceData)
    {
        UE_LOG(LogHunYuanImport, Error, TEXT("无法创建SourceData: %s"), *FilePath);
        return false;
    }

    // 创建导入参数
    FImportAssetParameters ImportParams;
    ImportParams.bIsAutomated = true;

    // 执行导入
    TArray<UObject*> ImportedAssets;

    bool bSuccess = InterchangeManager.ImportAsset(
        PackagePath,           // 直接使用 DestinationPath，不再拼接文件名
        SourceData,            // SourceData
        ImportParams,          // ImportAssetParameters
        ImportedAssets         // OutImportedObjects
    );

    if (bSuccess)
    {
        // 只收集静态网格体
        for (UObject* Asset : ImportedAssets)
        {
            if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
            {
                OutMeshes.Add(Mesh);
                UE_LOG(LogHunYuanImport, Log, TEXT("Interchange导入网格体: %s"), *Mesh->GetName());
            }
        }
    }

    return OutMeshes.Num() > 0;
}

EModelImportResult FModelImportManager::ImportModelWithFbxFactory(const FString& FilePath, const FString& DestinationPath, TArray<UObject*>& OutImportedAssets)
{
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

    // 创建FbxFactory
    UFbxFactory* FbxFactory = NewObject<UFbxFactory>();
    FbxFactory->AddToRoot();

    // 配置导入选项
    UFbxImportUI* ImportUI = NewObject<UFbxImportUI>();
    bool bIsObj = (FPaths::GetExtension(FilePath).ToLower() == TEXT("obj"));
    ImportUI->bIsObjImport = bIsObj;
    ImportUI->bImportMaterials = true;
    ImportUI->bImportTextures = true;
    ImportUI->bCreatePhysicsAsset = false;
    ImportUI->bAutoComputeLodDistances = false;
    ImportUI->bImportAnimations = false;

    // 材质搜索设置
    ImportUI->TextureImportData = NewObject<UFbxTextureImportData>();
    ImportUI->TextureImportData->MaterialSearchLocation = EMaterialSearchLocation::Local;

    // 静态网格体设置
    ImportUI->StaticMeshImportData = NewObject<UFbxStaticMeshImportData>();
    ImportUI->StaticMeshImportData->NormalImportMethod = EFBXNormalImportMethod::FBXNIM_ComputeNormals;
    ImportUI->StaticMeshImportData->NormalGenerationMethod = EFBXNormalGenerationMethod::MikkTSpace;
    ImportUI->StaticMeshImportData->bGenerateLightmapUVs = true;
    ImportUI->StaticMeshImportData->bAutoGenerateCollision = true;
    ImportUI->StaticMeshImportData->bRemoveDegenerates = true;

    FbxFactory->ImportUI = ImportUI;

    UAssetImportTask* ImportTask = NewObject<UAssetImportTask>();
    ImportTask->Filename = FilePath;
    ImportTask->DestinationPath = DestinationPath;
    ImportTask->bAutomated = true;
    ImportTask->bReplaceExisting = true;
    ImportTask->bSave = true;
    ImportTask->bAsync = false;
    ImportTask->Factory = FbxFactory;

    TArray<UAssetImportTask*> ImportTasks;
    ImportTasks.Add(ImportTask);

    AssetToolsModule.Get().ImportAssetTasks(ImportTasks);

    FbxFactory->RemoveFromRoot();

    if (ImportTask->ImportedObjectPaths.Num() > 0)
    {
        for (const FString& ObjectPath : ImportTask->ImportedObjectPaths)
        {
            FSoftObjectPath SoftPath(ObjectPath);
            UObject* Asset = SoftPath.TryLoad();
            if (Asset)
            {
                OutImportedAssets.Add(Asset);
                UE_LOG(LogHunYuanImport, Log, TEXT("FbxFactory导入成功: %s (%s)"),
                    *Asset->GetName(), *Asset->GetClass()->GetName());
            }
        }
        return EModelImportResult::Success;
    }

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