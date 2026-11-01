// SPDX-License-Identifier: GPL-3.0+
// Private Classic Mini frontend. Drawn directly by the emulator's GS UI.
#include <cmath>

static void DrawClassicMiniBackground()
{
	const ImVec2 size = ImGui::GetIO().DisplaySize;
	const float t = static_cast<float>(ImGui::GetTime());
	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	dl->AddRectFilledMultiColor(ImVec2(0, 0), size, IM_COL32(0, 1, 5, 255), IM_COL32(0, 3, 12, 255),
		IM_COL32(2, 14, 38, 255), IM_COL32(1, 5, 17, 255));
	const float unit = size.y / 720.0f;
	const ImVec2 horizon(size.x * 0.36f, size.y * 0.56f);
	// A deterministic, slowly rotating field of translucent memory towers.
	for (int row = 9; row >= 0; row--)
	{
		for (int col = 0; col < 11; col++)
		{
			const int seed = (row * 67 + col * 31 + 17) % 113;
			if (seed % 5 == 0)
				continue;
			const float depth = 1.0f / (1.0f + row * 0.38f);
			const float drift = std::sin(t * 0.07f + row * 0.17f) * 14.0f * unit;
			const float x = horizon.x + (col - 5.0f) * 98.0f * depth * unit + drift;
			const float y = horizon.y + (170.0f - row * 26.0f) * depth * unit;
			const float w = (22.0f + seed % 14) * depth * unit;
			const float h = (40.0f + seed % 95) * depth * unit * (0.96f + 0.04f * std::sin(t * 0.35f + seed));
			const float side = w * 0.42f;
			const int alpha = static_cast<int>(35 + depth * 105);
			dl->AddRectFilledMultiColor(ImVec2(x, y - h), ImVec2(x + w, y),
				IM_COL32(52, 102, 159, alpha), IM_COL32(30, 69, 112, alpha),
				IM_COL32(6, 17, 39, 0), IM_COL32(9, 26, 53, 0));
			dl->AddQuadFilled(ImVec2(x + w, y - h), ImVec2(x + w + side, y - h - side * 0.5f),
				ImVec2(x + w + side, y - side * 0.5f), ImVec2(x + w, y), IM_COL32(16, 46, 84, alpha));
			dl->AddQuadFilled(ImVec2(x, y - h), ImVec2(x + side, y - h - side * 0.5f),
				ImVec2(x + w + side, y - h - side * 0.5f), ImVec2(x + w, y - h), IM_COL32(113, 163, 206, alpha));
			dl->AddLine(ImVec2(x, y - h), ImVec2(x, y), IM_COL32(133, 184, 224, alpha / 2));
		}
	}
	for (int i = 0; i < 48; i++)
	{
		const float phase = i * 2.39996f;
		const float x = std::fmod(i * 139.7f + t * (1.4f + i % 3), size.x);
		const float y = std::fmod(size.y + i * 73.1f - t * (2.5f + i % 5), size.y);
		dl->AddCircleFilled(ImVec2(x + std::sin(phase + t * .1f) * 5 * unit, y), unit,
			IM_COL32(119, 173, 233, 28 + i % 55), 8);
	}
	if (FullscreenUI::s_current_main_window != FullscreenUI::MainWindowType::Landing)
		return;
	const ImVec2 center(size.x * 0.29f, size.y * 0.43f);
	for (int i = 0; i < 8; i++)
	{
		const float a = t * .32f + i * 0.785398f;
		const ImVec2 orb(center.x + std::cos(a) * 112 * unit, center.y + std::sin(a) * 67 * unit);
		const float radius = (4 + 1.5f * (1 + std::sin(a))) * unit;
		for (int glow = 5; glow >= 1; glow--)
			dl->AddCircleFilled(orb, radius + glow * 3.4f * unit, IM_COL32(37, 103, 242, 8 + (5 - glow) * 3), 24);
		dl->AddCircleFilled(orb, radius, IM_COL32(135, 189, 255, 220), 24);
		dl->AddCircleFilled(ImVec2(orb.x - unit, orb.y - unit), radius * .42f, IM_COL32(222, 241, 255, 245), 16);
	}
	const auto font = ImGuiFullscreen::g_large_font;
	dl->AddText(font.first, font.second * 1.3f, ImVec2(size.x * .10f, size.y * .14f),
		IM_COL32(153, 199, 247, 235), "P L A Y S T A T I O N  2");
	dl->AddText(ImGuiFullscreen::g_medium_font.first, ImGuiFullscreen::g_medium_font.second,
		ImVec2(size.x * .105f, size.y * .14f + 48 * unit), IM_COL32(91, 128, 172, 210), "C L A S S I C   M I N I");
}

