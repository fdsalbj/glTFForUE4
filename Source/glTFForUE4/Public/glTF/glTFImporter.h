// Copyright 2016 - 2020 Code 4 Game, Org. All Rights Reserved.

#pragma once

#include <vector>
#include <map>
#include <memory>

#include <libgltf/libgltf.h>

#include <Components.h>
#include <Engine/Texture.h>
#if ENGINE_MINOR_VERSION <= 12
#include <Curves/CurveBase.h>
#else
#include <Curves/RichCurve.h>
#endif

#include "glTFImporter.generated.h"

#define GLTF_TRIANGLE_POINTS_NUM            3
#define GLTF_JOINT_LAYERS_NUM_MAX           3

#if defined(LIBGLTF_CHARACTOR_ENCODING_IS_UTF8)
#   define GLTF_TCHAR_TO_GLTFSTRING(a)         TCHAR_TO_UTF8(a)
#   define GLTF_GLTFSTRING_TO_TCHAR(a)         UTF8_TO_TCHAR(a)
#elif defined(LIBGLTF_CHARACTOR_ENCODING_IS_UTF16)
#   error not supports the utf16
#elif defined(LIBGLTF_CHARACTOR_ENCODING_IS_UTF32)
#   error not supports the utf32
#elif defined(LIBGLTF_CHARACTOR_ENCODING_IS_UNICODE)
#   define GLTF_TCHAR_TO_GLTFSTRING(a)         TCHAR_TO_WCHAR(a)
#   define GLTF_GLTFSTRING_TO_TCHAR(a)         WCHAR_TO_TCHAR(a)
#else
#   error not supports
#endif

namespace glTFForUE4
{
    class GLTFFORUE4_API FFeedbackTaskWrapper
    {
    public:
        explicit FFeedbackTaskWrapper(class FFeedbackContext* InFeedbackContext, const FText& InTask, bool InShowProgressDialog);
        virtual ~FFeedbackTaskWrapper();

    public:
        FORCEINLINE class FFeedbackContext* Get() const { return FeedbackContext; }
        const FFeedbackTaskWrapper& Log(ELogVerbosity::Type InLogVerbosity, const FText& InMessge) const;
        const FFeedbackTaskWrapper& UpdateProgress(int32 InNumerator, int32 InDenominator) const;
        const FFeedbackTaskWrapper& StatusUpdate(int32 InNumerator, int32 InDenominator, const FText& InStatusText) const;
        const FFeedbackTaskWrapper& StatusForceUpdate(int32 InNumerator, int32 InDenominator, const FText& InStatusText) const;

    private:
        class FFeedbackContext* FeedbackContext;
    };
}

USTRUCT()
struct GLTFFORUE4_API FglTFImporterNodeInfo
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY()
    int32 ParentIndex;

    UPROPERTY()
    FTransform RelativeTransform;

    UPROPERTY()
    FTransform AbsoluteTransform;

    FglTFImporterNodeInfo();

    static const FglTFImporterNodeInfo Default;
};

USTRUCT()
struct GLTFFORUE4_API FglTFImporterCollection
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY()
    class UWorld* TargetWorld;
    
    UPROPERTY()
    TMap<int32, FglTFImporterNodeInfo> NodeInfos;
    
    UPROPERTY()
    TMap<int32, class UTexture*> Textures;
    
    UPROPERTY()
    TMap<int32, class UMaterialInterface*> Materials;
    
    UPROPERTY()
    TMap<int32, class UStaticMesh*> StaticMeshes;
    
    UPROPERTY()
    TMap<int32, class USkeletalMesh*> SkeletalMeshes;

    FglTFImporterCollection();

    const FglTFImporterNodeInfo& FindNodeInfo(int32 InNodeId) const;
};

class GLTFFORUE4_API FglTFBufferData
{
public:
    FglTFBufferData(const TArray<uint8>& InData);
    FglTFBufferData(const FString& InFileFolderRoot, const FString& InUri);
    virtual ~FglTFBufferData();

public:
    operator bool() const;
    const TArray<uint8>& GetData() const;
    bool IsFromFile() const;
    const FString& GetFilePath() const;

private:
    TArray<uint8> Data;
    FString FilePath;
    FString StreamType;
    FString StreamEncoding;
};

