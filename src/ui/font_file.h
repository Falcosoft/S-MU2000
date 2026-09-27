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

#include "imgui.h"   // add_cjk_font() hands back an ImFont*

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

// GDI loaded the bold face of a family from the file beside the regular one
// (YuGothMB.ttc and friends), which is what FW_BOLD was getting.
inline std::string cjk_bold_font_file()
{
	static const char *const NAMES[] = {
		"C:\\Windows\\Fonts\\YuGothMB.ttc",
		"C:\\Windows\\Fonts\\meiryobd.ttc",
		"C:\\Windows\\Fonts\\msgothicbd.ttc",
	};
	const std::string regular = cjk_font_file();
	for (const char *path : NAMES) {
		if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
			continue;
		if (regular == path)
			continue;               // same file: that is not a bold face
		return path;
	}
	return {};
}

#elif defined(__APPLE__)

#include <CoreText/CoreText.h>

// The family-name -> file-path walk CoreText does for us
inline std::string cjk_family_path(const char *family)
{
	CFStringRef cf = CFStringCreateWithCString(nullptr, family, kCFStringEncodingUTF8);
	if (!cf)
		return {};
	const void *keys[]   = { kCTFontFamilyNameAttribute };
	const void *values[] = { cf };
	CFDictionaryRef attrs = CFDictionaryCreate(nullptr, keys, values, 1,
	                                           &kCFTypeDictionaryKeyCallBacks,
	                                           &kCFTypeDictionaryValueCallBacks);
	CFRelease(cf);
	if (!attrs)
		return {};
	CTFontDescriptorRef desc = CTFontDescriptorCreateWithAttributes(attrs);
	CFRelease(attrs);
	if (!desc)
		return {};
	CFURLRef url = (CFURLRef)CTFontDescriptorCopyAttribute(desc, kCTFontURLAttribute);
	CFRelease(desc);
	if (!url)
		return {};
	char buf[1024] = {};
	std::string path;
	if (CFURLGetFileSystemRepresentation(url, true, (UInt8 *)buf, sizeof(buf)))
		path = buf;
	CFRelease(url);
	return path;
}

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
		if (std::string path = cjk_family_path(name); !path.empty())
			return path;
	}
	return {};
}

// **The weight goes in the family name here, not in a weight attribute.** A
// descriptor can be asked for kCTFontWeightTrait 600, but CoreText does not
// resolve that to a different face: it hands back HiraginoSans-W3 for 400, 600
// and 700 alike. Nor can the answer be checked by reading the weight back --
// the descriptor echoes the weight that was asked for, so a family with no bold
// at all (Osaka) also reports 600. The file is the only trustworthy answer, so
// this asks for the family at its bold name and keeps the result only if it is
// a *different file* from the regular face. That test earns its keep: "Hiragino
// Sans GB" has no W6, and asking for one returns the very same file, which
// without the comparison would look like a bold face and quietly draw regular.
inline std::string cjk_bold_font_file()
{
	static const char *const NAMES[] = {
		"Hiragino Sans",
		"Hiragino Kaku Gothic ProN",
		"Hiragino Kaku Gothic Pro",
		"Hiragino Sans GB",
	};
	const std::string regular = cjk_font_file();
	for (const char *name : NAMES) {
		const std::string bold = cjk_family_path((std::string(name) + " W6").c_str());
		if (bold.empty() || bold == regular)
			continue;
		return bold;
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

// Noto Sans CJK ships the weights as separate files, so asking fontconfig for a
// bold weight is enough to land on a different one.
inline std::string cjk_bold_font_file()
{
	FcPattern *pat = FcPatternCreate();
	if (!pat)
		return {};
	std::string path;
	FcPatternAddString(pat, FC_FAMILY,
	                   reinterpret_cast<const FcChar8 *>("Noto Sans CJK JP"));
	FcPatternAddDouble(pat, FC_SIZE, 16.0);
	FcPatternAddInteger(pat, FC_WEIGHT, FC_WEIGHT_BOLD);
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
	if (path.empty() || path == cjk_font_file())
		return {};                   // same file: that is not a bold face
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

// The bold face, read into memory once, the same way. Returns nullptr when this
// machine has no bold to be had, and the caller then draws the regular face --
// so a machine without one looks exactly as it does now rather than worse.
inline const void *cjk_bold_font_data(size_t &bytes)
{
	static std::vector<unsigned char> data;
	static const std::string path = cjk_bold_font_file();
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

// Put one CJK face into an atlas at a given size, and return the font ImGui
// will draw with -- or ImGui's built-in, if this machine has no Japanese font
// to be found. The buffer stays ours (FontDataOwnedByAtlas = false) because it
// outlives every atlas in the process, which the lazy bakes need.
//
// **One font setup for the whole program.** The panel (build_fonts), the
// window's own pieces (imshell::panel_fonts) and the five PC editor windows all
// come through here. The editor windows each used to carry their own table of
// font names, and having three copies of that list is how the Linux one came to
// be the only window in the light ImGui style: three copies drift, and nothing
// says they should not.
inline ImFont *add_cjk_font(ImFontAtlas *atlas, float px = 16.0f)
{
	size_t bytes = 0;
	if (const void *data = cjk_font_data(bytes)) {
		ImFontConfig cfg;
		cfg.FontDataOwnedByAtlas = false;
		if (ImFont *font = atlas->AddFontFromMemoryTTF(
		        const_cast<void *>(data), int(bytes), px, &cfg))
			return font;
	}
	return atlas->AddFontDefault();
}

#endif // S_MU2000_UI_FONT_FILE_H
