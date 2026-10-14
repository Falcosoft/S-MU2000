// license:BSD-3-Clause
//
// MIDI プレイヤーの曲の並びを、ファイルに書いて読み戻す（issue #211）。形は M3U（ほかのプレイヤーも読める、
// 1 行 1 ファイルのテキスト）。UTF-8 で、頭に "#EXTM3U"。# で始まる行は飛ばす。相対の道はリストのファイルの
// 場所から読む。gui は設定のフォルダーの playlist.m3u に、一覧が変わるたびに書き、起動のときに読み戻す

#ifndef S_MU2000_UI_PLAYLIST_H
#define S_MU2000_UI_PLAYLIST_H

#pragma once

#include "../compat/paths.h"

#include <cstdio>
#include <string>
#include <vector>

namespace ui::playlist {

// 設定のフォルダーの playlist.m3u（作れなければ空）
inline std::string default_path()
{
	const std::string base = smu2000::ensure_config_dir();
	return base.empty() ? std::string() : smu2000::join(base, "playlist.m3u");
}

// .m3u / .m3u8 か（大文字小文字を問わない）
inline bool is_playlist(const std::string &path)
{
	const auto ends = [&](const char *ext) {
		const size_t n = std::char_traits<char>::length(ext);
		if (path.size() < n)
			return false;
		for (size_t i = 0; i < n; i++) {
			char c = path[path.size() - n + i];
			if (c >= 'A' && c <= 'Z')
				c = char(c - 'A' + 'a');
			if (c != ext[i])
				return false;
		}
		return true;
	};
	return ends(".m3u") || ends(".m3u8");
}

// 読む。並んでいる道（読めなければ空）
inline std::vector<std::string> read(const std::string &path)
{
	std::vector<std::string> out;
	std::FILE *f = std::fopen(path.c_str(), "rb");
	if (!f)
		return out;
	std::string text;
	char buf[8192];
	size_t n;
	while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0 && text.size() < (8u << 20))
		text.append(buf, n);
	std::fclose(f);
	if (text.size() >= 3 && u8(text[0]) == 0xef && u8(text[1]) == 0xbb && u8(text[2]) == 0xbf)
		text.erase(0, 3);                       // UTF-8 の BOM
	const size_t slash = path.find_last_of("/\\");
	const std::string dir = slash == std::string::npos ? std::string() : path.substr(0, slash);
	size_t at = 0;
	while (at < text.size()) {
		size_t end = text.find('\n', at);
		if (end == std::string::npos)
			end = text.size();
		std::string line = text.substr(at, end - at);
		at = end + 1;
		while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
			line.pop_back();
		size_t lead = 0;
		while (lead < line.size() && (line[lead] == ' ' || line[lead] == '\t'))
			lead++;
		line.erase(0, lead);
		if (line.empty() || line[0] == '#')
			continue;
		// 絶対の道（/…、C:\…、\\…）でなければ、リストの場所から
		const bool absolute = line[0] == '/' || line[0] == '\\' || (line.size() > 2 && line[1] == ':');
		out.push_back(absolute || dir.empty() ? line : smu2000::join(dir, line));
	}
	return out;
}

// 書く。うまくいけば true
inline bool write(const std::string &path, const std::vector<std::string> &paths)
{
	std::string text = "#EXTM3U\n";
	for (const std::string &p : paths)
		if (!p.empty())
			text += p + "\n";
	const std::string tmp = path + ".tmp";
	std::FILE *f = std::fopen(tmp.c_str(), "wb");
	if (!f)
		return false;
	const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
	if (std::fclose(f) != 0 || !ok) {
		std::remove(tmp.c_str());
		return false;
	}
	std::remove(path.c_str());
	return std::rename(tmp.c_str(), path.c_str()) == 0;
}

} // namespace ui::playlist

#endif // S_MU2000_UI_PLAYLIST_H
