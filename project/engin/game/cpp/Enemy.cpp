#include "Enemy.h"
#include <numbers>
#include "Input.h"
#include "Model.h"
#include "Camera.h"
#include "MapChipField.h"


	
void Enemy::Initialize(ModelCommon* modelCommon, Model* model, Input* input, Model* bulletModel, MapChipField* mapField) {

	input_ = input;
	modelCommon_ = modelCommon;
	bulletModel_ = bulletModel;
	mapField_ = mapField;

	enemy_ = std::make_unique<Object3d>();

	enemy_->Initialize(modelCommon);
	enemy_->SetModel(model);
	enemy_->SetScale({ 0.3f,0.3f,0.3f });
	enemy_->SetEnableLighting(false);

	position_ = { 10.0f,0.0f,0.0f };
	enemy_->SetPosition(position_);

	lrDirection_ = LRDirection::kLeft;
	UpdateFacingDirection();

	std::random_device seedGenerator;
	randomEngine_.seed(seedGenerator());

	moveDurationRand_ = std::uniform_int_distribution<int>(30, 90); // 横移動フレーム数
	idleWaitRand_ = std::uniform_int_distribution<int>(60, 180);    // Idle待機フレーム
	idleWait_ = idleWaitRand_(randomEngine_);

	// 固定値（Normal相当）
	moveSpeedMultiplier_ = 1.0f;
	hp_ = 190;
	shootInterval_ = 60;
	jumpProbability_ = 0.01f;
}

void Enemy::SetPosition(const Vector3& position)
{
	position_ = position;
	if (enemy_) {
		enemy_->SetPosition(position_);
	}
}

void Enemy::Update(Camera *camera, float scrollSpeed) {

	// 地面のスクロールに合わせて左へ移動
	position_.x -= scrollSpeed;

	prevPos_ = position_;
	switch (state_) {

		case State::Idle:

		UpdateIdle();

		break;

		case State::Jump:

		UpdateJump();

		break;
	}

	velocityY_ -= 0.015f;
	position_.y += velocityY_;

	ResolveCollisions();
	Attack();
	UpdateBullets(camera);
	enemy_->SetPosition(position_);
	enemy_->Update();
}

void Enemy::UpdateIdle() {

	// 待機中はフレームをカウントダウン
	if (idleWait_ > 0) {
		idleWait_--;
		return;
	}

	// 待機終了後に横移動開始
	HorizontalMoveUpdate();

	// ジャンプ判定（移動中はジャンプしない）
	if (onGround_ && !isMoving_ && jumpChance_(randomEngine_) < jumpProbability_) {
		velocityY_ = 0.35f;
		onGround_ = false;
		state_ = State::Jump;
	}
}

void Enemy::UpdateJump()
{

	// 地面に着地したらIdleへ
	if (onGround_) {
		state_ = State::Idle;
	}
}

void Enemy::HorizontalMoveUpdate()
{
	// 横移動開始時（Idle 待機終了後のみ）
	if (!isMoving_ && turnChance_(randomEngine_) < 0.01f) {
		isMoving_ = true;
		moveDirection_ = (turnChance_(randomEngine_) < 0.5f);

		// LRDirection を更新して振り向き
		lrDirection_ = moveDirection_ ? LRDirection::kRight : LRDirection::kLeft;
		UpdateFacingDirection();

		moveDuration_ = moveDurationRand_(randomEngine_);
		moveSpeed_ = moveSpeedRand_(randomEngine_);
	}

	// 横移動中
	if (isMoving_) {
		position_.x += (moveDirection_ ? 1.0f : -1.0f) * moveSpeed_ * moveSpeedMultiplier_;
		moveDuration_--;

		if (moveDuration_ <= 0) {
			isMoving_ = false;
			idleWait_ = idleWaitRand_(randomEngine_);
		}
	}
}

void Enemy::UpdateFacingDirection()
{
	if (lrDirection_ == LRDirection::kRight) {
		// 右向きに回転（Y軸回転）
		enemy_->SetRotation({ 0.0f, std::numbers::pi_v<float> / 2.0f, 0.0f });
	} else {
		// 左向きに回転（右向きの反対）
		enemy_->SetRotation({ 0.0f, -std::numbers::pi_v<float> / 2.0f, 0.0f });
	}
}

void Enemy::Draw() {
	if (enemy_) {
		enemy_->Draw();
	}
	// 弾は DrawBullets() で描画（skyOverlay の上に重ねるため）
}

void Enemy::DrawBullets() {
	for (auto& bullet : bullets_) {
		bullet->Draw();
	}
}

void Enemy::DrawShadow() {
	if (enemy_) {
		enemy_->DrawShadow();
	}

	for (auto& bullet : bullets_) {
		bullet->DrawShadow();
	}
}


