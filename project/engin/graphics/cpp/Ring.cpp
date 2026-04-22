#include "Ring.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include <numbers>
#include <cassert>
#include <cmath>

using namespace Microsoft::WRL;

void Ring::Initialize(DirectXCommon* dxCommon, const std::string& textureFilePath)
{
    assert(dxCommon);
    dxCommon_        = dxCommon;
    textureFilePath_ = textureFilePath;

    TextureManager::GetInstance()->LoadTexture(textureFilePath_);

    materialBuffer_ = dxCommon_->CreateBufferResource(sizeof(MaterialCB));
    materialBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    materialData_->WVP   = MakeIdentity4x4();
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };

    vertexBuffer_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * kVertexCount);
    vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
    vbv_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vbv_.SizeInBytes    = static_cast<UINT>(sizeof(VertexData) * kVertexCount);
    vbv_.StrideInBytes  = sizeof(VertexData);

    RebuildVertices();
    CreatePipeline();
}

void Ring::RebuildVertices()
{
    if (!vertexData_) { return; }

    const float kPi = std::numbers::pi_v<float>;
    int idx = 0;

    for (int i = 0; i < kDivisions; ++i) {
        float theta0 = 2.0f * kPi * static_cast<float>(i)     / kDivisions;
        float theta1 = 2.0f * kPi * static_cast<float>(i + 1) / kDivisions;
        float u0 = static_cast<float>(i)     / kDivisions;
        float u1 = static_cast<float>(i + 1) / kDivisions;

        Vector4 inner0 = { std::cos(theta0) * innerRadius_, 0.0f, std::sin(theta0) * innerRadius_, 1.0f };
        Vector4 outer0 = { std::cos(theta0) * outerRadius_, 0.0f, std::sin(theta0) * outerRadius_, 1.0f };
        Vector4 inner1 = { std::cos(theta1) * innerRadius_, 0.0f, std::sin(theta1) * innerRadius_, 1.0f };
        Vector4 outer1 = { std::cos(theta1) * outerRadius_, 0.0f, std::sin(theta1) * outerRadius_, 1.0f };

        // 時計回り（D3D12 上面フロントフェイス）
        // Triangle 1: inner0, outer1, outer0
        vertexData_[idx++] = { inner0, { u0, 0.0f }, { 0.0f, 1.0f, 0.0f } };
        vertexData_[idx++] = { outer1, { u1, 1.0f }, { 0.0f, 1.0f, 0.0f } };
        vertexData_[idx++] = { outer0, { u0, 1.0f }, { 0.0f, 1.0f, 0.0f } };

        // Triangle 2: inner0, inner1, outer1
        vertexData_[idx++] = { inner0, { u0, 0.0f }, { 0.0f, 1.0f, 0.0f } };
        vertexData_[idx++] = { inner1, { u1, 0.0f }, { 0.0f, 1.0f, 0.0f } };
        vertexData_[idx++] = { outer1, { u1, 1.0f }, { 0.0f, 1.0f, 0.0f } };
    }
}

void Ring::Update(Camera* camera)
{
    Matrix4x4 world = MakeAffineMatrix(
        { scale_, scale_, scale_ },
        rotation_,
        position_
    );
    Matrix4x4 viewProj = Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());
    materialData_->WVP = Multiply(world, viewProj);
}

void Ring::Draw()
{
    ID3D12GraphicsCommandList* cmd = dxCommon_->GetCommandList();

    cmd->SetGraphicsRootSignature(rootSignature_.Get());
    cmd->SetPipelineState(pipelineState_.Get());
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &vbv_);

    cmd->SetGraphicsRootConstantBufferView(0, materialBuffer_->GetGPUVirtualAddress());

    SrvManager::GetInstance()->PreDraw();
    D3D12_GPU_DESCRIPTOR_HANDLE texHandle =
        TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_);
    cmd->SetGraphicsRootDescriptorTable(1, texHandle);

    cmd->DrawInstanced(kVertexCount, 1, 0, 0);
}

void Ring::CreatePipeline()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_DESCRIPTOR_RANGE texRange[1] = {};
    texRange[0].RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    texRange[0].NumDescriptors                    = 1;
    texRange[0].BaseShaderRegister                = 0; // t0
    texRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParams[2] = {};
    // [0] CBV b0 (WVP + color)
    rootParams[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;
    rootParams[0].Descriptor.ShaderRegister = 0;
    // [1] SRV t0 (texture)
    rootParams[1].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[1].DescriptorTable.pDescriptorRanges   = texRange;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; // CLAMP
    sampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MaxLOD           = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister   = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.NumParameters     = _countof(rootParams);
    rsDesc.pParameters       = rootParams;
    rsDesc.NumStaticSamplers = 1;
    rsDesc.pStaticSamplers   = &sampler;
    rsDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sigBlob, errBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                              &sigBlob, &errBlob);
    assert(SUCCEEDED(hr));
    hr = device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
                                      IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));

    IDxcBlob* vsBlob = dxCommon_->CompileShader(L"Resources/shaders/ring/Ring.VS.hlsl", L"vs_6_0");
    IDxcBlob* psBlob = dxCommon_->CompileShader(L"Resources/shaders/ring/Ring.PS.hlsl", L"ps_6_0");

    D3D12_INPUT_ELEMENT_DESC inputElems[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout    = { inputElems, _countof(inputElems) };
    psoDesc.VS             = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS             = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    // 加算ブレンド（発光エフェクト向け）
    auto& rt                 = psoDesc.BlendState.RenderTarget[0];
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    rt.BlendEnable           = TRUE;
    rt.SrcBlend              = D3D12_BLEND_SRC_ALPHA;
    rt.DestBlend             = D3D12_BLEND_ONE;
    rt.BlendOp               = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha         = D3D12_BLEND_ONE;
    rt.DestBlendAlpha        = D3D12_BLEND_ZERO;
    rt.BlendOpAlpha          = D3D12_BLEND_OP_ADD;

    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

    psoDesc.DepthStencilState.DepthEnable    = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc      = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    psoDesc.DSVFormat             = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.NumRenderTargets      = 1;
    psoDesc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleMask            = D3D12_DEFAULT_SAMPLE_MASK;
    psoDesc.SampleDesc.Count      = 1;

    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}
