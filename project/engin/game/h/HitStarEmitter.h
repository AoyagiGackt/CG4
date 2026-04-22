/**
 * @file HitStarEmitter.h
 * @brief 星型ヒットエフェクトを常時発生させるエミッター
 *
 * 使い方:
 *   // 事前にグループを1回だけ作成
 *   HitStarEmitter::CreateGroup();
 *
 *   // インスタンスを生成し、毎フレームUpdateを呼ぶ
 *   emitter_ = std::make_unique<HitStarEmitter>(position, color);
 *   emitter_->Update(); // 呼び続ける限り発生し続ける
 */
#pragma once
#include "MakeAffine.h"
#include <string>

/**
 * @brief 楕円パーティクルを8本ずつ一定間隔で放出し続ける星型エフェクト
 *
 * @note 各パーティクルの仕様
 *   - scaleX : 0.05f 固定
 *   - scaleY : 0.4〜1.5 の乱数
 *   - rotateZ: -π〜π の乱数
 */
class HitStarEmitter {
public:
    static const std::string kGroupName;

    /** @brief シーン初期化時に1回だけ呼ぶ */
    static void CreateGroup();

    /**
     * @param position エフェクトの発生座標
     * @param color    パーティクルの色（RGBA）
     */
    HitStarEmitter(const Vector3& position, const Vector4& color);

    /** @brief 毎フレーム呼び出す。一定間隔でEmitHitStarを呼び続ける */
    void Update();

    void SetPosition(const Vector3& position) { position_ = position; }
    void SetColor(const Vector4& color)       { color_    = color; }
    void SetFrequency(float freq)             { frequency_ = freq; }

private:
    Vector3 position_;
    Vector4 color_;

    float frequency_ = 0.05f; // 発生間隔（秒）
    float timeCount_ = 0.0f;
};
