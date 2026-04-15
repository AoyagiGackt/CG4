#include "Player.h"
#include "ImGuiManager.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>
#include <Windows.h>

void Player::Initialize(ModelCommon* modelCommon, Model* model, Input* input, MapChipField* mapField,Camera* camera)
{
    originalModel_ = model;

    player_ = std::make_unique<Object3d>();
    player_->Initialize(modelCommon);
    player_->SetModel(model);
    player_->SetPosition({ 10.0f, 6.0f, 0.0f });
    player_->SetScale({ 1.0f, 1.0f, 1.0f });
    player_->SetEnableLighting(false);

    input_ = input;
    mapField_ = mapField;
    camera_ = camera;
}

void Player::Update()
{
    // 移動前の座標を保存 (衝突解決の基準点)
    prevPos_ = player_->GetTransform().translate;
    Input::Stick stick = input_->GetLeftStick();

    // タイマー類の更新
    if (dashCooldownTimer_ > 0) {
        dashCooldownTimer_--;
    }

    if (invincibleTimer_ > 0) {
        invincibleTimer_--;
    }

    // 無敵中は点滅で視覚的にフィードバック
    if (invincibleTimer_ > 0) {
        // 6フレームごとに通常色と赤みがかった半透明を交互に切り替える
        float alpha = (invincibleTimer_ % 6 < 3) ? 1.0f : 0.3f;
        player_->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
    } else {
        // 無敵終了時に通常色に戻す
        player_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    }

    if (justGuardTimer_ > 0) {
        justGuardTimer_--;
    }

    // SPACEでジャストガード発動
    if (input_->TriggerKey(DIK_SPACE)) {
        justGuardTimer_ = kJustGuardWindow;
    }

    // 速度 (velocity_) の決定
    HandleDash(stick); // ダッシュの開始判定

    if (isDashing_) {
        // ダッシュ中の速度計算
        float t = (float)dashTimer_ / (float)kDashDuration;
        float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t) * (1.0f - t);

        velocity_.x = dashVelocity_.x * eased;
        velocity_.y = dashVelocity_.y; // ダッシュ中は設定されたy速度を使用

        dashTimer_--;
        if (dashTimer_ <= 0) {
            isDashing_ = false;
        }
    } else {
        // 通常時の移動・重力計算
        HandleMovement(stick);
    }

    // 移動の適用と衝突解決 (この中で player_->SetPosition が呼ばれる)
    // velocity_ に基づいて移動し、ブロックがあれば prevPos_ を考慮して押し戻す
    ResolveCollisions();

    // スワイプ入力：ホイールの立ち上がりエッジ or パッドボタンのみ（連続発火防止）
    bool wheelNow = (input_->GetWheel() != 0);
    bool wheelEdge = wheelNow && !prevWheelScrolled_; // 0→非0 の瞬間だけ
    prevWheelScrolled_ = wheelNow;

    bool padSwipe = input_->TriggerButton(XINPUT_GAMEPAD_LEFT_SHOULDER);

    if (swipeCooldown_ > 0) {
        swipeCooldown_--;
    } else if (swipeStock_ > 0 && (wheelEdge || padSwipe)) {
        OnSwipeSuccess();
        swipeCooldown_ = 20; // 連続入力防止（約0.3秒）
    }

    // 最終的なAABBの更新 (描画や他オブジェクトとの判定用)
    Vector3 finalPos = player_->GetTransform().translate;
    collider_.aabb.min = { finalPos.x - kHalfWidth, finalPos.y - kHalfHeight, finalPos.z - 0.5f };
    collider_.aabb.max = { finalPos.x + kHalfWidth, finalPos.y + kHalfHeight, finalPos.z + 0.5f };

    // 特殊判定 (画面外に押し出された時のリカバリ)
    float cameraLeftX = cameraPosX_ - kScreenHalfWidth;
    float playerLeftX = finalPos.x - kHalfWidth;

    if (playerLeftX < cameraLeftX && invincibleTimer_ <= 0) {
        TakeDamage(playerHp_ / 2);

        Vector3 safePos = { cameraPosX_, 15.0f, 0.0f };
        player_->SetPosition(safePos);
        prevPos_ = safePos;
        velocity_ = { 0.0f, 0.0f, 0.0f };
    }

    // アニメーション行列をワールド座標と合成してObject3dに適用
    // animationMatrix_ はローカルの回転・スケール情報のみ持つ
    // ワールド座標（SetPositionで設定済み）と合成して正しい位置にアニメーションを反映する
    Vector3 worldPos = player_->GetTransform().translate;
    Matrix4x4 worldTransMat = MakeTranslateMatrix(worldPos);

    {
        // アニメーション行列と合成
        Matrix4x4 finalMatrix = Multiply(animationMatrix_,worldTransMat);
        player_->SetLocalMatrix(finalMatrix);
    }

    // 最終的な行列更新
    player_->Update();
}

