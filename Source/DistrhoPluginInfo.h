/*
 * Spellbound Outflank — DPF plugin metadata.
 *
 * This file's macro set follows DPF's own DistrhoPluginInfo.h.template
 * and the shape of Hex's DistrhoPluginInfo.h (see
 * AudioPlugins/Hex/.claude/worktrees/dpf-stage0/Source/DistrhoPluginInfo.h).
 *
 * Outflank is a stereo effect, not a synth: it does not want MIDI input,
 * and it processes two audio inputs into two audio outputs. Unlike Hex, it
 * reports no latency (CrossoverMS is a zero-latency filter chain — no
 * delay lines) and declares no bypass parameter (the pre-migration JUCE
 * processor never had one either, confirmed by reading
 * Source/_juce_reference/PluginProcessor.cpp).
 */

#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "Spellbound"
#define DISTRHO_PLUGIN_NAME    "Outflank"
#define DISTRHO_PLUGIN_URI     "https://spellbound.audio/plugins/outflank"
#define DISTRHO_PLUGIN_CLAP_ID "com.spellbound.outflank"

#define DISTRHO_PLUGIN_BRAND_ID  Spbd
#define DISTRHO_PLUGIN_UNIQUE_ID Otfk

#define DISTRHO_PLUGIN_HAS_UI      1
#define DISTRHO_PLUGIN_IS_RT_SAFE  1
#define DISTRHO_PLUGIN_IS_SYNTH    0
#define DISTRHO_PLUGIN_NUM_INPUTS  2
#define DISTRHO_PLUGIN_NUM_OUTPUTS 2
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT 0
#define DISTRHO_PLUGIN_WANT_LATENCY    0

#define DISTRHO_PLUGIN_VST3_CATEGORIES "Fx|Stereo"
#define DISTRHO_PLUGIN_CLAP_FEATURES   "audio-effect", "utility", "stereo"

#define DISTRHO_UI_USE_NANOVG     1
#define DISTRHO_UI_USER_RESIZABLE 0
#define DISTRHO_UI_DEFAULT_WIDTH  320
#define DISTRHO_UI_DEFAULT_HEIGHT 200

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
