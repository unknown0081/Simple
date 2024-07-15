#include "UiPause.h"
#include "../../Library/Timer.h"
#include "../../Library/Input/InputManager.h"


void UiPause::Initialize()
{
	state = UiPauseState::Hidden;

	SetAllOffRender();
	imgBlack->SetColorA(0.0f);
}

bool UiPause::Update()
{
	switch (state)
	{
	case UiPauseState::Hidden:
		{
			// ポーズフラグがtrueになったら遷移
			if (InputManager::Instance().GetKeyPressed(DirectX::Keyboard::Back))
			{
				imgBlack->SetIsRender(true);

				isPause = true;
				state   = UiPauseState::Showing;
			}
		}
		break;
	case UiPauseState::Showing:
		{
			pauseTimer += Timer::Instance().DeltaTime();

			imgBlack->SetColorA(Easing::GetNowParam(Easing::OutQuad<float>, pauseTimer, imgBlackAlpha));

			// 表示中にポーズ解除されたら非表示遷移状態に遷移
			if (InputManager::Instance().GetKeyPressed(DirectX::Keyboard::Back))
			{
				isPause = false;
				state   = UiPauseState::Hide;
				break;
			}


			// 表示完了したら遷移
			if (pauseTimer > pauseTotalTime)
			{
				pauseTimer = pauseTotalTime;
				state      = UiPauseState::Visible;
			}
		}
		break;
	case UiPauseState::Visible:
		{
			// ポーズフラグがfalseになったら遷移
			if (InputManager::Instance().GetKeyPressed(DirectX::Keyboard::Back))
			{
				isPause = false;
				state   = UiPauseState::Hide;
			}
		}
		break;
	case UiPauseState::Hide:
		{
			pauseTimer -= Timer::Instance().DeltaTime();

			imgBlack->SetColorA(Easing::GetNowParam(Easing::OutQuad<float>, pauseTimer, imgBlackAlpha));

			// 表示中にポーズ解除されたら非表示遷移状態に遷移
			if (InputManager::Instance().GetKeyPressed(DirectX::Keyboard::Back))
			{
				isPause = true;
				state   = UiPauseState::Showing;
				break;
			}

			// 非表示完了したら遷移
			if (pauseTimer < 0.0)
			{
				imgBlack->SetIsRender(false);

				pauseTimer = 0.0f;
				state      = UiPauseState::Hidden;
				break;
			}
		}
		break;
	}

	return isPause;
}

// 描画
void UiPause::Render()
{
	//if (!isPause) return;

	imgBlack->Render();
}

void UiPause::SetAllOffRender()
{
	for (auto* sprite : sprites)
	{
		sprite->SetIsRender(false);
	}
}
