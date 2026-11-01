// SPDX-License-Identifier: GPL-3.0+
// Native Classic Mini data services. All mutations run on the CPU thread while the VM is stopped.
#include <QtCore/QDirIterator>
#include <QtCore/QSaveFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QCryptographicHash>
#include <QtCore/QRegularExpression>
#include <QtCore/QUuid>
#include <atomic>
#include "common/Error.h"
#include <stdexcept>
#include "pcsx2/SIO/Memcard/MemoryCardFile.h"
#include "pcsx2/SIO/Memcard/MemoryCardFolder.h"

static std::atomic_bool s_cm_busy{false};
bool Host::IsClassicMiniBusy() { return s_cm_busy.load(); }
static std::string CMPath(const QString& path) { return QDir::toNativeSeparators(path).toStdString(); }
static QString CMRoot() { return QString::fromStdString(EmuFolders::DataRoot); }
static void CMStatus(const QString& message)
{
 QDir().mkpath(CMRoot()+"/classic");
 QSaveFile f(CMRoot()+"/classic/status.txt");
 if (f.open(QIODevice::WriteOnly)) {f.write(message.toUtf8());f.commit();}
}
static void CMRequire(bool success, const char* message) { if (!success) throw std::runtime_error(message); }
static void CMSaveIni(INISettingsInterface& ini)
{
 Error error;if(!ini.Save(&error))throw std::runtime_error(error.GetDescription());
}
static QString CMId(const QString& value)
{
 CMRequire(QRegularExpression("^[A-Za-z0-9_-]{1,64}$").match(value).hasMatch(), "Ungueltige Kennung.");return value;
}
static QString CMProfileRoot(const QString& id) { return CMRoot()+"/classic/profiles/"+CMId(id); }
static QStringList CMSections(const QString& file)
{
 QFile f(file);QStringList sections;
 if(f.open(QIODevice::ReadOnly)) while(!f.atEnd()) {
  const QString line=QString::fromUtf8(f.readLine()).trimmed();
  if(line.startsWith('[')&&line.endsWith(']'))sections.append(line.mid(1,line.size()-2));
 }
 sections.removeDuplicates();return sections;
}
static void CMCopyFile(const QString& source,const QString& target)
{
 CMRequire(!QFileInfo(source).isSymLink(),"Verknuepfungen werden nicht als Nutzerdaten kopiert.");
 QFile in(source);CMRequire(in.open(QIODevice::ReadOnly),"Quelldatei kann nicht gelesen werden.");
 QDir().mkpath(QFileInfo(target).absolutePath());QSaveFile out(target);
 CMRequire(out.open(QIODevice::WriteOnly),"Zieldatei kann nicht geschrieben werden.");
 while(!in.atEnd()) {auto chunk=in.read(1024*1024);CMRequire(!chunk.isEmpty()||in.error()==QFileDevice::NoError,"Lesefehler.");CMRequire(out.write(chunk)==chunk.size(),"Schreibfehler.");}
 CMRequire(out.commit(),"Datei konnte nicht sicher gespeichert werden.");
}
static QString CMHash(const QString& path)
{
 QFile f(path);CMRequire(f.open(QIODevice::ReadOnly),"Datei fehlt fuer Integritaetspruefung.");
 QCryptographicHash h(QCryptographicHash::Sha256);CMRequire(h.addData(&f),"Integritaetspruefung fehlgeschlagen.");return QString::fromLatin1(h.result().toHex());
}
static void CMCopyTree(const QString& source,const QString& target)
{
 if(QFileInfo(source).isFile()){CMCopyFile(source,target);return;}
 CMRequire(QFileInfo(source).isDir()&&!QFileInfo(source).isSymLink(),"Quellordner fehlt.");
 QDir().mkpath(target);QDirIterator it(source,QDir::Files|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
 while(it.hasNext()){QString f=it.next();CMCopyFile(f,target+"/"+QDir(source).relativeFilePath(f));}
}
static QString CMRelative(const QString& path)
{
 QString rel=QDir::fromNativeSeparators(path);
 CMRequire(!QDir::isAbsolutePath(rel)&&!rel.contains(':')&&!rel.split('/').contains("..")&&!rel.startsWith('/'),"Unsicherer Sicherungspfad.");
 QString first=rel.section('/',0,0);
 CMRequire(QStringList{"inis","memcards","sstates","gamesettings","inputprofiles","classic"}.contains(first),"Sicherungspfad liegt ausserhalb der Nutzerdaten.");
 // Existing symlink parents must never redirect a restore outside the data root.
 QString current=CMRoot();for(const auto& part:rel.split('/')){current+="/"+part;CMRequire(!QFileInfo(current).isSymLink(),"Verknuepfung im Wiederherstellungspfad.");}
 return rel;
}
static void CMFlushSettings()
{
 auto lock=Host::GetSettingsLock();CMRequire(Host::Internal::GetBaseSettingsLayer()->Save(),"Einstellungen konnten nicht gesichert werden.");
}
static QString CMBackup(const QString& reason)
{
 CMFlushSettings();
 const QString id=QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+"-"+QUuid::createUuid().toString(QUuid::Id128).left(6);
 const QString root=CMRoot()+"/backups/"+id;QDir().mkpath(root);
 QJsonArray entries;QSet<QString> seen;
 QStringList folders{"inis","memcards","gamesettings","inputprofiles","classic"};
 for(const QString& folder:folders){
  QDirIterator it(CMRoot()+"/"+folder,QDir::Files|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
  while(it.hasNext()){
   QString source=it.next();QString rel=CMRelative(QDir(CMRoot()).relativeFilePath(source));if(seen.contains(rel))continue;seen.insert(rel);
   if(rel.endsWith(".tmp")||rel.endsWith("status.txt"))continue;
   QString dest=root+"/data/"+rel;CMCopyFile(source,dest);entries.append(QJsonObject{{"path",rel},{"sha256",CMHash(dest)}});
  }
 }
 QSaveFile manifest(root+"/manifest.json");CMRequire(manifest.open(QIODevice::WriteOnly),"Sicherungsmanifest kann nicht geschrieben werden.");
 manifest.write(QJsonDocument(QJsonObject{{"format",1},{"reason",reason},{"files",entries}}).toJson());CMRequire(manifest.commit(),"Sicherung konnte nicht abgeschlossen werden.");
 Host::SetBaseStringSettingValue("ClassicMini","LastBackup",std::to_string(QDateTime::currentSecsSinceEpoch()).c_str());Host::CommitBaseSettingChanges();
 return id;
}
static void CMPruneAutomaticBackups()
{
 const QString root=QDir(CMRoot()+"/backups").canonicalPath();if(root.isEmpty())return;
 QStringList automatic;
 for(const auto& item:QDir(root).entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name)){
  if(item.isSymLink()||item.canonicalFilePath()!=root+"/"+item.fileName())continue;
  QFile f(item.filePath()+"/manifest.json");if(!f.open(QIODevice::ReadOnly))continue;
  auto o=QJsonDocument::fromJson(f.readAll()).object();
  if(o["format"].toInt()==1&&o["reason"].toString()=="Automatisch")automatic.append(item.canonicalFilePath());
 }
 while(automatic.size()>7){const QString target=automatic.takeFirst();
  if(QFileInfo(target).canonicalPath()==root&&!QFileInfo(target).isSymLink())QDir(target).removeRecursively();
 }
}
static void CMApplyIni(const QString& path)
{
 INISettingsInterface input(CMPath(path));CMRequire(input.Load(),"Einstellungsdatei kann nicht geladen werden.");
 auto lock=Host::GetSettingsLock();auto* base=Host::Internal::GetBaseSettingsLayer();
 auto old=CMSections(QString::fromStdString(Path::Combine(EmuFolders::Settings,"PCSX2.ini")));
 for(const QString& sec:old)base->ClearSection(sec.toUtf8().constData());
 for(const QString& sec:CMSections(path))base->SetKeyValueList(sec.toUtf8().constData(),input.GetKeyValueList(sec.toUtf8().constData()));
 CMRequire(base->Save(),"Einstellungen konnten nicht gespeichert werden.");lock.unlock();Host::CommitBaseSettingChanges();VMManager::Internal::UpdateEmuFolders();VMManager::ApplySettings();Host::RefreshGameListAsync(false);
}
static void CMRestore(const QString& id)
{
 const QString root=CMRoot()+"/backups/"+CMId(id);QFile f(root+"/manifest.json");CMRequire(f.open(QIODevice::ReadOnly),"Sicherung fehlt.");
 auto doc=QJsonDocument::fromJson(f.readAll());CMRequire(doc.isObject()&&doc.object()["format"].toInt()==1,"Ungueltiges Sicherungsmanifest.");
 auto entries=doc.object()["files"].toArray();CMRequire(!entries.isEmpty(),"Sicherung ist leer.");
 for(auto e:entries){auto o=e.toObject();QString rel=CMRelative(o["path"].toString());CMRequire(CMHash(root+"/data/"+rel)==o["sha256"].toString(),"Sicherung ist beschaedigt. Nichts wurde wiederhergestellt.");}
 const QString recovery=CMBackup("Vor Wiederherstellung");QStringList applied;
 try{
  for(auto e:entries){QString rel=CMRelative(e.toObject()["path"].toString());CMCopyFile(root+"/data/"+rel,CMRoot()+"/"+rel);applied.append(rel);}
 }catch(...){for(const auto& rel:applied){QString old=CMRoot()+"/backups/"+recovery+"/data/"+rel;if(QFileInfo::exists(old))CMCopyFile(old,CMRoot()+"/"+rel);else QFile::remove(CMRoot()+"/"+rel);}throw;}
 CMApplyIni(QString::fromStdString(Path::Combine(EmuFolders::Settings,"PCSX2.ini")));
 CMStatus("Sicherung wiederhergestellt. Der vorherige Stand liegt in "+recovery+". Zusaetzliche Dateien wurden beibehalten.");
}
static void CMSwitchProfile(const QString& requested,const QString& new_name)
{
 const QString current=QString::fromStdString(Host::GetBaseStringSettingValue("ClassicMini","ActiveProfile","default"));
 const QString next=new_name.isEmpty()?CMId(requested):"user-"+QUuid::createUuid().toString(QUuid::Id128).left(16);
 CMRequire(current!=next,"Dieses Profil ist bereits aktiv.");
 CMFlushSettings();
 const QString main=QString::fromStdString(Path::Combine(EmuFolders::Settings,"PCSX2.ini"));
 QDir().mkpath(CMProfileRoot(current));CMCopyFile(main,CMProfileRoot(current)+"/settings.ini");
 QDir().mkpath(CMProfileRoot(next));
 if(!new_name.isEmpty()){
  CMRequire(new_name.size()<=60&&!new_name.trimmed().isEmpty(),"Bitte einen Profilnamen mit 1 bis 60 Zeichen verwenden.");
  CMCopyFile(main,CMProfileRoot(next)+"/settings.ini");
  INISettingsInterface profile(CMPath(CMProfileRoot(next)+"/settings.ini"));profile.Load();
  for(const auto& pair:std::vector<std::pair<const char*,const char*>>{{"MemoryCards","memcards"},{"Savestates","sstates"},{"GameSettings","gamesettings"},{"InputProfiles","inputprofiles"}}){
   const QString rel="classic/profiles/"+next+"/"+pair.second;QDir().mkpath(CMRoot()+"/"+rel);profile.SetStringValue("Folders",pair.first,rel.toUtf8().constData());
  }
  profile.SetStringValue("MemoryCards","Slot1_Filename","Mcd001.ps2");profile.SetStringValue("MemoryCards","Slot2_Filename","Mcd002.ps2");
  CMSaveIni(profile);
  INISettingsInterface registry(CMPath(CMRoot()+"/classic/profiles.ini"));registry.Load();registry.SetStringValue("Profiles",next.toUtf8().constData(),new_name.trimmed().toUtf8().constData());CMSaveIni(registry);
 }
 INISettingsInterface profile(CMPath(CMProfileRoot(next)+"/settings.ini"));CMRequire(profile.Load(),"Profil fehlt.");
 // Keep installation paths, the game library and console identity shared across profiles.
 INISettingsInterface shared(CMPath(main));shared.Load();
 for(const char* sec:{"GameList","ClassicMini"})profile.SetKeyValueList(sec,shared.GetKeyValueList(sec));
 for(const char* folder:{"Bios","Covers","Textures","Cache","Logs","Cheats"})profile.SetStringValue("Folders",folder,shared.GetStringValue("Folders",folder,"").c_str());
 profile.SetStringValue("ClassicMini","ActiveProfile",next.toUtf8().constData());CMSaveIni(profile);
 CMApplyIni(CMProfileRoot(next)+"/settings.ini");
 if(!new_name.isEmpty()){
  CMRequire(QDir(QString::fromStdString(EmuFolders::MemoryCards)).absolutePath()==QDir(CMProfileRoot(next)+"/memcards").absolutePath(),"Profil-Kartenordner wurde nicht korrekt umgeschaltet.");
  for(const char* card:{"Mcd001.ps2","Mcd002.ps2"}){
   CMRequire(!QFileInfo::exists(QString::fromStdString(EmuFolders::MemoryCards)+"/"+card),"Die neue Karte existiert bereits und wird nicht ueberschrieben.");
   CMRequire(FileMcd_CreateNewCard(card,MemoryCardType::File,MemoryCardFileType::PS2_8MB),"Leere Memory Card konnte nicht erstellt werden.");
  }
 }
 CMStatus("Profil gewechselt. Memory Cards, Savestates, Spieleinstellungen und Favoriten sind getrennt.");
}
static void CMConvertCard(const QString& name)
{
 CMRequire(name==QFileInfo(name).fileName(),"Ungueltiger Kartenname.");
 const std::string source=Path::Combine(EmuFolders::MemoryCards,name.toStdString());
 auto data=FileSystem::ReadBinaryFile(source.c_str());CMRequire(data.has_value(),"Memory Card kann nicht gelesen werden.");
 CMRequire(FileMcd_IsMemoryCardFormatted(source),"Memory Card ist noch nicht formatiert.");
 FileMcd_InsertECC(*data);
 CMRequire(data->size()%FolderMemoryCard::PageSizeRaw==0,"Ungueltige Memory-Card-Groesse.");
 const QString target=QString::fromStdString(EmuFolders::MemoryCards)+"/"+QFileInfo(name).completeBaseName()+"_Ordner";
 CMRequire(!QFileInfo::exists(target),"Die Ordnerkopie existiert bereits. Bitte diese Karte auswaehlen.");
 Pcsx2Config::McdOptions options;options.Enabled=true;options.Type=MemoryCardType::Folder;
 FolderMemoryCard card;
 for(int pass=0;pass<2;pass++){
  card.Open(CMPath(target),options,0,false,"",pass==0);
  for(size_t address=0;address<data->size();address+=FolderMemoryCard::PageSizeRaw)card.Save(data->data()+address,static_cast<u32>(address),FolderMemoryCard::PageSizeRaw);
  card.Close();
 }
 if(data->size()!=FolderMemoryCard::TotalSizeRaw){card.Open(CMPath(target),options,0,false,"",false);card.SetSizeInMB(8);card.Close();}
 CMStatus("Ordnerkarte erstellt. Das Original blieb erhalten. Die neue Karte kann jetzt durchsucht und in Slot 1 eingesetzt werden.");
}
void Host::RequestClassicMiniAction(std::string action,std::string value,std::string extra)
{
 if(s_cm_busy.exchange(true))return;
 Host::RunOnCPUThread([action=std::move(action),value=std::move(value),extra=std::move(extra)](){
  try{
   CMRequire(!VMManager::HasValidVM(),"Bitte zuerst das laufende Spiel beenden.");if(action!="auto-backup")CMStatus("Vorgang laeuft ...");
   QString v=QString::fromStdString(value),e=QString::fromStdString(extra);
   if(action=="backup"){const QString id=CMBackup("Manuell");CMStatus("Sicherung erfolgreich: "+id);}
   else if(action=="auto-backup"){
    bool ok=false;auto last=QString::fromStdString(Host::GetBaseStringSettingValue("ClassicMini","LastBackup","0")).toLongLong(&ok);
    if(Host::GetBaseBoolSettingValue("ClassicMini","AutomaticBackups",true)&&(!ok||QDateTime::currentSecsSinceEpoch()-last>=21600)) {CMBackup("Automatisch");CMPruneAutomaticBackups();CMStatus("Automatische Sicherung erfolgreich.");}

   }
   else if(action=="restore")CMRestore(v);
   else if(action=="profile")CMSwitchProfile(v,e);
   else if(action=="convert-card")CMConvertCard(v);
   else if(action=="create-folder-card"){
    CMRequire(!v.trimmed().isEmpty()&&v==QFileInfo(v).fileName()&&v!="."&&v!=".."&&v.size()<80,"Ungueltiger Kartenname.");
    CMRequire(!QFileInfo::exists(QString::fromStdString(EmuFolders::MemoryCards)+"/"+v),"Dieser Kartenname ist bereits vorhanden.");
    CMRequire(FileMcd_CreateNewCard(value,MemoryCardType::Folder,MemoryCardFileType::PS2_8MB),"Ordnerkarte konnte nicht erstellt werden.");CMStatus("Neue Ordnerkarte erstellt.");
   }
   else if(action=="copy-card"){
    CMRequire(v==QFileInfo(v).fileName(),"Ungueltiger Kartenname.");QString source=QString::fromStdString(EmuFolders::MemoryCards)+"/"+v;
    QString dest=source+"_Kopie_"+QDateTime::currentDateTimeUtc().toString("yyyyMMddHHmmss");
    if(QFileInfo(source).isFile())dest+=".ps2";CMRequire(!QFileInfo::exists(dest),"Ziel existiert bereits.");CMCopyTree(source,dest);CMStatus("Kartenkopie erstellt: "+QFileInfo(dest).fileName());
   }
   else if(action=="copy-save"){
    auto parts=v.split('/');CMRequire(parts.size()==2&&parts[0]!=".."&&parts[1]!=".."&&!parts.contains("."),"Ungueltiger Spielstandpfad.");
    CMRequire(e==QFileInfo(e).fileName(),"Ungueltige Zielkarte.");QString root=QString::fromStdString(EmuFolders::MemoryCards);
    QString source=root+"/"+v,target=root+"/"+e+"/"+parts[1];CMRequire(QFileInfo(root+"/"+e).isDir(),"Die Zielkarte muss eine Ordnerkarte sein.");CMRequire(!QFileInfo::exists(target),"Dieser Spielstand existiert bereits auf der Zielkarte und wurde nicht ueberschrieben.");
    CMBackup("Vor Spielstandkopie");CMCopyTree(source,target);CMStatus("Spielstand mit seinen Symbol- und Metadateien kopiert.");
   }
   else if(action=="insert-card"){
    CMRequire(v==QFileInfo(v).fileName()&&QFileInfo::exists(QString::fromStdString(EmuFolders::MemoryCards)+"/"+v),"Karte fehlt.");
    Host::SetBaseStringSettingValue("MemoryCards","Slot1_Filename",value.c_str());Host::SetBaseBoolSettingValue("MemoryCards","Slot1_Enable",true);Host::CommitBaseSettingChanges();VMManager::ApplySettings();CMStatus("Memory Card in Slot 1 eingesetzt.");
   }
  }catch(const std::exception& error){CMStatus("Nicht ausgefuehrt: "+QString::fromUtf8(error.what()));}
  s_cm_busy.store(false);
 });
}
void Host::RequestClassicMiniManual(std::string key,std::string source,int page)
{
 QMetaObject::invokeMethod(QCoreApplication::instance(),[key=std::move(key),source=std::move(source),page](){
  try{
   const QString id=CMId(QString::fromStdString(key));CMRequire(page>=0&&page<5000,"Ungueltige Handbuchseite.");
   const QString cache=QString::fromStdString(EmuFolders::Cache)+"/classic-manuals/"+id;
   const QString helper=QString::fromStdString(EmuFolders::AppRoot)+"/tools/ClassicMiniManual.exe";
   CMRequire(QFileInfo::exists(helper),"Das mitgelieferte Handbuch-Hilfsprogramm fehlt.");
   auto* process=new QProcess(QCoreApplication::instance());process->setProgram(helper);
   QStringList args{"--manual",CMRoot()+"/classic/manuals/"+id,"--cache",cache,"--page",QString::number(page)};
   if(!source.empty())args<<"--source"<<QString::fromStdString(source);process->setArguments(args);process->setProcessChannelMode(QProcess::MergedChannels);
#ifdef _WIN32
   process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* arguments){arguments->flags|=CREATE_NO_WINDOW;});
#endif
   QObject::connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),process,[process,cache,page](int result,QProcess::ExitStatus){
    if(result!=0)CMStatus("Handbuch konnte nicht geoeffnet werden: "+QString::fromUtf8(process->readAll()));
    else {CMStatus("Handbuchseite bereit.");const std::string path=CMPath(cache+QString("/page-%1.png").arg(page,4,10,QChar('0')));MTGS::RunOnGSThread([path](){ImGuiFullscreen::InvalidateCachedTexture(path);});}
    process->deleteLater();
   });
   QObject::connect(process,&QProcess::errorOccurred,process,[process](QProcess::ProcessError error){if(error==QProcess::FailedToStart){CMStatus("Handbuch-Hilfsprogramm konnte nicht gestartet werden.");process->deleteLater();}});
   CMStatus("Handbuchseite wird vorbereitet ...");process->start();
  }catch(const std::exception& error){CMStatus(QString::fromUtf8(error.what()));}
 },Qt::QueuedConnection);
}
void Host::RequestClassicMiniRollback()
{
 QMetaObject::invokeMethod(QCoreApplication::instance(),[](){
  if(VMManager::HasValidVM()||Host::IsClassicMiniBusy())return;
  const QString helper=QString::fromStdString(EmuFolders::AppRoot)+"/updates/ClassicMiniUpdater.exe";
  QStringList args{"--project",QString::fromStdString(EmuFolders::AppRoot)+"/development","--install",QString::fromStdString(EmuFolders::AppRoot),"--rollback"};
  if(QProcess::startDetached(helper,args,QString::fromStdString(EmuFolders::AppRoot)))QCoreApplication::quit();
  else CMStatus("Update-Rueckkehr konnte nicht gestartet werden.");
 },Qt::QueuedConnection);
}
#include <QtGui/QImageReader>
#include "pcsx2/GS/Renderers/HW/GSTextureReplacements.h"
#include "pcsx2/GS/Renderers/Common/GSRenderer.h"
static void CMTextureChange(const std::string& serial,u32 crc,const std::string& name,const std::string& source,int mode)
{

   CMId(QString::fromStdString(serial));const bool running=VMManager::HasValidVM();
   CMRequire(!running||(VMManager::GetState()==VMState::Paused&&VMManager::GetDiscSerial()==serial),"Bitte das zugehoerige Spiel zuerst pausieren.");
   const QString root=QString::fromStdString(EmuFolders::Textures)+"/"+QString::fromStdString(serial);
   const QString n=QString::fromStdString(name),target=root+"/replacements/"+n;
   auto backup=[&](const QString& file){if(QFileInfo::exists(file))CMCopyFile(file,root+"/replacement-backups/"+n+"/"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+".png");};
   if(mode==0||mode==1||mode==2||mode==6){
    CMRequire(n==QFileInfo(n).fileName()&&n.endsWith(".png",Qt::CaseInsensitive)&&QFileInfo::exists(root+"/dumps/"+n),"Bitte einen vorhandenen PNG-Dump auswaehlen.");
    if(mode==0){
     QImageReader reader(QString::fromStdString(source));QSize size=reader.size();CMRequire(size.isValid()&&size.width()<=16384&&size.height()<=16384&&qint64(size.width())*size.height()<=67108864,"Bildformat oder Bildgroesse nicht unterstuetzt.");
     QImage image=reader.read();CMRequire(!image.isNull(),"Das Ersatzbild kann nicht gelesen werden.");backup(target);QDir().mkpath(QFileInfo(target).absolutePath());QSaveFile out(target);
     CMRequire(out.open(QIODevice::WriteOnly)&&image.save(&out,"PNG")&&out.commit(),"Ersatzbild konnte nicht sicher gespeichert werden.");
    }else if(mode==1){CMRequire(QFileInfo::exists(target),"Kein aktiver Ersatz vorhanden.");backup(target+".disabled");CMCopyFile(target,target+".disabled");CMRequire(QFile::remove(target),"Ersatz konnte nicht deaktiviert werden.");}
    else if(mode==2){CMRequire(QFileInfo::exists(target+".disabled"),"Kein deaktivierter Ersatz vorhanden.");backup(target);CMCopyFile(target+".disabled",target);QFile::remove(target+".disabled");}
    else{
     QDir dir(root+"/replacement-backups/"+n);auto old=dir.entryList({"*.png"},QDir::Files,QDir::Name);CMRequire(!old.isEmpty(),"Noch keine Sicherung fuer diese Ersatztextur vorhanden.");QString previous=dir.filePath(old.last());backup(target);CMCopyFile(previous,target);
    }
   }
   const std::string settings=VMManager::GetGameSettingsPath(serial,crc);INISettingsInterface ini(settings);ini.Load();
   if(mode==3||mode==4)ini.SetBoolValue("EmuCore/GS","DumpReplaceableTextures",mode==3);
   if(mode==0||mode==2||mode==5||mode==6)ini.SetBoolValue("EmuCore/GS","LoadTextureReplacements",true);
   CMRequire(ini.Save(),"Textur-Einstellungen konnten nicht gespeichert werden.");
   if(running){VMManager::ReloadGameSettings();VMManager::ApplySettings();MTGS::RunOnGSThread([](){if(g_gs_renderer){GSTextureReplacements::ReloadReplacementMap();g_gs_renderer->PurgeTextureCache(true,false,true);}});}
   if(!name.empty()){
    const auto path=CMPath(target);MTGS::RunOnGSThread([path](){ImGuiFullscreen::InvalidateCachedTexture(path);ImGuiFullscreen::InvalidateCachedTexture(path+".disabled");});
   }
   CMStatus(mode==3?"Textur-Dumping aktiviert. Im Hardware-Renderer weiter spielen, um neue Texturen zu erfassen.":mode==4?"Textur-Dumping ausgeschaltet.":"Textur-Aenderung gespeichert. Original-Dump bleibt erhalten; Austauschdateien wurden neu geladen.");

}
void Host::RequestClassicMiniTexture(std::string serial,u32 crc,std::string name,std::string source,int mode)
{
 if(s_cm_busy.exchange(true))return;
 Host::RunOnCPUThread([serial=std::move(serial),crc,name=std::move(name),source=std::move(source),mode](){
  try{CMTextureChange(serial,crc,name,source,mode);}
  catch(const std::exception& error){CMStatus("Textur-Aenderung nicht ausgefuehrt: "+QString::fromUtf8(error.what()));}
  s_cm_busy.store(false);
 });
}

