// license:BSD-3-Clause
//
// 録った 1 音から、ハーモニックボード（src/vboard_harm.h）の音色を作る。「聞かせて真似させる」ための計算。
//
// やること:
//   1. 音の始まりと終わりを決める（大きさが山の 1% を超えた所から、5% を下回る所まで）
//   2. 基音の高さを測る（自己相関。山のあとの落ち着いた所で）
//   3. 4 つの時刻（鳴り始め・山・伸ばしている間・終わりぎわ）で、倍音 64 本の大きさを測る（窓を掛けたフーリエ変換を、
//      倍音の周波数の所だけ計算する）。倍音が整数倍から上へずれていれば、ずれた所を探して測り、ずれの量も出す
//   4. 大きさの移り変わりから、音量の包絡線（アタック・ディケイ・サステイン・リリース）と、4 枚の間を移る時間を決める
//
// 取り出すのは「倍音の大きさの数字」だけで、元の波形は音色に入らない。雑音の成分（息、打った瞬間の音）は測らないので、
// 出来た音色の雑音の帯は 0 のまま。必要なら後から足す。
//
// 和音や、高さが大きく動く音（ビブラートの深い音、グリッサンド）は測れない。1 音を、なるべく揺らさずに録ったものを渡す

#ifndef S_MU2000_HARM_ANALYZE_H
#define S_MU2000_HARM_ANALYZE_H

#pragma once

#include "vboard_harm.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace smu2000::vboard {

struct harm_analysis {
	harm_voice voice;
	double f0 = 0.0;            // 測った基音（Hz）
	int key = 60;               // いちばん近い鍵（MIDI の番号）
	double cents = 0.0;         // その鍵からのずれ（セント）
	double seconds = 0.0;       // 音の長さ（始まりから終わりまで）
	bool sustained = false;     // 伸ばしている音（true）か、鳴らしたら消えていく音（false）か
};

namespace harm_detail {

// x[at .. at+len) に Hann 窓を掛けて、周波数 hz の成分の大きさ（正弦波の振幅）を出す
inline double probe(const float *x, size_t at, size_t len, double hz, double rate)
{
	const double w = 6.283185307179586 * hz / rate;
	const double cw = std::cos(w), sw = std::sin(w);
	double c = 1.0, s = 0.0, re = 0.0, im = 0.0, wsum = 0.0;
	for (size_t i = 0; i < len; i++) {
		const double win = 0.5 - 0.5 * std::cos(6.283185307179586 * (double(i) + 0.5) / double(len));
		const double v = double(x[at + i]) * win;
		re += v * c;
		im += v * s;
		wsum += win;
		const double nc = c * cw - s * sw;
		s = c * sw + s * cw;
		c = nc;
	}
	return wsum > 0.0 ? 2.0 * std::hypot(re, im) / wsum : 0.0;
}

} // namespace harm_detail

