// license:BSD-3-Clause
//
// **ハーモニックボード**（架空のプラグインボードの 1 つ。src/vboard.h）。正弦波を足し合わせて音を作る加算合成の音源を
// 16 チャンネルぶん持つ、マルチパートのボード（口 E）。中身は全部ここで書いた計算と、ここで作った音色で、ROM も、
// 実在の機種の音色データも使わない。
//
//   ・1 声 = 正弦波の倍音 64 本。倍音ごとの大きさの並び（スペクトル）を 1 つの音色に 4 枚持つ:
//       0 鳴り始め → 1 山 → 2 伸ばしている間 → 3 離したあと
//     鍵を押してからの時間で 0 → 1 → 2 と移り、鍵を離すとそのときの形から 3 へ移る。音量の包絡線
//     （アタック・ディケイ・サステイン・リリース）は別に持つ
//   ・明るさ: 弱く弾くほど高い倍音を引く。CC74（Brightness）でも動く
//   ・ずれ: 倍音を整数倍から上へずらす（n 番目は n × √(1 + B(n² − 1)) 倍）。0 でオルガン、少しで弦、大きくすると鐘
//   ・奇数・偶数: 偶数か奇数の倍音を引く
//   ・雑音の帯: 帯域を絞った雑音を 1 本足す（息、弓、打った瞬間の音）。中心は基音の何倍かで決めるので、鍵に付いて動く
//   ・行き来: 音色ごとに「相手の音色」を 1 つ決めておくと、CC71（Harmonic Content）で 2 つのスペクトルの間を動かせる
//   ・音色は 32 個。GM の 16 分類に 2 つずつ割り当ててあり、プログラム番号（1-128）は GM の並びで選べる
//   ・チャンネル 10（か、ドラムのパートにしたチャンネル）はドラム。鍵ごとに決まった音
//   ・音色は書き換えられる。128 個のプログラムそれぞれが自分の音色（harm_voice）を持ち、その組が harm_bank。
//     組はファイル（.smuhm）に保存できる
//   ・32 声。ピッチベンド（±2 半音）、モジュレーション（CC1。ビブラート）、サステインペダル（CC64）、
//     CC120 / CC121 / CC123
//
// 倍音は 1 本ずつ、回る矢印（複素数の掛け算）で進める。鳴っていない倍音と、聞こえる高さを超える倍音は計算しない。
// 大きさは 32 サンプルごとに決め直して、その間はまっすぐつなぐ

#ifndef S_MU2000_VBOARD_HARM_H
#define S_MU2000_VBOARD_HARM_H

#pragma once

#include "compat/mamecompat.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace smu2000::vboard {

constexpr int HARM_PARTIALS = 64;
constexpr int HARM_FRAMES = 4;

struct harm_voice {
	char name[9] = "        ";                      // 8 文字
	float amp[HARM_FRAMES][HARM_PARTIALS] = {};     // 倍音の大きさ（0-1）。枚: 0 鳴り始め・1 山・2 伸ばしている間・3 離したあと
	float t_peak = 0.02f, t_sustain = 0.5f;         // 0 → 1、1 → 2 に移る秒
	float attack = 0.005f, decay = 0.0f, sustain = 1.0f, release = 0.15f;      // 音量。秒・秒（60dB 下がる時間。0 = 下がらない）・0-1・秒
	float bright = 0.3f;                            // 強さで明るさが変わる度合い（0-1）
	float inharm = 0.0f;                            // 倍音のずれ（0-1。B = 0.01 × inharm²）
	float odd_even = 0.0f;                          // 負で偶数の倍音を、正で奇数の倍音（基音は除く）を引く（-1〜1）
	float noise = 0.0f;                             // 雑音の帯の大きさ（0-1）
	float noise_center = 8.0f, noise_width = 1.5f;  // 帯の中心（基音の何倍か）と幅（オクターブ）
	float noise_decay = 0.0f;                       // 雑音だけが消えていく秒（0 = 音量の包絡線に付いていく）
	float drop = 0.0f;                              // 鳴り始めに上乗せする半音（30ms ほどで元の高さへ落ちる）
	int morph_to = -1;                              // CC71 で行き来する相手のプログラム番号（0-127。-1 = なし）
};

