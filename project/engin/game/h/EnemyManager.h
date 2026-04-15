#pragma once
#include "Enemy.h"
#include <vector>
#include <memory>
#include "Bullet.h"
#include "Collision.h"
#include "Player.h"
#include "Sprite.h"
#include "SpriteCommon.h"

class MapChipField;

class EnemyManager{
public:

	void Initialize(ModelCommon* modelCommon,Model* model,Input* input,Model* bulletModel,MapChipField* mapChipField);

	//============================
	// 更新
	//============================
	void Update(Camera* camera,std::list<std::unique_ptr<Bullet>>& playerBullets,Player* player, float scrollMultiplier);

	//============================
	// 描画
	//============================
	void Draw();

	/** @brief 弾のみ描画（skyOverlay の後に呼ぶ） */
	void DrawBullets();

	/** @brief シャドウパス用描画（全敵 + 全敵弾） */
	void DrawShadow();

	//============================
	// Enemy生成
	//============================
	void SpawnEnemy(const Vector3& position);

	//============================
	// Getter（デバッグ用）
	//============================
	const std::vector<std::unique_ptr<Enemy>>& GetEnemies() const{ return enemies_; }

	void SetDebugSpawnDisabled(bool v){ debugSpawnDisabled_ = v; }

	// スポーン制御
	void SetSpawnInterval(int interval) { spawnInterval_ = interval; }
	int  GetSpawnInterval() const { return spawnInterval_; }
	void SetMaxEnemy(int max) { maxEnemy_ = max; }
	int  GetMaxEnemy() const { return maxEnemy_; }

	// 全ての敵の弾をまとめて取得する関数
	std::list<std::unique_ptr<Bullet>>& GetBullets();

private:

	//============================
    // Enemy管理
    //============================
	std::vector<std::unique_ptr<Enemy>> enemies_;
	Model* bulletModel_ = nullptr;

	//============================
	// 全ての敵の弾をまとめて管理するリスト
	//============================
	std::list<std::unique_ptr<Bullet>> allEnemyBullets_;

	//============================
	// 共有リソース
	//============================
	ModelCommon* modelCommon_ = nullptr;
	Model* model_ = nullptr;
	Input* input_ = nullptr;
	MapChipField* mapChipField_;

	//============================
	// Spawn管理
	//============================
	int spawnTimer_    = 0;
	int spawnInterval_ = 180; // スポーン間隔（フレーム数）
	int maxEnemy_      = 10;
	bool debugSpawnDisabled_ = false;

	//playerの弾とenemyの当たり判定
	void CheckPlayerBulletCollisions(std::list<std::unique_ptr<Bullet>>& playerBullets);
	//playerとenemyのボディ当たり判定
	void CheckPlayerEnemyBodyCollisions(Player* player);
	//死んだ敵の削除処理
	void RemoveDeadEnemies(Player* player);

	//imguiの処理
	void DebugDraw();

};