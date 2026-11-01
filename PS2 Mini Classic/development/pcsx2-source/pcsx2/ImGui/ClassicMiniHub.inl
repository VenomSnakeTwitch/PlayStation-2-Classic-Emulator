// SPDX-License-Identifier: GPL-3.0+
#include <algorithm>
#include <set>
#include <sstream>

static int s_cm_page=0,s_cm_filter=0,s_cm_hub_pane=0;
// Fullscreen columns are separate ImGui windows: switch them explicitly instead
// of relying on mouse clicks or geometric navigation across window boundaries.
static void CMPaneKeys(int& pane, bool details_available)
{
 if(FullscreenUI::AreAnyDialogsOpen() || ImGui::GetIO().WantTextInput)return;
 if(ImGui::IsKeyPressed(ImGuiKey_GamepadL1,false)||(ImGui::IsKeyPressed(ImGuiKey_LeftArrow,false)||ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft,false)||ImGui::IsKeyPressed(ImGuiKey_GamepadLStickLeft,false))){
  pane=0;ImGuiFullscreen::ForceKeyNavEnabled();ImGuiFullscreen::QueueResetFocus(ImGuiFullscreen::FocusResetType::Other);
 }
 if(details_available&&(ImGui::IsKeyPressed(ImGuiKey_GamepadR1,false)||(ImGui::IsKeyPressed(ImGuiKey_RightArrow,false)||ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight,false)||ImGui::IsKeyPressed(ImGuiKey_GamepadLStickRight,false)))){
  pane=1;ImGuiFullscreen::ForceKeyNavEnabled();ImGuiFullscreen::QueueResetFocus(ImGuiFullscreen::FocusResetType::Other);
 }
}
static void CMPaneFocus(int& pane,int current)
{
 if(pane==current)ImGuiFullscreen::ResetFocusHere();
 if(ImGui::IsWindowFocused()&&!ImGuiFullscreen::IsFocusResetQueued())pane=current;
}
static std::string s_cm_selection,s_cm_card,s_cm_save,s_cm_filter_kind,s_cm_filter_value,s_cm_search;
static std::string s_cm_profile_loaded;
static std::unique_ptr<INISettingsInterface> s_cm_metadata,s_cm_personal;
static bool s_cm_manual=false,s_cm_graphics=false;
static int s_cm_manual_page=0;
static float s_cm_manual_zoom=1.0f;
static std::string CMData(const char* relative) {return Path::Combine(EmuFolders::DataRoot,relative);}
static std::string CMActive() {return Host::GetBaseStringSettingValue("ClassicMini","ActiveProfile","default");}
static std::string CMUserRoot() {return CMData(("classic/profiles/"+CMActive()).c_str());}
static void CMLoadLibrary(bool force=false)
{
 if(!force&&s_cm_metadata&&s_cm_profile_loaded==CMActive())return;
 FileSystem::CreateDirectoryPath(CMUserRoot().c_str(),false);
 s_cm_profile_loaded=CMActive();s_cm_metadata=std::make_unique<INISettingsInterface>(CMData("classic/library.ini"));s_cm_metadata->Load();
 s_cm_personal=std::make_unique<INISettingsInterface>(Path::Combine(CMUserRoot(),"library.ini"));s_cm_personal->Load();
}
static std::string CMGameKey(const GameList::Entry& game){return game.serial.empty()?fmt::format("CRC-{:08X}",game.crc):fmt::format("{}-{:08X}",game.serial,game.crc);}
static void CMInfo(const char* title,const std::string& text)
{
 ImGui::PushStyleColor(ImGuiCol_TextDisabled,ImGui::GetStyleColorVec4(ImGuiCol_Text));
 ImGuiFullscreen::MenuButton(title,text.c_str(),false);ImGui::PopStyleColor();
}
static void CMEdit(const std::string& game,const char* key,const char* label,bool personal=false)
{
 CMLoadLibrary();auto* ini=personal?s_cm_personal.get():s_cm_metadata.get();
 const std::string value=ini->GetStringValue(game.c_str(),key,"");
 ImGuiFullscreen::OpenInputStringDialog(label,"Wird lokal gespeichert. Leere Felder bleiben unbekannt.",label,"Speichern",
  [game,key=std::string(key),personal](std::string value){CMLoadLibrary();auto* file=personal?s_cm_personal.get():s_cm_metadata.get();file->SetStringValue(game.c_str(),key.c_str(),value.c_str());if(!file->Save())ImGuiFullscreen::ShowToast("Speichern","Die Aenderung konnte nicht gespeichert werden.");},value);
}
void FullscreenUI::OpenClassicMiniHub(int page,int filter)
{
 s_cm_page=page;s_cm_filter=filter;s_cm_hub_pane=0;s_cm_manual=false;s_cm_graphics=false;CMLoadLibrary(true);
 s_current_main_window=MainWindowType::ClassicHub;QueueResetFocus(FocusResetType::WindowChanged);
}
static void CMText(const std::string& text)
{
 ImGui::PushFont(ImGuiFullscreen::g_medium_font.first,ImGuiFullscreen::g_medium_font.second);ImGui::TextWrapped("%s",text.c_str());ImGui::PopFont();
}
static std::vector<GameList::Entry> CMGames()
{
 using namespace FullscreenUI;std::vector<GameList::Entry> games;
 {auto lock=GameList::GetLock();for(u32 i=0;i<GameList::GetEntryCount();i++)if(const auto* e=GameList::GetEntryByIndex(i))games.push_back(*e);}
 games.erase(std::remove_if(games.begin(),games.end(),[](const auto& g){
  const std::string key=CMGameKey(g);if(s_cm_filter==1&&g.last_played_time==0)return true;
  if(s_cm_filter==2&&!s_cm_personal->GetBoolValue(key.c_str(),"Favorite",g.is_favorite))return true;
  if(!s_cm_search.empty()) {std::string a=g.title+" "+g.serial,b=s_cm_search;std::transform(a.begin(),a.end(),a.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});std::transform(b.begin(),b.end(),b.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});if(a.find(b)==std::string::npos)return true;}
  if(s_cm_filter==3&&!s_cm_filter_kind.empty()){
   std::string actual=s_cm_filter_kind=="Region"?GameList::RegionToString(g.region,false):(s_cm_filter_kind=="Collection"?s_cm_personal.get():s_cm_metadata.get())->GetStringValue(key.c_str(),s_cm_filter_kind.c_str(),"");
   if(s_cm_filter_kind=="Players") {if(std::atoi(actual.c_str())<2)return true;}
   else if(actual!=s_cm_filter_value)return true;
  }return false;
 }),games.end());
 std::sort(games.begin(),games.end(),[](const auto& a,const auto& b){return s_cm_filter==1?a.last_played_time>b.last_played_time:a.GetTitle()<b.GetTitle();});return games;
}
static void CMChooseCollection()
{
 ImGuiFullscreen::OpenChoiceDialog("Sammlungen",false,{{"Genre",false},{"Mehrspieler",false},{"Region",false},{"Eigene Sammlung",false}},[](s32 choice,const std::string&,bool){ImGuiFullscreen::CloseChoiceDialog();
  if(choice<0)return;const char* keys[]={"Genre","Players","Region","Collection"};s_cm_filter_kind=keys[choice];s_cm_filter=0;
  if(choice==1){s_cm_filter=3;return;}
  std::set<std::string> unique;auto games=CMGames();for(const auto& g:games){std::string key=CMGameKey(g);auto value=choice==2?std::string(GameList::RegionToString(g.region,false)):(choice==3?s_cm_personal.get():s_cm_metadata.get())->GetStringValue(key.c_str(),keys[choice],"");if(!value.empty())unique.insert(value);}
  ImGuiFullscreen::ChoiceDialogOptions options;for(const auto& item:unique)options.emplace_back(item,false);
  if(options.empty()){ImGuiFullscreen::OpenInfoMessageDialog("Sammlungen","Noch keine passenden Angaben. Du kannst Genre, Spieleranzahl und eigene Sammlungen auf der Detailseite eines Spiels eintragen.");return;}
  ImGuiFullscreen::OpenChoiceDialog("Sammlung waehlen",false,std::move(options),[](s32 i,const std::string& name,bool){ImGuiFullscreen::CloseChoiceDialog();if(i>=0){s_cm_filter=3;s_cm_filter_value=name;}});
 });
}
static void CMStateProtection(const GameList::Entry& game)
{
 ImGuiFullscreen::ChoiceDialogOptions choices;std::vector<std::string> paths;
 for(s32 i=-1;i<=VMManager::NUM_SAVE_STATE_SLOTS;i++){
  const auto path=VMManager::GetSaveStateFileName(game.serial.c_str(),game.crc,i);if(!FileSystem::FileExists(path.c_str()))continue;
  bool locked=FileSystem::FileExists((path+".classic-lock").c_str());choices.emplace_back(fmt::format("{} {}",i<0?"Fortsetzen":fmt::format("Slot {}",i),locked?"[geschuetzt]":"[ungeschuetzt]"),false);paths.push_back(path);
 }
 if(choices.empty()){ImGuiFullscreen::ShowToast("Savestates","Noch keine Spielstaende vorhanden.");return;}
 ImGuiFullscreen::OpenChoiceDialog("Ueberschreibschutz umschalten",false,std::move(choices),[paths](s32 i,const std::string&,bool){ImGuiFullscreen::CloseChoiceDialog();
  if(i<0)return;const std::string marker=paths[i]+".classic-lock";bool ok=FileSystem::FileExists(marker.c_str())?FileSystem::DeleteFilePath(marker.c_str()):FileSystem::WriteStringToFile(marker.c_str(),"Classic Mini: geschuetzter Savestate\n");
  ImGuiFullscreen::ShowToast("Savestate-Schutz",ok?"Schutz umgeschaltet. Geschuetzte Spielstaende werden weder ueberschrieben noch geloescht.":"Schutz konnte nicht gespeichert werden.");
 });
}
static void CMDrawGraphics(const GameList::Entry& game)
{
 using namespace FullscreenUI;
 CMInfo("Grafik-Schnellprofile","Schematische Vorschau der internen Aufloesung. Die echte Darstellung und Leistung sind spielabhaengig.");
 const char* names[]={"Original PS2","Scharf in HD","Hohe Qualitaet"};const char* descriptions[]={"1x native Aufloesung, 4:3, originaler Bildcharakter.","3x native Aufloesung, 4:3, schaerfere 3D-Kanten.","6x native Aufloesung, 4:3, 16x anisotrope Filterung; leistungsfaehige Grafik benoetigt."};
 const int multipliers[]={1,3,6};
 for(int i=0;i<3;i++){
  const ImVec2 pos=ImGui::GetCursorScreenPos();const float w=ImGui::GetContentRegionAvail().x,h=LayoutScale(58.0f);
  ImDrawList* dl=ImGui::GetWindowDrawList();dl->AddRectFilled(pos,ImVec2(pos.x+w,pos.y+h),IM_COL32(5,15,35,255));
  const int steps=6*multipliers[i];for(int j=0;j<steps;j++){float x=pos.x+(w-12)*j/steps;float y=pos.y+h-8-h*.75f*j/steps;dl->AddRectFilled(ImVec2(x,y),ImVec2(x+w/steps+1,pos.y+h-4),IM_COL32(70,140,225,255));}ImGui::Dummy(ImVec2(w,h));
  if(ImGuiFullscreen::MenuButton(names[i],descriptions[i])){
   std::string path=VMManager::GetGameSettingsPath(game.serial,game.crc);INISettingsInterface ini(path);ini.Load();
   ini.SetFloatValue("EmuCore/GS","upscale_multiplier",static_cast<float>(multipliers[i]));ini.SetIntValue("EmuCore/GS","MaxAnisotropy",i==2?16:0);ini.SetStringValue("EmuCore/GS","AspectRatio","4:3");
   if(ini.Save()){GameList::SetCustomSettingsForPath(game.path,true);s_cm_personal->SetStringValue(CMGameKey(game).c_str(),"GraphicsPreset",names[i]);s_cm_personal->Save();ShowToast("Grafikprofil","Fuer dieses Spiel gespeichert.");}else ShowToast("Grafikprofil","Speichern fehlgeschlagen.");
  }
 }
 if(ImGuiFullscreen::MenuButtonWithoutSummary("Zurueck zum Spiel")){s_cm_graphics=false;QueueResetFocus(FocusResetType::Other);}
}
static void CMDrawManual(const GameList::Entry& game)
{
 using namespace FullscreenUI;const std::string key=CMGameKey(game);
 const std::string cache=Path::Combine(EmuFolders::Cache,"classic-manuals/"+key);
 const std::string page=Path::Combine(cache,fmt::format("page-{:04}.png",s_cm_manual_page));
 INISettingsInterface info(Path::Combine(cache,"info.ini"));info.Load();int count=info.GetIntValue("Manual","Pages",0);
 CMInfo("Handbuch",fmt::format("Seite {} / {} - Zoom {:.0f}%",s_cm_manual_page+1,std::max(1,count),s_cm_manual_zoom*100));
 if(ImGuiFullscreen::MenuButtonWithoutSummary("Vorherige Seite",s_cm_manual_page>0)){--s_cm_manual_page;Host::RequestClassicMiniManual(key,"",s_cm_manual_page);}
 if(ImGuiFullscreen::MenuButtonWithoutSummary("Naechste Seite",count>s_cm_manual_page+1)){++s_cm_manual_page;Host::RequestClassicMiniManual(key,"",s_cm_manual_page);}
 if(ImGuiFullscreen::MenuButtonWithoutSummary("Zoom wechseln"))s_cm_manual_zoom=s_cm_manual_zoom>=2.0f?1.0f:s_cm_manual_zoom+.5f;
 if(ImGuiFullscreen::MenuButtonWithoutSummary("Handbuch schliessen")){s_cm_manual=false;QueueResetFocus(FocusResetType::Other);}
 if(FileSystem::FileExists(page.c_str())){
  if(GSTexture* image=GetCachedTextureAsync(page.c_str())){float w=ImGui::GetContentRegionAvail().x*s_cm_manual_zoom;ImGui::Image(reinterpret_cast<ImTextureID>(image->GetNativeHandle()),ImVec2(w,w*image->GetHeight()/image->GetWidth()));}
 }else CMText("Seite wird vorbereitet. Bei Problemen erscheint die Meldung im Statusbereich.");
}
#include "ClassicMiniTextures.inl"
static void CMDrawGameDetails(const GameList::Entry& game)
{
 using namespace FullscreenUI;const std::string key=CMGameKey(game);
 if(s_cm_manual){CMDrawManual(game);return;}if(s_cm_graphics){CMDrawGraphics(game);return;}
 auto cover=GameList::GetCoverImagePathForEntry(&game);if(!cover.empty())if(auto* image=GetCachedTextureAsync(cover.c_str())){
  const float height=LayoutScale(155.f);ImGui::Image(reinterpret_cast<ImTextureID>(image->GetNativeHandle()),ImVec2(height*image->GetWidth()/image->GetHeight(),height));ImGui::SameLine();
 }
 ImGui::BeginGroup();CMText(game.GetTitle());CMText(game.serial+" | "+GameList::RegionToString(game.region,true));
 CMText("Spielzeit: "+GameList::FormatTimespan(game.total_played_time,true));CMText("Zuletzt: "+GameList::FormatTimestamp(game.last_played_time));
 CMText("Jahr: "+s_cm_metadata->GetStringValue(key.c_str(),"Year","unbekannt")+" | Genre: "+s_cm_metadata->GetStringValue(key.c_str(),"Genre","unbekannt"));
 CMText("Spieler: "+s_cm_metadata->GetStringValue(key.c_str(),"Players","unbekannt"));ImGui::EndGroup();
 CMText(s_cm_metadata->GetStringValue(key.c_str(),"Description","Noch keine Beschreibung. Unter Spielinfos bearbeiten kannst du die Angaben ergaenzen."));
 bool idle=!Host::IsClassicMiniBusy();
 if(ImGuiFullscreen::MenuButton("Spiel starten","Mit den Einstellungen des aktiven Profils.",idle))DoStartPath(game.path);
 bool favorite=s_cm_personal->GetBoolValue(key.c_str(),"Favorite",game.is_favorite);
 if(ImGuiFullscreen::MenuButton(favorite?"Aus Favoriten entfernen":"Zu Favoriten hinzufuegen","Nur fuer das aktive Benutzerprofil.")){s_cm_personal->SetBoolValue(key.c_str(),"Favorite",!favorite);s_cm_personal->Save();GameList::SaveFavoriteForPath(game.path,!favorite);}
 if(ImGuiFullscreen::MenuButton("Savestates als Bildergalerie","Mit Datum, gespeicherter Spielzeit und Vorschaubild.",idle))OpenLoadStateSelectorForGame(game.path);
 if(ImGuiFullscreen::MenuButton("Savestate schuetzen / freigeben","Geschuetzte Spielstaende koennen geladen werden; Speichern und Loeschen werden blockiert.",idle))CMStateProtection(game);
 if(ImGuiFullscreen::MenuButton("Grafik-Schnellprofile","Original PS2, Scharf in HD und Hohe Qualitaet mit schematischer Vorschau.")){s_cm_graphics=true;QueueResetFocus(FocusResetType::Other);}
 if(ImGuiFullscreen::MenuButton("Textur-Werkstatt","Gedumpte Texturen ansehen, ersetzen, deaktivieren und wiederherstellen."))CMOpenTextures(game.serial,game.crc);
 if(ImGuiFullscreen::MenuButton("Spielinfos bearbeiten","Beschreibung, Erscheinungsjahr, Genre und Spieleranzahl lokal pflegen.")){
  ImGuiFullscreen::OpenChoiceDialog("Spielinfos",false,{{"Beschreibung",false},{"Erscheinungsjahr",false},{"Genre",false},{"Spieleranzahl",false}},[key](s32 i,const std::string&,bool){ImGuiFullscreen::CloseChoiceDialog();const char* fields[]={"Description","Year","Genre","Players"};const char* labels[]={"Beschreibung","Erscheinungsjahr","Genre","Spieleranzahl"};if(i>=0)CMEdit(key,fields[i],labels[i]);});
 }
 if(ImGuiFullscreen::MenuButton("Eigene Sammlung","Ein eigener Sammlungsname fuer dieses Spiel, zum Beispiel Rennspiele oder Party."))CMEdit(key,"Collection","Eigene Sammlung",true);
 if(ImGuiFullscreen::MenuButton("Notizen und Tipps",s_cm_personal->GetStringValue(key.c_str(),"Notes","Eigene Tipps, Tastenkombinationen und Hinweise eintragen.").c_str()))CMEdit(key,"Notes","Notizen und Tipps",true);
 const bool manual=FileSystem::FileExists(CMData(("classic/manuals/"+key+"/manual.pdf").c_str()).c_str())||FileSystem::FileExists(CMData(("classic/manuals/"+key+"/manual.png").c_str()).c_str());
 if(ImGuiFullscreen::MenuButton("Handbuch oeffnen","PDF oder Bild direkt im Launcher lesen.",manual)){s_cm_manual=true;QueueResetFocus(FocusResetType::Other);s_cm_manual_page=0;Host::RequestClassicMiniManual(key,"",0);}
 if(ImGuiFullscreen::MenuButton("Handbuch hinzufuegen","Eigenes PDF-, PNG- oder JPG-Handbuch waehlen."))OpenFileSelector("Handbuch",false,[key](const std::string& path){if(!path.empty())Host::RequestClassicMiniManual(key,path,0);CloseFileSelector();},{"*.pdf","*.png","*.jpg","*.jpeg"});
}
static void CMDrawProfiles()
{
 using namespace FullscreenUI;INISettingsInterface registry(CMData("classic/profiles.ini"));registry.Load();auto names=registry.GetKeyValueList("Profiles");names.insert(names.begin(),{"default","Standard"});
 CMInfo("Aktives Profil",CMActive()=="default"?"Standard":registry.GetStringValue("Profiles",CMActive().c_str(),CMActive().c_str()));
 for(const auto& [id,name]:names)if(ImGuiFullscreen::MenuButton(name.c_str(),id==CMActive()?"Aktiv":"Zu diesem Benutzerprofil wechseln.",id!=CMActive()&&!Host::IsClassicMiniBusy()))Host::RequestClassicMiniAction("profile",id,"");
 if(ImGuiFullscreen::MenuButton("Neues Benutzerprofil","Eigene Memory Cards, Savestates, Einstellungen, Favoriten und Notizen.",!Host::IsClassicMiniBusy()))OpenInputStringDialog("Benutzerprofil","Ein neuer Benutzer beginnt mit leeren Memory Cards.","Name","Erstellen",[](std::string name){if(!name.empty())Host::RequestClassicMiniAction("profile","",name);});
 if(ImGuiFullscreen::MenuButton("Konsolenname",Host::GetBaseStringSettingValue("ClassicMini","ConsoleName","PS2 Classic Mini").c_str()))OpenInputStringDialog("Konsolenname","Wird in der Startanimation angezeigt.","Name","Speichern",[](std::string name){if(!name.empty()&&name.size()<=80){Host::SetBaseStringSettingValue("ClassicMini","ConsoleName",name.c_str());Host::CommitBaseSettingChanges();}},Host::GetBaseStringSettingValue("ClassicMini","ConsoleName","PS2 Classic Mini"));
 bool intro=Host::GetBaseBoolSettingValue("ClassicMini","IntroEnabled",true);if(ImGuiFullscreen::ToggleButton("PS2-Startanimation","Ueberspringbare Startsequenz mit deinem Konsolennamen.",&intro)){Host::SetBaseBoolSettingValue("ClassicMini","IntroEnabled",intro);Host::CommitBaseSettingChanges();}
}
static std::vector<std::string> CMCardNames(bool folders_only=false)
{
 std::vector<std::string> result;
 for(const auto& card:FileMcd_GetAvailableCards(true))if(!folders_only||card.type==MemoryCardType::Folder)result.push_back(card.name);
 std::sort(result.begin(),result.end());return result;
}
static void CMDrawCards()
{
 using namespace FullscreenUI;auto cards=CMCardNames();
 if(ImGuiFullscreen::MenuButton("Neue Ordnerkarte erstellen","Leere Karte fuer das Kopieren einzelner Spielstaende anlegen.",!Host::IsClassicMiniBusy()))OpenInputStringDialog("Neue Ordnerkarte","Ein kurzer Kartenname ohne Pfadangabe.","Name","Erstellen",[](std::string name){if(!name.empty())Host::RequestClassicMiniAction("create-folder-card",name,"");});
 if(ImGuiFullscreen::MenuButton("Alle Memory-Card-Einstellungen","Die vollstaendige PCSX2-Kartenverwaltung oeffnen.")){SwitchToSettings();s_settings_page=SettingsPage::MemoryCard;}
 if(ImGuiFullscreen::MenuButton("Originale PS2-Speicherverwaltung starten","Startet dein BIOS im selben Fenster, mit den originalen animierten Spielstandsymbolen.",!Host::IsClassicMiniBusy()))DoStartBIOS();
 CMInfo("Memory Cards","Dateikarten bleiben beim Durchsehen unveraendert. Eine Ordnerkopie macht einzelne Spielstaende zugaenglich.");
 for(const auto& name:cards)if(ImGuiFullscreen::MenuButton(name.c_str(),name==s_cm_card?"Ausgewaehlt":"Karte ansehen und verwalten."))s_cm_card=name;
 if(s_cm_card.empty())return;
 const std::string path=Path::Combine(EmuFolders::MemoryCards,s_cm_card);bool folder=FileSystem::DirectoryExists(path.c_str());MenuHeading(s_cm_card.c_str());
 if(ImGuiFullscreen::MenuButton("In Slot 1 einsetzen","Die Auswahl wird fuer das aktive Benutzerprofil gespeichert.",!Host::IsClassicMiniBusy()))Host::RequestClassicMiniAction("insert-card",s_cm_card,"");
 if(ImGuiFullscreen::MenuButton("Karte kopieren","Erstellt eine neue vollstaendige Kopie; das Original bleibt erhalten.",!Host::IsClassicMiniBusy()))Host::RequestClassicMiniAction("copy-card",s_cm_card,"");
 if(!folder){if(ImGuiFullscreen::MenuButton("Spielstaende durchsuchen: Ordnerkopie erstellen","Konvertiert eine formatierte PS2-Karte in eine zusaetzliche Ordnerkarte. Original wird beibehalten.",!Host::IsClassicMiniBusy()))Host::RequestClassicMiniAction("convert-card",s_cm_card,"");return;}
 FileSystem::FindResultsArray saves;FileSystem::FindFiles(path.c_str(),"*",FILESYSTEM_FIND_FOLDERS|FILESYSTEM_FIND_SORT_BY_NAME,&saves);
 for(const auto& save:saves){std::string name(Path::GetFileName(save.FileName));if(name.starts_with("_"))continue;
  if(ImGuiFullscreen::MenuButton((ICON_PF_MEMORY_CARD " "+name).c_str(),"Spielstand samt originalen Symbol- und Metadateien in eine andere Ordnerkarte kopieren.")){
   ImGuiFullscreen::ChoiceDialogOptions choices;std::vector<std::string> targets;for(const auto& target:CMCardNames(true))if(target!=s_cm_card){choices.emplace_back(target,false);targets.push_back(target);}
   if(choices.empty()){OpenInfoMessageDialog("Spielstand kopieren","Bitte zuerst eine zweite Ordnerkarte erstellen oder eine Dateikarte als Ordnerkopie konvertieren.");continue;}
   std::string source=s_cm_card+"/"+name;OpenChoiceDialog("Zielkarte",false,std::move(choices),[source,targets](s32 i,const std::string&,bool){ImGuiFullscreen::CloseChoiceDialog();if(i>=0)Host::RequestClassicMiniAction("copy-save",source,targets[i]);});
  }
 }
 if(ImGuiFullscreen::MenuButton("Originale PS2-Speicherverwaltung starten","Startet dein BIOS im selben Fenster. Dort werden die originalen animierten Spielstandsymbole angezeigt.",!Host::IsClassicMiniBusy()))DoStartBIOS();
}
static void CMDrawBackups()
{
 using namespace FullscreenUI;bool enabled=Host::GetBaseBoolSettingValue("ClassicMini","AutomaticBackups",true);
 if(ImGuiFullscreen::ToggleButton("Automatische Sicherungen","Alle sechs Stunden im Menue. Die letzten sieben automatischen Sicherungen bleiben erhalten; manuelle Sicherungen bleiben bestehen.",&enabled)){Host::SetBaseBoolSettingValue("ClassicMini","AutomaticBackups",enabled);Host::CommitBaseSettingChanges();}
 if(ImGuiFullscreen::MenuButton("Jetzt sichern","Neue Sicherung mit SHA-256-Pruefsummen erstellen.",!Host::IsClassicMiniBusy()))Host::RequestClassicMiniAction("backup","","");
 CMInfo("Wiederherstellen","Vor jeder Wiederherstellung wird der aktuelle Stand gesichert. Beschaedigte Sicherungen werden abgelehnt. Spiele und BIOS sind nicht Teil der Sicherung.");
 FileSystem::FindResultsArray entries;auto root=CMData("backups");FileSystem::FindFiles(root.c_str(),"*",FILESYSTEM_FIND_FOLDERS|FILESYSTEM_FIND_SORT_BY_NAME,&entries);
 for(auto it=entries.rbegin();it!=entries.rend();it++){
  if(!FileSystem::FileExists(Path::Combine(it->FileName,"manifest.json").c_str()))continue;std::string id(Path::GetFileName(it->FileName));
  if(ImGuiFullscreen::MenuButton(id.c_str(),"Diese Sicherung pruefen und wiederherstellen.",!Host::IsClassicMiniBusy()))OpenConfirmMessageDialog("Sicherung wiederherstellen","Memory Cards und Einstellungen aus dieser Sicherung uebernehmen? Der aktuelle Stand wird vorher gesichert.",[id](bool yes){if(yes)Host::RequestClassicMiniAction("restore",id,"");},false,"Wiederherstellen","Abbrechen");
 }
}
static bool DrawClassicMiniIntro()
{
 using namespace FullscreenUI;static double started=-1;static bool done=false;
 if(done||!Host::GetBaseBoolSettingValue("ClassicMini","IntroEnabled",true))return false;
 if(started<0)started=ImGui::GetTime();float elapsed=static_cast<float>(ImGui::GetTime()-started);
 if(elapsed>=3.8f){done=true;QueueResetFocus(FocusResetType::WindowChanged);return false;}
 const ImVec2 size=ImGui::GetIO().DisplaySize;
 if(BeginFullscreenWindow(ImVec2(0,0),size,"classic_intro",ImVec4(0,0,0,0))){
  ResetFocusHere();const auto title=Host::GetBaseStringSettingValue("ClassicMini","ConsoleName","PS2 Classic Mini");
  ImDrawList* dl=ImGui::GetWindowDrawList();float alpha=std::clamp(elapsed,0.f,1.f)*std::clamp((3.8f-elapsed)*1.5f,0.f,1.f);
  auto font=ImGuiFullscreen::g_large_font;auto extent=font.first->CalcTextSizeA(font.second,FLT_MAX,0,title.c_str());dl->AddText(font.first,font.second,ImVec2((size.x-extent.x)*.5f,size.y*.44f),IM_COL32(160,205,255,static_cast<int>(255*alpha)),title.c_str());
  ImGui::SetCursorPosY(size.y*.78f);BeginMenuButtons();if(ImGuiFullscreen::MenuButtonWithoutSummary("Ueberspringen")||ImGui::IsKeyPressed(ImGuiKey_Enter)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown))done=true;EndMenuButtons();
 }EndFullscreenWindow();return true;
}
static void DrawClassicMiniHub()
{
 using namespace FullscreenUI;CMLoadLibrary();const ImVec2 size=ImGui::GetIO().DisplaySize;const float header=LayoutScale(54.f),footer=LayoutScale(36.f);
 if(BeginFullscreenWindow(ImVec2(0,0),ImVec2(size.x,header),"classic_hub_header",UIPrimaryColor)){
  BeginNavBar();if(NavButton(ICON_PF_BACKWARD,true,true))SwitchToLanding();NavTitle(s_cm_page==0?"Spielebibliothek":s_cm_page==1?"Benutzerprofile":s_cm_page==2?"Memory Cards":"Sicherungen");EndNavBar();
 }EndFullscreenWindow();
 if(s_current_main_window!=MainWindowType::ClassicHub)return;
 if(s_cm_page==0){
  const float left=size.x*.43f;CMPaneKeys(s_cm_hub_pane,!s_cm_selection.empty());
  if(BeginFullscreenWindow(ImVec2(0,header),ImVec2(left,size.y-header-footer),"classic_library",UIBackgroundColor,0.f,ImVec2(),ImGuiWindowFlags_NoFocusOnAppearing)){
   CMPaneFocus(s_cm_hub_pane,0);BeginMenuButtons();
   if(ImGuiFullscreen::MenuButtonWithoutSummary("Alle Spiele")){s_cm_filter=0;s_cm_search.clear();}
   if(ImGuiFullscreen::MenuButtonWithoutSummary("Zuletzt gespielt"))s_cm_filter=1;
   if(ImGuiFullscreen::MenuButtonWithoutSummary("Favoriten"))s_cm_filter=2;
   if(ImGuiFullscreen::MenuButtonWithoutSummary("Sammlungen"))CMChooseCollection();
   if(ImGuiFullscreen::MenuButtonWithoutSummary("Suchen"))OpenInputStringDialog("Spiele suchen","Titel oder Seriennummer eingeben.","Suche","Suchen",[](std::string search){s_cm_search=std::move(search);},s_cm_search);
   if(ImGuiFullscreen::MenuButtonWithoutSummary("Klassische Coveransicht"))SwitchToGameList();
   MenuHeading(s_cm_filter==1?"Zuletzt gespielt":s_cm_filter==2?"Favoriten":s_cm_filter==3?s_cm_filter_value.c_str():"Alle Spiele");
   auto games=CMGames();for(const auto& game:games){std::string cover=GameList::GetCoverImagePathForEntry(&game);GSTexture* image=cover.empty()?nullptr:GetCachedTextureAsync(cover.c_str());
    const std::string label=(s_cm_selection==game.path?"> ":"")+game.GetTitle()+"###"+game.path;
    bool pressed=image?ImGuiFullscreen::MenuImageButton(label.c_str(),game.serial.c_str(),reinterpret_cast<ImTextureID>(image->GetNativeHandle()),LayoutScale(35.f,48.f),true,58.f):ImGuiFullscreen::MenuButton(label.c_str(),game.serial.c_str(),true,58.f);
    if(pressed){s_cm_selection=game.path;s_cm_manual=false;s_cm_graphics=false;s_cm_hub_pane=1;QueueResetFocus(FocusResetType::Other);}
   }
   if(games.empty())CMInfo("Keine Spiele in dieser Ansicht","Favoriten, Sammlungen und zuletzt gespielte Titel werden pro Benutzer gespeichert.");EndMenuButtons();
  }EndFullscreenWindow();
  if(BeginFullscreenWindow(ImVec2(left,header),ImVec2(size.x-left,size.y-header-footer),"classic_game_details",UIBackgroundColor,0.f,ImVec2(),ImGuiWindowFlags_NoFocusOnAppearing)){
   CMPaneFocus(s_cm_hub_pane,1);BeginMenuButtons();std::optional<GameList::Entry> selected;{auto lock=GameList::GetLock();if(const auto* e=GameList::GetEntryForPath(s_cm_selection.c_str()))selected=*e;}
   if(selected)CMDrawGameDetails(*selected);else CMInfo("Waehle ein Spiel","Hier erscheinen Cover, Spielinfos, Savestates, Grafikprofile, Handbuch und Notizen.");EndMenuButtons();
  }EndFullscreenWindow();
 }else{
  if(BeginFullscreenWindow(ImVec2(0,header),ImVec2(size.x,size.y-header-footer),"classic_hub_content",UIBackgroundColor,0.f,ImVec2(),ImGuiWindowFlags_NoFocusOnAppearing)){
   ResetFocusHere();BeginMenuButtons();CMInfo("Status",FileSystem::ReadFileToString(CMData("classic/status.txt").c_str()).value_or("Bereit."));
   if(s_cm_page==1)CMDrawProfiles();else if(s_cm_page==2)CMDrawCards();else CMDrawBackups();EndMenuButtons();
  }EndFullscreenWindow();
 }
 if(!AreAnyDialogsOpen()&&WantsToCloseMenu()){
  if(s_cm_manual||s_cm_graphics){s_cm_manual=false;s_cm_graphics=false;QueueResetFocus(FocusResetType::Other);}
  else if(s_cm_page==0&&s_cm_hub_pane==1){s_cm_hub_pane=0;QueueResetFocus(FocusResetType::Other);}
  else SwitchToLanding();
 }SetStandardSelectionFooterText(true);
 if(s_cm_page==0)ImGuiFullscreen::AppendToFullscreenFooterText(std::array{std::pair<const char*,std::string_view>{ICON_PF_DPAD_LEFT_RIGHT,"Bereich wechseln (auch L1/R1)"}});
}
