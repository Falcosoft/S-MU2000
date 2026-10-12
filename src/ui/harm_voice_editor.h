// license:BSD-3-Clause
//
// ハーモニックボードの音色エディタ（音色の窓で、ハーモニックボードのチャンネルを出しているときの右の面）。
// そのチャンネルでいま選んでいるプログラムの音色（src/vboard_harm.h の harm_voice）を触る:
//   ・倍音 64 本の棒を、マウスでなぞって描く。4 枚（鳴り始め・山・伸ばしている間・離したあと）を切り替える
//   ・のこぎり・矩形・三角・オルガンの形で埋める、4 枚とも同じにする
//   ・枚の間を移る時間、音量の包絡線、明るさ、ずれ、奇数・偶数、雑音の帯、鳴り始めの落ち幅、CC71 で行き来する相手
//   ・できあがる 1 周期の形
//   ・WAV を 1 つ聞かせて、その倍音を測って音色にする（src/harm_analyze.h）
// 触った値はすぐ音源へ渡る（鳴っている音にも効く）。組（128 個の音色）は harm_banks.h が設定のフォルダーに保存する。
// 何も開いていない状態で触ると、初期の音色の写しから新しい組を作る

#ifndef S_MU2000_UI_HARM_VOICE_EDITOR_H
#define S_MU2000_UI_HARM_VOICE_EDITOR_H

#pragma once

#include "harm_banks.h"
#include "xg_ui.h"
#include "ui/texts.h"
#include "../harm_analyze.h"
#include "../wav_in.h"

#include "imgui.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace ui {

