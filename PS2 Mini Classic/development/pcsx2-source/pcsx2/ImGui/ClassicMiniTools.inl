// SPDX-License-Identifier: GPL-3.0+
// All RAM access is scheduled on the emulation thread. UI state belongs to the GS thread.
#include "Memory.h"
#include <atomic>
#include <cstring>
#include <sstream>
#include <charconv>

static bool s_classic_tools_open = false;
static bool s_classic_scan_busy = false;
static std::atomic_bool s_classic_scan_cancel{false};
struct ClassicRamHit
{
	u32 address;
	u32 value;
};
static std::vector<ClassicRamHit> s_classic_hits;
static u32 s_classic_search_crc = 0;
static int s_classic_search_width = 4;
static std::string s_classic_search_status = "Zuerst nach einem bekannten Wert suchen.";
static char s_classic_pnach[65536] = {};
static u32 s_classic_editor_crc = 0;

static void ClassicStartScan(u32 start, u32 end, int width, int mode, u32 target)
{
	if (s_classic_scan_busy)
		return;
	if (start >= end || end > 0x02000000 || (width != 1 && width != 2 && width != 4) || end - start < static_cast<u32>(width))
	{
		s_classic_search_status = "Ungueltiger Bereich: 00000000 bis 02000000 (exklusiv).";
		return;
	}
	if (mode != 0 && s_classic_hits.empty())
	{
		s_classic_search_status = "Zuerst eine exakte Suche starten.";
		return;
	}
	const u32 crc = FullscreenUI::s_current_disc_crc;
	if (!s_classic_hits.empty() && (crc != s_classic_search_crc || width != s_classic_search_width))
	{
		s_classic_search_status = "Spiel oder Datenbreite geaendert: Neue Suche waehlen.";
		return;
	}
	s_classic_scan_busy = true;
	s_classic_scan_cancel.store(false);
	s_classic_search_status = "Suche laeuft ...";
	Host::RunOnCPUThread([start, end, width, mode, target, crc, previous = s_classic_hits]() {
		auto found = std::make_shared<std::vector<ClassicRamHit>>();
		std::string error;
		if (!VMManager::HasValidVM() || VMManager::GetDiscCRC() != crc || !eeMem)
			error = "Das Spiel wurde beendet oder gewechselt.";
		else
		{
			const auto test = [&](u32 address, u32 old) {
				u32 value = 0;
				std::memcpy(&value, eeMem->Main + address, width);
				const bool match = (mode == 0 && value == target) || (mode == 1 && value != old) ||
				                   (mode == 2 && value == old) || (mode == 3 && value > old) || (mode == 4 && value < old);
				if (match)
					found->push_back({address, value});
			};
			if (previous.empty())
			{
				for (u32 address = start; address <= end - width; address += width)
				{
					if (s_classic_scan_cancel.load() || found->size() >= 250000)
						break;
					test(address, 0);
				}
			}
			else
			{
				for (const ClassicRamHit& hit : previous)
				{
					if (s_classic_scan_cancel.load())
						break;
					test(hit.address, hit.value);
				}
			}
			if (s_classic_scan_cancel.load())
				error = "Suche abgebrochen. Vorherige Treffer bleiben erhalten.";
			else if (found->size() >= 250000)
				error = "Zu viele Treffer. Kleineren Bereich oder selteneren Wert suchen.";
		}
		MTGS::RunOnGSThread([found, error = std::move(error), crc, width]() {
			s_classic_scan_busy = false;
			if (!error.empty())
				s_classic_search_status = error;
			else if (crc != FullscreenUI::s_current_disc_crc)
				s_classic_search_status = "Spiel gewechselt: Suchergebnis verworfen.";
			else
			{
				s_classic_hits = std::move(*found);
				s_classic_search_crc = crc;
				s_classic_search_width = width;
				s_classic_search_status = fmt::format("{} Treffer (maximal 2.000 werden angezeigt).", s_classic_hits.size());
			}
		});
	});
}

static bool ClassicSavePnach(const std::string& content)
{
	const u32 crc = FullscreenUI::s_current_disc_crc;
	if (!crc)
		return false;
	const std::string path = Path::Combine(EmuFolders::Cheats, fmt::format("{:08X}.pnach", crc));
	if (auto old = FileSystem::ReadFileToString(path.c_str()); old.has_value())
	{
		if (!FileSystem::WriteStringToFile((path + ".bak").c_str(), *old))
			return false;
	}
	const std::string temporary = path + ".classic.tmp";
	if (!FileSystem::WriteStringToFile(temporary.c_str(), content))
		return false;
	return FileSystem::RenamePath(temporary.c_str(), path.c_str());
}