namespace harm_detail {

// ---- 初期の音色 32 個（GM の 16 分類 × 2）。形は式で書く

struct shaper {
	harm_voice v;
	explicit shaper(const char *name) { std::snprintf(v.name, sizeof(v.name), "%-8.8s", name); }
	// 枚 f を、n（1 から）の式で埋める
	template <typename F>
	shaper &frame(int f, F &&fn)
	{
		for (int n = 1; n <= HARM_PARTIALS; n++)
			v.amp[f][n - 1] = std::clamp(float(fn(n)), 0.0f, 1.0f);
		return *this;
	}
	template <typename F>
	shaper &all(F &&fn)
	{
		for (int f = 0; f < HARM_FRAMES; f++)
			frame(f, fn);
		return *this;
	}
	shaper &times(float peak, float sus) { v.t_peak = peak; v.t_sustain = sus; return *this; }
	shaper &env(float a, float d, float s, float r) { v.attack = a; v.decay = d; v.sustain = s; v.release = r; return *this; }
	shaper &noise(float level, float center, float width, float decay = 0.0f)
	{
		v.noise = level; v.noise_center = center; v.noise_width = width; v.noise_decay = decay;
		return *this;
	}
};

inline double roll(int n, double p) { return std::pow(double(n), -p); }
inline double odd(int n) { return (n & 1) ? 1.0 : 0.0; }
// はじく位置（弦の長さの 1/pos のところ）で決まる櫛
inline double pluck(int n, double pos) { return std::fabs(std::sin(double(n) * 3.141592653589793 / pos)); }
// n = c を中心にした山（幅 w）
inline double bump(int n, double c, double w) { const double d = (double(n) - c) / w; return std::exp(-d * d); }

inline harm_voice make_patch(int index)
{
	switch (index) {
	// ---- Piano
	case 0: {
		shaper s("HmPiano");
		s.frame(0, [](int n) { return roll(n, 0.9); }).frame(1, [](int n) { return roll(n, 1.2); })
		 .frame(2, [](int n) { return roll(n, 2.2); }).frame(3, [](int n) { return roll(n, 3.0); })
		 .times(0.01f, 1.4f).env(0.002f, 3.5f, 0.0f, 0.25f).noise(0.25f, 18.0f, 2.5f, 0.02f);
		s.v.inharm = 0.22f; s.v.bright = 0.7f;
		return s.v;
	}
	case 1: {
		shaper s("HmEPiano");
		s.frame(0, [](int n) { return n == 1 ? 1.0 : n == 2 ? 0.3 : n == 14 ? 0.45 : n == 15 ? 0.2 : 0.0; })
		 .frame(1, [](int n) { return n == 1 ? 1.0 : n == 2 ? 0.25 : n == 14 ? 0.2 : 0.0; })
		 .frame(2, [](int n) { return n == 1 ? 1.0 : n == 2 ? 0.15 : 0.0; })
		 .frame(3, [](int n) { return n == 1 ? 1.0 : 0.0; })
		 .times(0.02f, 0.6f).env(0.002f, 4.0f, 0.0f, 0.35f);
		s.v.bright = 0.8f;
		return s.v;
	}
	// ---- Chromatic percussion
	case 2: {
		shaper s("HmBell");
		s.frame(0, [](int n) { return n <= 9 ? (n == 1 ? 1.0 : (n & 1) ? 0.55 : 0.35) : 0.0; })
		 .frame(1, [](int n) { return n <= 8 ? (n == 1 ? 1.0 : (n & 1) ? 0.45 : 0.3) : 0.0; })
		 .frame(2, [](int n) { return n <= 4 ? (n == 1 ? 1.0 : 0.3) : 0.0; })
		 .frame(3, [](int n) { return n <= 2 ? (n == 1 ? 1.0 : 0.2) : 0.0; })
		 .times(0.02f, 2.0f).env(0.001f, 5.0f, 0.0f, 1.5f);
		s.v.inharm = 0.95f; s.v.bright = 0.5f;
		return s.v;
	}
	case 3: {
		shaper s("HmMarimb");
		s.frame(0, [](int n) { return n == 1 ? 1.0 : n == 4 ? 0.4 : n == 10 ? 0.15 : 0.0; })
		 .frame(1, [](int n) { return n == 1 ? 1.0 : n == 4 ? 0.25 : 0.0; })
		 .frame(2, [](int n) { return n == 1 ? 1.0 : 0.0; }).frame(3, [](int n) { return n == 1 ? 1.0 : 0.0; })
		 .times(0.03f, 0.15f).env(0.001f, 0.9f, 0.0f, 0.25f).noise(0.2f, 10.0f, 2.0f, 0.01f);
		return s.v;
	}
	// ---- Organ
	case 4: {
		shaper s("HmOrgan");
		s.all([](int n) { return n == 1 ? 1.0 : n == 2 ? 0.8 : n == 3 ? 0.6 : n == 4 ? 0.5 : n == 6 ? 0.3 : n == 8 ? 0.25 : 0.0; })
		 .env(0.005f, 0.0f, 1.0f, 0.06f).noise(0.12f, 12.0f, 2.0f, 0.008f);
		s.v.bright = 0.0f;
		return s.v;
	}
	case 5: {
		shaper s("HmPipeOr");
		s.all([](int n) { return roll(n, 1.1) * (0.6 + 0.4 * odd(n)); }).env(0.04f, 0.0f, 1.0f, 0.2f).noise(0.05f, 9.0f, 2.5f);
		s.v.bright = 0.1f;
		return s.v;
	}
	// ---- Guitar
	case 6: {
		shaper s("HmGuitar");
		s.frame(0, [](int n) { return roll(n, 0.8) * pluck(n, 5.0); }).frame(1, [](int n) { return roll(n, 1.1) * pluck(n, 5.0); })
		 .frame(2, [](int n) { return roll(n, 2.0) * pluck(n, 5.0); }).frame(3, [](int n) { return roll(n, 3.0); })
		 .times(0.01f, 0.9f).env(0.001f, 2.4f, 0.0f, 0.25f);
		s.v.inharm = 0.08f; s.v.bright = 0.6f;
		return s.v;
	}
	case 7: {
		shaper s("HmClavi");
		s.frame(0, [](int n) { return roll(n, 0.5) * (0.25 + 0.75 * odd(n)); }).frame(1, [](int n) { return roll(n, 0.8) * (0.25 + 0.75 * odd(n)); })
		 .frame(2, [](int n) { return roll(n, 1.4) * (0.25 + 0.75 * odd(n)); }).frame(3, [](int n) { return roll(n, 2.5); })
		 .times(0.005f, 0.5f).env(0.001f, 1.4f, 0.0f, 0.08f);
		s.v.bright = 0.5f;
		return s.v;
	}
	// ---- Bass
	case 8: {
		shaper s("HmBass");
		s.frame(0, [](int n) { return roll(n, 1.2); }).frame(1, [](int n) { return roll(n, 1.5); })
		 .frame(2, [](int n) { return roll(n, 2.5); }).frame(3, [](int n) { return roll(n, 3.0); })
		 .times(0.01f, 0.5f).env(0.001f, 2.5f, 0.3f, 0.1f);
		s.v.bright = 0.5f;
		return s.v;
	}
	case 9: {
		shaper s("HmSlapBs");
		s.frame(0, [](int n) { return roll(n, 0.5); }).frame(1, [](int n) { return roll(n, 1.0); })
		 .frame(2, [](int n) { return roll(n, 2.2); }).frame(3, [](int n) { return roll(n, 3.0); })
		 .times(0.03f, 0.25f).env(0.001f, 1.8f, 0.2f, 0.1f).noise(0.3f, 14.0f, 2.5f, 0.015f);
		s.v.bright = 0.7f;
		return s.v;
	}
	// ---- Strings
	case 10: {
		shaper s("HmString");
		s.frame(0, [](int n) { return roll(n, 2.0); }).frame(1, [](int n) { return roll(n, 1.0); })
		 .frame(2, [](int n) { return roll(n, 1.15); }).frame(3, [](int n) { return roll(n, 2.2); })
		 .times(0.15f, 0.6f).env(0.15f, 0.0f, 1.0f, 0.35f).noise(0.05f, 10.0f, 2.5f);
		s.v.bright = 0.5f;
		return s.v;
	}
	case 11: {
		shaper s("HmPizz");
		s.frame(0, [](int n) { return roll(n, 0.9); }).frame(1, [](int n) { return roll(n, 1.3); })
		 .frame(2, [](int n) { return roll(n, 2.5); }).frame(3, [](int n) { return roll(n, 3.0); })
		 .times(0.01f, 0.15f).env(0.001f, 0.35f, 0.0f, 0.15f);
		s.v.bright = 0.5f;
		return s.v;
	}
	// ---- Ensemble
	case 12: {
		shaper s("HmEnsmbl");
		s.frame(0, [](int n) { return roll(n, 2.2); }).frame(1, [](int n) { return roll(n, 1.1) * (0.7 + 0.5 * bump(n, 9.0, 4.0)); })
		 .frame(2, [](int n) { return roll(n, 1.25) * (0.7 + 0.5 * bump(n, 8.0, 4.0)); }).frame(3, [](int n) { return roll(n, 2.4); })
		 .times(0.3f, 1.0f).env(0.3f, 0.0f, 1.0f, 0.5f).noise(0.04f, 9.0f, 3.0f);
		s.v.bright = 0.4f;
		return s.v;
	}
	case 13: {
		shaper s("HmChoir");
		const auto ah = [](int n) { return roll(n, 0.6) * (0.08 + bump(n, 3.6, 1.3) + 0.7 * bump(n, 5.4, 1.2) + 0.35 * bump(n, 13.0, 2.5)); };
		s.frame(0, [&](int n) { return ah(n) * roll(n, 0.8); }).frame(1, ah).frame(2, ah).frame(3, [&](int n) { return ah(n) * roll(n, 1.0); })
		 .times(0.2f, 0.5f).env(0.2f, 0.0f, 1.0f, 0.45f).noise(0.07f, 10.0f, 2.5f);
		s.v.bright = 0.3f;
		return s.v;
	}
	// ---- Brass
	case 14: {
		shaper s("HmBrass");
		s.frame(0, [](int n) { return roll(n, 2.6); }).frame(1, [](int n) { return roll(n, 0.9); })
		 .frame(2, [](int n) { return roll(n, 1.3); }).frame(3, [](int n) { return roll(n, 2.6); })
		 .times(0.06f, 0.3f).env(0.03f, 0.0f, 1.0f, 0.15f);
		s.v.bright = 0.9f;
		return s.v;
	}
	case 15: {
		shaper s("HmSynBrs");
		s.frame(0, [](int n) { return roll(n, 2.0); }).frame(1, [](int n) { return roll(n, 0.75); })
		 .frame(2, [](int n) { return roll(n, 1.05); }).frame(3, [](int n) { return roll(n, 2.0); })
		 .times(0.03f, 0.2f).env(0.015f, 0.0f, 1.0f, 0.18f);
		s.v.bright = 0.7f;
		return s.v;
	}
	// ---- Reed
	case 16: {
		shaper s("HmSax");
		const auto body = [](int n) { return roll(n, 1.1) * (0.5 + 0.5 * odd(n)) * (0.6 + 0.9 * bump(n, 5.0, 2.5)); };
		s.frame(0, [&](int n) { return body(n) * roll(n, 0.8); }).frame(1, body).frame(2, body).frame(3, [&](int n) { return body(n) * roll(n, 1.0); })
		 .times(0.04f, 0.3f).env(0.03f, 0.0f, 1.0f, 0.12f).noise(0.06f, 8.0f, 2.5f);
		s.v.bright = 0.7f;
		return s.v;
	}
	case 17: {
		shaper s("HmOboe");
		const auto body = [](int n) { return roll(n, 0.7) * (0.15 + bump(n, 3.5, 1.6) + 0.4 * bump(n, 9.0, 2.0)); };
		s.all(body).env(0.02f, 0.0f, 1.0f, 0.12f);
		s.v.bright = 0.5f;
		return s.v;
	}
	// ---- Pipe
	case 18: {
		shaper s("HmFlute");
		s.frame(0, [](int n) { return n == 1 ? 1.0 : n == 2 ? 0.5 : n == 3 ? 0.2 : n == 4 ? 0.08 : 0.0; })
		 .frame(1, [](int n) { return n == 1 ? 1.0 : n == 2 ? 0.25 : n == 3 ? 0.08 : n == 4 ? 0.03 : 0.0; })
		 .frame(2, [](int n) { return n == 1 ? 1.0 : n == 2 ? 0.22 : n == 3 ? 0.07 : n == 4 ? 0.02 : 0.0; })
		 .frame(3, [](int n) { return n == 1 ? 1.0 : n == 2 ? 0.1 : 0.0; })
		 .times(0.08f, 0.3f).env(0.06f, 0.0f, 1.0f, 0.15f).noise(0.2f, 6.0f, 2.5f);
		s.v.bright = 0.4f;
		return s.v;
	}
	case 19: {
		shaper s("HmWhistl");
		s.all([](int n) { return n == 1 ? 1.0 : n == 2 ? 0.04 : 0.0; }).env(0.06f, 0.0f, 1.0f, 0.12f).noise(0.1f, 1.0f, 0.4f);
		s.v.bright = 0.0f;
		return s.v;
	}
	// ---- Synth lead
	case 20: {
		shaper s("HmSquare");
		s.all([](int n) { return odd(n) / double(n); }).env(0.002f, 0.0f, 1.0f, 0.1f);
		s.v.bright = 0.2f;
		return s.v;
	}
	case 21: {
		shaper s("HmSawLd");
		s.all([](int n) { return 1.0 / double(n); }).env(0.002f, 0.0f, 1.0f, 0.1f);
		s.v.bright = 0.2f;
		return s.v;
	}
	// ---- Synth pad
	case 22: {
		shaper s("HmPad");
		s.frame(0, [](int n) { return roll(n, 3.0); }).frame(1, [](int n) { return roll(n, 1.4); })
		 .frame(2, [](int n) { return roll(n, 2.2); }).frame(3, [](int n) { return roll(n, 3.0); })
		 .times(0.9f, 3.0f).env(0.6f, 0.0f, 1.0f, 1.0f);
		s.v.bright = 0.3f; s.v.morph_to = 92;              // 相手は同じ分類の 2 つ目（HmGlass。プログラム 93）
		return s.v;
	}
	case 23: {
		shaper s("HmGlass");
		const auto g = [](int n) { return n == 1 ? 1.0 : n == 3 ? 0.5 : n == 5 ? 0.4 : n == 9 ? 0.3 : n == 13 ? 0.2 : 0.0; };
		s.frame(0, g).frame(1, g).frame(2, [](int n) { return n == 1 ? 1.0 : n == 3 ? 0.3 : n == 5 ? 0.15 : 0.0; })
		 .frame(3, [](int n) { return n == 1 ? 1.0 : 0.0; }).times(0.02f, 2.0f).env(0.01f, 4.0f, 0.4f, 1.2f);
		s.v.inharm = 0.3f; s.v.bright = 0.4f;
		return s.v;
	}
	// ---- Synth effects
	case 24: {
		shaper s("HmRain");
		const auto r = [](int n) { return (n % 5 == 1 || n % 7 == 3) ? roll(n, 0.4) : 0.0; };
		s.frame(0, r).frame(1, r).frame(2, [&](int n) { return r(n) * roll(n, 1.2); }).frame(3, [](int n) { return n == 1 ? 1.0 : 0.0; })
		 .times(0.01f, 0.5f).env(0.001f, 1.2f, 0.0f, 0.6f);
		s.v.inharm = 1.0f; s.v.bright = 0.4f;
		return s.v;
	}
	case 25: {
		shaper s("HmCrystl");
		const auto c = [](int n) { return n == 1 ? 1.0 : n == 4 ? 0.5 : n == 7 ? 0.4 : n == 11 ? 0.35 : n == 16 ? 0.25 : 0.0; };
		s.frame(0, c).frame(1, c).frame(2, [](int n) { return n == 1 ? 1.0 : n == 4 ? 0.3 : n == 7 ? 0.15 : 0.0; })
		 .frame(3, [](int n) { return n == 1 ? 1.0 : n == 4 ? 0.15 : 0.0; }).times(0.02f, 1.5f).env(0.001f, 4.0f, 0.0f, 1.5f);
		s.v.inharm = 0.5f; s.v.bright = 0.4f;
		return s.v;
	}
	// ---- Ethnic
	case 26: {
		shaper s("HmSitar");
		s.frame(0, [](int n) { return roll(n, 0.3) * (0.4 + 0.6 * pluck(n, 9.0)); }).frame(1, [](int n) { return roll(n, 0.6) * (0.4 + 0.6 * pluck(n, 9.0)); })
		 .frame(2, [](int n) { return roll(n, 1.5); }).frame(3, [](int n) { return roll(n, 2.5); })
		 .times(0.02f, 1.2f).env(0.001f, 3.0f, 0.0f, 0.3f);
		s.v.inharm = 0.06f; s.v.bright = 0.5f;
		return s.v;
	}
	case 27: {
		shaper s("HmKoto");
		s.frame(0, [](int n) { return roll(n, 0.9) * pluck(n, 8.0); }).frame(1, [](int n) { return roll(n, 1.2) * pluck(n, 8.0); })
		 .frame(2, [](int n) { return roll(n, 2.4) * pluck(n, 8.0); }).frame(3, [](int n) { return roll(n, 3.0); })
		 .times(0.01f, 0.6f).env(0.001f, 1.8f, 0.0f, 0.3f).noise(0.2f, 16.0f, 2.0f, 0.01f);
		s.v.inharm = 0.1f; s.v.bright = 0.6f;
		return s.v;
	}
	// ---- Percussive
	case 28: {
		shaper s("HmTom");
		s.frame(0, [](int n) { return n == 1 ? 1.0 : n == 2 ? 0.45 : n == 3 ? 0.25 : 0.0; }).frame(1, [](int n) { return n == 1 ? 1.0 : n == 2 ? 0.3 : 0.0; })
		 .frame(2, [](int n) { return n == 1 ? 1.0 : 0.0; }).frame(3, [](int n) { return n == 1 ? 1.0 : 0.0; })
		 .times(0.02f, 0.1f).env(0.001f, 0.45f, 0.0f, 0.2f).noise(0.3f, 8.0f, 2.5f, 0.02f);
		s.v.inharm = 1.0f; s.v.drop = 7.0f;
		return s.v;
	}
	case 29: {
		shaper s("HmBlock");
		s.all([](int n) { return n == 1 ? 1.0 : n == 3 ? 0.45 : n == 5 ? 0.2 : 0.0; }).env(0.001f, 0.12f, 0.0f, 0.08f).noise(0.3f, 10.0f, 1.5f, 0.008f);
		s.v.inharm = 0.8f;
		return s.v;
	}
	// ---- Sound effects
	case 30: {
		shaper s("HmNoise");
		s.env(0.01f, 0.0f, 1.0f, 0.4f).noise(1.0f, 8.0f, 4.0f);
		return s.v;
	}
	default: {
		// スペクトルの山が、下から上へ、また下へと動く。足し算の音源でないと出来ない形
		shaper s("HmSweep");
		s.frame(0, [](int n) { return bump(n, 1.0, 1.2); }).frame(1, [](int n) { return 0.3 * roll(n, 1.0) + bump(n, 24.0, 5.0); })
		 .frame(2, [](int n) { return 0.4 * roll(n, 1.0) + bump(n, 6.0, 2.5); }).frame(3, [](int n) { return bump(n, 1.0, 1.0); })
		 .times(1.5f, 2.0f).env(0.01f, 0.0f, 1.0f, 0.8f);
		s.v.bright = 0.0f;
		return s.v;
	}
	}
}

// ---- ドラム（鍵ごとに決まった音）

struct harm_drum {
	harm_voice v;
	float fixed_key = 0.0f;         // 0 でなければ、鍵に関係なくこの高さ（MIDI の鍵番号）
	int group = 0;                  // 同じ組の音は、後から鳴ったものが前を止める（ハイハットの開閉）
};

enum { DR_KICK, DR_SNARE, DR_RIM, DR_CLAP, DR_HAT_C, DR_HAT_O, DR_TOM, DR_CRASH, DR_RIDE, DR_BELL, DR_PERC, DR_COUNT };

inline harm_drum make_drum(int which)
{
	harm_drum d;
	const auto metal = [](int n) { return (n == 2 || n == 3 || n == 5 || n == 7 || n == 11 || n == 13) ? 0.5 : 0.0; };
	switch (which) {
	case DR_KICK: {
		shaper s("Kick");
		s.frame(0, [](int n) { return n == 1 ? 1.0 : n == 2 ? 0.4 : n == 3 ? 0.15 : 0.0; }).frame(1, [](int n) { return n == 1 ? 1.0 : n == 2 ? 0.2 : 0.0; })
		 .frame(2, [](int n) { return n == 1 ? 1.0 : 0.0; }).frame(3, [](int n) { return n == 1 ? 1.0 : 0.0; })
		 .times(0.01f, 0.05f).env(0.001f, 0.35f, 0.0f, 0.2f).noise(0.5f, 40.0f, 2.5f, 0.01f);
		s.v.drop = 22.0f; s.v.bright = 0.0f;
		d.v = s.v; d.fixed_key = 31.0f;
		break;
	}
	case DR_SNARE: {
		shaper s("Snare");
		s.all([](int n) { return n == 1 ? 1.0 : n == 2 ? 0.5 : n == 3 ? 0.3 : 0.0; }).env(0.001f, 0.16f, 0.0f, 0.12f).noise(1.0f, 20.0f, 3.0f, 0.2f);
		s.v.inharm = 1.0f; s.v.drop = 5.0f; s.v.bright = 0.0f;
		d.v = s.v; d.fixed_key = 55.0f;
		break;
	}
	case DR_RIM: {
		shaper s("Rim");
		s.all([](int n) { return n == 1 ? 1.0 : n == 2 ? 0.6 : n == 4 ? 0.3 : 0.0; }).env(0.001f, 0.05f, 0.0f, 0.04f).noise(0.4f, 5.0f, 2.0f, 0.01f);
		s.v.inharm = 0.8f; s.v.bright = 0.0f;
		d.v = s.v; d.fixed_key = 76.0f;
		break;
	}
	case DR_CLAP: {
		shaper s("Clap");
		s.env(0.001f, 0.2f, 0.0f, 0.15f).noise(1.0f, 5.0f, 2.0f, 0.18f);
		d.v = s.v; d.fixed_key = 64.0f;
		break;
	}
	case DR_HAT_C:
	case DR_HAT_O: {
		const bool open = which == DR_HAT_O;
		shaper s(open ? "Open HH" : "ClosedHH");
		s.all(metal).env(0.001f, open ? 0.45f : 0.05f, 0.0f, open ? 0.1f : 0.04f).noise(1.0f, 2.6f, 1.5f, open ? 0.4f : 0.05f);
		s.v.inharm = 1.0f; s.v.bright = 0.0f;
		d.v = s.v; d.fixed_key = 103.0f; d.group = 1;
		break;
	}
	case DR_TOM: {
		shaper s("Tom");
		s.frame(0, [](int n) { return n == 1 ? 1.0 : n == 2 ? 0.4 : n == 3 ? 0.2 : 0.0; }).frame(1, [](int n) { return n == 1 ? 1.0 : n == 2 ? 0.25 : 0.0; })
		 .frame(2, [](int n) { return n == 1 ? 1.0 : 0.0; }).frame(3, [](int n) { return n == 1 ? 1.0 : 0.0; })
		 .times(0.02f, 0.1f).env(0.001f, 0.4f, 0.0f, 0.25f).noise(0.3f, 9.0f, 2.5f, 0.02f);
		s.v.inharm = 1.0f; s.v.drop = 7.0f; s.v.bright = 0.0f;
		d.v = s.v; d.fixed_key = 45.0f;
		break;
	}
	case DR_CRASH: {
		shaper s("Crash");
		s.all(metal).env(0.001f, 1.8f, 0.0f, 1.2f).noise(1.0f, 3.0f, 2.5f, 1.6f);
		s.v.inharm = 1.0f; s.v.bright = 0.0f;
		d.v = s.v; d.fixed_key = 96.0f;
		break;
	}
	case DR_RIDE: {
		shaper s("Ride");
		s.all([&](int n) { return metal(n) * 1.6; }).env(0.001f, 1.0f, 0.0f, 0.8f).noise(0.45f, 3.0f, 2.0f, 0.8f);
		s.v.inharm = 1.0f; s.v.bright = 0.0f;
		d.v = s.v; d.fixed_key = 98.0f;
		break;
	}
	case DR_BELL: {
		shaper s("Cowbell");
		s.all([](int n) { return n == 2 ? 1.0 : n == 3 ? 0.9 : n == 5 ? 0.2 : 0.0; }).env(0.001f, 0.25f, 0.0f, 0.15f);
		s.v.bright = 0.0f;
		d.v = s.v; d.fixed_key = 67.0f;
		break;
	}
	default: {
		shaper s("Perc");
		s.all([](int n) { return n == 1 ? 1.0 : n == 3 ? 0.4 : n == 4 ? 0.2 : 0.0; }).env(0.001f, 0.18f, 0.0f, 0.12f).noise(0.2f, 8.0f, 2.0f, 0.01f);
		s.v.inharm = 0.7f; s.v.drop = 3.0f; s.v.bright = 0.0f;
		d.v = s.v;
		break;
	}
	}
	return d;
}

inline const harm_drum &drum(int which)
{
	static const std::array<harm_drum, DR_COUNT> table = [] {
		std::array<harm_drum, DR_COUNT> t;
		for (int i = 0; i < DR_COUNT; i++)
			t[size_t(i)] = make_drum(i);
		return t;
	}();
	return table[size_t(which)];
}

// 鍵 → ドラムの音。tune はその音を何半音ずらすか（タムの高さの違い）
inline const harm_drum &drum_for_key(int key, float &tune)
{
	tune = 0.0f;
	switch (key) {
	case 35: case 36: return drum(DR_KICK);
	case 37: return drum(DR_RIM);
	case 38: case 40: tune = key == 40 ? 2.0f : 0.0f; return drum(DR_SNARE);
	case 39: return drum(DR_CLAP);
	case 42: case 44: return drum(DR_HAT_C);
	case 46: return drum(DR_HAT_O);
	case 41: case 43: case 45: case 47: case 48: case 50: {
		static const float TUNE[10] = { 0, 0, 3, 0, 6, 0, 9, 12, 0, 15 };
		tune = TUNE[key - 41];
		return drum(DR_TOM);
	}
	case 49: case 57: tune = key == 57 ? 2.0f : 0.0f; return drum(DR_CRASH);
	case 52: case 55: tune = key == 55 ? 5.0f : -3.0f; return drum(DR_CRASH);
	case 51: case 59: tune = key == 59 ? 2.0f : 0.0f; return drum(DR_RIDE);
	case 53: tune = 6.0f; return drum(DR_RIDE);
	case 56: return drum(DR_BELL);
	default: return drum(DR_PERC);
	}
}

inline const harm_voice &patch(int index)
{
	static const std::array<harm_voice, 32> table = [] {
		std::array<harm_voice, 32> t;
		for (int i = 0; i < 32; i++)
			t[size_t(i)] = make_patch(i);
		return t;
	}();
	return table[size_t(index & 31)];
}

} // namespace harm_detail