class harm_voice_editor
{
public:
	// program はそのチャンネルでいま選んでいるプログラム（0-127）
	void draw(int program, bridge &br)
	{
		namespace hb = harm_banks;
		namespace vb = smu2000::vboard;
		const float fs = ImGui::GetFontSize();
		ImDrawList *dl = ImGui::GetWindowDrawList();
		program &= 127;

		const double now = ImGui::GetTime();
		if (m_listed < 0 || now - m_listed > 1.0) {
			m_listed = now;
			m_list = hb::list();
		}
		std::shared_ptr<const hb::bank> cur = hb::current();
		hb::voice v = cur ? cur->prog[size_t(program)] : vb::harm_default_voice(program);
		bool changed = false;

		// ---- 組（128 個の音色のまとまり）
		{
			const std::string stem = hb::current_stem();
			ImGui::SetNextItemWidth(fs * 9);
			if (ImGui::BeginCombo("##hmset", cur ? cur->name : UI_TEXT(fme_default_set, "(initial voices)"))) {
				if (ImGui::Selectable(UI_TEXT(fme_default_set, "(initial voices)"), !cur) && cur) {
					hb::close(br);
					m_note.clear();
				}
				for (const std::string &s : m_list)
					if (ImGui::Selectable(s.c_str(), s == stem) && s != stem) {
						std::string err;
						m_note = hb::open(br, s, err) ? std::string() : err;
					}
				ImGui::EndCombo();
			}
			if (ImGui::IsItemHovered())
				xgui::hint("%s", UI_TEXT(hme_set_tip, "Voice set\nThe 128 voices of the harmonic board, kept as one file in the \"harmsets\" folder of the settings folder. \"(initial voices)\" is the built-in set; touching anything there starts a new set from a copy of it. Changes are saved as you go."));
			ImGui::SameLine();
			if (ImGui::SmallButton(UI_TEXT(fme_new_set, "New set"))) {
				hb::create(br);
				m_listed = -1;
				m_note.clear();
			}
			cur = hb::current();
			if (cur) {
				// 組の名前。変えるとファイルの名前も変わる
				const std::string path = hb::current_path();
				if (m_name_for != path && !ImGui::IsAnyItemActive()) {
					m_name_for = path;
					std::snprintf(m_set_name, sizeof(m_set_name), "%s", cur->name);
				}
				ImGui::SameLine();
				ImGui::SetNextItemWidth(fs * 8);
				ImGui::InputText("##hmsetname", m_set_name, sizeof(m_set_name));
				if (ImGui::IsItemDeactivatedAfterEdit()) {
					char clean[15];
					vb::harm_detail::clean_text(clean, reinterpret_cast<const u8 *>(m_set_name), 14, false);
					for (size_t n = std::strlen(clean); n && clean[n - 1] == ' '; n--)
						clean[n - 1] = 0;
					const std::string to = hb::file_for(clean);
					if (!clean[0] || (to != path && smu2000::is_file(to))) {
						m_note = UI_TEXT(fme_name_taken, "That name is empty or already used by another set");
					} else {
						m_note.clear();
						auto nb = std::make_shared<hb::bank>(*cur);
						std::snprintf(nb->name, sizeof(nb->name), "%s", clean);
						hb::commit(br, nb);
						cur = nb;
					}
					m_name_for.clear();
				}
			}
		}

		// ---- この音色: 番号、名前、初期に戻す、写す・貼る
		ImGui::SameLine(0, fs * 1.5f);
		ImGui::AlignTextToFramePadding();
		ImGui::Text(UI_TEXT(fme_program_fmt, "Program %d"), program + 1);
		ImGui::SameLine();
		{
			char name[9];
			std::snprintf(name, sizeof(name), "%s", v.name);
			for (size_t n = std::strlen(name); n && name[n - 1] == ' '; n--)
				name[n - 1] = 0;
			ImGui::SetNextItemWidth(fs * 6);
			if (ImGui::InputText("##hmname", name, sizeof(name))) {
				vb::harm_detail::clean_text(v.name, reinterpret_cast<const u8 *>(name), 8, true);
				changed = true;
			}
		}
		ImGui::SameLine();
		if (ImGui::SmallButton(UI_TEXT(fme_reset_voice, "Initial voice"))) {
			v = vb::harm_default_voice(program);
			changed = true;
		}
		if (ImGui::IsItemHovered())
			xgui::hint("%s", UI_TEXT(fme_reset_tip, "Initial voice\nPuts the built-in voice of this program number back."));
		ImGui::SameLine();
		if (ImGui::SmallButton(UI_TEXT(fme_copy, "Copy"))) {
			m_clip = v;
			m_has_clip = true;
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!m_has_clip);
		if (ImGui::SmallButton(UI_TEXT(fme_paste, "Paste"))) {
			v = m_clip;
			changed = true;
		}
		ImGui::EndDisabled();

		// ---- どの枚を描くか、形で埋める
		const char *const frame_names[vb::HARM_FRAMES] = {
			UI_TEXT(hme_frame_0, "1 Onset"), UI_TEXT(hme_frame_1, "2 Peak"), UI_TEXT(hme_frame_2, "3 Held"), UI_TEXT(hme_frame_3, "4 Released"),
		};
		for (int f = 0; f < vb::HARM_FRAMES; f++) {
			if (f)
				ImGui::SameLine();
			if (ImGui::RadioButton(frame_names[f], m_frame == f))
				m_frame = f;
			if (ImGui::IsItemHovered())
				xgui::hint("%s", UI_TEXT(hme_frame_tip, "The four spectra of a voice\nA voice has four pictures of its partials. A note starts at 1, moves to 2 and then to 3 while the key is held, and goes to 4 after the key is released. Pick the one to draw below."));
		}
		ImGui::SameLine(0, fs * 1.2f);
		const auto fill = [&](int kind) {
			for (int n = 1; n <= vb::HARM_PARTIALS; n++) {
				float a = 0.0f;
				switch (kind) {
				case 0: a = 1.0f / float(n); break;
				case 1: a = (n & 1) ? 1.0f / float(n) : 0.0f; break;
				case 2: a = (n & 1) ? 1.0f / float(n * n) : 0.0f; break;
				case 3: a = n == 1 ? 1.0f : n == 2 ? 0.8f : n == 3 ? 0.6f : n == 4 ? 0.5f : n == 6 ? 0.3f : n == 8 ? 0.25f : 0.0f; break;
				default: break;
				}
				v.amp[m_frame][n - 1] = a;
			}
			changed = true;
		};
		if (ImGui::SmallButton(UI_TEXT(hme_shape_saw, "Saw")))       fill(0);
		ImGui::SameLine();
		if (ImGui::SmallButton(UI_TEXT(hme_shape_square, "Square"))) fill(1);
		ImGui::SameLine();
		if (ImGui::SmallButton(UI_TEXT(hme_shape_tri, "Triangle")))  fill(2);
		ImGui::SameLine();
		if (ImGui::SmallButton(UI_TEXT(hme_shape_organ, "Organ")))   fill(3);
		ImGui::SameLine();
		if (ImGui::SmallButton(UI_TEXT(hme_shape_clear, "Clear")))   fill(4);
		ImGui::SameLine(0, fs * 1.2f);
		if (ImGui::SmallButton(UI_TEXT(hme_to_all, "Same for all four"))) {
			for (int f = 0; f < vb::HARM_FRAMES; f++)
				if (f != m_frame)
					std::memcpy(v.amp[f], v.amp[m_frame], sizeof(v.amp[f]));
			changed = true;
		}
		if (ImGui::IsItemHovered())
			xgui::hint("%s", UI_TEXT(hme_to_all_tip, "Same for all four\nCopies the spectrum shown here to the other three, so the tone no longer changes while the note sounds."));

		// ---- 倍音の棒（左）と 1 周期の形（右）。棒の高さは dB（上が 0dB、下が -48dB。いちばん下まで下げると 0）
		{
			const float h = fs * 9.0f;
			const float wave_w = fs * 9.0f;
			const float w = std::max(fs * 16.0f, ImGui::GetContentRegionAvail().x - wave_w - fs * 0.5f);
			const ImVec2 p = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton("##hmbars", ImVec2(w, h));
			const bool hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive();
			const ImU32 back = IM_COL32(16, 20, 26, 255), line = IM_COL32(110, 200, 255, 255), ghost = IM_COL32(70, 100, 130, 255);
			dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), back, 4.0f);
			for (int db = 12; db < 48; db += 12)
				dl->AddLine(ImVec2(p.x, p.y + h * float(db) / 48.0f), ImVec2(p.x + w, p.y + h * float(db) / 48.0f), IM_COL32(40, 48, 60, 255));
			const float bw = w / float(vb::HARM_PARTIALS);
			const auto height = [](float a) { return a <= 0.0f ? 0.0f : std::clamp(1.0f + 20.0f * std::log10(a) / 48.0f, 0.02f, 1.0f); };
			const ImVec2 mouse = ImGui::GetIO().MousePos;
			if (act) {
				// なぞった所の棒を、マウスの高さにする。速く動かして棒を飛ばしたぶんは、前の所からまっすぐつなぐ
				const int bar = std::clamp(int((mouse.x - p.x) / bw), 0, vb::HARM_PARTIALS - 1);
				const float t = std::clamp(1.0f - (mouse.y - p.y) / h, 0.0f, 1.0f);
				const float a = t < 0.03f ? 0.0f : std::pow(10.0f, (t - 1.0f) * 48.0f / 20.0f);
				const int from = m_drag_bar < 0 ? bar : m_drag_bar;
				const float from_a = m_drag_bar < 0 ? a : m_drag_amp;
				const int lo = std::min(from, bar), hi = std::max(from, bar);
				for (int b = lo; b <= hi; b++) {
					const float x = hi == lo ? 1.0f : float(b - from) / float(bar - from);
					v.amp[m_frame][b] = from_a + (a - from_a) * x;
				}
				m_drag_bar = bar;
				m_drag_amp = a;
				changed = true;
			} else {
				m_drag_bar = -1;
			}
			// 薄い線: 1 つ前の枚（見比べ用）。棒: いまの枚
			const int prev = m_frame > 0 ? m_frame - 1 : -1;
			for (int n = 0; n < vb::HARM_PARTIALS; n++) {
				const float x0 = p.x + bw * float(n) + 0.5f, x1 = p.x + bw * float(n + 1) - 0.5f;
				const float t = height(v.amp[m_frame][n] * vb::harm_parity(v, n + 1));
				if (t > 0.0f)
					dl->AddRectFilled(ImVec2(x0, p.y + h * (1.0f - t)), ImVec2(x1, p.y + h), (n % 8) == 7 || n == 0 ? IM_COL32(150, 220, 255, 255) : line);
				if (prev >= 0) {
					const float g = height(v.amp[prev][n] * vb::harm_parity(v, n + 1));
					if (g > 0.0f)
						dl->AddLine(ImVec2(x0, p.y + h * (1.0f - g)), ImVec2(x1, p.y + h * (1.0f - g)), ghost, 1.5f);
				}
			}
			if (hov || act) {
				const int bar = std::clamp(int((mouse.x - p.x) / bw), 0, vb::HARM_PARTIALS - 1);
				xgui::hint(UI_TEXT(hme_bars_tip_fmt, "Partial %d of 64\nDrag across the bars to draw the spectrum: the height is the level of that partial (top = full, each line = 12 dB down, bottom = off). Partial 1 is the note's own pitch, 2 an octave above, 3 an octave and a fifth, and so on. The thin marks show the previous of the four spectra."), bar + 1);
			}
			ImGui::SameLine(0, fs * 0.5f);
			float cyc[256];
			vb::harm_cycle(v, m_frame, cyc, 256);
			const ImVec2 q = ImGui::GetCursorScreenPos();
			ImGui::Dummy(ImVec2(wave_w, h));
			dl->AddRectFilled(q, ImVec2(q.x + wave_w, q.y + h), back, 4.0f);
			dl->AddLine(ImVec2(q.x, q.y + h * 0.5f), ImVec2(q.x + wave_w, q.y + h * 0.5f), IM_COL32(80, 90, 110, 255));
			for (int i = 0; i + 1 < 256; i++)
				dl->AddLine(ImVec2(q.x + wave_w * float(i) / 255.0f, q.y + h * 0.5f * (1.0f - 0.9f * cyc[i])),
				            ImVec2(q.x + wave_w * float(i + 1) / 255.0f, q.y + h * 0.5f * (1.0f - 0.9f * cyc[i + 1])), line, 1.5f);
		}

		// ---- 値。3 列: 時間と音量 / 音の性格 / 雑音の帯
		if (ImGui::BeginTable("hmvals", 3, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::PushItemWidth(-fs * 7.5f);
			changed |= ImGui::DragFloat(UI_TEXT(hme_t_peak, "1 > 2 time"), &v.t_peak, 0.002f, 0.0f, 10.0f, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
			if (ImGui::IsItemHovered())
				xgui::hint("%s", UI_TEXT(hme_t_peak_tip, "1 > 2 time\nHow long the tone takes to move from spectrum 1 (onset) to spectrum 2 (peak). Short for a struck or plucked sound, longer for brass that swells."));
			changed |= ImGui::DragFloat(UI_TEXT(hme_t_sustain, "2 > 3 time"), &v.t_sustain, 0.01f, 0.0f, 20.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
			if (ImGui::IsItemHovered())
				xgui::hint("%s", UI_TEXT(hme_t_sustain_tip, "2 > 3 time\nHow long the tone takes to move from spectrum 2 (peak) to spectrum 3 (held). A piano loses its high partials over a second or two; a pad can take several."));
			changed |= ImGui::DragFloat(UI_TEXT(fme_attack, "Attack"), &v.attack, 0.002f, 0.0f, 5.0f, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
			changed |= ImGui::DragFloat(UI_TEXT(fme_decay, "Decay"), &v.decay, 0.01f, 0.0f, 20.0f, v.decay <= 0.0f ? "-" : "%.2f s", ImGuiSliderFlags_AlwaysClamp);
			changed |= ImGui::SliderFloat(UI_TEXT(fme_sustain, "Sustain"), &v.sustain, 0.0f, 1.0f, "%.2f");
			changed |= ImGui::DragFloat(UI_TEXT(fme_release, "Release"), &v.release, 0.005f, 0.0f, 10.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
			if (ImGui::IsItemHovered())
				xgui::hint("%s", UI_TEXT(hme_release_tip, "Release\nAfter the key is released, the time for the sound to die away. The tone moves to spectrum 4 over the same time."));
			ImGui::PopItemWidth();

			ImGui::TableNextColumn();
			ImGui::PushItemWidth(-fs * 10.5f);      // この列はラベルが長い
			changed |= ImGui::SliderFloat(UI_TEXT(hme_bright, "Touch brightness"), &v.bright, 0.0f, 1.0f, "%.2f");
			if (ImGui::IsItemHovered())
				xgui::hint("%s", UI_TEXT(hme_bright_tip, "Touch brightness\nHow much a soft key strike dulls the tone: the softer the note, the more the high partials are turned down. 0 gives the same tone at every strength. Controller 74 (Brightness) moves the same thing from a sequencer."));
			changed |= ImGui::SliderFloat(UI_TEXT(hme_inharm, "Stretch"), &v.inharm, 0.0f, 1.0f, "%.2f");
			if (ImGui::IsItemHovered())
				xgui::hint("%s", UI_TEXT(hme_inharm_tip, "Stretch\nPushes the partials above their exact multiples, the higher the more. 0 is an organ or a wind instrument; a little is a piano string; a lot is a bell or a metal bar."));
			changed |= ImGui::SliderFloat(UI_TEXT(hme_odd_even, "Even / odd"), &v.odd_even, -1.0f, 1.0f, "%+.2f");
			if (ImGui::IsItemHovered())
				xgui::hint("%s", UI_TEXT(hme_odd_even_tip, "Even / odd\nTo the left turns the even partials (2, 4, 6...) down: hollow, like a clarinet or a square wave. To the right turns the odd partials above the first down."));
			changed |= ImGui::SliderFloat(UI_TEXT(fme_drop, "Pitch drop (semitones)"), &v.drop, 0.0f, 36.0f, "%.0f");
			if (ImGui::IsItemHovered())
				xgui::hint("%s", UI_TEXT(fme_drop_tip, "Pitch drop\nThe note starts this many semitones high and falls to pitch in about 30 ms: the thump of a tom or a kick, or a zap."));
			{
				char label[40];
				const auto name_of = [&](int prog, char *out, size_t n) {
					const hb::voice &o = cur ? cur->prog[size_t(prog)] : vb::harm_default_voice(prog);
					std::snprintf(out, n, "%03d %s", prog + 1, o.name);
				};
				if (v.morph_to >= 0)
					name_of(v.morph_to, label, sizeof(label));
				else
					std::snprintf(label, sizeof(label), "%s", UI_TEXT(hme_morph_none, "(none)"));
				if (ImGui::BeginCombo(UI_TEXT(hme_morph, "Morph partner"), label)) {
					if (ImGui::Selectable(UI_TEXT(hme_morph_none, "(none)"), v.morph_to < 0)) {
						v.morph_to = -1;
						changed = true;
					}
					for (int i = 0; i < 128; i++) {
						char row[40];
						name_of(i, row, sizeof(row));
						if (ImGui::Selectable(row, v.morph_to == i)) {
							v.morph_to = i;
							changed = true;
						}
					}
					ImGui::EndCombo();
				}
				if (ImGui::IsItemHovered())
					xgui::hint("%s", UI_TEXT(hme_morph_tip, "Morph partner\nAnother program of this set. Controller 71 (Harmonic Content) then slides this voice's four spectra towards that program's: 0 is this voice, 127 is the partner's partials with this voice's timing and volume."));
			}
			ImGui::PopItemWidth();

			ImGui::TableNextColumn();
			ImGui::PushItemWidth(-fs * 10.5f);      // この列はラベルが長い
			changed |= ImGui::SliderFloat(UI_TEXT(hme_noise, "Noise band"), &v.noise, 0.0f, 1.0f, "%.2f");
			if (ImGui::IsItemHovered())
				xgui::hint("%s", UI_TEXT(hme_noise_tip, "Noise band\nA strip of filtered noise added to the partials: the breath of a flute, the bow on a string, the knock of a hammer. Sine partials alone cannot make these."));
			changed |= ImGui::DragFloat(UI_TEXT(hme_noise_center, "Band centre"), &v.noise_center, 0.05f, 0.25f, 64.0f, "x %.2f", ImGuiSliderFlags_AlwaysClamp);
			if (ImGui::IsItemHovered())
				xgui::hint("%s", UI_TEXT(hme_noise_center_tip, "Band centre\nWhere the noise sits, as a multiple of the note's pitch, so it follows the keys (8 = three octaves above the note)."));
			changed |= ImGui::SliderFloat(UI_TEXT(hme_noise_width, "Band width (octaves)"), &v.noise_width, 0.1f, 6.0f, "%.1f");
			changed |= ImGui::DragFloat(UI_TEXT(hme_noise_decay, "Noise fade-out"), &v.noise_decay, 0.002f, 0.0f, 10.0f, v.noise_decay <= 0.0f ? "-" : "%.3f s", ImGuiSliderFlags_AlwaysClamp);
			if (ImGui::IsItemHovered())
				xgui::hint("%s", UI_TEXT(hme_noise_decay_tip, "Noise fade-out\nThe time for the noise alone to die away after the note starts: a few milliseconds gives a click or a knock at the start. \"-\" keeps the noise for as long as the note sounds."));
			ImGui::PopItemWidth();
			ImGui::EndTable();
		}

		// ---- 聞かせて真似させる
		{
			std::vector<u8> opened;
			if (xgui::take_opened_wav_for(xgui::WAV_FOR_HARMONIC, opened))
				changed |= listen(opened, v);
			if (xgui::file_dialogs()) {
				if (ImGui::SmallButton(UI_TEXT(hme_listen, "Listen to a WAV...")))
					xgui::ask_open_wav_for(xgui::WAV_FOR_HARMONIC);
				if (ImGui::IsItemHovered())
					xgui::hint("%s", UI_TEXT(hme_listen_tip, "Listen to a WAV\nPick a recording of one note. Its pitch, the levels of its 64 partials at four moments, how far the partials are stretched and how its volume moves are measured, and this voice is replaced by the result. Only those numbers are kept, not the recording. It works for a single steady note; chords, noise and sliding pitches cannot be measured. Breath and knock noises are not copied (add a noise band by hand)."));
				ImGui::SameLine();
			}
			ImGui::SetNextItemWidth(fs * 16);
			ImGui::InputTextWithHint("##hmpath", UI_TEXT(smp_wav_path, "WAV file path"), m_path, sizeof(m_path));
			ImGui::SameLine();
			ImGui::BeginDisabled(!m_path[0]);
			if (ImGui::SmallButton(UI_TEXT(hme_listen_load, "Listen"))) {
				std::vector<u8> bytes;
				if (std::FILE *f = std::fopen(m_path, "rb")) {
					u8 buf[65536];
					size_t n;
					while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0 && bytes.size() < (64u << 20))
						bytes.insert(bytes.end(), buf, buf + n);
					std::fclose(f);
				}
				changed |= listen(bytes, v);
			}
			ImGui::EndDisabled();
		}

		if (changed) {
			std::shared_ptr<hb::bank> nb;
			if (cur) {
				nb = std::make_shared<hb::bank>(*cur);
			} else {
				nb = hb::fresh();             // 初期の音色の写しから、新しい組を作る
				m_listed = -1;
			}
			nb->prog[size_t(program)] = v;
			hb::commit(br, nb);
		}
		// つまみを離したら、ファイルへ書く
		if (hb::unsaved() && !ImGui::IsAnyItemActive()) {
			if (!hb::save(br))
				m_note = UI_TEXT(fme_save_fail, "Could not write the voice set file");
			m_listed = -1;
		}
		if (!m_note.empty())
			ImGui::TextWrapped("%s", m_note.c_str());
	}

private:
	// WAV の中身を測って、音色 v を置き換える（名前はそのまま）。置き換えたら true。結果は m_note に書く
	bool listen(const std::vector<u8> &bytes, harm_banks::voice &v)
	{
		namespace vb = smu2000::vboard;
		smu2000::wav_data w;
		std::string err;
		if (bytes.empty() || !smu2000::parse_wav(bytes, w, err) || !w.frames()) {
			m_note = std::string(UI_TEXT(hme_listen_fail, "Could not listen to that file: ")) + (err.empty() ? "cannot read the file" : err);
			return false;
		}
		// 全部のチャンネルを足して 1 本に
		std::vector<float> mono(w.frames(), 0.0f);
		for (const std::vector<float> &c : w.ch)
			for (size_t i = 0; i < mono.size() && i < c.size(); i++)
				mono[i] += c[i] / float(w.ch.size());
		vb::harm_analysis an;
		if (!vb::harm_analyze(mono.data(), mono.size(), double(w.rate), an, err)) {
			m_note = std::string(UI_TEXT(hme_listen_fail, "Could not listen to that file: ")) + err;
			return false;
		}
		char name[9];
		std::memcpy(name, v.name, sizeof(name));
		const int keep_morph = v.morph_to;
		v = an.voice;
		std::memcpy(v.name, name, sizeof(name));
		v.morph_to = keep_morph;
		int partials = 0;
		for (int n = 0; n < vb::HARM_PARTIALS; n++)
			partials += v.amp[1][n] > 0.0f || v.amp[2][n] > 0.0f;
		static const char *const KEYS[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
		char text[240];
		std::snprintf(text, sizeof(text), UI_TEXT(hme_listen_done_fmt, "Heard %s%d (%.1f Hz, %+.0f cents), %d partials, %.2f s, %s. Stretch %.2f."),
		              KEYS[an.key % 12], an.key / 12 - 2, an.f0, an.cents, partials, an.seconds,
		              an.sustained ? UI_TEXT(hme_listen_held, "a held sound") : UI_TEXT(hme_listen_dying, "a sound that dies away"), double(v.inharm));
		m_note = text;
		return true;
	}

	std::vector<std::string> m_list;
	double m_listed = -1;
	std::string m_note, m_name_for;
	char m_set_name[15] = {};
	char m_path[512] = {};
	harm_banks::voice m_clip;
	bool m_has_clip = false;
	int m_frame = 1;
	int m_drag_bar = -1;
	float m_drag_amp = 0.0f;
};

} // namespace ui

#endif // S_MU2000_UI_HARM_VOICE_EDITOR_H
