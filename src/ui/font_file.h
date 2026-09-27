// license:BSD-3-Clause
//
// The panel's CJK TrueType file, found the way each platform finds one, never
// a hard-coded path. An empty result means "none here" and the caller falls
// back to ImGui's embedded font (English only).
//
// Split out of imgui_shell.h because panel.cpp needs the path too (it
// re-rasterizes at the sizes the LCD asks for, see panel.cpp) and must not
// drag the renderer backends in with it: panel.cpp only ever draws.

#ifndef S_MU2000_UI_FONT_FILE_H
#define S_MU2000_UI_FONT_FILE_H

#pragma once

#include <cstdio>
#include <string>
#include <vector>

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

inline std::string cjk_font_file()
{
	static const char *const NAMES[] = {
		"C:\\Windows\\Fonts\\YuGothM.ttc",
		"C:\\Windows\\Fonts\\meiryo.ttc",
		"C:\\Windows\\Fonts\\msgothic.ttc",
	};
	for (const char *path : NAMES)
		if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
			return path;
	return {};
}

#elif defined(__APPLE__)

#include <CoreText/CoreText.h>

// The family-name -> file-path walk CoreText does for us
inline std::string cjk_font_file()
{
	static const char *const NAMES[] = {
		"Hiragino Sans",
		"Hiragino Kaku Gothic ProN",
		"Hiragino Kaku Gothic Pro",
		"Hiragino Sans GB",
		"Osaka",
	};
	for (const char *name : NAMES) {
		CFStringRef family = CFStringCreateWithCString(nullptr, name, kCFStringEncodingUTF8);
		if (!family)
			continue;
		const void *keys[]   = { kCTFontFamilyNameAttribute };
		const void *values[] = { family };
		CFDictionaryRef attrs = CFDictionaryCreate(nullptr, keys, values, 1,
		                                           &kCFTypeDictionaryKeyCallBacks,
		                                           &kCFTypeDictionaryValueCallBacks);
		CFRelease(family);
		if (!attrs)
			continue;
		CTFontDescriptorRef desc = CTFontDescriptorCreateWithAttributes(attrs);
		CFRelease(attrs);
		if (!desc)
			continue;
		CFURLRef url = (CFURLRef)CTFontDescriptorCopyAttribute(desc, kCTFontURLAttribute);
		CFRelease(desc);
		if (!url)
			continue;
		char buf[1024] = {};
		std::string path;
		if (CFURLGetFileSystemRepresentation(url, true, (UInt8 *)buf, sizeof(buf)))
			path = buf;
		CFRelease(url);
		if (!path.empty())
			return path;
	}
	return {};
}

#else

#include <fontconfig/fontconfig.h>

inline std::string cjk_font_file()
{
	FcPattern *pat = FcPatternCreate();
	if (!pat)
		return {};
	std::string path;
	FcPatternAddString(pat, FC_FAMILY,
	                   reinterpret_cast<const FcChar8 *>("Noto Sans CJK JP"));
	FcPatternAddDouble(pat, FC_SIZE, 16.0);
	FcConfigSubstitute(nullptr, pat, FcMatchPattern);
	FcDefaultSubstitute(pat);
	FcResult res = FcResultNoMatch;
	FcPattern *m = FcFontMatch(nullptr, pat, &res);
	if (m) {
		FcChar8 *file = nullptr;
		if (FcPatternGetString(m, FC_FILE, 0, &file) == FcResultMatch && file)
			path = reinterpret_cast<const char *>(file);
		FcPatternDestroy(m);
	}
	FcPatternDestroy(pat);
	return path;
}

#endif

// The same file, read into memory once. ImFontAtlas::AddFontFromFileTTF reads
// it from disk on every call and has no cache, and the panel re-rasterizes
// its sizes whenever the window changes size -- a CJK face is megabytes, so
// that would be megabytes per size per resize. Glyph rasterization itself is
// lazy in 1.92, so only the glyphs actually drawn cost anything.
//
// The buffer lives to process exit and the atlas never frees it
// (FontDataOwnedByAtlas = false below), which is what the lazy bakes need.
inline const void *cjk_font_data(size_t &bytes)
{
	static std::vector<unsigned char> data;
	static const std::string path = cjk_font_file();
	if (data.empty() && !path.empty()) {
		FILE *f = std::fopen(path.c_str(), "rb");
		if (f) {
			unsigned char buf[65536];
			size_t n;
			while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
				data.insert(data.end(), buf, buf + n);
			std::fclose(f);
		}
	}
	bytes = data.size();
	return data.empty() ? nullptr : data.data();
}

#endif // S_MU2000_UI_FONT_FILE_H
