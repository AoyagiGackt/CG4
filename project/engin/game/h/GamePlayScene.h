/**
 * @file GamePlayScene.h
 * @brief ゲームプレイ本編のシーンロジックを管理するファイル
 */
#pragma once

 // --- 標準ライブラリ ---
#include <list>
#include <memory>
#include <string>
#include <vector>

// --- エンジンシステム・基盤 ---
#include "Audio.h"
#include "BaseScene.h"
#include "Camera.h"
#include "CollisionManager.h"
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
#include "Bullet.h"
#include "EnemyManager.h"
#include "GameTime.h"
#include "MapChipField.h"
#include "Player.h"
#include "PlayerManager.h"
#include "Skydome.h"
#include "hoge.h"

/**
 * @brief ゲームプレイ本編のシーンクラス
 */
class GamePlayScene : public BaseScene{
public:
	// --- 基本関数 ---
	void Initialize(DirectXCommon* dxCommon,Input* input,Audio* audio) override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

	// --- パブリックメソッド ---
	void SetImGuiManager(ImGuiManager* imgui){ imguiManager_ = imgui; }

private:
	// --- 内部処理関数 ---
	void UpdateDebugUI();
	void DrawShadowPass();

	// --- データセーブ・ロード関数 ---
	void SaveCameraParams();
	void LoadCameraParams();
	void SaveEnemyParams();
	void LoadEnemyParams();
	void SaveModelPaths();
	void LoadModelPaths();
	void SaveUILayout();
	void LoadUILayout();

	// =================================================
	// メンバ変数
	// =================================================

	// --- 外部システムポインタ ---
	DirectXCommon* dxCommon_ = nullptr;
	Input* input_ = nullptr;
	Audio* audio_ = nullptr;
	ImGuiManager* imguiManager_ = nullptr;

	// --- 描画・共通基盤リソース ---
	std::unique_ptr<SpriteCommon> spriteCommon_;
	std::unique_ptr<ModelCommon> modelCommon_;
	std::unique_ptr<Object3dCommon> objectCommon_;
	std::unique_ptr<ShadowManager> shadowManager_;
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<CollisionManager> collisionManager_;

	// --- マネージャクラス ---
	std::unique_ptr<PlayerManager> playerManager_;
	std::unique_ptr<EnemyManager> enemyManager_;

	// --- 3Dモデル群 ---
	std::unique_ptr<Model> modelPlayer_;
	std::unique_ptr<Model> modelEnemy_;
	std::unique_ptr<Model> modelBullet_;
	std::unique_ptr<Model> modelBeam_;
	std::unique_ptr<Model> modelSkydome_;
	std::unique_ptr<Model> modelHuman_;

	// --- human オブジェクト ---
	std::unique_ptr<Object3d> objectHuman_;

	// --- 天球 ---
	std::unique_ptr<Skydome> skydome_;

	// --- ゲームオブジェクト群 ---
	std::vector<std::unique_ptr<GameObject>> gameObjects_;
	std::list<std::unique_ptr<Bullet>> bullets_;
	std::unique_ptr<MapChipField> mapField_;

	// --- 進行・状態管理 ---
	GameTime gameTime_;

	// --- 楕円パーティクル ---
	float ellipseParticleTimer_ = 0.0f; ///< 放出タイマー（秒）
	static constexpr float kEllipseEmitInterval = 0.05f; ///< 放出間隔（秒）

	// --- デバッグ・エディタ関連 ---
	bool debugScrollPaused_ = false;
	bool debugSpawnDisabled_ = false;
	bool debugEditMode_ = false;

	// --- Human パラメータ ---
	Vector3 humanPosition_        = { 14.5f, 0.0f, 0.0f };
	Vector4 humanColor_           = { 0.9f, 0.9f, 0.9f, 1.0f }; // シルバー
	float   humanEnvMapIntensity_ = 1.0f;
	float   humanShininess_       = 128.0f;
	bool    humanUseTexture_      = true;

	std::string playerObjPath_ = "Resources/player/player.obj";
	std::string playerTexPath_ = "Resources/player/player.png";
	std::string enemyObjPath_ = "Resources/boss/boss.obj";
	std::string enemyTexPath_ = "Resources/boss/boss.png";

	enum class SelectedType{ None,Player,Enemy,Camera,EnemySettings,UIElement,Human };
	SelectedType editorSelectedType_ = SelectedType::None;
	int editorSelectedIndex_ = -1;

	struct UIEntry{
		std::string name;
		std::unique_ptr<Sprite> sprite;
		std::string texPath;
	};
	std::vector<UIEntry> uiElements_;
};