namespace EglTFBufferSource
{
    enum Type
    {
        Binaries,
        Images,
        Buffers,
        Max,
    };
}

class GLTFFORUE4_API FglTFBuffers
{
public:
    FglTFBuffers(bool InConstructByBinary = false);
    virtual ~FglTFBuffers();

public:
    bool CacheBinary(uint32 InIndex, const TArray<uint8>& InData);
    bool CacheImages(uint32 InIndex, const FString& InFileFolderRoot, const std::shared_ptr<libgltf::SImage>& InImage);
    bool CacheBuffers(uint32 InIndex, const FString& InFileFolderRoot, const std::shared_ptr<libgltf::SBuffer>& InBuffer);
    bool Cache(const FString& InFileFolderRoot, const std::shared_ptr<libgltf::SGlTF>& InglTF);

public:
    template<EglTFBufferSource::Type SourceType>
    const TArray<uint8>& GetData(int32 InIndex, FString& OutFilePath) const
    {
        if (!IndexToIndex[SourceType].Contains(InIndex)) return DataEmpty;
        uint32 DataIndex = IndexToIndex[SourceType][InIndex];
        if (DataIndex >= static_cast<uint32>(Datas.Num())) return DataEmpty;
        const TSharedPtr<FglTFBufferData>& Data = Datas[DataIndex];
        if (!Data.IsValid()) return DataEmpty;
        OutFilePath = Data->GetFilePath();
        return Data->GetData();
    }

    template<typename TElem, EglTFBufferSource::Type SourceType>
    bool Get(int32 InIndex, TArray<TElem>& OutBufferSegment, FString& OutFilePath, int32 InStart = 0, int32 InCount = 0, int32 InStride = 0) const
    {
        if (InStride == 0) InStride = sizeof(TElem);
        checkfSlow(sizeof(TElem) > InStride, TEXT("Stride is too smaller!"));
        if (sizeof(TElem) > InStride) return false;
        if (InStart < 0) return false;
        const TArray<uint8>& BufferSegment = GetData<SourceType>(InIndex, OutFilePath);
        if (BufferSegment.Num() <= 0) return false;
        if (InCount <= 0) InCount = BufferSegment.Num();
        if (BufferSegment.Num() < (InStart + InCount * InStride)) return false;

        OutBufferSegment.SetNumUninitialized(InCount);
        if (InStride == sizeof(TElem))
        {
            FMemory::Memcpy((void*)OutBufferSegment.GetData(), (void*)(BufferSegment.GetData() + InStart), InCount * sizeof(TElem));
        }
        else
        {
            for (int32 i = 0; i < InCount; ++i)
            {
                FMemory::Memcpy((void*)(OutBufferSegment.GetData() + i), (void*)(BufferSegment.GetData() + InStart + i * InStride), InStride);
            }
        }
        return true;
    }

    template<typename TElem>
    bool GetImageData(const std::shared_ptr<libgltf::SGlTF>& InglTF, int32 InImageIndex, TArray<TElem>& OutBufferSegment, FString& OutFilePath) const
    {
        if (!InglTF) return false;
        if (InImageIndex < 0 || InImageIndex >= static_cast<int32>(InglTF->images.size())) return false;
        const std::shared_ptr<libgltf::SImage>& Image = InglTF->images[InImageIndex];
        if (!Image) return false;
        if (Image->uri.empty())
        {
            if (!!(Image->bufferView))
            {
                return GetBufferViewData<TElem>(InglTF, (int32)(*Image->bufferView), OutBufferSegment, OutFilePath);
            }
        }
        else
        {
            return Get<TElem, EglTFBufferSource::Images>(InImageIndex, OutBufferSegment, OutFilePath);
        }
        return false;
    }

