#define NOMINMAX
#include "SpinBlade.h"
#include "Player.h"
#include "EnemyManager.h"

#include <cmath>
#include <algorithm>

void SpinBlade::InitBlade(Player* player, Model* bladeModel, ModelCommon* modelCommon)
{
    player_ = player;

    for (int i = 0; i < kBladeCount; i++)
    {
        blades_[i] = std::make_unique<Object3d>();
        blades_[i]->Initialize(modelCommon);
        blades_[i]->SetModel(bladeModel);
        blades_[i]->SetPosition(player_->GetPosition());
    }

    state_ = State::None;
    isActive_ = false;
}

void SpinBlade::Initialize(Player*) {}

void SpinBlade::Update()
{
    if (state_ == State::None || !player_) { return; }

    Vector3 playerPos = player_->GetPosition();

    switch (state_)
    {
        case State::Charge:
        UpdateCharge(playerPos);
        break;

        case State::Active:
        UpdateActive(playerPos);
        break;

        case State::Finish:
        UpdateFinish(playerPos);
        break;
    }
}

void SpinBlade::UpdateCharge(const Vector3& playerPos)
{
    if (chargeTimer_ > 0) { chargeTimer_--; }

    rotationAngle_ += kChargeRotateSpeed;

    for (int i = 0; i < kBladeCount; i++)
    {
        float angle = rotationAngle_ + i * kAngleStep;
        Vector3 pos = CalcOrbitPosition(playerPos, angle, orbitRadius_);

        UpdateBlade(i, pos);
    }

    if (chargeTimer_ <= 0)
    {
        state_ = State::Active;
        activeTimer_ = kMaxActiveTime;
    }
}

void SpinBlade::UpdateActive(const Vector3& playerPos)
{
    EnemyManager* enemyManager = player_->GetEnemyManager();
    if (!enemyManager) { return; }

    const auto& enemies = enemyManager->GetEnemies();

    if (--activeTimer_ <= 0)
    {
        state_ = State::Finish;
        finishTimer_ = kFinishTime;

        for (auto& enemy : enemies)
        {
            if (!enemy) { continue; }

            if (IsHit(enemy->GetPosition(), playerPos, finishRadius_))
            {
                enemy->Damage(40);
            }
        }
        return;
    }

    rotationSpeed_ += (maxRotationSpeed_ - rotationSpeed_) * 0.05f;

    rotationAngle_ += rotationSpeed_;
    orbitRadius_ = std::min(orbitRadius_ + kOrbitExpandSpeed, kMaxOrbitRadius);

    for (int i = 0; i < kBladeCount; i++)
    {
        float angle = rotationAngle_ + i * kAngleStep;

        Vector3 pos = CalcOrbitPosition(playerPos, angle, orbitRadius_);
        pos.y += std::sin(angle * kBladeWaveSpeed) * kBladeWaveHeight;

        Vector3 rot { 0.0f, angle * 3.0f, rotationAngle_ * 5.0f };

        blades_[i]->SetRotation(rot);
        UpdateBlade(i, pos);

        damageTimer_[i]++;

        if (damageTimer_[i] >= 10)
        {
            for (auto& enemy : enemies)
            {
                if (!enemy) { continue; }

                if (IsHit(enemy->GetPosition(), pos, attackRadius_))
                {
                    enemy->Damage(5);
                }
            }

            damageTimer_[i] = 0;
        }
    }
}

void SpinBlade::UpdateFinish(const Vector3& playerPos)
{
    finishTimer_--;

    orbitRadius_ += kFinishExpandSpeed;
    rotationAngle_ += 0.5f;

    for (int i = 0; i < kBladeCount; i++)
    {
        float angle = rotationAngle_ + i * kAngleStep;
        Vector3 pos = CalcOrbitPosition(playerPos, angle, orbitRadius_);

        UpdateBlade(i, pos);
    }

    if (finishTimer_ <= 0)
    {
        state_ = State::None;
        isActive_ = false;
    }
}

void SpinBlade::UpdateBlade(int i, const Vector3& pos)
{
    if (!blades_[i]) { return; }

    blades_[i]->GetTransform().translate = pos;
    blades_[i]->Update();
}

Vector3 SpinBlade::CalcOrbitPosition(const Vector3& center, float angle, float radius) const
{
    Vector3 pos;

    pos.x = center.x + std::cos(angle) * radius;
    pos.z = center.z + std::sin(angle) * radius;
    pos.y = center.y;

    return pos;
}

bool SpinBlade::IsHit(const Vector3& a, const Vector3& b, float radius) const
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;

    return (dx * dx + dy * dy + dz * dz) <= radius * radius;
}

void SpinBlade::Draw()
{
    if (state_ == State::None) { return; }

    for (auto& blade : blades_)
    {
        if (blade)
        {
            blade->Draw();
        }
    }
}

void SpinBlade::Activate()
{
    if (!player_ || state_ != State::None) { return; }

    state_ = State::Charge;
    isActive_ = true;

    chargeTimer_ = kChargeTime;
    activeTimer_ = kMaxActiveTime;

    rotationAngle_ = 0.0f;
    orbitRadius_ = 2.0f;
    rotationSpeed_ = 0.1f;

    for (int i = 0; i < kBladeCount; i++)
    {
        damageTimer_[i] = 0;

        float angle = i * kAngleStep;
        Vector3 pos = CalcOrbitPosition(player_->GetPosition(), angle, orbitRadius_);

        blades_[i]->SetPosition(pos);
    }
}