// pcm: モノラル、±1.0。rate: サンプリング周波数。うまくいけば true。だめなら err に理由（英語の短い文）
inline bool harm_analyze(const float *pcm, size_t n, double rate, harm_analysis &out, std::string &err)
{
	out = harm_analysis();
	if (!pcm || n < 64 || !(rate >= 8000.0 && rate <= 192000.0)) {
		err = "no sound to analyze";
		return false;
	}
	float peak = 0.0f;
	for (size_t i = 0; i < n; i++) {
		if (!(pcm[i] == pcm[i])) {
			err = "the sound has broken values";
			return false;
		}
		peak = std::max(peak, std::fabs(pcm[i]));
	}
	if (peak < 1e-4f) {
		err = "the sound is silent";
		return false;
	}
	// 始まり: 山の 1% を超えた最初の所
	size_t start = 0;
	while (start < n && std::fabs(pcm[start]) < 0.01f * peak)
		start++;
	// 大きさの移り変わり（5ms ごと、10ms の窓の RMS）
	const size_t hop = std::max<size_t>(1, size_t(rate * 0.005)), win = hop * 2;
	std::vector<double> rms;
	for (size_t at = start; at + win <= n; at += hop) {
		double e = 0.0;
		for (size_t i = 0; i < win; i++)
			e += double(pcm[at + i]) * double(pcm[at + i]);
		rms.push_back(std::sqrt(e / double(win)));
	}
	if (rms.size() < 20) {
		err = "the sound is too short (0.1 s or more is needed)";
		return false;
	}
	size_t i_pk = 0;
	for (size_t i = 1; i < rms.size(); i++)
		if (rms[i] > rms[i_pk])
			i_pk = i;
	// 終わり: 山の 5% を最後に上回った所
	size_t i_end = rms.size() - 1;
	while (i_end > i_pk && rms[i_end] < 0.05 * rms[i_pk])
		i_end--;
	const double t_of = double(hop) / rate;         // rms の 1 目盛りの秒
	out.seconds = double(i_end + 2) * t_of;
	const size_t end = std::min(n, start + (i_end + 2) * hop);
	if (end - start < size_t(rate * 0.1)) {
		err = "the sound is too short (0.1 s or more is needed)";
		return false;
	}

	// ---- 基音。山のあとの落ち着いた所（山から終わりまでの 3 分の 1 の所）を中心に、自己相関で
	const size_t body = end - start;
	const size_t aw = std::min<size_t>(body, size_t(rate * 0.12));
	const size_t i_mid = i_pk + (i_end - i_pk) / 3;
	size_t a0 = start + i_mid * hop;
	if (a0 + aw > end)
		a0 = end - aw;
	const size_t lag_lo = std::max<size_t>(2, size_t(rate / 4000.0)), lag_hi = std::min<size_t>(aw / 2, size_t(rate / 30.0));
	if (lag_hi <= lag_lo + 2) {
		err = "the sound is too short to find its pitch";
		return false;
	}
	std::vector<double> ac(lag_hi + 2, 0.0);
	double best = 0.0;
	for (size_t lag = lag_lo; lag <= lag_hi + 1; lag++) {
		double xy = 0.0, xx = 0.0, yy = 0.0;
		const size_t m = aw - lag;
		for (size_t i = 0; i < m; i++) {
			const double x = pcm[a0 + i], y = pcm[a0 + i + lag];
			xy += x * y;
			xx += x * x;
			yy += y * y;
		}
		ac[lag] = xx > 0.0 && yy > 0.0 ? xy / std::sqrt(xx * yy) : 0.0;
		best = std::max(best, ac[lag]);
	}
	if (best < 0.5) {
		err = "no clear pitch was found (noise, or more than one note?)";
		return false;
	}
	// いちばん短い周期で、いちばん高い山の 9 割を超える山を取る（倍の周期を拾わないように）
	size_t lag = 0;
	for (size_t l = lag_lo + 1; l <= lag_hi; l++)
		if (ac[l] >= 0.9 * best && ac[l] >= ac[l - 1] && ac[l] >= ac[l + 1]) {
			lag = l;
			break;
		}
	if (!lag) {
		err = "no clear pitch was found";
		return false;
	}
	const double d = ac[lag - 1] - 2.0 * ac[lag] + ac[lag + 1];
	const double fine = std::fabs(d) > 1e-12 ? 0.5 * (ac[lag - 1] - ac[lag + 1]) / d : 0.0;
	double f0 = rate / (double(lag) + std::clamp(fine, -0.5, 0.5));

	// ---- 倍音を測る窓。基音 8 周期ぶん（短い音では縮める）
	const size_t len = std::clamp<size_t>(size_t(rate / f0 * 8.0), 256, std::max<size_t>(256, body / 2));
	if (len > body) {
		err = "the sound is too short for its pitch";
		return false;
	}
	const auto window_of = [&](double sec, size_t length) {        // 中心が sec（始まりからの秒）になるように。はみ出すぶんは寄せる
		const double centre = double(start) + sec * rate;
		const double lo = double(start), hi = double(end - length);
		return size_t(std::clamp(centre - double(length) / 2.0, lo, hi));
	};
	const auto window_at = [&](double sec) { return window_of(sec, len); };
	// 高さを細かく決めるための長い窓（0.15 秒まで）。短い窓では山が平たく、隣の倍音の漏れで 1% ほど動いてしまう
	const size_t long_len = std::clamp<size_t>(size_t(rate * 0.15), len, std::max(len, body / 2));
	// 基音をもう少し細かく決める。倍音がずれている音では、自己相関の出す周期は基音より数 % 高い側へ寄るので、
	// 前後 8% を 0.1% 刻みで探してから、見つけた所の前後 0.1% を 0.01% 刻みで探す
	{
		const size_t at = window_of(double(i_mid) * t_of, long_len);
		const auto search = [&](double centre, double span, int steps) {
			double bf = centre, bv = -1.0;
			for (int k = -steps; k <= steps; k++) {
				const double f = centre * (1.0 + span * k / steps);
				const double val = harm_detail::probe(pcm, at, long_len, f, rate);
				if (val > bv) {
					bv = val;
					bf = f;
				}
			}
			return bf;
		};
		f0 = search(search(f0, 0.08, 80), 0.001, 10);
	}
	out.f0 = f0;
	const double note = 69.0 + 12.0 * std::log2(f0 / 440.0);
	out.key = std::clamp(int(std::lround(note)), 0, 127);
	out.cents = (note - double(out.key)) * 100.0;

	// ---- 4 つの時刻
	const double t_pk = double(i_pk) * t_of, t_end = double(i_end) * t_of;
	const double half = double(len) / rate / 2.0;
	const double t0 = half;                                             // 取れるいちばん早い所
	const double t1 = std::max(t_pk, t0 + 0.005);
	const double t2 = t1 + (t_end - t1) * 0.5;
	const double t3 = t1 + (t_end - t1) * 0.9;
	const double times[HARM_FRAMES] = { t0, t1, t2, t3 };

	// ずれ: 山の所（倍音がいちばん多く出ている）で、n 本目が n × f0 からどれだけ上にあるかを下から順に探す。
	// B = ((f_n / (n f0))² − 1) / (n² − 1)。それまでに出た B の中央値で次の倍音の場所を見当づけて、その前後を探す
	// （ずれの大きい音では、上の倍音は整数倍から何割も離れる）
	double b_coef = 0.0;
	{
		const size_t at = window_of(t1, long_len);
		std::vector<double> bs;
		const double a1 = harm_detail::probe(pcm, at, long_len, f0, rate);
		double guess = 0.0;
		for (int h = 2; h <= 24; h++) {
			const double pred = f0 * double(h) * std::sqrt(1.0 + guess * (double(h) * double(h) - 1.0));
			const double lo = pred * 0.985, hi = pred * 1.07;
			if (hi >= rate * 0.45)
				break;
			double bf = 0.0, bv = 0.0;
			for (int k = 0; k <= 170; k++) {
				const double f = lo + (hi - lo) * k / 170.0;
				const double val = harm_detail::probe(pcm, at, long_len, f, rate);
				if (val > bv) {
					bv = val;
					bf = f;
				}
			}
			if (bv > 0.02 * a1) {
				const double r = bf / (f0 * double(h));
				bs.push_back(std::max(0.0, (r * r - 1.0) / (double(h) * double(h) - 1.0)));
				std::vector<double> sorted = bs;
				std::sort(sorted.begin(), sorted.end());
				guess = sorted[sorted.size() / 2];
			}
		}
		if (bs.size() >= 2 && guess > 2e-5)
			b_coef = std::min(guess, 0.01);
	}
	harm_voice &v = out.voice;
	v = harm_voice();
	v.inharm = float(std::sqrt(b_coef / 0.01));

	// ---- 倍音の大きさ。枚ごとに、いちばん大きい倍音を 1 にそろえる。山の 0.3% より小さいものは 0
	for (int f = 0; f < HARM_FRAMES; f++) {
		const size_t at = window_at(times[f]);
		double a[HARM_PARTIALS] = {}, top = 0.0;
		for (int h = 1; h <= HARM_PARTIALS; h++) {
			const double hz = f0 * harm_ratio(v, h);
			if (hz >= rate * 0.45)
				break;
			a[h - 1] = harm_detail::probe(pcm, at, len, hz, rate);
			top = std::max(top, a[h - 1]);
		}
		for (int h = 0; h < HARM_PARTIALS; h++) {
			const double x = top > 0.0 ? a[h] / top : 0.0;
			v.amp[f][h] = x >= 0.003 ? float(x) : 0.0f;
		}
	}

	// ---- 音量の包絡線
	const auto level_at = [&](double sec) {
		const size_t i = std::min(rms.size() - 1, size_t(std::max(0.0, sec / t_of)));
		return rms[i];
	};
	const double pk = rms[i_pk];
	// アタック: 山の 9 割に届くまで
	size_t i_att = 0;
	while (i_att < i_pk && rms[i_att] < 0.9 * pk)
		i_att++;
	v.attack = float(std::clamp(double(i_att) * t_of, 0.001, 5.0));
	// 伸ばしている音か: 山から終わりまでの 4 割の所と 7 割の所の大きさが 3dB 以内
	const double la = level_at(t_pk + (t_end - t_pk) * 0.4), lb = level_at(t_pk + (t_end - t_pk) * 0.7);
	out.sustained = lb > 0.0 && la / lb < 1.41 && lb > 0.15 * pk && (t_end - t_pk) > 0.15;
	if (out.sustained) {
		v.sustain = float(std::clamp(0.5 * (la + lb) / pk, 0.05, 1.0));
		// 山から落ち着くまでの時間を、60dB ぶんに直す
		size_t i_set = i_pk;
		while (i_set < i_end && rms[i_set] > 1.1 * double(v.sustain) * pk)
			i_set++;
		const double drop_db = -20.0 * std::log10(std::max(1e-6, double(v.sustain)));
		v.decay = v.sustain >= 0.95f || drop_db < 0.5 ? 0.0f : float(std::clamp(double(i_set - i_pk) * t_of * 60.0 / drop_db, 0.01, 30.0));
		v.release = 0.15f;
	} else {
		// 消えていく音: 山から終わり（山の 5%）までで 26dB 下がっている
		v.sustain = 0.0f;
		v.decay = float(std::clamp((t_end - t_pk) * 60.0 / 26.0, 0.02, 30.0));
		v.release = float(std::clamp(double(v.decay) * 0.25, 0.03, 0.5));
	}
	v.t_peak = float(std::clamp(t1 - t0, 0.001, 30.0));
	v.t_sustain = float(std::clamp(t2 - t1, 0.001, 30.0));
	v.bright = 0.3f;
	return true;
}

} // namespace smu2000::vboard

#endif // S_MU2000_HARM_ANALYZE_H
