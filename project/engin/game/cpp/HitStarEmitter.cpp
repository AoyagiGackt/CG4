#include "HitStarEmitter.h"
#include "ParticleManager.h"

const std::string HitStarEmitter::kGroupName = "hitStar";

void HitStarEmitter::CreateGroup()
{
    ParticleManager::GetInstance()->CreateParticleGroup(kGroupName, "Resources/hitStarEffect.png");
}

HitStarEmitter::HitStarEmitter(const Vector3& position, const Vector4& color)
    : position_(position)
    , color_(color)
    , frequency_(0.05f)
    , timeCount_(frequency_) // 最初のUpdateで即座に発生させる
{
}

void HitStarEmitter::Update()
{
    timeCount_ += 1.0f / 60.0f;

    if (timeCount_ >= frequency_) {
        ParticleManager::GetInstance()->EmitHitStar(kGroupName, position_, color_);
        timeCount_ -= frequency_;
    }
}
