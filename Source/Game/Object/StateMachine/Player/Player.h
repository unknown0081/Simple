#pragma once

#include <variant>
#include "../StateMachine.h"
#include "../../../../../Library/Resource/Model/Animator.h"
#include "../../Source/Game/Object/Base/AnimatedObject.h"

#include "../../Library/3D/CameraBase.h"


enum class PlayerAnimNum
{
	Attack,
	Drink,
	Idle,
	Run,
	Walk,
};

class Player : public AnimatedObject
{
public:
	// ゲーム中変動する値
	struct AbilityStatus {
		float hp        = 0;
		float strength  = 15;
		float moveSpeed = 7;
		float attackCount = 0;
		float attackTimer = 0;

		//回避
		float dodgeTimer = 0;
		float notAcceptTimer = 0;
		float justDodgeSlowTimer = 0;
		bool isJustDodge = false;

		//体幹
		float bodyTrunkStrength = 1;
		float bodyTrunkStrengthRange = 1; // ランダムの範囲

		float hitDamage = 0;
		bool isHitDamage = false;

		Vector3 flyVec = { 0,0,0 };
		bool isFlying = false;
	};

	// 外部要因(ImGuiとか)でのみ変動する値
	struct ConstantStatus {
		float maxHp = 1000;

		float walkSpeed      = 5;
		float shiftDashTimer = 0.4f;
		float dashSpeed      = 2.4f;
		float dashDeadZone   = 0.7f;

		// 回避
		float dodgePower      = 24;
		float dodgeTime       = 1.5f;
		float dodgeLowestTime = 0.5f;
		float dodgeInvincibleTime = 0.5f;
		float justDodgeTime   = 0.1f;

		// 攻撃
		float maxAttackCombo = 4;
		float attackReceptionTime = 0.1f;
		float notAcceptTime = 0.16f;
		float leastStrength = 15;
		float maxStrength = 60;
		float incrementStrength = 2;

		float leastBt = 1;
		float maxBt = 10;
		float incrementBt = 2;
	};

private:

	Player(const char* filePath);

	~Player() override
	{
	}
public:
	static Player& Instance()
	{
		static Player instance("Data/Fbx/GaoPlayer/gaoplayer_6.model");
		return instance;
	}

	void Initialize();
	void Update() override;
	void Render(bool isShadow = false) override;
	void DrawDebugImGui(int number);

	void UpdateAnimationParam();

	// 入力データ取得
	void Input();
	void InputAttack();
	// 歩行移動量計算
	void CalcWalkVelocity();
	// 走り移動量計算
	void CalcRunVelocity();
	// 回避
	void CalcDodgeVelocity();
	// 攻撃
	void CalcAttackVelocity();
	void CalcJustDodge();

	// ダメージ
	void HitDamaged(float damage, bool flying = false, Vector3 vec = {0,0,0});

	// RootAnimation
	void CalcRootAnimationVelocity();

	// 移動している方向に向く
	void Turn();
	// 移動
	void Move();

	// 敵との衝突処理
	void CollisionVsEnemy();

	void SetCamera(CameraBase* camera) { this->camera = camera; }

	void DrawDebug();

	void OnHitAttack(bool hitWeak);

	void UpdateHitStopTimer();

public:
	enum class State
	{
		Normal,
	};

	enum class Normal
	{
		Idle,
		Walk,
		Run,
		Attack,
		Dodge,
		Damage,
		Drink,
	};

private:
	std::unique_ptr<StateMachine<Player>> stateMachine;

	DirectX::XMFLOAT3 velocity  = {0, 0, 0};
	//float             moveSpeed = 3.0f; // プレイヤーが１秒間に加速する加速度
	float             rot       = 720;  // プレイヤーが１秒間に回転する角度

	// 入力データ保管用
	using inputData = std::variant<bool, int, float, DirectX::XMFLOAT2>;
	std::map<std::string, inputData> inputMap;

#ifdef _DEBUG
	std::map<std::string, inputData> debug;
#endif

	// --- カメラのポインタ ---
	CameraBase* camera;

	//Animator animator;

	// ステータス
	AbilityStatus ability;
	ConstantStatus constant;

public:
	StateMachine<Player>* GetStateMachine() { return stateMachine.get(); }

	DirectX::XMFLOAT3 GetVelocity() { return velocity; }
	void              SetVelocity(DirectX::XMFLOAT3 velocity) { this->velocity = velocity; }


	template <typename T>
	const T& GetInputMap(const std::string& str)
	{
		return std::get<T>(inputMap[str]);
	}

	void SetInputMap(const std::string& str, const inputData& data) { inputMap[str] = data; }

	AbilityStatus& AStatus() { return ability; }
	ConstantStatus& CStatus() { return constant; }
	//Animator& GetAnimator() { return animator; }

	void ClampPosition(const float range);
	void Respawn();
	
	bool enterStage;
	bool lockOn;
	bool enterEntrance;

	Vector3 restRoomCenter = { 0.0f, 0.0f, 132.0f };
	float restRoomRadius = 28.0f;
	Vector3 size = { 10.0f, 10.0f, 10.0f };
	bool isHit;
	Vector3 entranceCubePosition = { 0.0f, 0.0f, 92.0f };
	Vector3 entranceCubeSize = { 7.0f, 1.0f, 31.0f };
	struct EntranceCube
	{
		Vector3 position;
		Vector3 size;
	}entrance = { {0.0f, 0.0f, 95.0f}, {7.0f, 1.0f, 31.0f} },
		entranceLWall = { {9.0f, 0.0f, 94.25f}, {11.0f, 12.0f, 26.0f} },
		entranceRWall = { {-9.0f, 0.0f, 94.25f}, {11.0f, 12.0f, 26.0f} };

	std::vector<Vector3> wallSpheres;
	float wallSphereRadius = 1.0f;
	float playerRadius = 1.0f;
	float hitStopTimer;
	float hitStopTime = 0.05f;
	float weakHitStopTime = 0.1f;
};