// 初期の音色（プログラム番号ごと。内蔵の 32 個を GM の並びに置いたもの: 分類の前半 4 つが 1 つ目、後半 4 つが 2 つ目）
inline const harm_voice &harm_default_voice(int program)
{
	program &= 127;
	return harm_detail::patch((program / 8) * 2 + ((program % 8) >= 4 ? 1 : 0));
}

struct harm_bank {
	char name[15] = "HARMONIC SET";     // 組の名前（ファイルの名前にもなる）
	std::array<harm_voice, 128> prog;
	harm_bank()
	{
		for (int i = 0; i < 128; i++)
			prog[size_t(i)] = harm_default_voice(i);
	}
};

// n 番目（1 から）の倍音の、基音に対する周波数の比
inline double harm_ratio(const harm_voice &v, int n)
{
	const double b = 0.01 * double(v.inharm) * double(v.inharm);
	return double(n) * std::sqrt(1.0 + b * (double(n) * double(n) - 1.0));
}

// 奇数・偶数の引き具合を掛けた、n 番目（1 から）の倍音の倍率
inline float harm_parity(const harm_voice &v, int n)
{
	if (v.odd_even < 0.0f && !(n & 1))
		return 1.0f + v.odd_even;
	if (v.odd_even > 0.0f && (n & 1) && n > 1)
		return 1.0f - v.odd_even;
	return 1.0f;
}

