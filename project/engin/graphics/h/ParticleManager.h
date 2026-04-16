/**
 * @file ParticleManager.h
 * @brief インスタンシングを利用して大量のパーティクルを効率的に描画・管理するファイル
 */
#pragma once
#include "Camera.h"
#include "DirectXCommon.h"
#include "MakeAffine.h"
#include "Model.h"
#include "SrvManager.h"
#include <d3d12.h>
#include <list>
#include <string>
#include <unordered_map>
#include <wrl/client.h>

 /**
  * @brief CPU側で計算・管理するパーティクル1粒のデータ
  */
struct Particle{
	Transform transform; /// パーティクルの座標・回転・スケール
	Vector3 velocity; /// パーティクルの移動方向とスピード
	Vector4 color; /// パーティクルの色（RGBA）
	float lifeTime; /// パーティクルが消滅するまでの寿命（秒）
	float currentTime; /// 発生してからの経過時間（秒）
};

/**
 * @brief 描画のためにGPUへ転送するパーティクル1粒のデータ
 */
struct ParticleForGPU{
	Matrix4x4 WVP; /// ワールド・ビュー・プロジェクション行列
	Matrix4x4 World; /// ワールド行列
	Vector4 color; /// パーティクルの色
};

/**
 * @brief 同じテクスチャを共有するパーティクルの集まり（グループ）
 */
struct ParticleGroup{

	std::string textureFilePath; /// このグループが使用するテクスチャのパス

	std::list<Particle> particles; /// 現在生存しているパーティクルのリスト

	uint32_t srvIndex = 0; /// インスタンシングデータ用SRVのインデックス

	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource; /// GPU上のインスタンシング用バッファリソース

	const uint32_t kNumMaxInstance = 1024; /// 1グループあたりの最大パーティクル発生数

	ParticleForGPU* instancingData = nullptr; /// GPUへデータを書き込むためのマップ済みポインタ
};

/**
 * @brief パーティクル全体を管理し、インスタンシング描画を行うシングルトンクラス
 * @note グループごとにテクスチャを分け、それぞれ最大1024個までのパーティクルを1回のドローコールで一括描画します。
 */
class ParticleManager{
public:

	/**
	 * @brief ParticleManagerの唯一のインスタンスを取得する
	 * @return ParticleManager* シングルトンインスタンスへのポインタ
	 */
	static ParticleManager* GetInstance();

	/**
	 * @brief マネージャーの初期化。ルートシグネチャとPSOを生成する
	 * @param dxCommon DirectX基盤のポインタ
	 */
	void Initialize(DirectXCommon* dxCommon);

	/** @brief D3D12リソースを解放する */
	void Finalize();

	/**
	 * @brief 全パーティクルの座標や寿命を更新し、ビルボード（カメラの方向を向く）計算を行う
	 * @param camera 描画に使用するカメラ（ビルボード行列の計算に必要）
	 */
	void Update(Camera* camera);

	/**
	 * @brief 登録されている全パーティクルグループをインスタンシング描画する
	 * @param camera 描画に使用するカメラ
	 */
	void Draw(Camera* camera);

	/**
	 * @brief 指定したグループに新しいパーティクルを発生させる
	 * @param name 発生させるパーティクルグループの名前
	 * @param position 発生場所の初期座標
	 * @param velocity パーティクルの初速（移動方向とスピード）
	 */
	void Emit(const std::string& name,const Vector3& position,const Vector3& velocity);

	/**
	 * @brief 色と寿命を指定してパーティクルを1粒発生させる
	 */
	void EmitWithColor(const std::string& name,const Vector3& position,
		const Vector3& velocity,const Vector4& color,float lifeTime = 1.0f,float scale = 1.0f);

	/**
	 * @brief 楕円形パーティクルを1粒発生させる（X/Yを個別にスケーリング）
	 * @param scaleX 横方向の倍率（大きいほど横長）
	 * @param scaleY 縦方向の倍率（大きいほど縦長）
	 */
	void EmitEllipse(const std::string& name, const Vector3& position,
		const Vector3& velocity, const Vector4& color,
		float lifeTime = 1.0f, float scaleX = 2.0f, float scaleY = 1.0f);

	/**
	 * @brief ヒット斬撃エフェクトを放出する（中心から線が放射状に飛び散る）
	 * @param name     パーティクルグループ名
	 * @param position ヒット座標
	 * @param angle    斬撃の主方向（ラジアン）。この角度を中心に±60°で散る
	 * @param color    エフェクトの色
	 * @param radius   線の長さ基準
	 */
	void EmitSlash(const std::string& name, const Vector3& position,
		float angle, const Vector4& color, float radius = 1.0f);

	/**
	 * @brief 既存のパーティクルグループのテクスチャを変更する
	 * @param groupName 変更したいパーティクルグループの名前
	 * @param textureFilePath 新しく適用するテクスチャのパス
	 */
	void SetTexture(const std::string& groupName,const std::string& textureFilePath);

	/**
	 * @brief 新しいパーティクルグループ（個別のテクスチャと最大1024個の枠）を作成する
	 * @param name 作成するグループの名前
	 * @param textureFilePath グループに適用するテクスチャのパス
	 * @note パーティクルを Emit() する前に、必ずこの関数でグループを作成しておくこと
	 */
	void CreateParticleGroup(const std::string& name,const std::string& textureFilePath);

	/**
	 * @brief パーティクルの基準となるモデル（形状）をセットする
	 * @param model 描画に使用するモデル（通常は平面のポリゴンなどを指定）
	 */
	void SetModel(Model* model){ model_ = model; }

	/** @brief 全パーティクルグループを破棄する（シーン終了時に呼ぶ） */
	void ClearAllGroups(){ particleGroups_.clear(); }

private:
	ParticleManager() = default;
	~ParticleManager() = default;
	ParticleManager(const ParticleManager&) = delete;
	ParticleManager& operator=(const ParticleManager&) = delete;

	// ルートシグネチャの作成
	void CreateRootSignature();
	// グラフィックスパイプラインの作成
	void CreatePipelineState();

private:
	DirectXCommon* dxCommon_ = nullptr;
	Model* model_ = nullptr;

	// パイプライン関連
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

	/** @brief 名前（文字列）をキーにしてパーティクルグループを管理する連想配列 */
	std::unordered_map<std::string,ParticleGroup> particleGroups_;
};