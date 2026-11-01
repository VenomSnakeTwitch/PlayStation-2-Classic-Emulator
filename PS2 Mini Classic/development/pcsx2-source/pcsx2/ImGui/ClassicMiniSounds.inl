// SPDX-License-Identifier: GPL-3.0+
#include "Config.h"
#include "common/HostSys.h"
#include <cstring>

static int s_classic_menu_sound = 0;
static bool s_classic_menu_active = false;

static void ClassicQueueMenuSound(const char* label)
{
	const bool back = std::strstr(label, "Back") || std::strstr(label, "Zurueck") ||
	                  std::strstr(label, "Zurück") || std::strstr(label, "Schließen") ||
	                  std::strstr(label, "Abbrechen") || std::strstr(label, "Cancel") ||
	                  std::strstr(label, "Close") || std::strstr(label, ICON_PF_BACKWARD);
	s_classic_menu_sound = std::max(s_classic_menu_sound, back ? 3 : 2);
}

static void ClassicFinishMenuSounds()
{
	ImGuiContext& context = *ImGui::GetCurrentContext();
	ImGuiContext& g = context; // Required by ImGui's configurable gamepad key aliases.
	static ImGuiID previous_nav = 0;
	static ImGuiID previous_hover = 0;
	static double last_scroll = -1.0;
	const bool active = s_classic_menu_active || FullscreenUI::HasActiveWindow();
	if (active)
	{
		const bool nav_changed = context.NavId && previous_nav && context.NavId != previous_nav;
		const bool mouse_changed = context.HoveredId && context.HoveredId != previous_hover &&
		                           (context.IO.MouseDelta.x != 0 || context.IO.MouseDelta.y != 0);
		if (nav_changed || mouse_changed || context.IO.MouseWheel != 0)
			s_classic_menu_sound = std::max(s_classic_menu_sound, 1);
		if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) ||
			ImGui::IsKeyPressed(ImGuiKey_NavGamepadActivate, false) ||
			(ImGui::IsMouseReleased(ImGuiMouseButton_Left) && context.HoveredId))
			s_classic_menu_sound = std::max(s_classic_menu_sound, 2);
		if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_NavGamepadCancel, false))
			s_classic_menu_sound = 3;
	}
	previous_nav = active ? context.NavId : 0;
	previous_hover = active ? context.HoveredId : 0;
	const int sound = std::exchange(s_classic_menu_sound, 0);
	if ((!active && !sound) || !Host::GetBaseBoolSettingValue("ClassicMini", "MenuSounds", true) ||
		!sound || (sound == 1 && ImGui::GetTime() - last_scroll < .075))
		return;
	if (sound == 1)
		last_scroll = ImGui::GetTime();
	const char* filename = sound == 3 ? "back.wav" : sound == 2 ? "confirm.wav" :
	                                                              "scroll.wav";
	const std::string path = Path::Combine(EmuFolders::Resources, std::string("sounds/classic/") + filename);
	Common::PlaySoundAsync(path.c_str());
}