// 1 周期の形（n 点）。枚 frame のスペクトルをそのまま足したもの（ずれと雑音は入れない）。画面の絵用。山が ±1 になるようにそろえる
inline void harm_cycle(const harm_voice &v, int frame, float *out, int n)
{
	frame = std::clamp(frame, 0, HARM_FRAMES - 1);
	float peak = 0.0f;
	for (int k = 0; k < n; k++) {
		double s = 0.0;
		for (int h = 1; h <= HARM_PARTIALS; h++) {
			const float a = v.amp[frame][h - 1] * harm_parity(v, h);
			if (a > 0.0f)
				s += double(a) * std::sin(6.283185307179586 * double(h) * double(k) / double(n));
		}
		out[k] = float(s);
		peak = std::max(peak, std::fabs(out[k]));
	}
	if (peak > 0.0f)
		for (int k = 0; k < n; k++)
			out[k] /= peak;
}

// ---- 組のファイル。全部リトルエンディアン
//   "SMUHMBNK"  u32 版（1）  名前 16 バイト
//   音色 128 個: 名前 8 バイト  i32 行き来の相手  f32 × 16（移る秒 2 つ・音量 4 つ・明るさ・ずれ・奇偶・
//                雑音の大きさ・中心・幅・消える秒・落ち幅・空き 2 つ）  f32 × 256（4 枚 × 64 本）

