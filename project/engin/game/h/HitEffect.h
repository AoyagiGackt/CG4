#pragma once
#include "ParticleManager.h"

/**
 * @brief ヒットエフェクトを発火するクラス
 *
 * Trigger() を呼ぶと ParticleManager にパーティクルを一括登録する。
 * パーティクル寿命が切れれば自動消滅するため、Update は不要。
 */
class HitEffect {
public:
    enum class Type {
        Slash,      // 斬撃: 5方向スラッシュ + 楕円3枚
        Impact,     // 衝撃波: 12+8方向楕円展開
        Explosion,  // 爆発: 24+12方向パーティクル + フラッシュ
    };

    void Trigger(const Vector3& position, Type type);
};
