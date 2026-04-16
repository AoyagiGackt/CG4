#include "TeleportBomb.h"
#include "Player.h"
#include "EnemyManager.h"

void TeleportBomb::InitBomb(Player* player,Model* bulletModel,ModelCommon* modelCommon){
	player_ = player;

	// 弾オブジェクトの生成と初期化
	projectileObject_ = std::make_unique<Object3d>();
	projectileObject_->Initialize(modelCommon);
	projectileObject_->SetModel(bulletModel);

	state_ = State::None;
	isActive_ = false;
}

void TeleportBomb::Initialize(Player*){
	// 親クラスの制約用
}

void TeleportBomb::Update(){
	if (state_ == State::None || !projectileObject_) { return; }

	switch(state_){
	case State::ProjectileFlying:
		// 1. 弾の移動
		projectilePos_.x += projectileVelocity_.x;
		projectilePos_.y += projectileVelocity_.y;
		projectilePos_.z += projectileVelocity_.z;

		// 座標更新
		projectileObject_->GetTransform().translate = projectilePos_;
		projectileObject_->Update();

		// 寿命タイマー
		if(--projectileLifetime_ <= 0){
			state_ = State::None;
			isActive_ = false;
		}
		break;

	case State::TeleportExploding:
		// 2. 爆発演出タイマー
		if(--explosionTimer_ <= 0){
			auto* playerObj = player_->GetBaseObject();
			if(playerObj){
				// 元のモデルとスケールに戻す
				if(player_->GetOriginalModel()){
					playerObj->SetModel(player_->GetOriginalModel());
				}
				playerObj->SetScale({1.0f, 1.0f, 1.0f});
			}
			state_ = State::None;
			isActive_ = false;
			player_->SetInvincible(false);
		}
		break;
	}
}

void TeleportBomb::Draw(){
	if(state_ == State::ProjectileFlying && projectileObject_){
		projectileObject_->Draw();
	}
}

void TeleportBomb::Activate(){
	if (!player_ || !projectileObject_) { return; }

	switch(state_){
	case State::None:
		// 1段階目：発射
		state_ = State::ProjectileFlying;
		isActive_ = true;
		projectileLifetime_ = 30;

		projectilePos_ = player_->GetPosition();
		projectileVelocity_ = {projectileSpeed_, 0.0f, 0.0f};

		projectileObject_->GetTransform().translate = projectilePos_;
		projectileObject_->Update();
		break;

	case State::ProjectileFlying:
	{
		// 2段階目：ワープ & 爆発
		projectilePos_ = projectileObject_->GetTransform().translate;

		state_ = State::TeleportExploding;
		explosionTimer_ = 30;

		player_->SetPosition(projectilePos_);
		player_->SetVelocity({0.0f, 0.0f, 0.0f});
		player_->SetInvincible(true);

		auto* playerObj = player_->GetBaseObject();
		if(playerObj){
			// 爆発用モデルとスケールの設定
			if(player_->GetSpecialMoveModel()){
				playerObj->SetModel(player_->GetSpecialMoveModel());
			}
			playerObj->SetScale({explosionRadius_, explosionRadius_, explosionRadius_});
		}

		// 爆発ダメージ判定
		if(player_->GetEnemyManager()){
			const auto& enemies = player_->GetEnemyManager()->GetEnemies();

			// 判定用パラメータ
			const float kHitRadiusXZ = 2.0f;
			const float kHitHeightLimit = 3.0f;

			for(auto& enemy : enemies){
				if (!enemy) { continue; }

				Vector3 enemyPos = enemy->GetPosition();
				float dx = enemyPos.x - projectilePos_.x;
				float dz = enemyPos.z - projectilePos_.z;
				float distSqXZ = (dx * dx) + (dz * dz);
				float dy = fabsf(enemyPos.y - projectilePos_.y);

				// 円柱判定
				if(distSqXZ <= (kHitRadiusXZ * kHitRadiusXZ) && dy <= kHitHeightLimit){
					enemy->Damage(1000);
				}
			}
		}
		break;
	}
	default:
		break;
	}
}

void TeleportBomb::OnCollisionBlock(){
	// 飛んでいる状態なら消滅させる
	if(state_ == State::ProjectileFlying){
		state_ = State::None;
		isActive_ = false;
	}
}