    template<typename TElem>
    bool GetBufferViewData(const std::shared_ptr<libgltf::SGlTF>& InglTF, int32 InBufferViewIndex, TArray<TElem>& OutBufferSegment, FString& OutFilePath, int32 InOffset = 0, int32 InCount = 0) const
    {
        if (!InglTF) return false;
        if (InBufferViewIndex < 0 || InBufferViewIndex >= static_cast<int32>(InglTF->bufferViews.size())) return false;
        const std::shared_ptr<libgltf::SBufferView>& BufferView = InglTF->bufferViews[InBufferViewIndex];
        if (!BufferView || !BufferView->buffer) return false;
        int32 BufferIndex = (int32)(*BufferView->buffer);
        return (bConstructByBinary
            ? Get<TElem, EglTFBufferSource::Binaries>(BufferIndex, OutBufferSegment, OutFilePath, BufferView->byteOffset + InOffset, InCount != 0 ? InCount : BufferView->byteLength, BufferView->byteStride)
            : Get<TElem, EglTFBufferSource::Buffers>(BufferIndex, OutBufferSegment, OutFilePath, BufferView->byteOffset + InOffset, InCount != 0 ? InCount : BufferView->byteLength, BufferView->byteStride));
    }

private:
    bool bConstructByBinary;
    TMap<uint32, uint32> IndexToIndex[EglTFBufferSource::Max];
    TArray<TSharedPtr<FglTFBufferData>> Datas;
    const TArray<uint8> DataEmpty;
};

struct GLTFFORUE4_API FglTFAnimationSequenceKeyData
{
    FglTFAnimationSequenceKeyData();

    float Time;
    FTransform Transform;
    ERichCurveInterpMode TranslationInterpolation;
    ERichCurveInterpMode RotationInterpolation;
    ERichCurveInterpMode ScaleInterpolation;
};

struct GLTFFORUE4_API FglTFAnimationSequenceData
{
    FglTFAnimationSequenceData();

    int32 NodeIndex;
    TArray<FglTFAnimationSequenceKeyData> KeyDatas;

    FglTFAnimationSequenceKeyData* FindOrAddSequenceKeyData(float InTime);
    void FindOrAddSequenceKeyDataAndSetTranslation(float InTime, const FVector& InValue, ERichCurveInterpMode InInterpolation);
    void FindOrAddSequenceKeyDataAndSetRotation(float InTime, const FQuat& InValue, ERichCurveInterpMode InInterpolation);
    void FindOrAddSequenceKeyDataAndSetScale(float InTime, const FVector& InValue, ERichCurveInterpMode InInterpolation);
};

struct GLTFFORUE4_API FglTFAnimationSequenceDatas
{
    FglTFAnimationSequenceDatas();

    TArray<FglTFAnimationSequenceData> Datas;

    FglTFAnimationSequenceData* FindOrAddSequenceData(int32 InNodeIndex);
    void FindOrAddSequenceDataAndSetTranslation(int32 InNodeIndex, float InTime, const FVector& InValue, ERichCurveInterpMode InInterpolation);
    void FindOrAddSequenceDataAndSetRotation(int32 InNodeIndex, float InTime, const FQuat& InValue, ERichCurveInterpMode InInterpolation);
    void FindOrAddSequenceDataAndSetScale(int32 InNodeIndex, float InTime, const FVector& InValue, ERichCurveInterpMode InInterpolation);
};

