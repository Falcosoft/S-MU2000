// Message protocol between the live page (main thread) and the synth
// AudioWorklet processor. Types only; imported with `import type`.
export interface RomImage {
    kind: number;
    data: ArrayBuffer;
}

export type MainToWorklet =
    | { type: "init"; roms: RomImage[] }
    | { type: "midi"; port: number; bytes: number[] }
    | { type: "panic" };

export type WorkletToMain =
    | { type: "boot"; fraction: number; text: string }
    | { type: "live" }
    | { type: "error"; message: string };