void Enemy::FireBullet()
{
	Vector3 bulletPosition = position_;

	Vector3 dir;

	if (lrDirection_ == LRDirection::kRight) {
		dir = { 1.0f,0.0f,0.0f };
	} else {
		dir = { -1.0f,0.0f,0.0f };
	}

	float speed = 0.5f * moveSpeedMultiplier_;

	Vector3 velocity;
	velocity.x = dir.x * speed;
	velocity.y = dir.y * speed;
	velocity.z = dir.z * speed;

	bullets_.push_back(
		Bullet::Create(modelCommon_, bulletModel_, bulletPosition, velocity)
	);
	bullets_.back()->SetOwner(BulletOwner::Enemy);
}

void Enemy::Attack()
{
	if (shootInterval_ < 0) { return; } // 攻撃しない

	// 弾のランダム射撃（確率で発射）
	if (shotCooldown_ <= 0) {

		float shootChance = 0.03f;

		if (shotDist_(randomEngine_) <= shootChance)
		{
			FireBullet();
			shotCooldown_ = shootInterval_;
		}
	} else {
		shotCooldown_--;
	}
}

void Enemy::UpdateBullets(Camera* camera)
{
	float cameraX = camera->GetTransform().translate.x;

	// 弾の更新
	for (auto& b : bullets_) {

		b->Update();

	}


	//削除処理
	bullets_.erase(
		std::remove_if(
			bullets_.begin(),
			bullets_.end(),
			[cameraX](const std::unique_ptr<Bullet>& b)
			{
				float x = b->GetPosition().x;
				return (x < cameraX - 30 || x > cameraX + 30);
			}),
		bullets_.end());

}

void Enemy::ResolveCollisions()
{
	Vector3 posX = prevPos_;
	Vector3 posY = prevPos_;

	onGround_ = false;

	// ---- X移動 ----
	posX.x = position_.x;

	if (mapField_) {

		const auto& chips = mapField_->GetMapChips();

		for (const auto& chip : chips) {

			Vector3 blockPos = chip->GetTransform().translate;

			float blockLeft = blockPos.x - kBlockSize * 0.5f;
			float blockRight = blockPos.x + kBlockSize * 0.5f;
			float blockBottom = blockPos.y - kBlockSize * 0.5f;
			float blockTop = blockPos.y + kBlockSize * 0.5f;

			float enemyLeft = posX.x - kHalfWidth;
			float enemyRight = posX.x + kHalfWidth;

			float enemyTop = prevPos_.y + kHalfHeight;
			float enemyBottom = prevPos_.y - kHalfHeight;

			bool overlapX =
				enemyLeft < blockRight &&
				enemyRight > blockLeft;

			bool overlapY =
				enemyTop > blockBottom &&
				enemyBottom < blockTop;

			if (overlapX && overlapY) {

				if (position_.x > prevPos_.x) {
					posX.x = blockLeft - kHalfWidth - kPushBack;
				} else if (position_.x < prevPos_.x) {
					posX.x = blockRight + kHalfWidth + kPushBack;
				}
			}
		}
	}

	// ---- Y移動 ----
	posY = posX;
	posY.y = position_.y;

	if (mapField_) {

		const auto& chips = mapField_->GetMapChips();

		for (const auto& chip : chips) {

			Vector3 blockPos = chip->GetTransform().translate;

			float blockLeft = blockPos.x - kBlockSize * 0.5f;
			float blockRight = blockPos.x + kBlockSize * 0.5f;
			float blockBottom = blockPos.y - kBlockSize * 0.5f;
			float blockTop = blockPos.y + kBlockSize * 0.5f;

			float enemyLeft = posX.x - kHalfWidth;
			float enemyRight = posX.x + kHalfWidth;

			bool overlapX =
				enemyLeft < blockRight &&
				enemyRight > blockLeft;

			float enemyTop = posY.y + kHalfHeight;
			float enemyBottom = posY.y - kHalfHeight;

			bool overlapY =
				enemyTop > blockBottom &&
				enemyBottom < blockTop;

			if (overlapX && overlapY) {

				if (velocityY_ <= 0.0f) {

					posY.y = blockTop + kHalfHeight + kPushBack;
					velocityY_ = 0.0f;
					onGround_ = true;

				} else {

					posY.y = blockBottom - kHalfHeight - kPushBack;
					velocityY_ = 0.0f;
				}
			}
		}
	}

	position_ = posY;
}


void Enemy::Damage(int damage)
{
	hp_ -= damage;

	if (hp_ < 0) {
		hp_ = 0;
	}
}

AABB Enemy::GetAABB() const
{

	AABB aabb;

	aabb.min = {
		position_.x - kHalfWidth,
		position_.y - kHalfHeight,
		position_.z - 0.5f
	};

	aabb.max = {
		position_.x + kHalfWidth,
		position_.y + kHalfHeight,
		position_.z + 0.5f
	};

	return aabb;

}
