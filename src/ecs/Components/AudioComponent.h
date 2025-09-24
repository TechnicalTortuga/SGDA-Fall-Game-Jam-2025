#pragma once

#include "../Component.h"
#include <string>

struct AudioComponent : public Component {
    const char* GetTypeName() const override { return "AudioComponent"; }

    // Audio category/type - determines default spatial behavior and processing
    enum class AudioType {
        SFX_3D,         // 3D positional sound effects (default)
        SFX_2D,         // 2D non-positional sound effects
        MUSIC,          // Background music (2D, high priority)
        UI,             // User interface sounds (2D, high priority)
        AMBIENT,        // Environmental/ambient sounds (often 3D)
        VOICE,          // Dialogue/voice audio (can be 2D or 3D)
        MASTER          // Global/system audio (2D, highest priority)
    };

    AudioType audioType = AudioType::SFX_3D; // Audio category

    // Audio clip properties
    std::string clipPath;                    // Path to audio file (relative to assets/)
    float volume = 1.0f;                     // Volume multiplier (0.0-1.0)
    float pitch = 1.0f;                      // Pitch multiplier (0.5-2.0)
    bool loop = false;                       // Loop playback
    bool playOnStart = false;                // Auto-play when entity is created

    // 3D spatial audio properties
    float spatialBlend = 0.0f;               // 2D=0.0, 3D=1.0
    float minDistance = 1.0f;                // Minimum attenuation distance
    float maxDistance = 50.0f;               // Maximum attenuation distance

    // Attenuation mode
    enum class RolloffMode {
        Linear,
        Logarithmic,
        Custom
    };
    RolloffMode rolloffMode = RolloffMode::Linear;

    // Advanced audio properties
    float dopplerLevel = 1.0f;               // Doppler effect intensity (0.0-5.0)
    float spread = 0.0f;                     // Stereo spread angle (0-360 degrees)
    float reverbZoneMix = 1.0f;              // Reverb zone mix (0.0-1.1)

    // Playback state (runtime)
    bool isPlaying = false;
    bool isPaused = false;
    float currentTime = 0.0f;                // Current playback position (seconds)

    // Audio source priority
    int priority = 128;                      // Playback priority (0-256, lower = higher priority)

    // Mute and bypass flags
    bool mute = false;
    bool bypassEffects = false;
    bool bypassListenerEffects = false;
    bool bypassReverbZones = false;

    // Output routing
    std::string outputAudioMixerGroup = "Master";  // Audio mixer group

    // Internal audio engine handles
    uint64_t audioSystemId = 0;              // For audio system integration
    uint64_t clipSystemId = 0;               // For audio clip management

    // Audio metadata (pure data)
    std::string audioName = "default_audio";
    bool isActive = true;
    bool needsAudioUpdate = true;
};
