#include "GamePlayScene.h"
#include <algorithm>
#include <cmath>
#include <commdlg.h>
#include <fstream>
#include <sstream>
#pragma comment(lib, "comdlg32.lib")
#include "ImguiManager.h"
#include "ParticleManager.h"
#include "SceneManager.h"
#include "ScoreManager.h"
#include "TextureManager.h"

// =====================================================
// 初期化
// =====================================================

void GamePlayScene::Initialize(DirectXCommon* dxCommon, Input* input, Audio* audio)
{
    dxCommon_ = dxCommon;
    input_    = input;
    audio_    = audio;

    // ----- 描画共通設定 -----
    spriteCommon_ = std::make_unique<SpriteCommon>();
    spriteCommon_->Initialize(dxCommon_);

    modelCommon_ = std::make_unique<ModelCommon>();
    modelCommon_->Initialize(dxCommon_);

    objectCommon_ = std::make_unique<Object3dCommon>();
    objectCommon_->Initialize(dxCommon_);

    shadowManager_ = std::make_unique<ShadowManager>();
    shadowManager_->Initialize(dxCommon_, SrvManager::GetInstance());

    // ----- カメラ -----
    camera_ = std::make_unique<Camera>();
    camera_->SetTranslate({ 14.5f, 6.0f, -30.0f });
    Object3d::SetCommonCamera(camera_.get());

    // ----- 天球 -----
    modelSkydome_ = std::make_unique<Model>();
    modelSkydome_->Initialize(modelCommon_.get(),
        "Resources/SkyDome/SkyDome.obj",
        "Resources/rostock_laage_airport_4k.dds");

    skydome_ = std::make_unique<Skydome>();
    skydome_->Initialize(modelCommon_.get(), modelSkydome_.get());

    // ----- ヒットエフェクト -----
    hitEffect_ = std::make_unique<HitEffect>();

    ParticleManager::GetInstance()->CreateParticleGroup("hit_slash",     "Resources/gradationLine.png");
    ParticleManager::GetInstance()->CreateParticleGroup("hit_impact",    "Resources/circle2.png");
    ParticleManager::GetInstance()->CreateParticleGroup("hit_explosion", "Resources/white.png");

    ScoreManager::GetInstance()->LoadScores();
    ScoreManager::GetInstance()->ResetCurrentScore();
    gameTime_.Initialize();

    // ----- デバッグパラメータ読み込み -----
    LoadCameraParams();
    LoadUILayout();
}

// =====================================================
// 更新
// =====================================================

void GamePlayScene::Update()
{
    gameTime_.Update(1.0f);
    float timeRatio = gameTime_.GetElapsedMinutes() / GameTime::kTotalGameMinutes;

    skydome_->Update(camera_.get(), timeRatio);

    shadowManager_->Update(objectCommon_->GetLightDirection());
    Object3d::SetLightViewProjection(shadowManager_->GetLightViewProjection());

    for (auto& obj : gameObjects_) {
        obj->Update();
    }

    // Auto Loop が有効なら選択中エフェクトを繰り返し発火
    if (hitEffectAutoLoop_) {
        hitEffectLoopTimer_ += 1.0f / 60.0f;
        if (hitEffectLoopTimer_ >= hitEffectLoopInterval_) {
            hitEffectLoopTimer_ = 0.0f;
            hitEffect_->Trigger({ 14.5f, 2.0f, 0.0f },
                static_cast<HitEffect::Type>(selectedEffectType_));
        }
    }

    UpdateDebugUI();
}

// =====================================================
// ファイルダイアログヘルパー（デバッグ専用）
// =====================================================

#ifdef USE_IMGUI
static std::string OpenFileDialog(const char* filter, const char* initialDir = nullptr)
{
    char szFile[MAX_PATH] = {};
    OPENFILENAMEA ofn     = {};
    ofn.lStructSize       = sizeof(ofn);
    ofn.hwndOwner         = GetActiveWindow();
    ofn.lpstrFilter       = filter;
    ofn.lpstrFile         = szFile;
    ofn.nMaxFile          = MAX_PATH;
    ofn.lpstrInitialDir   = initialDir;
    ofn.Flags             = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameA(&ofn) ? std::string(szFile) : "";
}
#endif

