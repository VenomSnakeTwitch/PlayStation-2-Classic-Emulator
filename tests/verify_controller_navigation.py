from pathlib import Path
import subprocess,os,shutil
root=Path(__file__).resolve().parents[1];src=root/'PS2 Mini Classic/development/pcsx2-source';test=root/'tests/controller-navigation';test.mkdir(exist_ok=True)
def function(text,signature):
 a=text.index(signature);b=text.index('{',a);depth=1;i=b+1
 while depth:
  if text[i]=='{':depth+=1
  elif text[i]=='}':depth-=1
  i+=1
 return text[a:i]
full=(src/'pcsx2/ImGui/ImGuiFullscreen.cpp').read_text();hub=(src/'pcsx2/ImGui/ClassicMiniHub.inl').read_text()
code=r'''
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdio>
#include <cstdlib>
namespace FullscreenUI {static bool modal=false;bool AreAnyDialogsOpen(){return modal;}}
namespace ImGuiFullscreen {
enum class FocusResetType {None,PopupOpened,PopupClosed,WindowChanged,Other};
static FocusResetType s_focus_reset_queued=FocusResetType::None;
static int s_close_button_state=0;
void QueueResetFocus(FocusResetType);bool ResetFocusHere();bool IsFocusResetQueued();void ForceKeyNavEnabled();
}
'''
for name in ('void ImGuiFullscreen::QueueResetFocus','bool ImGuiFullscreen::ResetFocusHere','bool ImGuiFullscreen::IsFocusResetQueued','void ImGuiFullscreen::ForceKeyNavEnabled'):
 code+=function(full,name)+'\n'
for name in ('static void CMPaneKeys','static void CMPaneFocus'):code+=function(hub,name)+'\n'
code+=r'''
static int pane=0,confirmations=0;
static void check(bool value,const char* label){if(!value){std::printf("FAIL: %s\n",label);std::exit(1);}std::printf("PASS: %s\n",label);}
static void frame(){
 ImGui::NewFrame();CMPaneKeys(pane,true);
 ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImVec2(300,400));
 ImGui::Begin("list",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoFocusOnAppearing);CMPaneFocus(pane,0);
 ImGui::BeginDisabled();ImGui::Button("Information");ImGui::EndDisabled();ImGui::Button("Dumping");ImGui::Button("Texture");ImGui::End();
 ImGui::SetNextWindowPos(ImVec2(300,0));ImGui::SetNextWindowSize(ImVec2(300,400));
 ImGui::Begin("details",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoFocusOnAppearing);CMPaneFocus(pane,1);ImGui::TextUnformatted("Preview");if(ImGui::Button("Replace"))++confirmations;ImGui::Button("Restore");ImGui::End();
 ImGui::EndFrame();
}
static void press(ImGuiKey key){auto& io=ImGui::GetIO();io.AddKeyEvent(key,true);frame();io.AddKeyEvent(key,false);frame();frame();}
int main(){
 ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize=ImVec2(800,600);io.DeltaTime=1.f/60;io.IniFilename=nullptr;
 io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_NavEnableGamepad;
 io.BackendFlags|=ImGuiBackendFlags_HasGamepad|ImGuiBackendFlags_RendererHasTextures;
 ImGuiFullscreen::QueueResetFocus(ImGuiFullscreen::FocusResetType::WindowChanged);frame();frame();
 check(GImGui->NavWindow && GImGui->NavWindow->ID==ImHashStr("list") && GImGui->NavId!=0,"initial list has focus without a click");
 press(ImGuiKey_GamepadR1);check(pane==1 && GImGui->NavWindow->ID==ImHashStr("details") && GImGui->NavId!=0,"R1 enters texture actions");
 press(ImGuiKey_GamepadFaceDown);check(confirmations==1,"gamepad confirm activates the replacement action without a click");
 press(ImGuiKey_GamepadDpadLeft);check(pane==0 && GImGui->NavWindow->ID==ImHashStr("list"),"D-pad left returns to texture list");
 press(ImGuiKey_GamepadDpadRight);check(pane==1,"D-pad right enters actions");
 press(ImGuiKey_GamepadL1);check(pane==0,"L1 returns to list");
 press(ImGuiKey_RightArrow);check(pane==1,"keyboard right enters actions");
 press(ImGuiKey_LeftArrow);check(pane==0,"keyboard left returns to list");
 FullscreenUI::modal=true;press(ImGuiKey_GamepadR1);check(pane==0,"dialog prevents underlying pane shortcuts");FullscreenUI::modal=false;
 GImGui->NavInputSource=ImGuiInputSource_Gamepad;GImGui->ActiveIdSource=ImGuiInputSource_Keyboard;ImGuiFullscreen::ForceKeyNavEnabled();
 check(GImGui->NavInputSource==ImGuiInputSource_Gamepad,"gamepad input source survives focus activation");
 pane=1;ImGuiFullscreen::QueueResetFocus(ImGuiFullscreen::FocusResetType::PopupClosed);frame();frame();check(pane==1 && GImGui->NavWindow->ID==ImHashStr("details"),"closing file dialog returns to actions");
 ImGui::DestroyContext();puts("PASS: controller navigation regression suite");
}
'''
cpp=test/'navigation.cpp';cpp.write_text(code)
env={k.upper():v for k,v in os.environ.items()};raw=subprocess.check_output(['cmd','/c',str(root/'build/native/environment.cmd')],text=True,errors='replace')
for line in raw.splitlines():
 if '=' in line and not line.startswith('='):
  k,v=line.split('=',1);env[k.upper()]=v
exe=test/'navigation.exe';cmd=[shutil.which('cl',path=env['PATH']),'/nologo','/std:c++20','/MD','/EHsc','/utf-8','/I'+str(src/'3rdparty/imgui/include'),str(cpp),str(root/'build/native/3rdparty/imgui/imgui.lib'),str(src/'deps/lib/freetype.lib'),'/Fe:'+str(exe),'/link','user32.lib','imm32.lib']
subprocess.run(cmd,cwd=test,env=env,check=True)
env['PATH']=str(root/'PS2 Mini Classic')+';'+env['PATH'];subprocess.run([str(exe)],env=env,check=True)
