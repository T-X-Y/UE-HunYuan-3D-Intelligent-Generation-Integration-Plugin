#pragma once

// 其他 include 放在前面
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

#include "RagHttpClient.generated.h"

DECLARE_DELEGATE_ThreeParams(FOnHttpRequestComplete, FHttpRequestPtr, FHttpResponsePtr, bool);

UCLASS()
class UE_RAG_SDK_API URagHttpClient : public UObject
{
    GENERATED_BODY()

public:
    void SetBaseUrl(const FString& Url) { BaseUrl = Url; }

    void PostJson(const FString& Endpoint, const FString& JsonContent, FOnHttpRequestComplete CompleteDelegate);
    bool PostJsonSync(const FString& Endpoint, const FString& JsonContent, TSharedPtr<FJsonObject>& OutResponse);

    void PostFile(const FString& Endpoint, const FString& FilePath, const FString& FileFieldName,
        const TMap<FString, FString>& FormFields, FOnHttpRequestComplete CompleteDelegate);

private:
    FString BaseUrl = TEXT("http://localhost:8000");

    void OnRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully,
        FOnHttpRequestComplete CompleteDelegate);
};