// =====================================================
// デバッグ UI（ImGui）
// =====================================================

void GamePlayScene::UpdateDebugUI()
{
#ifdef USE_IMGUI
    if (!imguiManager_) { return; }

    // =====================================================
    // Hierarchy（左パネル）
    // =====================================================
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(220, 280), ImGuiCond_Once);
    ImGui::Begin("Hierarchy");

    if (ImGui::Selectable("Camera", editorSelectedType_ == SelectedType::Camera)) {
        editorSelectedType_  = SelectedType::Camera;
        editorSelectedIndex_ = -1;
    }
    if (ImGui::Selectable("Skydome", editorSelectedType_ == SelectedType::Skydome)) {
        editorSelectedType_  = SelectedType::Skydome;
        editorSelectedIndex_ = -1;
    }
    if (ImGui::Selectable("Hit Effects", editorSelectedType_ == SelectedType::HitEffects)) {
        editorSelectedType_  = SelectedType::HitEffects;
        editorSelectedIndex_ = -1;
    }

    // ----- UI Elements -----
    char uiHeader[48];
    snprintf(uiHeader, sizeof(uiHeader), "UI Elements (%d)", (int)uiElements_.size());
    bool uiOpen = ImGui::TreeNodeEx(uiHeader);
    ImGui::SameLine();
    if (ImGui::SmallButton("+##addUI")) {
        UIEntry entry;
        entry.name   = "UI Element " + std::to_string(uiElements_.size() + 1);
        entry.sprite = std::make_unique<Sprite>();
        entry.sprite->Initialize(spriteCommon_.get(), entry.texPath);
        entry.sprite->SetPosition({ 640.0f, 360.0f });
        entry.sprite->SetSize({ 100.0f, 100.0f });
        uiElements_.push_back(std::move(entry));
    }
    if (uiOpen) {
        for (int i = 0; i < (int)uiElements_.size(); i++) {
            bool sel = (editorSelectedType_ == SelectedType::UIElement && editorSelectedIndex_ == i);
            char label[80];
            snprintf(label, sizeof(label), "  %s", uiElements_[i].name.c_str());
            if (ImGui::Selectable(label, sel)) {
                editorSelectedType_  = SelectedType::UIElement;
                editorSelectedIndex_ = i;
            }
        }
        ImGui::TreePop();
    }

    // ----- Save ボタン -----
    ImGui::Separator();
    static float savedTimer = 0.0f;
    if (savedTimer > 0.0f) {
        savedTimer -= 1.0f / 60.0f;
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Saved!");
    } else {
        if (ImGui::Button("Save", ImVec2(-1, 0))) {
            switch (editorSelectedType_) {
            case SelectedType::Camera:    SaveCameraParams(); savedTimer = 1.5f; break;
            case SelectedType::UIElement: SaveUILayout();     savedTimer = 1.5f; break;
            default: break;
            }
        }
    }

    ImGui::End();

    // =====================================================
    // Inspector（右パネル）
    // =====================================================
    ImGui::SetNextWindowPos(ImVec2(1060, 0), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(220, 500), ImGuiCond_Once);
    ImGui::Begin("Inspector");

    if (editorSelectedType_ == SelectedType::UIElement &&
        (editorSelectedIndex_ < 0 || editorSelectedIndex_ >= (int)uiElements_.size())) {
        editorSelectedType_ = SelectedType::None;
    }

    switch (editorSelectedType_) {

    case SelectedType::Camera: {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "[Camera]");
        ImGui::Separator();
        Vector3 pos = camera_->GetTranslate();
        if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) { camera_->SetTranslate(pos); }
        Vector3 rot = camera_->GetRotate();
        if (ImGui::DragFloat3("Rotation", &rot.x, 0.01f)) { camera_->SetRotate(rot); }
        ImGui::Separator();
        if (ImGui::Button("Save##inspCam")) { SaveCameraParams(); }
        break;
    }

    case SelectedType::Skydome: {
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1), "[Skydome]");
        ImGui::Separator();
        if (ImGui::ColorEdit4("Sky Color", &skyColor_.x)) { skydome_->SetSkyColor(skyColor_); }
        ImGui::Separator();
        if (ImGui::SliderFloat("Rotation Offset Y", &skyRotOffsetY_, -3.14159265f, 3.14159265f)) {
            skydome_->SetRotationOffsetY(skyRotOffsetY_);
        }
        if (ImGui::Button("Reset Offset")) {
            skyRotOffsetY_ = 0.0f;
            skydome_->SetRotationOffsetY(0.0f);
        }
        break;
    }

    case SelectedType::HitEffects: {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1), "[Hit Effects]");
        ImGui::Separator();

        // エフェクト種類の選択
        ImGui::Text("Type:");
        ImGui::RadioButton("Slash",     &selectedEffectType_, 0); ImGui::SameLine();
        ImGui::RadioButton("Impact",    &selectedEffectType_, 1); ImGui::SameLine();
        ImGui::RadioButton("Explosion", &selectedEffectType_, 2);

        ImGui::Separator();

        // 手動発火
        const Vector3 kCenter = { 14.5f, 2.0f, 0.0f };
        if (ImGui::Button("Fire!", ImVec2(-1, 0))) {
            hitEffect_->Trigger(kCenter, static_cast<HitEffect::Type>(selectedEffectType_));
        }

        ImGui::Separator();

        // 自動ループ
        ImGui::Checkbox("Auto Loop", &hitEffectAutoLoop_);
        if (hitEffectAutoLoop_) {
            ImGui::SliderFloat("Interval##he", &hitEffectLoopInterval_, 0.2f, 3.0f);
            ImGui::Text("Timer: %.2f / %.2f", hitEffectLoopTimer_, hitEffectLoopInterval_);
        }
        break;
    }

    case SelectedType::UIElement: {
        UIEntry& entry = uiElements_[editorSelectedIndex_];
        Sprite*  sp    = entry.sprite.get();
        ImGui::TextColored(ImVec4(1, 0.5f, 1, 1), "[UI Element]");
        ImGui::Separator();

        static int  lastUIIdx  = -2;
        static char uiNameBuf[64] = {};
        if (lastUIIdx != editorSelectedIndex_) {
            lastUIIdx = editorSelectedIndex_;
            strncpy_s(uiNameBuf, entry.name.c_str(), sizeof(uiNameBuf) - 1);
        }
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##uiname", uiNameBuf, sizeof(uiNameBuf))) { entry.name = uiNameBuf; }

        ImGui::Separator();

        Vector2 pos = sp->GetPosition();
        if (ImGui::DragFloat2("Position", &pos.x, 1.0f)) { sp->SetPosition(pos); }
        Vector2 sz = sp->GetSize();
        if (ImGui::DragFloat2("Size", &sz.x, 1.0f, 1.0f, 4096.0f)) { sp->SetSize(sz); }
        float rot = sp->GetRotation();
        if (ImGui::DragFloat("Rotation", &rot, 0.01f)) { sp->SetRotation(rot); }
        Vector4 col = sp->GetColor();
        if (ImGui::ColorEdit4("Color", &col.x)) { sp->SetColor(col); }

        ImGui::Separator();
        ImGui::TextDisabled("%.40s", entry.texPath.empty() ? "(no texture)" : entry.texPath.c_str());
        if (ImGui::Button("Browse##uitex")) {
            std::string p = OpenFileDialog("PNG Files\0*.png\0All Files\0*.*\0\0", "Resources");
            if (!p.empty()) { entry.texPath = p; sp->SetTexture(p); }
        }

        ImGui::Separator();
        if (ImGui::Button("Delete##ui")) {
            uiElements_.erase(uiElements_.begin() + editorSelectedIndex_);
            editorSelectedType_  = SelectedType::None;
            editorSelectedIndex_ = -1;
            lastUIIdx = -2;
            SaveUILayout();
        } else {
            ImGui::SameLine();
            if (ImGui::Button("Save##inspUI"))  { SaveUILayout();  }
            ImGui::SameLine();
            if (ImGui::Button("Load##inspUI"))  { LoadUILayout();  }
        }
        break;
    }

    default:
        ImGui::TextDisabled("(Nothing selected)");
        ImGui::TextDisabled("Select an object");
        ImGui::TextDisabled("in the Hierarchy.");
        break;
    }

    ImGui::End();

    // =====================================================
    // Scene Controls（左下パネル）
    // =====================================================
    ImGui::SetNextWindowPos(ImVec2(0, 280), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(220, 240), ImGuiCond_Once);
    ImGui::Begin("Scene Controls");

    if (ImGui::CollapsingHeader("Score")) {
        ImGui::Text("Current : %d", ScoreManager::GetInstance()->GetCurrentScore());
        const auto& ranking = ScoreManager::GetInstance()->GetRanking();
        if (ranking.empty()) { ImGui::TextDisabled("  (no records)"); }
        for (int i = 0; i < (int)ranking.size(); ++i) { ImGui::Text("  %2d. %d", i + 1, ranking[i]); }
        if (ImGui::Button("Reset All Scores")) { ScoreManager::GetInstance()->ResetAllScores(); }
    }

    if (ImGui::CollapsingHeader("Game Time")) {
        ImGui::Text("Time : %02d:%02d", gameTime_.GetHour(), gameTime_.GetMinute());
        if (ImGui::Button("Skip 1 Hour")) { gameTime_.SkipMinutes(60.0f); }
    }

    if (ImGui::CollapsingHeader("Actions")) {
        if (ImGui::Button("Game Clear")) { SceneManager::GetInstance()->ChangeScene("CLEAR"); }
        ImGui::SameLine();
        if (ImGui::Button("Game Over"))  { SceneManager::GetInstance()->ChangeScene("GAMEOVER"); }
    }

    ImGui::End();

    // =====================================================
    // Edit Mode（フローティングボタン）
    // =====================================================
    ImGui::SetNextWindowPos(ImVec2(230, 0), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(160, 60), ImGuiCond_Once);
    ImGui::Begin("Edit Mode", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
    if (debugEditMode_) {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.8f, 0.3f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.4f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.6f, 0.2f, 0.0f, 1.0f));
        if (ImGui::Button("EDIT MODE ON", ImVec2(-1, 0))) { debugEditMode_ = false; }
        ImGui::PopStyleColor(3);
    } else {
        if (ImGui::Button("EDIT MODE OFF", ImVec2(-1, 0))) { debugEditMode_ = true; }
    }
    ImGui::End();

    // =====================================================
    // Camera Control（常時表示・画面上部中央）
    // =====================================================
    ImGui::SetNextWindowPos(ImVec2(400, 0), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(300, 130), ImGuiCond_Once);
    ImGui::Begin("Camera Control");
    Vector3 camPos = camera_->GetTranslate();
    Vector3 camRot = camera_->GetRotate();
    if (ImGui::DragFloat3("Pos", &camPos.x, 0.1f))  { camera_->SetTranslate(camPos); }
    if (ImGui::DragFloat3("Rot", &camRot.x, 0.01f)) { camera_->SetRotate(camRot); }
    ImGui::Spacing();
    if (ImGui::Button("Center")) { camera_->SetTranslate({ 0,0,0 }); camera_->SetRotate({ 0,0,0 }); }
    ImGui::SameLine();
    if (ImGui::Button("Save##cam")) { SaveCameraParams(); }
    ImGui::SameLine();
    if (ImGui::Button("Load##cam")) { LoadCameraParams(); }
    ImGui::End();