void Player::HandleDash(const Input::Stick& stick)
{
    if ((input_->TriggerKey(DIK_Y) || input_->TriggerButton(XINPUT_GAMEPAD_Y)) && !isDashing_ && dashCooldownTimer_ <= 0) {

        isDashing_ = true;
        dashTimer_ = kDashDuration; // ※ここを10〜15くらいの短い値に設定すると「回避」っぽくなります
        dashCooldownTimer_ = kDashCoolTime;

        // 左に入力されていれば左へ、右なら右へサッと避ける
        if (stick.x < -0.3f || input_->PushKey(DIK_A)) {
            dashVelocity_.x = -kDashSpeed; // 左へ回避
        } else if (stick.x > 0.3f || input_->PushKey(DIK_D)) {
            dashVelocity_.x = kDashSpeed; // 右へ回避
        } else {
            // 何も入力されていない場合は、とりあえず右（あるいはキャラクターが向いている方向）にする
            dashVelocity_.x = kDashSpeed;
        }

        dashVelocity_.y = 0.0f; // 空中でも水平に移動する

        jumpCount_ = 1; // 回避後はジャンプ回数を消費
    }
}

void Player::HandleMovement(const Input::Stick& stick)
{
    float currentSpeed = baseSpeed_;

    // 左右移動
    velocity_.x = 0.0f;
    // キーボード、スティック、DPAD のいずれかで左移動
    if (input_->PushKey(DIK_A) || stick.x < -0.3f || input_->PushButton(XINPUT_GAMEPAD_DPAD_LEFT)) {
        velocity_.x = -currentSpeed;
    }
    // キーボード、スティック、DPAD のいずれかで右移動
    else if (input_->PushKey(DIK_D) || stick.x > 0.3f || input_->PushButton(XINPUT_GAMEPAD_DPAD_RIGHT)) {
        velocity_.x = currentSpeed;
    }

    // 重力の適用
    if (!onGround_) {
        velocity_.y += kGravity;
        velocity_.y = (std::max)(velocity_.y, kTerminalVelocity);
    } else {
        velocity_.y = 0.0f;
    }

    // 地面にいる間はタイマーをリセット、離れたらカウントダウン
    if (onGround_) {
        coyoteTimer_ = kCoyoteTime;
    } else if (coyoteTimer_ > 0) {
        coyoteTimer_--;
    }

    bool jumpInput = input_->TriggerKey(DIK_W) || input_->TriggerButton(XINPUT_GAMEPAD_A);

    // ジャンプできる条件：
    // 1. 地面にいる時（通常の1段目ジャンプ）
    // 2. コヨーテタイム中（プラットフォームの端から落ちた直後の猶予）
    bool canGroundJump = onGround_ || (coyoteTimer_ > 0 && jumpCount_ == 0);
    bool canJump = canGroundJump;

    if (canJump && jumpInput) {
        velocity_.y = kJumpForce;

        onGround_ = false;
        justJumped_ = true;
        coyoteTimer_ = 0; // コヨーテタイムを消費
        jumpCount_++; // ジャンプ回数を増やす（1段目で1、2段目で2になる）
    }
}