namespace harm_detail {
inline void put_f32(std::vector<u8> &o, float f)
{
	u32 v;
	std::memcpy(&v, &f, 4);
	for (int i = 0; i < 4; i++)
		o.push_back(u8(v >> (8 * i)));
}
inline float get_f32(const u8 *p, float lo, float hi, float fallback)
{
	const u32 v = u32(p[0]) | u32(p[1]) << 8 | u32(p[2]) << 16 | u32(p[3]) << 24;
	float f;
	std::memcpy(&f, &v, 4);
	return f >= lo && f <= hi ? f : fallback;       // NaN もここで落ちる
}
inline void clean_text(char *dst, const u8 *src, size_t n, bool pad)
{
	size_t i = 0;
	for (; i < n && src[i]; i++)
		dst[i] = src[i] >= 0x20 && src[i] < 0x7f ? char(src[i]) : '?';
	const size_t end = i;
	for (; i < n; i++)
		dst[i] = ' ';
	dst[pad ? n : end] = 0;
}
constexpr size_t HARM_VOICE_BYTES = 8 + 4 + 16 * 4 + HARM_FRAMES * HARM_PARTIALS * 4;
} // namespace harm_detail

inline std::vector<u8> write_harm_bank(const harm_bank &b)
{
	std::vector<u8> o = { 'S', 'M', 'U', 'H', 'M', 'B', 'N', 'K', 1, 0, 0, 0 };
	char name[16] = {};
	std::snprintf(name, sizeof(name), "%s", b.name);
	o.insert(o.end(), name, name + 16);
	for (const harm_voice &v : b.prog) {
		o.insert(o.end(), v.name, v.name + 8);
		const u32 m = u32(s32(v.morph_to));
		for (int i = 0; i < 4; i++)
			o.push_back(u8(m >> (8 * i)));
		for (float f : { v.t_peak, v.t_sustain, v.attack, v.decay, v.sustain, v.release, v.bright, v.inharm, v.odd_even,
		                 v.noise, v.noise_center, v.noise_width, v.noise_decay, v.drop, 0.0f, 0.0f })
			harm_detail::put_f32(o, f);
		for (int f = 0; f < HARM_FRAMES; f++)
			for (int n = 0; n < HARM_PARTIALS; n++)
				harm_detail::put_f32(o, v.amp[f][n]);
	}
	return o;
}

