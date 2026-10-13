"""MOTIF-RACK ES のノーマルボイスのエレメントの欄の表。Data List の「MIDI PARAMETER CHANGE TABLE (NORMAL VOICE ELEMENT)」から。

Data List の PDF は 2 段組が崩れて文字起こしされるので、番地と大きさの並び・名前の並びを別々に拾って、数で突き合わせて作った。
実機で確かめた欄には OK と書いた（パラメーターチェンジで書いて音や読み戻しで確かめたもの）。それ以外は表からの読みで、ずれている可能性がある。

  BLOCKS[(番地の 1 バイトめ, 3 バイトめ)] = [(区画の中の位置, バイト数, 名前), …]
2 バイトの値は「上位 7bit、下位 7bit」の順。"""

ELEMENT_BLOCKS = {
    (0x41, 0x00): [   # 発振。29 バイト
        (0x00, 1, "element_switch"),            # OK
        (0x03, 2, "wave_number"),               # OK
        (0x08, 1, "pan"),                       # OK  1-127、64 が真ん中
        (0x09, 1, "random_pan_depth"),
        (0x0A, 1, "alternate_pan_depth"),       # OK  64 が「効かせない」
        (0x0B, 1, "scaling_pan_depth"),
        (0x0C, 1, "note_limit_low"),            # OK
        (0x0D, 1, "note_limit_high"),           # OK
        (0x0E, 1, "velocity_limit_low"),        # OK
        (0x0F, 1, "velocity_limit_high"),       # OK
        (0x10, 1, "velocity_cross_fade"),
        (0x11, 1, "key_on_delay"),
        (0x12, 1, "key_on_delay_tempo_sync"),
        (0x16, 1, "insertion_effect_switch"),   # 0 thru / 1 ins1 / 2 ins2
        (0x18, 2, "control_box_sw"),
        (0x1A, 2, "lfo_box_sw"),
        (0x1C, 1, "key_on_delay_tempo"),
    ],
    (0x41, 0x20): [   # アンプ。30 バイト
        (0x00, 1, "element_level"),             # OK
        (0x01, 1, "level_velocity_sensitivity"),  # OK  64 が 0
        (0x02, 1, "level_velocity_curve"),
        (0x04, 1, "aeg_attack_time"),           # OK
        (0x05, 1, "aeg_decay1_time"),           # OK
        (0x06, 1, "aeg_decay2_time"),
        (0x07, 1, "aeg_release_time"),          # OK
        (0x08, 1, "aeg_init_level"),            # OK
        (0x09, 1, "aeg_attack_level"),
        (0x0A, 1, "aeg_decay1_level"),          # OK
        (0x0B, 1, "aeg_decay2_level"),          # OK
        (0x0D, 1, "aeg_time_velocity_segment"),
        (0x0E, 1, "aeg_time_velocity_sensitivity"),
        (0x0F, 1, "level_key_follow_sensitivity"),
        (0x10, 1, "level_scaling_break_point1"),
        (0x11, 1, "level_scaling_break_point2"),
        (0x12, 1, "level_scaling_break_point3"),
        (0x13, 1, "level_scaling_break_point4"),
        (0x14, 2, "level_scaling_offset1"),
        (0x16, 2, "level_scaling_offset2"),
        (0x18, 2, "level_scaling_offset3"),
        (0x1A, 2, "level_scaling_offset4"),
        (0x1C, 1, "aeg_time_key_follow_sensitivity"),
        (0x1D, 1, "aeg_time_key_follow_center_note"),
    ],
    (0x42, 0x00): [   # 音程。30 バイト
        (0x00, 1, "coarse_tune"),               # 0x40 が 0
        (0x01, 1, "fine_tune"),
        (0x02, 1, "pitch_velocity_sensitivity"),
        (0x04, 1, "random_pitch_depth"),
        (0x05, 1, "peg_hold_time"),
        (0x06, 1, "peg_attack_time"),
        (0x07, 1, "peg_decay1_time"),
        (0x08, 1, "peg_decay2_time"),
        (0x09, 1, "peg_release_time"),
        (0x0A, 2, "peg_hold_level"),
        (0x0C, 2, "peg_attack_level"),
        (0x0E, 2, "peg_decay1_level"),
        (0x10, 2, "peg_decay2_level"),
        (0x12, 2, "peg_release_level"),
        (0x14, 1, "peg_depth"),
        (0x15, 1, "peg_time_velocity_segment"),
        (0x16, 1, "peg_time_velocity_sensitivity"),
        (0x17, 1, "peg_level_velocity_sensitivity"),
        (0x18, 1, "peg_level_velocity_curve"),
        (0x19, 1, "pitch_key_follow_sensitivity"),
        (0x1A, 1, "pitch_key_follow_center_note"),
        (0x1B, 1, "peg_time_key_follow_sensitivity"),
        (0x1C, 1, "peg_time_key_follow_center_note"),
        (0x1D, 1, "pitch_fine_scaling_sensitivity"),
    ],
    (0x43, 0x00): [   # フィルター。50 バイト
        (0x00, 1, "filter_type"),               # OK  0 = LPF24D
        (0x01, 2, "filter_cutoff"),             # OK  0-255
        (0x03, 1, "filter_cutoff_velocity_sensitivity"),
        (0x05, 1, "filter_resonance"),          # OK
        (0x06, 1, "filter_resonance_velocity_sensitivity"),
        (0x07, 2, "hpf_cutoff"),
        (0x0A, 2, "distance"),
        (0x0C, 2, "filter_gain"),
        (0x0E, 1, "feg_hold_time"),
        (0x0F, 1, "feg_attack_time"),
        (0x10, 1, "feg_decay1_time"),
        (0x11, 1, "feg_decay2_time"),
        (0x12, 1, "feg_release_time"),
        (0x13, 2, "feg_hold_level"),
        (0x15, 2, "feg_attack_level"),
        (0x17, 2, "feg_decay1_level"),
        (0x19, 2, "feg_decay2_level"),
        (0x1B, 2, "feg_release_level"),
        (0x1D, 1, "feg_depth"),
        (0x1E, 1, "feg_time_velocity_segment"),
        (0x1F, 1, "feg_time_velocity_sensitivity"),
        (0x20, 1, "feg_level_velocity_sensitivity"),
        (0x21, 1, "feg_level_velocity_curve"),
        (0x22, 1, "filter_cutoff_key_follow_sensitivity"),
        (0x23, 1, "filter_cutoff_scaling_break_point1"),
        (0x24, 1, "filter_cutoff_scaling_break_point2"),
        (0x25, 1, "filter_cutoff_scaling_break_point3"),
        (0x26, 1, "filter_cutoff_scaling_break_point4"),
        (0x27, 2, "filter_cutoff_scaling_offset1"),
        (0x29, 2, "filter_cutoff_scaling_offset2"),
        (0x2B, 2, "filter_cutoff_scaling_offset3"),
        (0x2D, 2, "filter_cutoff_scaling_offset4"),
        (0x2F, 1, "hpf_cutoff_key_follow_sensitivity"),
        (0x30, 1, "feg_time_key_follow_sensitivity"),
        (0x31, 1, "feg_time_key_follow_center_note"),
    ],
    (0x44, 0x00): [   # EQ。8 バイト
        (0x00, 1, "eq_type"),                   # 0 EQ L/H、1 P.EQ、2-4 Boost 6/12/18、5 thru
        (0x01, 1, "eq_resonance"),
        (0x02, 2, "eq1_frequency"),
        (0x04, 1, "eq1_gain"),
        (0x05, 2, "eq2_frequency"),
        (0x07, 1, "eq2_gain"),
    ],
    (0x44, 0x40): [   # エレメントの LFO。8 バイト
        (0x00, 1, "lfo_wave"),                  # 0 saw、1 tri、2 squ
        (0x01, 1, "lfo_key_on_sync"),
        (0x02, 1, "lfo_key_on_delay_time"),
        (0x03, 1, "lfo_speed"),                 # OK  0-63
        (0x04, 1, "lfo_amod_depth"),
        (0x05, 1, "lfo_pmod_depth"),            # OK
        (0x06, 1, "lfo_fmod_depth"),
        (0x07, 1, "lfo_fade_in_time"),
    ],
    (0x44, 0x50): [   # 共通 LFO の受け方。4 バイト
        (0x00, 1, "common_lfo_phase_offset"),
        (0x01, 1, "common_lfo_box1_depth_ratio"),
        (0x02, 1, "common_lfo_box2_depth_ratio"),
        (0x03, 1, "common_lfo_box3_depth_ratio"),
    ],
}


def decode(block_key, data):
    """区画のバイト列を {名前: 値} に。表に無い位置は捨てる"""
    out = {}
    for off, size, name in ELEMENT_BLOCKS[block_key]:
        if off + size > len(data):
            continue
        out[name] = data[off] if size == 1 else (data[off] << 7 | data[off + 1])
    return out