void Player::CheckBulletHit(std::list<std::unique_ptr<Bullet>>& bullets)
{
    for (auto& bullet : bullets) {
        if (bullet->IsDead()) {
            continue;
        }

        // 自分が撃った弾には当たらない
        if (bullet->GetOwner() == BulletOwner::Player) {
            continue;
        }

        if (Collision::CheckCollision(collider_.aabb, bullet->GetCollider().aabb)) {
            if (justGuardTimer_ > 0) {
                // ジャストガード成功：弾を跳ね返す
                bullet->Reflect();
            } else {
                bullet->OnCollision();
                TakeDamage(10.0f);
            }
        }
    }
}

void Player::ResolveCollisions()
{
    // 1. 判定用の座標を、移動前の位置で初期化
    Vector3 posX = prevPos_;
    Vector3 posY = prevPos_;
    bool hadHorizontalCollision = false;
    std::vector<Vector3> horizCollidedBlockPositions;

    // 毎フレーム判定前に接地をオフにする
    onGround_ = false;

    // --- X軸移動と横衝突処理 ---

    // 画面の右端の座標制限（速度をクランプする処理）
    const float kRightMargin = 1.0f;
    float cameraRightX = (cameraPosX_ + kScreenHalfWidth) - kRightMargin;
    float nextPlayerRightX = posX.x + velocity_.x + kHalfWidth;

    if (nextPlayerRightX > cameraRightX) {
        velocity_.x = cameraRightX - (posX.x + kHalfWidth);
    }

    // 二重移動を防止。現在の速度分だけをposXに適用します
    posX.x += velocity_.x;

    if (mapField_) {
        const auto& chips = mapField_->GetMapChips();
        for (const auto& chip : chips) {
            Vector3 blockPos = chip->GetTransform().translate;
            float blockLeft = blockPos.x - kBlockSize / 2.0f;
            float blockRight = blockPos.x + kBlockSize / 2.0f;
            float blockBottom = blockPos.y - kBlockSize / 2.0f;
            float blockTop = blockPos.y + kBlockSize / 2.0f;

            // Y軸にマージンを持たせて、床の段差に引っかからないようにする
            const float kMarginY = 0.02f;
            float playerTopY = prevPos_.y + kHalfHeight - kMarginY;
            float playerBottomY = prevPos_.y - kHalfHeight + kMarginY;
            bool overlapY = (playerTopY > blockBottom) && (playerBottomY < blockTop);

            float playerLeftX = posX.x - kHalfWidth;
            float playerRightX = posX.x + kHalfWidth;
            bool overlapX = (playerLeftX < blockRight) && (playerRightX > blockLeft);

            if (overlapY && overlapX) {
                // 移動方向で押し出す方向を確定
                if (velocity_.x > 0.0f) {
                    posX.x = blockLeft - kHalfWidth - kPushBack;
                } else if (velocity_.x < 0.0f) {
                    posX.x = blockRight + kHalfWidth + kPushBack;
                } else {
                    float overlapLeft = playerRightX - blockLeft;
                    float overlapRight = blockRight - playerLeftX;
                    if (overlapLeft < overlapRight) {
                        posX.x -= (overlapLeft + kPushBack);
                    } else {
                        posX.x += (overlapRight + kPushBack);
                    }
                }

                hadHorizontalCollision = true;
                horizCollidedBlockPositions.push_back(blockPos);
                velocity_.x = 0.0f;

                // ダッシュ中に壁に当たったらダッシュ状態を解除（ワープ防止）
                if (isDashing_) {
                    isDashing_ = false;
                    dashTimer_ = 0;
                }
            }
        }
    }

    // --- Y軸移動と上下衝突処理 ---
    // X軸の移動結果をposYに反映し、そこにYの速度を加える
    posY = posX;
    posY.y += velocity_.y;

    if (mapField_) {
        if (justJumped_) {
            justJumped_ = false;
        } else {
            const auto& chips = mapField_->GetMapChips();
            for (const auto& chip : chips) {
                Vector3 blockPos = chip->GetTransform().translate;
                float blockLeft = blockPos.x - kBlockSize / 2.0f;
                float blockRight = blockPos.x + kBlockSize / 2.0f;
                float blockBottom = blockPos.y - kBlockSize / 2.0f;
                float blockTop = blockPos.y + kBlockSize / 2.0f;

                // 壁にこすりながらジャンプした時の引っかかり防止
                const float kMarginX = 0.02f;
                float playerLeftX = posX.x - kHalfWidth + kMarginX;
                float playerRightX = posX.x + kHalfWidth - kMarginX;
                bool overlapX = (playerLeftX < blockRight) && (playerRightX > blockLeft);

                float playerTopY = posY.y + kHalfHeight;
                float playerBottomY = posY.y - kHalfHeight;
                bool overlapY = (playerTopY > blockBottom) && (playerBottomY < blockTop);

                bool thisBlockCausedHorizontal = false;
                if (hadHorizontalCollision) {
                    const float eps = 0.001f;
                    for (const auto& hPos : horizCollidedBlockPositions) {
                        if (std::fabs(blockPos.x - hPos.x) < eps && std::fabs(blockPos.y - hPos.y) < eps) {
                            thisBlockCausedHorizontal = true;
                            break;
                        }
                    }
                }

                if (overlapX && overlapY && !thisBlockCausedHorizontal) {
                    if (velocity_.y <= 0.0f) { // 落下して床に着地
                        posY.y = blockTop + kHalfHeight + kPushBack;
                        velocity_.y = 0.0f;
                        onGround_ = true;
                        jumpCount_ = 0;
                    } else if (velocity_.y > 0.0f) { // 上昇して天井にぶつかる
                        posY.y = blockBottom - kHalfHeight - kPushBack;
                        velocity_.y = 0.0f;
                    }
                } else if (overlapX && !thisBlockCausedHorizontal) {
                    float playerBottom = posY.y - kHalfHeight;
                    if (playerBottom >= (blockTop - 0.05f) && (playerBottom - blockTop) <= kSnapThreshold && velocity_.y <= 0.02f) {
                        posY.y = blockTop + kHalfHeight + kPushBack;
                        velocity_.y = 0.0f;
                        onGround_ = true;
                        jumpCount_ = 0;
                    }
                }
            }
        }
    }

    const float kScreenTopY = 13.0f;
    if (posY.y + kHalfHeight > kScreenTopY) {
        posY.y = kScreenTopY - kHalfHeight;
        if (velocity_.y > 0.0f) {
            velocity_.y = 0.0f;
        }
    }

    player_->SetPosition(posY);
}

