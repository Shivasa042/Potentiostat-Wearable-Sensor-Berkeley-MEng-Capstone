package com.example.helpstat

import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Date
import java.util.Locale
import java.util.TimeZone

/**
 * Calendar-day boundaries in [America/Los_Angeles] (Pacific Time, automatic PST/PDT).
 */
object PstTime {
    private val zone: TimeZone get() = TimeZone.getTimeZone("America/Los_Angeles")

    /** ISO-style key `yyyy-MM-dd` for the given instant in Pacific time. */
    fun dateKeyUtcMillis(utcMillis: Long = System.currentTimeMillis()): String {
        val cal = Calendar.getInstance(zone).apply { timeInMillis = utcMillis }
        val y = cal.get(Calendar.YEAR)
        val m = cal.get(Calendar.MONTH) + 1
        val d = cal.get(Calendar.DAY_OF_MONTH)
        return String.format(Locale.US, "%04d-%02d-%02d", y, m, d)
    }

    /** Human-readable local Pacific time for labels (e.g. "3:45 PM PST"). */
    fun timeLabelUtcMillis(utcMillis: Long = System.currentTimeMillis()): String {
        val fmt = SimpleDateFormat("h:mm a zzz", Locale.US).apply { timeZone = zone }
        return fmt.format(Date(utcMillis))
    }
}
