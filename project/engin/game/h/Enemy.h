#pragma once
#include "Object3d.h"
#include <memory>
#include <random>
#include "Bullet.h"

class Camera;
class Model;
class Input;
class MapChipField;

class Enemy {
public:
    /**
     * @brief 天球オブジェクトの生成と設定
     * @param modelCommon 描画設定
     * @param model 天球用のモデルデータ
     */
    void Initialize(ModelCommon *modelCommon, Model *model, Input *input, Model* bulletModel, MapChipField* mapField);

    //============================
    // 更新
    //============================
    void Update(Camera *camera, float scrollSpeed);

    //============================
    // 描画
    //============================
    void Draw();

    /** @brief 弾のみ描画（skyOverlay の後に呼ぶ） */
    void DrawBullets();

    /** @brief シャドウパス用描画（敵本体 + 敵弾） */
    void DrawShadow();

    //============================
    // 外部操作
    //============================
    void SetPosition(const Vector3 &position);

    bool IsCounted() const{ return counted_; }
    void SetCounted(bool counted){ counted_ = counted; }

    //============================
    // Getter（デバッグ用）
    //============================
    int GetHP() const { return hp_; }
    float GetMoveSpeedMultiplier() const { return moveSpeedMultiplier_; }
    int GetShootInterval() const { return shootInterval_; }

    const char *GetStateName() const
    {
        switch (state_)
        {
            case State::Idle: return "Idle";
            case State::Jump: return "Jump";
        }
        return "Unknown";
    }

    Vector3 GetPosition() const { return position_; }

    void Damage(int damage);
    bool IsDead() const { return hp_ <= 0; }

    float GetHalfWidth() { return kHalfWidth;}
    float GetHalfHeight() { return kHalfHeight; }
   // 弾リストを外部から参照するための関数
    std::list<std::unique_ptr<Bullet>>& GetBullets(){ return bullets_; }

    AABB GetAABB() const;

private:

    //============================
    // オブジェクト
    //============================
    std::unique_ptr<Object3d> enemy_;

    Input *input_ = nullptr; // 入力管理のポインタ（必要に応じて使用）
    MapChipField* mapField_ = nullptr; // マップチップフィールドへのポインタ

    //============================
    // 状態
    //============================
    enum class State
    {
        Idle,
        Jump
    };

    State state_ = State::Idle;

    //============================
    // 向き
    //============================
    // 左右
    enum class LRDirection
    {
        kRight,
        kLeft,
    };

    LRDirection lrDirection_ = LRDirection::kRight;

    float moveSpeedMultiplier_ = 1.0f;
    int hp_ = 1;
    int shootInterval_ = 60;
    float jumpProbability_ = 0.01f;

    //============================
    // 位置
    //============================
    Vector3 position_ { 10.0f,3.0f,0.0f };

    //============================
    // ジャンプ
    //============================
    
    float velocityY_ = 0.0f;

    //============================
    // 横移動
    //============================
    bool moveDirection_ = true;
    bool isMoving_ = false;

    int moveDuration_ = 0;
    int idleWait_ = 0;

    float moveSpeed_ = 0.05f;

    //============================
    // 乱数
    //============================
    std::mt19937 randomEngine_;

    std::uniform_real_distribution<float> jumpChance_ { 0.0f,1.0f };
    std::uniform_real_distribution<float> shotDist_{ 0.0f, 1.0f };

    //============================
   // 弾
   //============================

    Model* bulletModel_ = nullptr;
    //クールタイム
    int shotCooldown_ = 0;
    std::list<std::unique_ptr<Bullet>> bullets_;

    //弾を生成して発射
    void FireBullet();

    void Attack();

    void UpdateBullets(Camera* camera);

    ModelCommon* modelCommon_ = nullptr;
    std::uniform_real_distribution<float> turnChance_ { 0.0f, 1.0f };

    std::uniform_real_distribution<float> moveSpeedRand_ { 0.04f,0.06f };
    std::uniform_int_distribution<int> moveDurationRand_;
    std::uniform_int_distribution<int> idleWaitRand_;

    bool counted_ = false; // 既にチュートリアルの撃破数に数えられたか

    //============================
    // 内部処理
    //============================
    void UpdateIdle();
    void UpdateJump();
    void HorizontalMoveUpdate();
    void UpdateFacingDirection();

    //当たり判定
    void ResolveCollisions();

    Vector3 prevPos_;
    bool onGround_ = false;

    static constexpr float kBlockSize = 2.0f;
    static constexpr float kHalfWidth = 0.4f;
    static constexpr float kHalfHeight = 0.9f;
    static constexpr float kPushBack = 0.001f;

 
};