#ifdef CLASSIC_MINI_DATA_SELFTEST
void Host::RunClassicMiniDataSelfTest()
{
 Host::RunOnCPUThread([](){
  const QString root=QDir::fromNativeSeparators(QDir(CMRoot()).canonicalPath());
  if(!root.endsWith("/tests/backend-validation/PCSX2")){CMStatus("Self-test refused: data directory is not the isolated test fixture.");QMetaObject::invokeMethod(QCoreApplication::instance(),&QCoreApplication::quit,Qt::QueuedConnection);return;}
  try{
   CMRequire(!VMManager::HasValidVM(),"Test requires a stopped VM.");
   const QString originalCards=QString::fromStdString(EmuFolders::MemoryCards);
   const QString originalHash=CMHash(originalCards+"/Fixture.ps2");
   CMSwitchProfile("","Integrationstest");
   const QString profile=QString::fromStdString(Host::GetBaseStringSettingValue("ClassicMini","ActiveProfile","default"));
   CMRequire(profile!="default"&&QFileInfo::exists(CMProfileRoot(profile)+"/memcards/Mcd001.ps2"),"Profile/card creation failed.");
   const QString card=QString::fromStdString(EmuFolders::MemoryCards)+"/Mcd001.ps2";
   const QString hash=CMHash(card);const QString snapshot=CMBackup("Integrationstest");
   QSaveFile changed(card);CMRequire(changed.open(QIODevice::WriteOnly),"Cannot mutate test card.");changed.write("test-mutation");CMRequire(changed.commit(),"Cannot commit test mutation.");
   CMRestore(snapshot);CMRequire(CMHash(card)==hash,"Card restore hash mismatch.");
   CMSwitchProfile("default","");CMRequire(QString::fromStdString(EmuFolders::MemoryCards)==originalCards,"Default profile did not restore its card directory.");
   CMRequire(CMHash(originalCards+"/Fixture.ps2")==originalHash,"Original fixture card changed during profile/restore test.");
   CMConvertCard("Fixture.ps2");CMRequire(QFileInfo::exists(originalCards+"/Fixture_Ordner"),"Folder card conversion failed.");
   CMRequire(CMHash(originalCards+"/Fixture.ps2")==originalHash,"Conversion modified original card.");
   const QString texroot=QString::fromStdString(EmuFolders::Textures)+"/CLASSIC-TEST";
   QDir().mkpath(texroot+"/dumps");QImage original(32,32,QImage::Format_RGBA8888);original.fill(Qt::blue);CMRequire(original.save(texroot+"/dumps/fixture.png"),"Cannot create texture fixture.");
   const QString dumpHash=CMHash(texroot+"/dumps/fixture.png");QImage replacement(128,128,QImage::Format_RGBA8888);replacement.fill(Qt::red);replacement.save(CMRoot()+"/replacement.png");
   CMTextureChange("CLASSIC-TEST",1,"fixture.png",CMPath(CMRoot()+"/replacement.png"),0);const QString replacementHash=CMHash(texroot+"/replacements/fixture.png");
   CMTextureChange("CLASSIC-TEST",1,"fixture.png","",1);CMRequire(!QFileInfo::exists(texroot+"/replacements/fixture.png")&&QFileInfo::exists(texroot+"/replacements/fixture.png.disabled"),"Texture disable failed.");
   CMTextureChange("CLASSIC-TEST",1,"fixture.png","",2);CMRequire(CMHash(texroot+"/replacements/fixture.png")==replacementHash,"Texture enable failed.");
   replacement.fill(Qt::green);replacement.save(CMRoot()+"/replacement.png");CMTextureChange("CLASSIC-TEST",1,"fixture.png",CMPath(CMRoot()+"/replacement.png"),0);
   CMTextureChange("CLASSIC-TEST",1,"fixture.png","",6);CMRequire(CMHash(texroot+"/replacements/fixture.png")==replacementHash,"Texture backup restore failed.");
   CMRequire(CMHash(texroot+"/dumps/fixture.png")==dumpHash,"Original texture dump changed.");
   QSaveFile result(CMRoot()+"/SELFTEST-PASS.txt");CMRequire(result.open(QIODevice::WriteOnly),"Cannot save test result.");result.write("PASS: native profile creation, separate cards, backup, hash-verified restore, return to default and non-destructive card conversion, texture import/disable/enable/backup-restore with original dump preserved.\n");result.commit();
  }catch(const std::exception& e){QSaveFile result(CMRoot()+"/SELFTEST-FAIL.txt");if(result.open(QIODevice::WriteOnly)){result.write(e.what());result.commit();}}
  QMetaObject::invokeMethod(QCoreApplication::instance(),&QCoreApplication::quit,Qt::QueuedConnection);
 });
}
#endif