void Player::OnCollision()
{
    // ダメージを受けた時などにつかう
    if (collider_.isHit) {
        if (velocity_.y < 0.0f) {
            onGround_ = true;
            velocity_.y = 0.0f;
            player_->SetPosition(prevPos_);
        } else {
            player_->SetPosition(prevPos_);
        }
        player_->Update();
    }
}

void Player::TakeDamage(float damageAmount)
{
    // 無敵中なら何もしない（これを入れておくと、どこから呼ばれても安全！）
     if(invincibleTimer_ > 0){
    	return;
    }

    // HPを減らす
    playerHp_ -= damageAmount;

    // ダメージを受けたら無敵時間にする（共通化）
    invincibleTimer_ = 120;
}

void Player::Draw()
{
    if (player_) {
        player_->Draw();
    }
}

void Player::DrawShadow()
{
    if (player_) {
        player_->DrawShadow();
    }
}

void Player::OnEnemyDefeated()
{
    swipeStock_++;
}

bool Player::CheckSwipeInput()
{
    // 1. マウスホイール (上下どちらでもOKな場合)
    if (input_->GetWheel() != 0) {
        return true;
    }
    // 2. パッドのLBまたはRB (XINPUT_GAMEPAD_LEFT_SHOULDER / RIGHT_SHOULDER)
    if (input_->TriggerButton(XINPUT_GAMEPAD_LEFT_SHOULDER) || input_->TriggerButton(XINPUT_GAMEPAD_RIGHT_SHOULDER)) {
        return true;
    }
    return false;
}

void Player::OnSwipeSuccess(){
    swipeFired_ = true;
    swipeStock_--;
    swipeSuccessCount_++;
    totalSwipeCount_++;

    // 3回スワイプで累計カウント
    if(swipeSuccessCount_ >= 3){
        swipeSuccessCount_ = 0;
    }
    if(totalSwipeCount_ >= 10){
        isBuffReady_ = true;  // 「準備できたよ！」と手を挙げる
        totalSwipeCount_ = 0;

    }
}