inline std::shared_ptr<harm_bank> read_harm_bank(const u8 *d, size_t n, std::string &err)
{
	if (n < 28 || std::memcmp(d, "SMUHMBNK", 8) != 0) {
		err = "not a harmonic voice set";
		return nullptr;
	}
	if (d[8] != 1 || d[9] || d[10] || d[11]) {
		err = "unknown version";
		return nullptr;
	}
	if (n < 28 + 128 * harm_detail::HARM_VOICE_BYTES) {
		err = "file is cut short";
		return nullptr;
	}
	auto b = std::make_shared<harm_bank>();
	harm_detail::clean_text(b->name, d + 12, 14, false);
	const u8 *p = d + 28;
	for (harm_voice &v : b->prog) {
		using harm_detail::get_f32;
		harm_detail::clean_text(v.name, p, 8, true);
		const s32 m = s32(u32(p[8]) | u32(p[9]) << 8 | u32(p[10]) << 16 | u32(p[11]) << 24);
		v.morph_to = m >= 0 && m < 128 ? m : -1;
		const u8 *q = p + 12;
		v.t_peak = get_f32(q, 0.0f, 30.0f, 0.02f);
		v.t_sustain = get_f32(q + 4, 0.0f, 30.0f, 0.5f);
		v.attack = get_f32(q + 8, 0.0f, 10.0f, 0.005f);
		v.decay = get_f32(q + 12, 0.0f, 30.0f, 0.0f);
		v.sustain = get_f32(q + 16, 0.0f, 1.0f, 1.0f);
		v.release = get_f32(q + 20, 0.0f, 30.0f, 0.15f);
		v.bright = get_f32(q + 24, 0.0f, 1.0f, 0.0f);
		v.inharm = get_f32(q + 28, 0.0f, 1.0f, 0.0f);
		v.odd_even = get_f32(q + 32, -1.0f, 1.0f, 0.0f);
		v.noise = get_f32(q + 36, 0.0f, 1.0f, 0.0f);
		v.noise_center = get_f32(q + 40, 0.25f, 64.0f, 8.0f);
		v.noise_width = get_f32(q + 44, 0.1f, 6.0f, 1.5f);
		v.noise_decay = get_f32(q + 48, 0.0f, 30.0f, 0.0f);
		v.drop = get_f32(q + 52, -48.0f, 48.0f, 0.0f);
		q += 64;
		for (int f = 0; f < HARM_FRAMES; f++)
			for (int h = 0; h < HARM_PARTIALS; h++, q += 4)
				v.amp[f][h] = get_f32(q, 0.0f, 1.0f, 0.0f);
		p += harm_detail::HARM_VOICE_BYTES;
	}
	return b;
}

inline bool save_harm_bank(const std::string &path, const harm_bank &b)
{
	const std::vector<u8> bytes = write_harm_bank(b);
	std::FILE *f = std::fopen(path.c_str(), "wb");
	if (!f)
		return false;
	const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
	return std::fclose(f) == 0 && ok;
}

inline std::shared_ptr<harm_bank> load_harm_bank(const std::string &path, std::string &err)
{
	std::FILE *f = std::fopen(path.c_str(), "rb");
	if (!f) {
		err = "cannot open the file";
		return nullptr;
	}
	std::vector<u8> bytes;
	u8 buf[16384];
	size_t n;
	while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0 && bytes.size() < (1u << 21))
		bytes.insert(bytes.end(), buf, buf + n);
	std::fclose(f);
	return read_harm_bank(bytes.data(), bytes.size(), err);
}

class harm_synth
{
public:
	static constexpr int VOICES = 32;
	static constexpr double RATE = 44100.0;
	static constexpr int TICK = 32;             // 大きさと高さを決め直す間隔（サンプル）

	// 音色の組を替える（nullptr なら初期の組）。鳴っている音にも、新しい値をすぐ効かせる（包絡線の進み具合はそのまま）
	void set_bank(std::shared_ptr<const harm_bank> bank)
	{
		m_bank = std::move(bank);
		for (voice &v : m_v)
			if (v.on && v.prog >= 0) {
				load(v, voice_of(v.prog));
				setup(v);
				v.hz = 0.0;         // 次の区切りで角度を決め直す
			}
	}
	const harm_bank *bank() const { return m_bank.get(); }
	// そのプログラムの音色（組が無ければ初期の音色）
	const harm_voice &voice_of(int program) const { return m_bank ? m_bank->prog[size_t(program & 127)] : harm_default_voice(program); }

	void reset()
	{
		for (voice &v : m_v)
			v.on = false;
		for (int c = 0; c < 16; c++) {
			m_c[size_t(c)] = chan();
			m_c[size_t(c)].drum = c == 9;
		}
	}

	u8 program(int channel) const { return m_c[size_t(channel & 15)].program; }
	bool drum(int channel) const { return m_c[size_t(channel & 15)].drum; }
	void set_drum(int channel, bool on) { m_c[size_t(channel & 15)].drum = on; }

	void midi(u8 status, u8 d0, u8 d1)
	{
		const int ch = status & 15;
		chan &c = m_c[size_t(ch)];
		switch (status & 0xf0) {
		case 0x90:
			if (d1) {
				note_on(ch, d0, d1);
				break;
			}
			[[fallthrough]];
		case 0x80:
			for (voice &v : m_v)
				if (v.on && v.ch == ch && v.key == d0 && !v.released) {
					if (c.pedal)
						v.held = true;
					else
						release(v);
				}
			break;
		case 0xb0:
			switch (d0) {
			case 1:   c.mod = d1 / 127.0f; break;
			case 71:  c.morph = d1 / 127.0f; break;
			case 74:
				c.bright = (d1 - 64) / 64.0f;
				for (voice &v : m_v)
					if (v.on && v.ch == ch)
						setup(v);
				break;
			case 64:
				c.pedal = d1 >= 64;
				if (!c.pedal)
					for (voice &v : m_v)
						if (v.on && v.ch == ch && v.held) {
							v.held = false;
							release(v);
						}
				break;
			case 120:
				for (voice &v : m_v)
					if (v.ch == ch)
						v.on = false;
				break;
			case 121:
				c.mod = 0.0f;
				c.bend = 0.0f;
				c.morph = 0.0f;
				c.bright = 0.0f;
				c.pedal = false;
				[[fallthrough]];
			case 123:
				for (voice &v : m_v)
					if (v.on && v.ch == ch && (d0 == 123 || v.held)) {
						v.held = false;
						if (!v.released)
							release(v);
					}
				break;
			default: break;
			}
			break;
		case 0xc0:
			c.program = d0 & 127;
			break;
		case 0xe0:
			c.bend = float((((d1 << 7) | d0) - 8192) / 8192.0 * 2.0);      // 半音
			break;
		default:
			break;
		}
	}

	bool sounding() const
	{
		for (const voice &v : m_v)
			if (v.on)
				return true;
		return false;
	}

	// いま計算している倍音の本数（全部の声の合計）。重さの目安
	int partials_running() const
	{
		int n = 0;
		for (const voice &v : m_v)
			if (v.on)
				n += v.count;
		return n;
	}