#endif
}

// =====================================================
// デバッグパラメータ JSON 保存 / 読み込み
// =====================================================

static float ReadJsonFloat(const std::string& src, const std::string& key, float def)
{
    std::string needle = "\"" + key + "\": ";
    auto pos = src.find(needle);
    if (pos == std::string::npos) { return def; }
    pos += needle.size();
    try { return std::stof(src.substr(pos)); } catch (...) { return def; }
}

static int ReadJsonInt(const std::string& src, const std::string& key, int def)
{
    std::string needle = "\"" + key + "\": ";
    auto pos = src.find(needle);
    if (pos == std::string::npos) return def;
    pos += needle.size();
    try { return std::stoi(src.substr(pos)); } catch (...) { return def; }
}

static std::string ReadJsonString(const std::string& src, const std::string& key, const std::string& def)
{
    std::string needle = "\"" + key + "\":";
    auto pos = src.find(needle);
    if (pos == std::string::npos) return def;
    pos += needle.size();
    while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t')) pos++;
    if (pos >= src.size() || src[pos] != '"') return def;
    pos++;
    auto end = src.find('"', pos);
    if (end == std::string::npos) return def;
    return src.substr(pos, end - pos);
}

// ---- カメラ ----
void GamePlayScene::SaveCameraParams()
{
    std::ofstream f("Resources/debug_camera.json");
    if (!f) return;
    Vector3 cp = camera_->GetTranslate();
    Vector3 cr = camera_->GetRotate();
    f << "{\n";
    f << "  \"camera_pos_x\": " << cp.x << ",\n";
    f << "  \"camera_pos_y\": " << cp.y << ",\n";
    f << "  \"camera_pos_z\": " << cp.z << ",\n";
    f << "  \"camera_rot_x\": " << cr.x << ",\n";
    f << "  \"camera_rot_y\": " << cr.y << ",\n";
    f << "  \"camera_rot_z\": " << cr.z << "\n";
    f << "}\n";
}

