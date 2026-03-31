package com.example.helpstat

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/**
 * Persists completed EIS sweeps as JSON under [Context.filesDir]/helpstat_history/YYYY-MM-DD.json
 * using Pacific (America/Los_Angeles) calendar days.
 */
object EisHistoryStore {
    private const val DIR_NAME = "helpstat_history"

    private fun dir(context: Context): File {
        val d = File(context.filesDir, DIR_NAME)
        if (!d.exists()) d.mkdirs()
        return d
    }

    private fun fileForDay(context: Context, pstDateKey: String): File =
        File(dir(context), "$pstDateKey.json")

    /**
     * Sorted newest-first list of day keys that have saved data.
     */
    fun listDayKeys(context: Context): List<String> {
        val names = dir(context).listFiles()
            ?.mapNotNull { f ->
                if (f.isFile && f.name.endsWith(".json")) f.name.removeSuffix(".json") else null
            }
            .orEmpty()
        return names.sortedDescending()
    }

    fun loadDayFileJson(context: Context, pstDateKey: String): JSONObject? {
        val f = fileForDay(context, pstDateKey)
        if (!f.exists() || !f.canRead()) return null
        return try {
            JSONObject(f.readText())
        } catch (_: Exception) {
            null
        }
    }

    fun loadSweepsForDay(context: Context, pstDateKey: String): List<EisSweepRecord> {
        val root = loadDayFileJson(context, pstDateKey) ?: return emptyList()
        val arr = root.optJSONArray("sweeps") ?: return emptyList()
        val out = ArrayList<EisSweepRecord>(arr.length())
        for (i in 0 until arr.length()) {
            val o = arr.optJSONObject(i) ?: continue
            out.add(EisSweepRecord.fromJson(o))
        }
        return out
    }

    /**
     * Saves the current sweep if frequency/real/imag lengths match. Returns true if written.
     */
    fun appendCompletedSweep(context: Context): Boolean {
        val n = data_main.listFreq.size
        if (n == 0 || data_main.listReal.size != n || data_main.listImag.size != n) return false
        if (data_main.listPhase.size != n || data_main.listMagnitude.size != n) return false

        val now = System.currentTimeMillis()
        val dayKey = PstTime.dateKeyUtcMillis(now)
        val points = JSONArray()
        for (i in 0 until n) {
            points.put(
                JSONObject().apply {
                    put("f", data_main.listFreq[i].toDouble())
                    put("re", data_main.listReal[i].toDouble())
                    put("im", data_main.listImag[i].toDouble())
                    put("ph", data_main.listPhase[i].toDouble())
                    put("mag", data_main.listMagnitude[i].toDouble())
                }
            )
        }
        val sweep = JSONObject().apply {
            put("savedAtUtcMs", now)
            put("pstDateKey", dayKey)
            put("pstTimeLabel", PstTime.timeLabelUtcMillis(now))
            put("rct", data_main.calculated_rct ?: "")
            put("rs", data_main.calculated_rs ?: "")
            put("points", points)
        }

        val f = fileForDay(context, dayKey)
        val root = if (f.exists()) {
            try {
                JSONObject(f.readText())
            } catch (_: Exception) {
                JSONObject().put("sweeps", JSONArray())
            }
        } else {
            JSONObject().put("sweeps", JSONArray())
        }
        val sweeps = root.optJSONArray("sweeps") ?: JSONArray()
        sweeps.put(sweep)
        root.put("sweeps", sweeps)
        root.put("version", 1)
        root.put("pstZone", "America/Los_Angeles")

        f.writeText(root.toString(2))
        return true
    }
}

data class EisSweepRecord(
    val savedAtUtcMs: Long,
    val pstDateKey: String,
    val pstTimeLabel: String,
    val rct: String?,
    val rs: String?,
    val freq: FloatArray,
    val real: FloatArray,
    val imag: FloatArray,
    val phase: FloatArray,
    val magnitude: FloatArray,
) {
    companion object {
        fun fromJson(o: JSONObject): EisSweepRecord {
            val pts = o.optJSONArray("points") ?: JSONArray()
            val n = pts.length()
            val f = FloatArray(n)
            val re = FloatArray(n)
            val im = FloatArray(n)
            val ph = FloatArray(n)
            val mg = FloatArray(n)
            for (i in 0 until n) {
                val p = pts.optJSONObject(i) ?: continue
                f[i] = p.optDouble("f", 0.0).toFloat()
                re[i] = p.optDouble("re", 0.0).toFloat()
                im[i] = p.optDouble("im", 0.0).toFloat()
                ph[i] = p.optDouble("ph", 0.0).toFloat()
                mg[i] = p.optDouble("mag", 0.0).toFloat()
            }
            return EisSweepRecord(
                savedAtUtcMs = o.optLong("savedAtUtcMs", 0L),
                pstDateKey = o.optString("pstDateKey", ""),
                pstTimeLabel = o.optString("pstTimeLabel", ""),
                rct = o.optString("rct", "").ifEmpty { null },
                rs = o.optString("rs", "").ifEmpty { null },
                freq = f,
                real = re,
                imag = im,
                phase = ph,
                magnitude = mg,
            )
        }
    }

    fun label(): String = pstTimeLabel.ifEmpty {
        "${savedAtUtcMs / 1000L}s"
    }
}