	// 1 サンプル（44.1kHz）。チャンネルごとの音を足し込む（モノラル）。±1.0 が全振幅
	void render(float out[16])
	{
		m_lfo += 5.5 / RATE;
		if (m_lfo >= 1.0)
			m_lfo -= 1.0;
		const bool tick = !(m_tick++ % TICK);
		const float vib = tick ? float(std::sin(m_lfo * 6.283185307179586)) * 0.4f : 0.0f;      // ±0.4 半音まで
		for (voice &v : m_v) {
			if (!v.on)
				continue;
			const chan &c = m_c[size_t(v.ch)];
			if (tick || v.fresh) {
				control(v, c, vib);
				if (!v.on)
					continue;
			}
			// 倍音。矢印を 1 つ分回して、縦の成分に大きさを掛けて足す。4 本ずつ別の入れ物に足す（まとめて計算しやすい形）
			float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f, s3 = 0.0f;
			const int n4 = v.count & ~3;
			float *re = v.re, *im = v.im, *a = v.a;
			const float *rc = v.rc, *rs = v.rs, *da = v.da;
			for (int i = 0; i < n4; i += 4) {
				const float r0 = re[i] * rc[i] - im[i] * rs[i], r1 = re[i + 1] * rc[i + 1] - im[i + 1] * rs[i + 1];
				const float r2 = re[i + 2] * rc[i + 2] - im[i + 2] * rs[i + 2], r3 = re[i + 3] * rc[i + 3] - im[i + 3] * rs[i + 3];
				im[i] = re[i] * rs[i] + im[i] * rc[i];
				im[i + 1] = re[i + 1] * rs[i + 1] + im[i + 1] * rc[i + 1];
				im[i + 2] = re[i + 2] * rs[i + 2] + im[i + 2] * rc[i + 2];
				im[i + 3] = re[i + 3] * rs[i + 3] + im[i + 3] * rc[i + 3];
				re[i] = r0; re[i + 1] = r1; re[i + 2] = r2; re[i + 3] = r3;
				a[i] += da[i]; a[i + 1] += da[i + 1]; a[i + 2] += da[i + 2]; a[i + 3] += da[i + 3];
				s0 += im[i] * a[i]; s1 += im[i + 1] * a[i + 1]; s2 += im[i + 2] * a[i + 2]; s3 += im[i + 3] * a[i + 3];
			}
			for (int i = n4; i < v.count; i++) {
				const float r = re[i] * rc[i] - im[i] * rs[i];
				im[i] = re[i] * rs[i] + im[i] * rc[i];
				re[i] = r;
				a[i] += da[i];
				s0 += im[i] * a[i];
			}
			float s = (s0 + s1) + (s2 + s3);
			// 雑音の帯（状態変数フィルターの帯域通過）
			if (v.nz_gain > 0.0f || v.nz_now > 1e-6f) {
				v.lfsr = v.lfsr * 1664525u + 1013904223u;
				const float white = float(v.lfsr >> 8) * (1.0f / 8388608.0f) - 1.0f;
				const float hp = (white - v.nz_k * v.nz_s1 - v.nz_g * v.nz_s1 - v.nz_s2) * v.nz_d;
				const float bp = v.nz_g * hp + v.nz_s1;
				v.nz_s1 = v.nz_g * hp + bp;
				const float lp = v.nz_g * bp + v.nz_s2;
				v.nz_s2 = v.nz_g * bp + lp;
				v.nz_now += v.nz_step;
				s += bp * v.nz_now;
			}
			out[v.ch] += s * v.gain;
		}
	}

private:
	static constexpr float QUICK = 1.0f / 441.0f;     // 10 ミリ秒で上がりきる、1 サンプルごとの上がり幅

	struct chan {
		u8 program = 0;
		bool drum = false, pedal = false;
		float bend = 0.0f, mod = 0.0f, morph = 0.0f, bright = 0.0f;
	};
	struct voice {
		bool on = false, released = false, held = false, has_alt = false;
		bool fresh = false;           // 鳴らし始めてから、まだ 1 度も大きさを決めていない
		int ch = 0, group = 0, count = 0;
		u8 key = 60;
		int prog = -1;                // メロディの音色ならプログラム番号、ドラムは -1
		harm_voice cur{}, alt{};      // 鳴らしている音色の写しと、行き来の相手の写し
		float vel = 1.0f, base_key = 60.0f, drop = 0.0f, gain = 0.0f;
		double hz = 0.0;              // いま回している基音
		u32 t = 0, t_rel = 0;         // 鍵を押してから・離してからのサンプル
		// 音量の包絡線
		float env = 0.0f, d_attack = 1.0f, k_decay = 1.0f, k_release = 0.0f, sustain = 1.0f;
		bool rising = true;
		// 倍音ごと
		alignas(16) float re[HARM_PARTIALS], im[HARM_PARTIALS], rc[HARM_PARTIALS], rs[HARM_PARTIALS];
		alignas(16) float a[HARM_PARTIALS], da[HARM_PARTIALS];
		float shade[HARM_PARTIALS];   // 強さ・奇偶・明るさで決まる倍率
		float limit[HARM_PARTIALS];   // 聞こえる高さを超えた倍音は 0
		float from[HARM_PARTIALS];    // 鍵を離したときのスペクトル
		float norm = 0.0f;
		// 雑音の帯
		u32 lfsr = 1;
		float nz_g = 0.0f, nz_k = 1.0f, nz_d = 1.0f, nz_s1 = 0.0f, nz_s2 = 0.0f;
		float nz_gain = 0.0f, nz_env = 1.0f, nz_k_decay = 1.0f, nz_now = 0.0f, nz_step = 0.0f;
		u32 age = 0;
	};

	// seconds で 60dB 下がる、1 サンプルごとの倍率
	static float fall(float seconds) { return seconds <= 0.0f ? 1.0f : float(std::pow(0.001, 1.0 / (double(seconds) * RATE))); }

	void load(voice &v, const harm_voice &hv)
	{
		v.cur = hv;
		v.has_alt = hv.morph_to >= 0 && hv.morph_to < 128 && v.prog >= 0;
		if (v.has_alt)
			v.alt = voice_of(hv.morph_to);
	}

	// 音色の写しから、変わらないもの（包絡線の速さ・倍音ごとの倍率・大きさのそろえ）を決める
	void setup(voice &v) const
	{
		const harm_voice &hv = v.cur;
		const chan &c = m_c[size_t(v.ch)];
		v.sustain = hv.sustain;
		v.d_attack = hv.attack <= 0.0f ? 1.0f : float(1.0 / (double(hv.attack) * RATE));
		v.k_decay = hv.decay <= 0.0f || hv.sustain >= 1.0f ? 1.0f : fall(hv.decay);
		v.k_release = fall(std::max(hv.release, 0.003f));
		// 明るさ: 弱いほど高い倍音を引く（強さ 0 で 1 オクターブごとに 9dB まで）。CC74 は上げると足し、下げると引く
		const double tilt = double(hv.bright) * 1.5 * (1.0 - double(v.vel)) - double(c.bright);
		int top = 0;
		double power = 0.0;
		for (int f = 0; f < HARM_FRAMES; f++) {
			double p = 0.0;
			for (int n = 0; n < HARM_PARTIALS; n++) {
				const double x = double(hv.amp[f][n]) * double(harm_parity(hv, n + 1));
				p += x * x;
			}
			power = std::max(power, p);
		}
		for (int n = 0; n < HARM_PARTIALS; n++) {
			v.shade[n] = harm_parity(hv, n + 1) * float(std::min(4.0, std::pow(double(n + 1), -tilt)));
			bool used = false;
			for (int f = 0; f < HARM_FRAMES; f++)
				used |= hv.amp[f][n] > 0.0f || (v.has_alt && v.alt.amp[f][n] > 0.0f);
			if (used)
				top = n + 1;
		}
		v.count = top;
		v.norm = power > 0.0 ? float(1.0 / std::sqrt(power)) : 0.0f;
		v.nz_gain = hv.noise * 1.5f;
		v.nz_k_decay = hv.noise_decay > 0.0f ? float(std::pow(0.001, double(TICK) / (double(hv.noise_decay) * RATE))) : 1.0f;
	}