void GamePlayScene::LoadCameraParams()
{
    std::ifstream f("Resources/debug_camera.json");
    if (!f) return;
    std::string src((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    camera_->SetTranslate({
        ReadJsonFloat(src, "camera_pos_x", camera_->GetTranslate().x),
        ReadJsonFloat(src, "camera_pos_y", camera_->GetTranslate().y),
        ReadJsonFloat(src, "camera_pos_z", camera_->GetTranslate().z),
    });
    camera_->SetRotate({
        ReadJsonFloat(src, "camera_rot_x", camera_->GetRotate().x),
        ReadJsonFloat(src, "camera_rot_y", camera_->GetRotate().y),
        ReadJsonFloat(src, "camera_rot_z", camera_->GetRotate().z),
    });
}

// ---- UI レイアウト ----
void GamePlayScene::SaveUILayout()
{
    std::ofstream f("Resources/debug_ui.json");
    if (!f) return;
    f << "{\n";
    f << "  \"count\": " << uiElements_.size();
    for (int i = 0; i < (int)uiElements_.size(); i++) {
        const UIEntry& e  = uiElements_[i];
        Sprite*        sp = e.sprite.get();
        Vector2 pos = sp->GetPosition();
        Vector2 sz  = sp->GetSize();
        Vector4 col = sp->GetColor();
        float   rot = sp->GetRotation();
        f << ",\n";
        f << "  \"ui_" << i << "_name\":  \"" << e.name     << "\",\n";
        f << "  \"ui_" << i << "_tex\":   \"" << e.texPath  << "\",\n";
        f << "  \"ui_" << i << "_pos_x\": " << pos.x << ",\n";
        f << "  \"ui_" << i << "_pos_y\": " << pos.y << ",\n";
        f << "  \"ui_" << i << "_sz_x\":  " << sz.x  << ",\n";
        f << "  \"ui_" << i << "_sz_y\":  " << sz.y  << ",\n";
        f << "  \"ui_" << i << "_rot\":   " << rot    << ",\n";
        f << "  \"ui_" << i << "_col_r\": " << col.x << ",\n";
        f << "  \"ui_" << i << "_col_g\": " << col.y << ",\n";
        f << "  \"ui_" << i << "_col_b\": " << col.z << ",\n";
        f << "  \"ui_" << i << "_col_a\": " << col.w;
    }
    f << "\n}\n";
}

void GamePlayScene::LoadUILayout()
{
    std::ifstream f("Resources/debug_ui.json");
    if (!f) return;
    std::string src((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    int count = ReadJsonInt(src, "count", 0);
    uiElements_.clear();
    editorSelectedType_  = SelectedType::None;
    editorSelectedIndex_ = -1;
    for (int i = 0; i < count; i++) {
        auto mk = [&](const char* field) { return "ui_" + std::to_string(i) + field; };
        UIEntry entry;
        entry.name    = ReadJsonString(src, mk("_name"), "UI Element");
        entry.texPath = ReadJsonString(src, mk("_tex"),  "");
        entry.sprite  = std::make_unique<Sprite>();
        entry.sprite->Initialize(spriteCommon_.get(), entry.texPath);
        entry.sprite->SetPosition({ ReadJsonFloat(src, mk("_pos_x"), 640.0f), ReadJsonFloat(src, mk("_pos_y"), 360.0f) });
        entry.sprite->SetSize(    { ReadJsonFloat(src, mk("_sz_x"),  100.0f), ReadJsonFloat(src, mk("_sz_y"),  100.0f) });
        entry.sprite->SetRotation(  ReadJsonFloat(src, mk("_rot"),   0.0f));
        entry.sprite->SetColor(   { ReadJsonFloat(src, mk("_col_r"), 1.0f),
                                    ReadJsonFloat(src, mk("_col_g"), 1.0f),
                                    ReadJsonFloat(src, mk("_col_b"), 1.0f),
                                    ReadJsonFloat(src, mk("_col_a"), 1.0f) });
        uiElements_.push_back(std::move(entry));
    }
}

// =====================================================
// 描画
// =====================================================

void GamePlayScene::DrawShadowPass()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    shadowManager_->BeginShadowPass(commandList);
    modelCommon_->BeginShadowPass();

    shadowManager_->EndShadowPass(commandList);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv     = dxCommon_->GetCurrentBackBufferHandle();
    D3D12_CPU_DESCRIPTOR_HANDLE dsv     = dxCommon_->GetDsvHandle();
    commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    D3D12_VIEWPORT vp      = { 0, 0, 1280.0f, 720.0f, 0.0f, 1.0f };
    D3D12_RECT     scissor = { 0, 0, 1280, 720 };
    commandList->RSSetViewports(1, &vp);
    commandList->RSSetScissorRects(1, &scissor);
}

void GamePlayScene::Draw()
{
    DrawShadowPass();

    // 3D
    modelCommon_->CommonDrawSettings();
    objectCommon_->SetDefaultLight(dxCommon_->GetCommandList());
    shadowManager_->SetShadowMap(dxCommon_->GetCommandList(), SrvManager::GetInstance());

    skydome_->Draw();

    for (auto& obj : gameObjects_) {
        obj->Draw();
    }

    // パーティクル（ヒットエフェクト）
    ParticleManager::GetInstance()->Update(camera_.get());
    ParticleManager::GetInstance()->Draw(camera_.get());

    // 2D UI
    spriteCommon_->CommonDrawSettings();
    shadowManager_->SetShadowMap(dxCommon_->GetCommandList(), SrvManager::GetInstance());
    for (auto& e : uiElements_) {
        e.sprite->Update();
        e.sprite->Draw();
    }
}

// =====================================================
// 終了
// =====================================================

void GamePlayScene::Finalize()
{
    if (audio_) {
        audio_->StopBGM();
        audio_->StopAllSE();
    }
    ParticleManager::GetInstance()->ClearAllGroups();
}
