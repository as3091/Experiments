package com.Apoorv.tvaudioswitcher

import android.content.Context
import android.util.Log
import java.io.File
import java.io.FileWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object FileLogger {
    private var logFile: File? = null
    private val dateFormat = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.getDefault())
    private val logEntryFormat = SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.getDefault())

    fun initialize(context: Context) {
        val logsDir = File(context.filesDir, "logs")
        if (!logsDir.exists()) {
            logsDir.mkdirs()
        }
        val timestamp = dateFormat.format(Date())
        logFile = File(logsDir, "${timestamp}.log")
        log("SYSTEM", "Logger initialized. File: ${logFile?.absolutePath}")
    }

    fun log(tag: String, message: String) {
        val timestamp = logEntryFormat.format(Date())
        val logLine = "$timestamp [$tag] $message\n"

        // Also print to standard Logcat
        Log.d(tag, message)

        try {
            logFile?.let {
                FileWriter(it, true).use { writer ->
                    writer.append(logLine)
                }
            }
        } catch (e: Exception) {
            Log.e("FileLogger", "Failed to write to log file", e)
        }
    }

    fun getLogFilePath(): String? = logFile?.absolutePath
}