static void ClassicEditNumber(const char* label,u32* value,bool hex=false)
{
 ImGui::PushID(label);
 ImGui::InputScalar(label,ImGuiDataType_U32,value,nullptr,nullptr,hex?"%08X":"%u",hex?ImGuiInputTextFlags_CharsHexadecimal:0);
 if(ImGui::Button("Mit Bildschirmtastatur eingeben"))
  ImGuiFullscreen::OpenInputStringDialog(label,hex?"Hexadezimalzahl ohne 0x eingeben.":"Dezimalzahl eingeben.",label,"Uebernehmen",
   [value,hex](std::string text){
    u32 parsed=0;auto result=std::from_chars(text.data(),text.data()+text.size(),parsed,hex?16:10);
    if(result.ec==std::errc{}&&result.ptr==text.data()+text.size())*value=parsed;
    else ImGuiFullscreen::ShowToast("Eingabe","Bitte eine gueltige 32-Bit-Zahl eingeben.");
   },hex?fmt::format("{:08X}",*value):std::to_string(*value),hex?ImGuiFullscreen::InputFilterType::None:ImGuiFullscreen::InputFilterType::Numeric);
 ImGui::PopID();
}

static void ClassicPnachControllerEdit(bool remove=false)
{
 auto lines=std::make_shared<std::vector<std::string>>();std::istringstream stream(s_classic_pnach);std::string line;
 while(std::getline(stream,line))lines->push_back(line);
 ImGuiFullscreen::ChoiceDialogOptions options;
 if(!remove)options.emplace_back("Neue Zeile anhaengen",false);
 for(size_t i=0;i<lines->size();i++)options.emplace_back(fmt::format("{}: {}",i+1,(*lines)[i]),false);
 if(options.empty()){ImGuiFullscreen::ShowToast("PNACH","Keine Zeilen vorhanden.");return;}
 ImGuiFullscreen::OpenChoiceDialog(remove?"Zeile entfernen":"PNACH mit Controller bearbeiten",false,std::move(options),[lines,remove](s32 choice,const std::string&,bool){ImGuiFullscreen::CloseChoiceDialog();
  if(choice<0)return;
  const int index=remove?choice:choice-1;
  auto apply=[lines,index](std::string text){
   if(index<0)lines->push_back(std::move(text));else (*lines)[index]=std::move(text);
   std::string result;for(const auto& row:*lines){result+=row;result+='\n';}
   if(result.size()<sizeof(s_classic_pnach))StringUtil::Strlcpy(s_classic_pnach,result.c_str(),sizeof(s_classic_pnach));
   else ImGuiFullscreen::ShowToast("PNACH","Der Editor ist voll. Die Aenderung wurde nicht uebernommen.");
  };
  if(remove){lines->erase(lines->begin()+index);std::string result;for(const auto& row:*lines){result+=row;result+='\n';}StringUtil::Strlcpy(s_classic_pnach,result.c_str(),sizeof(s_classic_pnach));}
  else ImGuiFullscreen::OpenInputStringDialog("PNACH-Zeile","Unverschluesselten PNACH-Code eingeben. Speichern erfolgt anschliessend mit PNACH speichern.","Code","Uebernehmen",std::move(apply),index<0?"":(*lines)[index]);
 });
}