	// 32 サンプルごと。包絡線を進め、いまのスペクトルと高さを決める
	void control(voice &v, const chan &c, float vib)
	{
		const harm_voice &hv = v.cur;
		const bool first = v.fresh;
		v.fresh = false;
		// 音量の包絡線（TICK サンプルぶんまとめて）。速い立ち上がり（10ms まで）は、途中で離されても上がりきってから下げる
		if (!first) {
			for (int i = 0; i < TICK; i++) {
				if (v.released && !(v.rising && v.d_attack >= QUICK)) {
					v.env *= v.k_release;
					v.rising = false;
				} else if (v.rising) {
					v.env += v.d_attack;
					if (v.env >= 1.0f) {
						v.env = 1.0f;
						v.rising = false;
					}
				} else if (v.k_decay < 1.0f) {
					v.env = v.sustain + (v.env - v.sustain) * v.k_decay;
				}
			}
			v.t += TICK;
			if (v.released)
				v.t_rel += TICK;
			v.nz_env *= v.nz_k_decay;
		} else if (v.d_attack >= 1.0f) {
			v.env = 1.0f;
			v.rising = false;
		}
		if (!v.rising && v.env <= 1e-4f) {
			v.on = false;
			return;
		}
		// 高さ。ドラムの落ち幅は 30ms ほどで消える
		const float semis = v.base_key + c.bend + c.mod * vib + v.drop;
		v.drop *= 0.97f;
		const double hz = 440.0 * std::pow(2.0, (double(semis) - 69.0) / 12.0);
		if (first || std::fabs(hz - v.hz) > hz * 1e-6) {
			v.hz = hz;
			tune(v);
		} else {
			// 丸めで矢印の長さがずれていくのを戻す
			for (int n = 0; n < v.count; n++) {
				const float k = 1.5f - 0.5f * (v.re[n] * v.re[n] + v.im[n] * v.im[n]);
				v.re[n] *= k;
				v.im[n] *= k;
			}
		}
		// スペクトル。押している間は 0 → 1 → 2、離したあとは離したときの形 → 3
		const float sec = float(double(v.t) / RATE);
		int f0 = 0, f1 = 1;
		float x = 0.0f;
		if (sec < hv.t_peak) {
			x = hv.t_peak > 0.0f ? sec / hv.t_peak : 1.0f;
		} else {
			f0 = 1;
			f1 = 2;
			x = hv.t_sustain > 0.0f ? std::min(1.0f, (sec - hv.t_peak) / hv.t_sustain) : 1.0f;
		}
		const float m = v.has_alt ? c.morph : 0.0f;
		const auto at = [&](int f, int n) {
			return v.has_alt ? hv.amp[f][n] + (v.alt.amp[f][n] - hv.amp[f][n]) * m : hv.amp[f][n];
		};
		const float rel = v.released ? std::min(1.0f, float(double(v.t_rel) / RATE) / std::max(hv.release, 0.003f)) : 0.0f;
		const float level = v.env * v.norm;
		for (int n = 0; n < v.count; n++) {
			float s;
			if (v.released) {
				s = v.from[n] + (at(3, n) - v.from[n]) * rel;
			} else {
				s = at(f0, n) + (at(f1, n) - at(f0, n)) * x;
				v.from[n] = s;
			}
			const float target = s * v.shade[n] * v.limit[n] * level;
			if (first)
				v.a[n] = target;
			v.da[n] = (target - v.a[n]) * (1.0f / float(TICK));
		}
		// 雑音の帯: 中心は基音に付いて動く
		if (v.nz_gain > 0.0f) {
			const double fc = std::clamp(v.hz * double(hv.noise_center), 30.0, RATE * 0.45);
			const double q = 1.0 / (2.0 * std::sinh(0.34657359 * double(std::max(hv.noise_width, 0.1f))));
			v.nz_g = float(std::tan(3.141592653589793 * fc / RATE));
			v.nz_k = float(1.0 / std::max(q, 0.05));
			v.nz_d = 1.0f / (1.0f + v.nz_g * (v.nz_g + v.nz_k));
			const float target = v.nz_gain * (hv.noise_decay > 0.0f ? v.nz_env * (v.released ? v.env : 1.0f) : v.env) * v.nz_k;
			if (first)
				v.nz_now = target;
			v.nz_step = (target - v.nz_now) * (1.0f / float(TICK));
		} else {
			v.nz_step = -v.nz_now * (1.0f / float(TICK));
		}
	}

	// 倍音ごとの回す角度を決め直す。ずれが無ければ、基音の角度を掛け重ねて作る（三角関数は 1 回）
	static void tune(voice &v)
	{
		const double w = 6.283185307179586 * v.hz / RATE;
		const double top = 0.45 * 6.283185307179586;
		if (v.cur.inharm <= 0.0f) {
			const double c1 = std::cos(w), s1 = std::sin(w);
			double c = 1.0, s = 0.0;
			for (int n = 0; n < v.count; n++) {
				const double nc = c * c1 - s * s1;
				s = c * s1 + s * c1;
				c = nc;
				v.rc[n] = float(c);
				v.rs[n] = float(s);
				v.limit[n] = w * double(n + 1) < top ? 1.0f : 0.0f;
			}
		} else {
			for (int n = 0; n < v.count; n++) {
				const double wn = w * harm_ratio(v.cur, n + 1);
				v.rc[n] = float(std::cos(wn));
				v.rs[n] = float(std::sin(wn));
				v.limit[n] = wn < top ? 1.0f : 0.0f;
			}
		}
	}

	static void release(voice &v)
	{
		v.released = true;
		v.t_rel = 0;
	}

	void note_on(int ch, u8 key, u8 vel)
	{
		const chan &c = m_c[size_t(ch)];
		float tune_semis = 0.0f;
		const harm_detail::harm_drum *drum = c.drum ? &harm_detail::drum_for_key(key, tune_semis) : nullptr;
		const int group = drum ? drum->group : 0;
		// 同じ組（ハイハットの開閉）で鳴っている音は止める
		if (group)
			for (voice &o : m_v)
				if (o.on && o.ch == ch && o.group == group)
					o.on = false;
		voice *use = nullptr;
		for (voice &v : m_v)
			if (!v.on) {
				use = &v;
				break;
			}
		// 空きが無ければ、いちばん小さく鳴っている声を譲ってもらう
		if (!use) {
			float least = 1e9f;
			for (voice &v : m_v) {
				const float e = (v.rising ? 1.0f : v.env) * v.gain;
				if (e < least) {
					least = e;
					use = &v;
				}
			}
		}
		voice &v = *use;
		v.on = true;
		v.released = v.held = false;
		v.ch = ch;
		v.key = key;
		v.group = group;
		v.prog = drum ? -1 : int(c.program);
		load(v, drum ? drum->v : voice_of(c.program));
		v.base_key = (drum && drum->fixed_key > 0.0f ? drum->fixed_key : float(key)) + tune_semis;
		v.drop = v.cur.drop;
		v.age = ++m_age;
		v.lfsr = m_age * 2654435761u | 1u;
		v.vel = vel / 127.0f;
		v.gain = 0.12f * v.vel * v.vel;
		v.hz = 0.0;
		v.fresh = true;
		v.t = v.t_rel = 0;
		v.env = 0.0f;
		v.rising = true;
		v.nz_env = 1.0f;
		v.nz_s1 = v.nz_s2 = v.nz_now = v.nz_step = 0.0f;
		for (int n = 0; n < HARM_PARTIALS; n++) {
			v.re[n] = 1.0f;         // 矢印は右向きから始める（縦の成分 = 正弦が 0 から立ち上がる）
			v.im[n] = 0.0f;
			v.a[n] = v.da[n] = v.from[n] = 0.0f;
			v.limit[n] = 0.0f;
		}
		setup(v);
	}

	std::shared_ptr<const harm_bank> m_bank;
	std::array<voice, VOICES> m_v{};
	std::array<chan, 16> m_c = [] { std::array<chan, 16> a{}; a[9].drum = true; return a; }();
	double m_lfo = 0.0;
	u32 m_tick = 0, m_age = 0;
};

} // namespace smu2000::vboard

#endif // S_MU2000_VBOARD_HARM_H
