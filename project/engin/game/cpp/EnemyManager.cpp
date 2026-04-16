#include "EnemyManager.h"
#include "EnemyDeathEffect.h"
#include "BulletHitEffect.h"
#include "ImGuiManager.h"
#include "MapChipField.h"
#include "ScoreManager.h"
#include "Camera.h"

void EnemyManager::Initialize(ModelCommon* modelCommon, Model* model, Input* input, Model* bulletModel, MapChipField* mapChipField) {
	modelCommon_ = modelCommon;
	model_ = model;
	input_ = input;
	bulletModel_ = bulletModel;
	mapChipField_ = mapChipField;
}

void EnemyManager::SpawnEnemy(const Vector3& position) {
	if (!modelCommon_ || !model_ || !input_ || !bulletModel_ || !mapChipField_) {
		return;
	}

	auto enemy = std::make_unique<Enemy>();
	if (!enemy) {
		return;
	}

	// 修正：5つの引数を渡す
	enemy->Initialize(modelCommon_, model_, input_, bulletModel_, mapChipField_);

	enemy->SetPosition(position);

	enemies_.push_back(std::move(enemy));
}

void EnemyManager::Update(Camera* camera, std::list<std::unique_ptr<Bullet>>& playerBullets, Player* player, float scrollMultiplier) {

	// スポーン管理
	if (!debugSpawnDisabled_) { spawnTimer_++; }
	if (!debugSpawnDisabled_ && spawnTimer_ > spawnInterval_ && enemies_.size() < (size_t)maxEnemy_) {
		float x = 20.0f + (rand() % 5);
		SpawnEnemy({ x,3,0 });
		spawnTimer_ = 0;
	}

	// 地面スクロール速度（MapChipField と同じ基準値）
	float scrollSpeed = 0.1f * scrollMultiplier;

	//敵の更新
	for (auto& enemy : enemies_) {
		enemy->Update(camera, scrollSpeed);
	}

	// 画面外（左端）に出た敵を削除
	float leftEdge = camera->GetTranslate().x - 16.0f;
	enemies_.erase(
		std::remove_if(enemies_.begin(), enemies_.end(),
			[leftEdge](const std::unique_ptr<Enemy>& e) {
				return e->GetPosition().x < leftEdge;
			}),
		enemies_.end()
	);

	CheckPlayerBulletCollisions(playerBullets);
	CheckPlayerEnemyBodyCollisions(player);
	RemoveDeadEnemies(player);
	DebugDraw();

}

void EnemyManager::Draw() {

	for (auto& enemy : enemies_) {

		enemy->Draw();
	}
}

void EnemyManager::DrawBullets() {
	for (auto& enemy : enemies_) {
		enemy->DrawBullets();
	}
}

void EnemyManager::DrawShadow() {

	for (auto& enemy : enemies_) {

		enemy->DrawShadow();
	}
}

void EnemyManager::CheckPlayerBulletCollisions(std::list<std::unique_ptr<Bullet>>& playerBullets) {

	for (auto& enemy : enemies_) {

		auto it = playerBullets.begin();
		while (it != playerBullets.end()) {
			Bullet* bullet = it->get();

			Vector3 ePos = enemy->GetPosition();
			Vector3 bPos = bullet->GetPosition();

			bool hit =
				bPos.x + bullet->GetRadius() > ePos.x - enemy.get()->GetHalfWidth() &&
				bPos.x - bullet->GetRadius() < ePos.x + enemy.get()->GetHalfWidth() &&
				bPos.y + bullet->GetRadius() > ePos.y - enemy.get()->GetHalfHeight() &&
				bPos.y - bullet->GetRadius() < ePos.y + enemy.get()->GetHalfHeight();

			if (hit) {
				BulletHitEffect::Emit(bPos);
				enemy->Damage(bullet->GetDamage());
				it = playerBullets.erase(it); // 当たった弾は消す
			} else {
				++it;
			}
		}
	}

}

void EnemyManager::CheckPlayerEnemyBodyCollisions(Player* player) {
	if (!player || player->IsInvincible()) {
		return;
	}

	AABB playerAABB = player->GetAABB();

	for (auto& enemy : enemies_) {
		Vector3 ePos = enemy->GetPosition();
		AABB enemyAABB = {
			{ ePos.x - enemy->GetHalfWidth(), ePos.y - enemy->GetHalfHeight(), ePos.z - 0.5f },
			{ ePos.x + enemy->GetHalfWidth(), ePos.y + enemy->GetHalfHeight(), ePos.z + 0.5f }
		};

		if (Collision::CheckCollision(playerAABB, enemyAABB)) {
			player->TakeDamage(20.0f);
			break; // 1フレームで複数敵から同時ダメージを防ぐ
		}
	}
}

void EnemyManager::RemoveDeadEnemies(Player* player) {
	// 1. 削除する前に、死んでいる敵を探してプレイヤーに通知する
	for (auto& enemy : enemies_) {
		if (enemy->IsDead()) {
			// 撃破エフェクト
			EnemyDeathEffect::Emit(enemy->GetPosition());
			// プレイヤーに撃破を伝える（ここでストック計算が行われる）
			player->OnEnemyDefeated();
			// スコアを加算
			ScoreManager::GetInstance()->AddScore(200);
		}
	}

	// 2. その後、実際にリストから削除する（既存の処理）
	enemies_.erase(
		std::remove_if(enemies_.begin(), enemies_.end(),
			[](const std::unique_ptr<Enemy>& e)
	{
		return e->IsDead();
	}),
		enemies_.end()
	);
}

void EnemyManager::DebugDraw() {
    // 統合Inspectorパネルへ移行
}

std::list<std::unique_ptr<Bullet>>& EnemyManager::GetBullets() {
	// リストを一度空にする
	allEnemyBullets_.clear();

	// 現在生きている全敵の弾を一つのリストに集約する
	for (auto& enemy : enemies_) {
		auto& enemyBullets = enemy->GetBullets(); // Enemyクラスに作ったGetBulletsを呼ぶ
		for (auto& bullet : enemyBullets) {
		}
	}
	return allEnemyBullets_;
}