static void DrawClassicMiniTools()
{
	if (!s_classic_tools_open || ImGuiFullscreen::IsInputDialogOpen() || ImGuiFullscreen::IsChoiceDialogOpen())
		return;
	if (!VMManager::HasValidVM())
	{
		s_classic_tools_open = false;
		s_classic_scan_cancel.store(true);
		return;
	}
	if (!s_classic_scan_busy && s_classic_search_crc && s_classic_search_crc != FullscreenUI::s_current_disc_crc)
	{
		s_classic_hits.clear();
		s_classic_search_crc = 0;
		s_classic_search_status = "Neues Spiel: vorherige Suche zurueckgesetzt.";
	}
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * .5f, ImGuiCond_Always, ImVec2(.5f, .5f));
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize * .84f, ImGuiCond_Always);
	if (!ImGui::IsPopupOpen("Classic Mini - Cheats und RAM"))
		ImGui::OpenPopup("Classic Mini - Cheats und RAM");
	if (!ImGui::BeginPopupModal("Classic Mini - Cheats und RAM", &s_classic_tools_open, ImGuiWindowFlags_NoResize))
		return;
	ImGuiFullscreen::ResetFocusHere();
	ImGui::Text("Spiel: %s / %08X", FullscreenUI::s_current_disc_serial.c_str(), FullscreenUI::s_current_disc_crc);
	if (ImGui::BeginTabBar("classic_tools_tabs"))
	{
		if (ImGui::BeginTabItem("Speichersuche"))
		{
			static u32 start = 0x00100000, end = 0x00200000, value = 100, address = 0;
			static int width_index = 2, mode = 0;
			ImGui::TextWrapped("EE-RAM, vorzeichenlose Ganzzahlen. Start/Ende/Adresse in Hex, Wert dezimal. Beim Suchen bleibt das Spiel pausiert.");
			ClassicEditNumber("Start (Hex)",&start,true);
			ClassicEditNumber("Ende exklusiv (Hex)",&end,true);
			ImGui::Combo("Datenbreite", &width_index, "8 Bit\0"
													  "16 Bit\0"
													  "32 Bit\0");
			ClassicEditNumber("Wert",&value);
			ImGui::Combo("Vergleich", &mode, "Exakt\0Geaendert\0Unveraendert\0Groesser\0Kleiner\0");
			const int width = 1 << width_index;
			ImGui::BeginDisabled(s_classic_scan_busy);
			if (ImGui::Button("Suchen / Eingrenzen"))
				ClassicStartScan(start, end, width, mode, value);
			ImGui::SameLine();
			if (ImGui::Button("Neue Suche"))
			{
				s_classic_hits.clear();
				s_classic_search_crc = 0;
				s_classic_search_status = "Neue Suche bereit.";
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("Abbrechen"))
				s_classic_scan_cancel.store(true);
			ImGui::TextWrapped("%s", s_classic_search_status.c_str());
			if (ImGui::BeginChild("classic_hits", ImVec2(0, ImGui::GetContentRegionAvail().y * .45f), ImGuiChildFlags_Borders))
			{
				ImGuiListClipper clipper;
				clipper.Begin(static_cast<int>(std::min<size_t>(s_classic_hits.size(), 2000)));
				while (clipper.Step())
					for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
					{
						const ClassicRamHit& hit = s_classic_hits[i];
						if (ImGui::Selectable(fmt::format("{:08X}    {:10d}    0x{:X}", hit.address, hit.value, hit.value).c_str(), address == hit.address))
							address = hit.address;
					}
			}
			ImGui::EndChild();
			ClassicEditNumber("Adresse (Hex)",&address,true);
			const bool safe = address <= 0x02000000u - width && (width == 4 || value < (1u << (width * 8)));
			ImGui::BeginDisabled(!safe || s_classic_scan_busy);
			if (ImGui::Button("Wert einmalig schreiben"))
			{
				const u32 crc = FullscreenUI::s_current_disc_crc;
				Host::RunOnCPUThread([address = address, value = value, width, crc]() {
					if (!VMManager::HasValidVM() || VMManager::GetDiscCRC() != crc || Achievements::IsHardcoreModeActive())
						return;
					if (width == 1)
						vtlb_ramWrite<mem8_t>(address, static_cast<u8>(value));
					else if (width == 2)
						vtlb_ramWrite<mem16_t>(address, static_cast<u16>(value));
					else
						vtlb_ramWrite<mem32_t>(address, value);
					Host::AddOSDMessage("RAM-Wert geschrieben.");
				});
			}
			ImGui::SameLine();
			if (ImGui::Button("Als PNACH ergaenzen"))
			{
				const std::string path = Path::Combine(EmuFolders::Cheats, fmt::format("{:08X}.pnach", FullscreenUI::s_current_disc_crc));
				const std::string content = FileSystem::ReadFileToString(path.c_str()).value_or("") +
				                            fmt::format("\npatch=1,EE,{:08X},{},{:08X}\n", address, width == 1 ? "byte" : width == 2 ? "short" :
																																	   "word",
												value);
				s_classic_search_status = ClassicSavePnach(content) ? "PNACH gespeichert; im Spielprofil aktivieren." : "PNACH konnte nicht gespeichert werden.";
			}
			ImGui::EndDisabled();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("PNACH-Editor"))
		{
			const u32 crc = FullscreenUI::s_current_disc_crc;
			if (s_classic_editor_crc != crc)
			{
				s_classic_editor_crc = crc;
				const std::string path = Path::Combine(EmuFolders::Cheats, fmt::format("{:08X}.pnach", crc));
				const std::string text = FileSystem::ReadFileToString(path.c_str()).value_or("// Eigene PNACH-Codes\n");
				StringUtil::Strlcpy(s_classic_pnach, text.c_str(), sizeof(s_classic_pnach));
			}
			ImGui::TextWrapped("Unverschluesselte PNACH-Codes. AR MAX / Game Genie muessen zuerst konvertiert werden. Vorhandene Datei wird gesichert.");
			if(ImGui::Button("Zeilen mit Controller bearbeiten"))ClassicPnachControllerEdit();
			ImGui::SameLine();if(ImGui::Button("Zeile entfernen"))ClassicPnachControllerEdit(true);
			ImGui::InputTextMultiline("##classic_pnach", s_classic_pnach, sizeof(s_classic_pnach), ImVec2(-1, ImGui::GetContentRegionAvail().y - 110));
			if (ImGui::Button("PNACH speichern"))
				Host::AddOSDMessage(ClassicSavePnach(s_classic_pnach) ? "PNACH gespeichert." : "Speichern fehlgeschlagen.");
			ImGui::SameLine();
			if (ImGui::Button("Cheat-Liste des Spielprofils"))
			{
				s_classic_tools_open = false;
				ImGui::CloseCurrentPopup();
				FullscreenUI::SwitchToGameSettings();
				FullscreenUI::s_settings_page = FullscreenUI::SettingsPage::Cheats;
			}
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	if (ImGui::Button("Zurueck zum Pausenmenue") || (ImGui::GetTopMostPopupModal()==ImGui::GetCurrentWindow()&&ImGuiFullscreen::WantsToCloseMenu()))
	{
		s_classic_tools_open = false;
		s_classic_scan_cancel.store(true);
		ImGui::CloseCurrentPopup();
		ImGuiFullscreen::QueueResetFocus(ImGuiFullscreen::FocusResetType::PopupClosed);
	}
	ImGui::EndPopup();
}
