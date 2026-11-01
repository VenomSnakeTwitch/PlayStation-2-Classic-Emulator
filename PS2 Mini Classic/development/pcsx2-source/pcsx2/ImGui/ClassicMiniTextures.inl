// SPDX-License-Identifier: GPL-3.0+
static bool s_cm_texture_open=false;
static int s_cm_texture_pane=0;
static std::string s_cm_texture_serial,s_cm_texture_name;
static u32 s_cm_texture_crc=0;
static void CMOpenTextures(std::string serial,u32 crc)
{
 s_cm_texture_serial=std::move(serial);s_cm_texture_crc=crc;s_cm_texture_name.clear();s_cm_texture_open=true;s_cm_texture_pane=0;
 ImGuiFullscreen::QueueResetFocus(ImGuiFullscreen::FocusResetType::WindowChanged);
}
static void CMDrawTextures()
{
 using namespace FullscreenUI;const ImVec2 size=ImGui::GetIO().DisplaySize;const float header=LayoutScale(54.f),left=size.x*.43f;
 CMPaneKeys(s_cm_texture_pane,!s_cm_texture_name.empty());
 const std::string root=Path::Combine(EmuFolders::Textures,s_cm_texture_serial),dumps=Path::Combine(root,"dumps"),replacements=Path::Combine(root,"replacements");
 if(BeginFullscreenWindow(ImVec2(0,0),ImVec2(size.x,header),"texture_header",UIPrimaryColor)){
  BeginNavBar();if(NavButton(ICON_PF_BACKWARD,true,true)){s_cm_texture_open=false;QueueResetFocus(FocusResetType::WindowChanged);}NavTitle(("Textur-Werkstatt / "+s_cm_texture_serial).c_str());EndNavBar();
 }EndFullscreenWindow();
 if(!s_cm_texture_open)return; // Preserve the focus request for the menu being reopened.
 if(BeginFullscreenWindow(ImVec2(0,header),ImVec2(left,size.y-header-LayoutScale(36.f)),"texture_list",UIBackgroundColor,0.f,ImVec2(),ImGuiWindowFlags_NoFocusOnAppearing)){
  CMPaneFocus(s_cm_texture_pane,0);BeginMenuButtons();
  CMInfo("Gedumpte Texturen","Eine Textur auswaehlen, um Original und Ersatz nebeneinander zu sehen. PNG/JPG/BMP werden als passende PNG-Ersatztextur gespeichert.");
  if(ImGuiFullscreen::MenuButton("Textur-Dumping einschalten","Speichert Texturen dieses Spiels beim Spielen.",!Host::IsClassicMiniBusy()))Host::RequestClassicMiniTexture(s_cm_texture_serial,s_cm_texture_crc,"","",3);
  if(ImGuiFullscreen::MenuButton("Textur-Dumping ausschalten","Beendet neue Dumps; vorhandene Dateien bleiben erhalten.",!Host::IsClassicMiniBusy()))Host::RequestClassicMiniTexture(s_cm_texture_serial,s_cm_texture_crc,"","",4);
  if(ImGuiFullscreen::MenuButton("Ersatztexturen jetzt neu laden","Laedt Austauschdateien neu und leert den Textur-Cache. Hardware-Renderer erforderlich.",!Host::IsClassicMiniBusy()))Host::RequestClassicMiniTexture(s_cm_texture_serial,s_cm_texture_crc,"","",5);
  FileSystem::FindResultsArray files;FileSystem::FindFiles(dumps.c_str(),"*.png",FILESYSTEM_FIND_FILES|FILESYSTEM_FIND_SORT_BY_NAME,&files);
  for(const auto& file:files){std::string name(Path::GetFileName(file.FileName));bool replaced=FileSystem::FileExists(Path::Combine(replacements,name).c_str());auto* image=GetCachedTextureAsync(file.FileName.c_str());
   const std::string label=(name==s_cm_texture_name?"> ":"")+name+"###"+name;
   bool pressed=image?ImGuiFullscreen::MenuImageButton(label.c_str(),replaced?"Ersatz vorhanden":"Original-Dump",reinterpret_cast<ImTextureID>(image->GetNativeHandle()),LayoutScale(48.f,48.f),true,58.f):ImGuiFullscreen::MenuButton(label.c_str(),replaced?"Ersatz vorhanden":"Original-Dump",true,58.f);
   if(pressed){s_cm_texture_name=name;s_cm_texture_pane=1;QueueResetFocus(FocusResetType::Other);}
  }
  if(files.empty())CMInfo("Noch keine Dumps vorhanden","Dumping einschalten, ein Spiel mit Hardware-Renderer laufen lassen und dieses Menue erneut oeffnen. Texturen erscheinen, sobald das Spiel sie verwendet.");
  EndMenuButtons();
 }EndFullscreenWindow();
 if(BeginFullscreenWindow(ImVec2(left,header),ImVec2(size.x-left,size.y-header-LayoutScale(36.f)),"texture_details",UIBackgroundColor,0.f,ImVec2(),ImGuiWindowFlags_NoFocusOnAppearing)){
  CMPaneFocus(s_cm_texture_pane,1);BeginMenuButtons();CMInfo("Status",FileSystem::ReadFileToString(CMData("classic/status.txt").c_str()).value_or("Bereit."));
  if(!s_cm_texture_name.empty()){
   const std::string original=Path::Combine(dumps,s_cm_texture_name),replacement=Path::Combine(replacements,s_cm_texture_name);bool active=FileSystem::FileExists(replacement.c_str()),disabled=FileSystem::FileExists((replacement+".disabled").c_str());
   const float width=ImGui::GetContentRegionAvail().x*.46f;ImGui::BeginGroup();CMText("Original-Dump");
   if(auto* image=GetCachedTextureAsync(original.c_str())){ImGui::Image(reinterpret_cast<ImTextureID>(image->GetNativeHandle()),ImVec2(width,width));CMText(fmt::format("{} x {} Pixel",image->GetWidth(),image->GetHeight()));}ImGui::EndGroup();ImGui::SameLine();ImGui::BeginGroup();CMText(active?"Ersatz aktiv":disabled?"Ersatz deaktiviert":"Noch kein Ersatz");
   std::string preview=active?replacement:disabled?replacement+".disabled":"";
   if(!preview.empty())if(auto* image=GetCachedTextureAsync(preview.c_str())){ImGui::Image(reinterpret_cast<ImTextureID>(image->GetNativeHandle()),ImVec2(width,width));CMText(fmt::format("{} x {} Pixel",image->GetWidth(),image->GetHeight()));}ImGui::EndGroup();
   if(ImGuiFullscreen::MenuButton("Ersatzbild auswaehlen",active?"Vorhandener Ersatz wird vorher gesichert. Der originale Dump bleibt erhalten.":"Das Bild wird mit dem passenden Textur-Dateinamen importiert.",!Host::IsClassicMiniBusy())){
    const auto serial=s_cm_texture_serial,name=s_cm_texture_name;const u32 crc=s_cm_texture_crc;
    OpenFileSelector("Ersatzbild auswaehlen",false,[serial,name,crc](const std::string& source){if(!source.empty())Host::RequestClassicMiniTexture(serial,crc,name,source,0);CloseFileSelector();},{"*.png","*.jpg","*.jpeg","*.bmp"},Path::Combine(EmuFolders::Textures,serial));
   }
   if(ImGuiFullscreen::MenuButton("Diesen Austausch deaktivieren","Die Ersatzdatei bleibt gespeichert; das Spiel verwendet wieder das Original.",active&&!Host::IsClassicMiniBusy()))Host::RequestClassicMiniTexture(s_cm_texture_serial,s_cm_texture_crc,s_cm_texture_name,"",1);
   if(ImGuiFullscreen::MenuButton("Diesen Austausch aktivieren","Aktiviert die gespeicherte Ersatzdatei wieder.",disabled&&!Host::IsClassicMiniBusy()))Host::RequestClassicMiniTexture(s_cm_texture_serial,s_cm_texture_crc,s_cm_texture_name,"",2);
   if(ImGuiFullscreen::MenuButton("Vorherigen Ersatz wiederherstellen","Stellt die letzte Sicherung dieser Ersatztextur wieder her.",!Host::IsClassicMiniBusy()))Host::RequestClassicMiniTexture(s_cm_texture_serial,s_cm_texture_crc,s_cm_texture_name,"",6);
   CMInfo("Hinweis","Austauschdateien liegen unter textures / Seriennummer / replacements. Ein Bildeditor ist nicht enthalten. Im pausierten Spiel werden Aenderungen direkt neu geladen.");
  }else CMInfo("Textur auswaehlen","Links findest du alle PNG-Dumps fuer dieses Spiel.");
  EndMenuButtons();
 }EndFullscreenWindow();
 if(!AreAnyDialogsOpen()&&WantsToCloseMenu()){
  if(s_cm_texture_pane==1){s_cm_texture_pane=0;QueueResetFocus(FocusResetType::Other);}
  else {s_cm_texture_open=false;QueueResetFocus(FocusResetType::WindowChanged);}
 }SetStandardSelectionFooterText(true);
 ImGuiFullscreen::AppendToFullscreenFooterText(std::array{std::pair<const char*,std::string_view>{ICON_PF_DPAD_LEFT_RIGHT,"Texturliste / Aktionen (auch L1/R1)"}});
}
