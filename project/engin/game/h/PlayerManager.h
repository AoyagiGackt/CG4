#pragma once
/**
 * @file PlayerManager.h
 * @brief プレイヤーの生成・更新・描画を一括管理するファイル
 */
#pragma once
#include "Player.h"
#include <memory>

class ModelCommon;
class Model;
class Input;
class MapChipField;

/**
 * @brief プレイヤーを管理するマネージャークラス
 * @note GamePlaySceneの負担を減らすため、プレイヤーの初期化や更新処理をカプセル化してます
 */
class PlayerManager{
public:
	/**
	 * @brief 初期化処理
	 * @param modelCommon 共通描画設定
	 * @param model プレイヤー用モデル
	 * @param input 入力デバイスのポインタ
	 * @param mapField 当たり判定用のマップチップフィールド
	 */
	void Initialize(ModelCommon* modelCommon,Model* model,Input* input,MapChipField* mapField,Camera* camera);

	/**
	 * @brief 毎フレームの更新処理
	 */
	void Update(float cameraPosX);

	/**
	 * @brief 描画処理
	 */
	void Draw();

	/** @brief シャドウパス用描画 */
	void DrawShadow();

	/**
	 * @brief プレイヤー本体のポインタを取得する
	 * @return Player* プレイヤーへのポインタ（カメラ追従や敵との当たり判定に使用）
	 */
	Player* GetPlayer() const{ return player_.get(); }

	float GetConditionSpeedMultiplier() const{ return 1.0f; }
	float GetScrollSpeedMultiplier() const{ return 1.0f; }

private:
	/** @brief プレイヤー本体のインスタンス */
	std::unique_ptr<Player> player_;
};