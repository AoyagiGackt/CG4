#include "HitEffect.h"
#include <cmath>
#include <numbers>

void HitEffect::Trigger(const Vector3& position, Type type)
{
    auto* pm = ParticleManager::GetInstance();
    const float kPi = std::numbers::pi_v<float>;

    switch (type) {

    case Type::Slash: {
        // 5方向にスラッシュ
        pm->EmitSlash("hit_slash", position,  0.0f,        { 1.0f, 1.0f,  1.0f, 1.0f }, 6.0f);
        pm->EmitSlash("hit_slash", position,  kPi * 0.25f, { 0.5f, 0.9f,  1.0f, 1.0f }, 5.0f);
        pm->EmitSlash("hit_slash", position, -kPi * 0.25f, { 0.5f, 0.9f,  1.0f, 1.0f }, 5.0f);
        pm->EmitSlash("hit_slash", position,  kPi * 0.5f,  { 0.3f, 0.7f,  1.0f, 1.0f }, 4.0f);
        pm->EmitSlash("hit_slash", position, -kPi * 0.5f,  { 0.3f, 0.7f,  1.0f, 1.0f }, 4.0f);

        pm->EmitEllipse("hit_slash", position, { 0.0f,  0.02f, 0.0f }, { 0.6f, 1.0f, 1.0f, 1.0f }, 0.6f, 5.0f, 0.8f);
        pm->EmitEllipse("hit_slash", position, { 0.0f, -0.02f, 0.0f }, { 0.4f, 0.8f, 1.0f, 1.0f }, 0.5f, 4.0f, 0.6f);
        pm->EmitEllipse("hit_slash", position, { 0.0f,  0.0f,  0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, 0.4f, 6.0f, 0.5f);
        break;
    }

    case Type::Impact: {
        // 12方向に XZ 平面展開
        for (int i = 0; i < 12; ++i) {
            float angle = i * (kPi * 2.0f / 12.0f);
            float r = 0.10f + (i % 3) * 0.03f;
            Vector3 vel = { std::cos(angle) * r, 0.0f, std::sin(angle) * r };
            Vector4 col = (i % 2 == 0)
                ? Vector4{ 1.0f, 1.0f, 0.3f, 1.0f }
                : Vector4{ 1.0f, 0.8f, 0.0f, 1.0f };
            pm->EmitEllipse("hit_impact", position, vel, col, 1.0f, 3.0f, 0.8f);
        }

        // 上方向にも飛ばす
        for (int i = 0; i < 8; ++i) {
            float angle = i * (kPi * 2.0f / 8.0f);
            float r = 0.08f;
            Vector3 vel = { std::cos(angle) * r, 0.06f, std::sin(angle) * r };
            pm->EmitEllipse("hit_impact", position, vel,
                { 1.0f, 0.9f, 0.2f, 1.0f }, 0.8f, 2.5f, 0.7f);
        }

        pm->EmitWithColor("hit_impact", position, { 0.0f, 0.0f, 0.0f },
            { 1.0f, 1.0f, 0.5f, 1.0f }, 0.4f, 6.0f);
        pm->EmitHitStar("hit_impact", position, { 1.0f, 1.0f, 0.3f, 1.0f });
        pm->EmitHitStar("hit_impact", position, { 1.0f, 0.8f, 0.0f, 1.0f });
        break;
    }

    case Type::Explosion: {
        // 24方向に赤/橙パーティクル
        for (int i = 0; i < 24; ++i) {
            float angle = i * (kPi * 2.0f / 24.0f);
            float speed = 0.12f + (i % 4) * 0.04f;
            Vector3 vel = { std::cos(angle) * speed, std::sin(angle) * speed, 0.0f };
            Vector4 col = (i % 3 == 0)
                ? Vector4{ 1.0f, 0.0f, 0.0f, 1.0f }
                : (i % 3 == 1)
                ? Vector4{ 1.0f, 0.5f, 0.0f, 1.0f }
                : Vector4{ 1.0f, 0.9f, 0.1f, 1.0f };
            pm->EmitWithColor("hit_explosion", position, vel, col, 1.0f, 2.5f);
        }

        // 上方向
        for (int i = 0; i < 12; ++i) {
            float angle = i * (kPi * 2.0f / 12.0f);
            float speed = 0.08f + (i % 3) * 0.03f;
            Vector3 vel = { std::cos(angle) * speed, 0.10f + (i % 2) * 0.05f, std::sin(angle) * speed };
            pm->EmitWithColor("hit_explosion", position, vel,
                { 1.0f, 0.3f, 0.0f, 1.0f }, 0.9f, 2.0f);
        }

        pm->EmitWithColor("hit_explosion", position, { 0.0f, 0.0f, 0.0f },
            { 1.0f, 0.8f, 0.2f, 1.0f }, 0.4f, 7.0f);
        pm->EmitHitStar("hit_explosion", position, { 1.0f, 0.5f, 0.0f, 1.0f });
        pm->EmitHitStar("hit_explosion", position, { 1.0f, 0.2f, 0.0f, 1.0f });
        pm->EmitHitStar("hit_explosion", position, { 1.0f, 0.9f, 0.0f, 1.0f });
        break;
    }

    }
}
