#include "Model.h"
#include "ModelCommon.h"
#include "TextureManager.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

using namespace Microsoft::WRL;

void Model::Initialize(ModelCommon* modelCommon, const std::string& modelFilePath, const std::string& textureFilePath)
{
    modelCommon_ = modelCommon;
    textureFilePath_ = textureFilePath;

    TextureManager::GetInstance()->LoadTexture(textureFilePath);
    isCubemap_ = TextureManager::GetInstance()->GetMetaData(textureFilePath).IsCubemap();

    // 拡張子で読み込み関数を切り替える
    std::string ext = modelFilePath.substr(modelFilePath.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
    
    if (ext == "obj") {
        LoadObjFile(modelFilePath);
    } else {
        LoadGltfFile(modelFilePath);
    }

    ID3D12Device* device = modelCommon_->GetDxCommon()->GetDevice();
    size_t sizeInBytes = sizeof(VertexData) * vertices_.size();

    D3D12_HEAP_PROPERTIES uploadHeapProperties { D3D12_HEAP_TYPE_UPLOAD };

    D3D12_RESOURCE_DESC resourceDesc {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeInBytes;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
        &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&vertexResource_));

    VertexData* data = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&data));
    std::copy(vertices_.begin(), vertices_.end(), data);
    vertexResource_->Unmap(0, nullptr);

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeInBytes);
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void Model::Draw(ModelCommon* modelCommon)
{
    ID3D12GraphicsCommandList* commandList = modelCommon->GetDxCommon()->GetCommandList();

    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_);
    commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);

    // スロット5（TextureCube）: 環境マップが指定されていればそちらを、なければ通常テクスチャを流用
    if (!envCubemapFilePath_.empty()) {
        D3D12_GPU_DESCRIPTOR_HANDLE cubeHandle = TextureManager::GetInstance()->GetSrvHandleGPU(envCubemapFilePath_);
        commandList->SetGraphicsRootDescriptorTable(5, cubeHandle);
    } else {
        commandList->SetGraphicsRootDescriptorTable(5, textureSrvHandle);
    }

    commandList->DrawInstanced(static_cast<UINT>(vertices_.size()), 1, 0, 0);
}

void Model::DrawGeometryOnly(ModelCommon* modelCommon)
{
    ID3D12GraphicsCommandList* commandList = modelCommon->GetDxCommon()->GetCommandList();
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->DrawInstanced(static_cast<UINT>(vertices_.size()), 1, 0, 0);
}

// OBJファイル読み込み
void Model::LoadObjFile(const std::string& filePath)
{
    std::ifstream file(filePath);
    assert(file.is_open());

    std::vector<Vector4> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::string line;

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string identifier;
        ss >> identifier;

        if (identifier == "v") {
            Vector4 position;
            ss >> position.x >> position.y >> position.z;
            position.w = 1.0f;
            positions.push_back(position);
        } else if (identifier == "vt") {
            Vector2 texcoord;
            ss >> texcoord.x >> texcoord.y;
            texcoord.y = 1.0f - texcoord.y;
            texcoords.push_back(texcoord);
        } else if (identifier == "vn") {
            Vector3 normal;
            ss >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        } else if (identifier == "f") {
            // 1行分の頂点データを一時的に格納するベクトル
            std::vector<VertexData> faceVertices;
            std::string s;

            // 行の終わりまで全ての頂点（v/vt/vnのセット）を読み込む
            while (ss >> s) {
                std::stringstream ss2(s);
                std::string indexStr;
                int indices[3] = { 0, 0, 0 };
                int count = 0;

                // スラッシュ区切りでインデックスを分解
                while (std::getline(ss2, indexStr, '/')) {
                    if (!indexStr.empty()) {
                        indices[count] = std::stoi(indexStr);
                    }

                    count++;
                }

                // 頂点データを作成して一時保存
                VertexData vd;
                
                if (indices[0] > 0) {
                    vd.position = positions[indices[0] - 1];
                }

                if (indices[1] > 0) {
                    vd.texcoord = texcoords[indices[1] - 1];
                }

                if (indices[2] > 0) {
                    vd.normal = normals[indices[2] - 1];
                }

                faceVertices.push_back(vd);
            }

            // 読み込んだ頂点を三角形に分割（ファン状に分割）して登録
            // 4角形(0,1,2,3)なら、(0,1,2)と(0,2,3)の2つの三角形に分ける
            for (size_t i = 1; i < faceVertices.size() - 1; ++i) {
                vertices_.push_back(faceVertices[0]);
                vertices_.push_back(faceVertices[i]);
                vertices_.push_back(faceVertices[i + 1]);
            }
        }
    }
}

void Model::LoadGltfFile(const std::string& filePath)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filePath,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenSmoothNormals |
        aiProcess_MakeLeftHanded |
        aiProcess_FlipWindingOrder);

    assert(scene && scene->mNumMeshes > 0);

    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        aiMesh* mesh = scene->mMeshes[meshIndex];

        for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            const aiFace& face = mesh->mFaces[faceIndex];

            for (uint32_t i = 0; i < face.mNumIndices; ++i) {
                uint32_t vIdx = face.mIndices[i];
                VertexData vd {};

                vd.position = {
                    mesh->mVertices[vIdx].x,
                    mesh->mVertices[vIdx].y,
                    mesh->mVertices[vIdx].z,
                    1.0f
                };

                if (mesh->HasNormals()) {
                    vd.normal = {
                        mesh->mNormals[vIdx].x,
                        mesh->mNormals[vIdx].y,
                        mesh->mNormals[vIdx].z
                    };
                }

                if (mesh->HasTextureCoords(0)) {
                    vd.texcoord = {
                        mesh->mTextureCoords[0][vIdx].x,
                        mesh->mTextureCoords[0][vIdx].y
                    };
                }

                vertices_.push_back(vd);
            }
        }
    }
}