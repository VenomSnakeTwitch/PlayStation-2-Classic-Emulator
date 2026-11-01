// SPDX-License-Identifier: GPL-3.0+
// Serial-only lookups. Existing user covers always win. All state is owned by the GS thread.
#include "common/HTTPDownloader.h"
#include "common/Image.h"
#include <unordered_map>

class ClassicCoverProgress final : public BaseProgressCallback
{
public:
	bool cancelled = false;
	bool IsCancelled() const override { return cancelled; }
	void SetTitle(const char*) override {}
	void DisplayError(const char*) override {}
	void DisplayWarning(const char*) override {}
	void DisplayInformation(const char*) override {}
	void DisplayDebugMessage(const char*) override {}
	void ModalError(const char*) override {}
	bool ModalConfirmation(const char*) override { return false; }
	void ModalInformation(const char*) override {}
};

static std::unique_ptr<HTTPDownloader> s_classic_cover_http;
static ClassicCoverProgress s_classic_cover_progress;
static std::unordered_map<std::string, double> s_classic_cover_retry;
static double s_classic_cover_next_scan = 0;
static u32 s_classic_cover_pending = 0;
static std::string s_classic_cover_status = "Fehlende Cover werden automatisch anhand der Seriennummer geladen.";

static void ClassicStopAutomaticCovers()
{
	if (s_classic_cover_http)
	{
		s_classic_cover_progress.cancelled = true;
		s_classic_cover_http->WaitForAllRequests();
		s_classic_cover_http.reset();
	}
	s_classic_cover_pending = 0;
	s_classic_cover_next_scan = 0;
	s_classic_cover_progress.cancelled = false;
}

static void ClassicPollAutomaticCovers()
{
	if (s_classic_cover_http)
		s_classic_cover_http->PollRequests();
	if (!Host::GetBaseBoolSettingValue("ClassicMini", "AutoCovers", true))
	{
		if (s_classic_cover_http)
			ClassicStopAutomaticCovers();
		return;
	}
	const double now = ImGui::GetTime();
	if (now < s_classic_cover_next_scan || s_classic_cover_pending || VMManager::HasValidVM())
		return;
	s_classic_cover_next_scan = now + 5.0;
	struct CoverRequest
	{
		std::string game_path;
		std::string serial;
	};
	std::vector<CoverRequest> requests;
	{
		auto lock = GameList::GetLock();
		for (u32 i = 0; i < GameList::GetEntryCount() && requests.size() < 16; i++)
		{
			const GameList::Entry* entry = GameList::GetEntryByIndex(i);
			if (entry->serial.empty() || !GameList::GetCoverImagePathForEntry(entry).empty())
				continue;
			// Do not send a title or filesystem path to the cover provider.
			if (entry->serial.size() != 10 || entry->serial[4] != '-' ||
				!std::all_of(entry->serial.begin(), entry->serial.begin() + 4, [](char c) { return c >= 'A' && c <= 'Z'; }) ||
				!std::all_of(entry->serial.begin() + 5, entry->serial.end(), [](char c) { return c >= '0' && c <= '9'; }))
				continue;
			if (const auto it = s_classic_cover_retry.find(entry->serial); it != s_classic_cover_retry.end() && it->second > now)
				continue;
			s_classic_cover_retry[entry->serial] = now + 300.0;
			requests.push_back({entry->path, entry->serial});
		}
	}
	if (requests.empty())
		return;
	if (!s_classic_cover_http)
	{
		s_classic_cover_http = HTTPDownloader::Create(Host::GetHTTPUserAgent());
		if (!s_classic_cover_http)
		{
			s_classic_cover_status = "Cover-Quelle momentan nicht erreichbar. Erneuter Versuch spaeter.";
			return;
		}
		s_classic_cover_http->SetTimeout(12.0f);
		s_classic_cover_http->SetMaxActiveRequests(2);
	}
	for (CoverRequest& request : requests)
	{
		const std::string url = "https://raw.githubusercontent.com/xlenore/ps2-covers/main/covers/default/" + request.serial + ".jpg";
		s_classic_cover_pending++;
		s_classic_cover_status = fmt::format("Cover wird gesucht: {}", request.serial);
		s_classic_cover_http->CreateRequest(url, [game_path = std::move(request.game_path), serial = std::move(request.serial)](s32 status, const std::string&, HTTPDownloader::Request::Data data) {
				s_classic_cover_pending--;
				if (status == HTTPDownloader::HTTP_STATUS_CANCELLED)
					return;
				if (status != HTTPDownloader::HTTP_STATUS_OK || data.empty() || data.size() > 8 * 1024 * 1024)
				{
					s_classic_cover_retry[serial] = ImGui::GetTime() + (status == 404 ? 86400.0 : 300.0);
					s_classic_cover_status = status == 404 ? fmt::format("Kein Cover fuer {} in der Quelle gefunden.", serial) : "Offline oder Download fehlgeschlagen; spaeterer Wiederholungsversuch.";
					return;
				}
				RGBA8Image image;
				if (!image.LoadFromBuffer("cover.jpg", data.data(), data.size()) || image.GetWidth() > 4096 || image.GetHeight() > 4096)
				{
					s_classic_cover_status = fmt::format("Ungueltige Cover-Datei fuer {} verworfen.", serial);
					return;
				}
				std::string destination;
				{
					auto lock = GameList::GetLock();
					const GameList::Entry* entry = GameList::GetEntryForPath(game_path.c_str());
					if (!entry || entry->serial != serial || !GameList::GetCoverImagePathForEntry(entry).empty())
						return;
					destination = GameList::GetNewCoverImagePathForEntry(entry, "cover.jpg", true);
				}
				FileSystem::CreateDirectoryPath(EmuFolders::Covers.c_str(), false);
				const std::string temporary = destination + ".classic-download";
				if (FileSystem::WriteBinaryFile(temporary.c_str(), data.data(), data.size()) && FileSystem::RenamePath(temporary.c_str(), destination.c_str()))
				{
					FullscreenUI::s_cover_image_map.erase(game_path);
					s_classic_cover_retry[serial] = ImGui::GetTime() + 86400.0;
					s_classic_cover_status = fmt::format("Cover geladen: {}", serial);
					Console.WriteLnFmt("Classic Mini: Auto-Cover {} gespeichert.", serial);
				}
				else
					s_classic_cover_status = "Cover konnte nicht gespeichert werden. Cover-Ordner pruefen."; }, &s_classic_cover_progress);
	}
}