static void DrawClassicMiniLanding()
{
	using namespace FullscreenUI;
	const ImVec2 size = ImGui::GetIO().DisplaySize;
	const ImVec2 pos(size.x * .56f, size.y * .20f);
	const ImVec2 extent(size.x * .40f, size.y * .74f);
	if (BeginFullscreenWindow(pos, extent, "classic_landing", ImVec4(0, 0, 0, 0), 0.0f,
			ImVec2(10, 10), ImGuiWindowFlags_NoBackground))
	{
		ResetFocusHere();
		BeginMenuButtons(9, .5f, 12, 8, 40);
		if (ImGuiFullscreen::MenuButtonWithoutSummary("Spielebibliothek", true, 40))
			OpenClassicMiniHub(0);
		if (ImGuiFullscreen::MenuButtonWithoutSummary("Systemkonfiguration", true, 40))
			SwitchToSettings();
		if (ImGuiFullscreen::MenuButtonWithoutSummary("Memory Cards", true, 40))
		{
			SwitchToSettings();
			OpenClassicMiniHub(2);
		}
		if (ImGuiFullscreen::MenuButtonWithoutSummary("Spiel / PS2-System starten", true, 40))
		{
			s_current_main_window = MainWindowType::StartGame;
			QueueResetFocus(FocusResetType::WindowChanged);
		}
		if (ImGuiFullscreen::MenuButtonWithoutSummary("Ordner und Bibliothek", true, 40))
		{
			SwitchToSettings();
			s_settings_page = SettingsPage::Folders;
		}
		if (ImGuiFullscreen::MenuButtonWithoutSummary("Nach Updates suchen", true, 40))
		{
			Host::RequestClassicMiniUpdateCheck();
			ImGuiFullscreen::ShowToast("Updates", "Update-Pruefung gestartet. Ergebnis unter Systemkonfiguration / Updates.");
		}
		if (ImGuiFullscreen::MenuButtonWithoutSummary("Benutzerprofile", true, 40))
			OpenClassicMiniHub(1);
		if (ImGuiFullscreen::MenuButtonWithoutSummary("Sicherungen", true, 40))
			OpenClassicMiniHub(3);
		if (ImGuiFullscreen::MenuButtonWithoutSummary("Ausschalten", true, 40) ||
			(!AreAnyDialogsOpen() && WantsToCloseMenu()))
		{
			s_current_main_window = MainWindowType::Exit;
			QueueResetFocus(FocusResetType::WindowChanged);
		}
		EndMenuButtons();
	}
	EndFullscreenWindow();
	if (ImGui::Shortcut(ImGuiKey_F11))
		DoToggleFullscreen();
	SetStandardSelectionFooterText(true);
}

void FullscreenUI::DrawClassicMiniUpdateNotice()
{
	ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImGui::GetStyleColorVec4(ImGuiCol_Text));
	MenuHeading("Hinweis zur Entwickler-Ausgabe");
	ImGuiFullscreen::MenuButton("Einmalige Einrichtung auf jedem neuen PC",
		"Nach dem Entpacken development\\Compiler einrichten.cmd starten und das Microsoft-Setup abschliessen.", false);
	ImGuiFullscreen::MenuButton("Werkzeuge pruefen",
		"Danach development\\Werkzeuge pruefen.cmd ausfuehren. Zum Spielen ist kein Compiler-Setup erforderlich.", false);
	ImGuiFullscreen::MenuButton("Automatische Updates",
		"Neue PCSX2-Versionen werden mit den Classic-Mini-Anpassungen neu gebaut. Internet und freier Speicher sind erforderlich.", false);
	ImGuiFullscreen::MenuButton("Bei inkompatiblen Aenderungen",
		"Schlaegt der Neubau fehl, bleibt die funktionierende Version erhalten. Zukuenftige Versionen koennen neue Werkzeuge benoetigen.", false);
	ImGuiFullscreen::MenuButton("Deine Daten bleiben erhalten",
		"Updates ersetzen Programmdateien. Spiele, BIOS, Memory Cards und Savestates bleiben erhalten.", false);
	ImGui::PopStyleColor();
}

static void DrawClassicMiniFirstStart()
{
	using namespace FullscreenUI;
	const ImVec2 size = ImGui::GetIO().DisplaySize;
	if (BeginFullscreenWindow(ImVec2(0, 0), size, "classic_first_start", ImVec4(0.01f, 0.03f, 0.08f, 0.97f)))
	{
		ResetFocusHere();
		BeginMenuButtons();
		MenuHeading("Willkommen bei PS2 Classic Mini");
		DrawClassicMiniUpdateNotice();
		if (ImGuiFullscreen::MenuButton("Verstanden - weiter zum Hauptmenue",
			"Dieser Hinweis erscheint beim ersten Start. Du findest ihn jederzeit unter Systemkonfiguration / Updates."))
		{
			Host::SetBaseBoolSettingValue("ClassicMini", "UpdateNoticeAcknowledged", true);
			Host::CommitBaseSettingChanges();
			QueueResetFocus(FocusResetType::WindowChanged);
		}
		EndMenuButtons();
	}
	EndFullscreenWindow();
	SetStandardSelectionFooterText(false);
}
