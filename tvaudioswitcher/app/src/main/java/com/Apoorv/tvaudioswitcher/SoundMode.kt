package com.Apoorv.tvaudioswitcher

enum class SoundMode(val id: String, val label: String) {
    INTERNAL("tv_speaker", "Internal Speakers"),
    ARC("external_arc", "HDMI ARC"),
    ORCHESTRA("tv_speaker_arc", "WOW Orchestra");

    companion object {
        fun fromId(id: String): SoundMode {
            return entries.find { it.id == id } ?: INTERNAL
        }

        fun next(currentId: String): SoundMode {
            val current = fromId(currentId)
            return when (current) {
                INTERNAL -> ARC
                ARC -> ORCHESTRA
                ORCHESTRA -> INTERNAL
            }
        }
    }
}
