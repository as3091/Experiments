package com.Apoorv.tvaudioswitcher

import kotlinx.serialization.Serializable

@Serializable
data class SmartTv(
    val name: String,
    val ip: String,
    val isPaired: Boolean = false
)
