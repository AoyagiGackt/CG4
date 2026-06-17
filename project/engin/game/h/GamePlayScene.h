/**
 * @file GamePlayScene.h
 * @brief ゲームプレイ本編のシーンロジックを管理するファイル
 */
#pragma once

// --- 標準ライブラリ ---
#include <memory>
#include <string>
#include <vector>

// --- エンジンシステム・基盤 ---
#include "Audio.h"
#include "BaseScene.h"
#include "Camera.h"
#include "DirectXCommon.h"
#include "GameObject.h"
#include "ImGuiManager.h"
#include "Input.h"
#include "Model.h"
#include "ModelCommon.h"
#include "Object3dCommon.h"
#include "ShadowManager.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "SrvManager.h"

// --- ゲームロジック・オブジェクト ---
#include "Object3d.h"
#include "GameTime.h"
#include "Skydome.h"
#include "HitEffect.h"

/**
 * @brief ゲームプレイ本編のシーンクラス
 */
class GamePlayScene : public BaseScene {
public:
	// --- 基本関数 ---
	void Initialize(DirectXCommon* dxCommon, Input* input, Audio* audio) override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

	// --- パブリックメソッド ---
	void SetImGuiManager(ImGuiManager* imgui) { imguiManager_ = imgui; }

private:
	// --- 内部処理関数 ---
	void UpdateDebugUI();
	void DrawShadowPass();

	// --- データセーブ・ロード関数 ---
	void SaveCameraParams();
	void LoadCameraParams();
	void SaveUILayout();
	void LoadUILayout();

	// =================================================
	// メンバ変数
	// =================================================

	// --- 外部システムポインタ ---
	DirectXCommon* dxCommon_     = nullptr;
	Input*         input_        = nullptr;
	Audio*         audio_        = nullptr;
	ImGuiManager*  imguiManager_ = nullptr;

	// --- 描画・共通基盤リソース ---
	std::unique_ptr<SpriteCommon>   spriteCommon_;
	std::unique_ptr<ModelCommon>    modelCommon_;
	std::unique_ptr<Object3dCommon> objectCommon_;
	std::unique_ptr<ShadowManager>  shadowManager_;
	std::unique_ptr<Camera>         camera_;

	// --- 天球 ---
	std::unique_ptr<Model>    modelSkydome_;
	std::unique_ptr<Skydome>  skydome_;

	// --- ゲームオブジェクト群（将来拡張用） ---
	std::vector<std::unique_ptr<GameObject>> gameObjects_;

	// --- 進行・状態管理 ---
	GameTime gameTime_;

	// --- ヒットエフェクト ---
	std::unique_ptr<HitEffect> hitEffect_;
	float hitEffectTimer_ = 0.0f;
	int   hitEffectIndex_ = 0;
	static constexpr float kHitEffectInterval = 1.2f;

	// --- Skydome パラメータ ---
	Vector4 skyColor_      = { 1.0f, 1.0f, 1.0f, 1.0f };
	float   skyRotOffsetY_ = 0.0f;

	// --- デバッグ・エディタ関連 ---
	bool debugEditMode_ = false;

	enum class SelectedType { None, Camera, Skydome, UIElement };
	SelectedType editorSelectedType_  = SelectedType::None;
	int          editorSelectedIndex_ = -1;

	struct UIEntry {
		std::string             name;
		std::unique_ptr<Sprite> sprite;
		std::string             texPath;
	};
	std::vector<UIEntry> uiElements_;
};