class GLTFFORUE4_API FglTFImporter
{
public:
    static TSharedPtr<FglTFImporter> Get(UObject* InParent, FName InName, EObjectFlags InFlags, class FFeedbackContext* InFeedbackContext);

public:
    FglTFImporter();
    virtual ~FglTFImporter();

public:
    virtual FglTFImporter& Set(UObject* InParent, FName InName, EObjectFlags InFlags, class FFeedbackContext* InFeedbackContext);
    virtual UObject* Create(const TWeakPtr<struct FglTFImporterOptions>& InglTFImporterOptions
        , const std::shared_ptr<libgltf::SGlTF>& InGlTF, const FglTFBuffers& InglTFBuffers
        , const glTFForUE4::FFeedbackTaskWrapper& InFeedbackTaskWrapper) const;

protected:
    UObject* InputParent;
    FName InputName;
    EObjectFlags InputFlags;
    class FFeedbackContext* FeedbackContext;

public:
    static bool GetStaticMeshData(const std::shared_ptr<libgltf::SGlTF>& InGlTF, const std::shared_ptr<libgltf::SMeshPrimitive>& InMeshPrimitive, const FglTFBuffers& InBuffers
        , TArray<uint32>& OutTriangleIndices, TArray<FVector>& OutVertexPositions, TArray<FVector>& OutVertexNormals, TArray<FVector4>& OutVertexTangents, TArray<FVector2D> OutVertexTexcoords[MAX_TEXCOORDS], bool bSwapYZ = true);
    static bool GetSkeletalMeshData(const std::shared_ptr<libgltf::SGlTF>& InGlTF, const std::shared_ptr<libgltf::SMeshPrimitive>& InMeshPrimitive, const FglTFBuffers& InBuffers
        , TArray<uint32>& OutTriangleIndices, TArray<FVector>& OutVertexPositions, TArray<FVector>& OutVertexNormals, TArray<FVector4>& OutVertexTangents, TArray<FVector2D> OutVertexTexcoords[MAX_TEXCOORDS], TArray<FVector4> OutJointsIndices[GLTF_JOINT_LAYERS_NUM_MAX], TArray<FVector4> OutJointsWeights[GLTF_JOINT_LAYERS_NUM_MAX], bool bSwapYZ = true);
    static bool GetInverseBindMatrices(const std::shared_ptr<libgltf::SGlTF>& InGlTF, const std::shared_ptr<libgltf::SSkin>& InSkin, const FglTFBuffers& InBuffers, TArray<FMatrix>& OutInverseBindMatrices, bool bSwapYZ = true);
    static bool GetAnimationSequenceData(const std::shared_ptr<libgltf::SGlTF>& InGlTF, const std::shared_ptr<libgltf::SAnimation>& InglTFAnimation, const FglTFBuffers& InBuffers, FglTFAnimationSequenceDatas& OutAnimationSequenceDatas, bool bSwapYZ = true);
    static bool GetNodeParentIndices(const std::shared_ptr<libgltf::SGlTF>& InGlTF, TArray<int32>& OutParentIndices);
    static bool GetNodeRelativeTransforms(const std::shared_ptr<libgltf::SGlTF>& InGlTF, TArray<FTransform>& OutRelativeTransforms, bool bSwapYZ = true);
    static bool GetNodeParentIndicesAndTransforms(const std::shared_ptr<libgltf::SGlTF>& InGlTF, TArray<int32>& OutParentIndices, TArray<FTransform>& OutRelativeTransforms, TArray<FTransform>& OutAbsoluteTransforms, bool bSwapYZ = true);
    static bool GetNodeInfos(const std::shared_ptr<libgltf::SGlTF>& InGlTF, TMap<int32, FglTFImporterNodeInfo>& OutNodeInfos, bool bSwapYZ = true);
    static bool SpawnStaticMeshActor(class UWorld* InWorld, const FTransform& InTransform, class UStaticMesh* InStaticMesh);
    static bool SpawnSkeletalMeshActor(class UWorld* InWorld, const FTransform& InTransform, class USkeletalMesh* InSkeletalMesh);

public:
    static FString SanitizeObjectName(const FString& InObjectName);

public:
    static const FMatrix& GetglTFSpaceToUnrealSpace(bool bSwapYZ = true, bool bInverseX = false);
    static FMatrix& ConvertToUnrealSpace(FMatrix& InOutValue, bool bSwapYZ = true, bool bInverseX = false);
    static FTransform& ConvertToUnrealSpace(FTransform& InOutValue, bool bSwapYZ = true, bool bInverseX = false);
    static TextureFilter MagFilterToTextureFilter(int32 InValue);
    static TextureFilter MinFilterToTextureFilter(int32 InValue);
    static TextureAddress WrapSToTextureAddress(int32 InValue);
    static TextureAddress WrapTToTextureAddress(int32 InValue);
    static ERichCurveInterpMode StringToRichCurveInterpMode(const FString& InInterpolation);
};
