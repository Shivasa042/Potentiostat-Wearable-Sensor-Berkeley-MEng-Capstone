package com.example.helpstat

import android.os.Bundle
import android.view.View
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.Spinner
import android.widget.TextView
import androidx.activity.ComponentActivity
import com.androidplot.xy.XYPlot

/**
 * Browse saved EIS sweeps by Pacific calendar day (fitness-style history) and plot Nyquist / Bode.
 */
class DataVisualizationActivity : ComponentActivity() {

    private lateinit var spinnerDay: Spinner
    private lateinit var spinnerSweep: Spinner
    private lateinit var summary: TextView
    private lateinit var plotNyquist: XYPlot
    private lateinit var plotBodeMag: XYPlot
    private lateinit var plotBodePhase: XYPlot

    private var dayKeys: List<String> = emptyList()
    private var sweepsForDay: List<EisSweepRecord> = emptyList()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_data_visualization)

        spinnerDay = findViewById(R.id.spinner_day)
        spinnerSweep = findViewById(R.id.spinner_sweep)
        summary = findViewById(R.id.text_rct_rs_summary)
        plotNyquist = findViewById(R.id.dv_xy_Nyquist)
        plotBodeMag = findViewById(R.id.dv_xy_Bode_Magnitude)
        plotBodePhase = findViewById(R.id.dv_xy_Bode_Phase)

        title = getString(R.string.data_visualization_title)
        refreshDaySpinner()

        spinnerDay.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                if (dayKeys.isEmpty()) return
                val key = dayKeys[position]
                sweepsForDay = EisHistoryStore.loadSweepsForDay(this@DataVisualizationActivity, key)
                refreshSweepSpinner()
            }

            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }

        spinnerSweep.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                if (sweepsForDay.isEmpty()) {
                    clearPlots()
                    return
                }
                val rec = sweepsForDay.getOrNull(position) ?: return
                showSweep(rec)
            }

            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }
    }

    override fun onResume() {
        super.onResume()
        refreshDaySpinner()
    }

    private fun refreshDaySpinner() {
        dayKeys = EisHistoryStore.listDayKeys(this)
        val adapter = ArrayAdapter(
            this,
            android.R.layout.simple_spinner_item,
            if (dayKeys.isEmpty()) listOf(getString(R.string.no_saved_days)) else dayKeys,
        )
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        spinnerDay.adapter = adapter

        if (dayKeys.isNotEmpty()) {
            val preferred = intent.getStringExtra(EXTRA_OPEN_DAY)
            val idx = if (preferred != null) dayKeys.indexOf(preferred).coerceAtLeast(0) else 0
            spinnerDay.setSelection(idx)
            val key = dayKeys[idx]
            sweepsForDay = EisHistoryStore.loadSweepsForDay(this, key)
            refreshSweepSpinner()
        } else {
            sweepsForDay = emptyList()
            refreshSweepSpinner()
            clearPlots()
            summary.text = getString(R.string.connect_and_run_hint)
        }
    }

    private fun refreshSweepSpinner() {
        val labels = sweepsForDay.mapIndexed { i, r ->
            getString(R.string.sweep_number_time, i + 1, r.label())
        }
        val adapter = ArrayAdapter(
            this,
            android.R.layout.simple_spinner_item,
            if (labels.isEmpty()) listOf(getString(R.string.no_sweeps_this_day)) else labels,
        )
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        spinnerSweep.adapter = adapter
        if (sweepsForDay.isNotEmpty()) {
            spinnerSweep.setSelection(sweepsForDay.size - 1)
            showSweep(sweepsForDay.last())
        } else {
            clearPlots()
            summary.text = getString(R.string.no_sweeps_this_day)
        }
    }

    private fun showSweep(rec: EisSweepRecord) {
        summary.text = getString(
            R.string.rct_rs_line,
            rec.rct ?: "—",
            rec.rs ?: "—",
        )
        val f = rec.freq.toList()
        val re = rec.real.toList()
        val im = rec.imag.toList()
        val ph = rec.phase.toList()
        val mg = rec.magnitude.toList()
        if (f.size != re.size || re.size != im.size) {
            clearPlots()
            return
        }
        EisPlotHelper.redrawNyquist(plotNyquist, re, im, rec.rct, rec.rs)
        if (ph.size == f.size && mg.size == f.size) {
            EisPlotHelper.redrawBode(plotBodeMag, plotBodePhase, f, ph, mg)
        }
    }

    private fun clearPlots() {
        summary.text = getString(R.string.rct_rs_line, "—", "—")
        EisPlotHelper.redrawNyquist(plotNyquist, emptyList(), emptyList(), null, null)
        EisPlotHelper.redrawBode(plotBodeMag, plotBodePhase, emptyList(), emptyList(), emptyList())
    }

    companion object {
        const val EXTRA_OPEN_DAY = "open_pst_day"
    }
}
