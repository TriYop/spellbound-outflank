/*
 * Spellbound Outflank — DPF plugin metadata.
 *
 * This file's macro set follows DPF's own DistrhoPluginInfo.h.template
 * and the shape of Hex's `Source/DistrhoPluginInfo.h`.
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
#define DISTRHO_UI_FILE_BROWSER   1

/*
 * Host parameter indices, shared between the DSP adapter and the UI.
 * Declared here (rather than in OutflankPluginAdapter.h) so the UI
 * translation unit can use them without pulling in DistrhoPlugin.hpp --
 * same placement Hex's DistrhoPluginInfo.h uses.
 */
enum OutflankParameters {
    kParameterFrequency = 0,
    kParameterQ,
    kParameterRejection,
    kParameterCount   // 3 -- no bypass, no meters, on any format
};

/* Ranges/defaults carried over verbatim from the JUCE-era
   Source/_juce_reference/PluginProcessor.cpp createParameterLayout(). */
#define OUTFLANK_PARAM_FREQUENCY_MIN     40.0f
#define OUTFLANK_PARAM_FREQUENCY_MAX     2000.0f
#define OUTFLANK_PARAM_FREQUENCY_DEFAULT 250.0f

#define OUTFLANK_PARAM_Q_MIN     0.3f
#define OUTFLANK_PARAM_Q_MAX     4.0f
#define OUTFLANK_PARAM_Q_DEFAULT 0.707f

#define OUTFLANK_PARAM_REJECTION_MIN     0.0f
#define OUTFLANK_PARAM_REJECTION_MAX     100.0f
#define OUTFLANK_PARAM_REJECTION_DEFAULT 0.0f

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
