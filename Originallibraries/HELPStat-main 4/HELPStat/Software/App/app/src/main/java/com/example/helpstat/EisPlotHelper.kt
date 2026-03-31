package com.example.helpstat

import android.graphics.Color
import android.util.Log
import com.androidplot.ui.Anchor
import com.androidplot.xy.BoundaryMode
import com.androidplot.xy.LineAndPointFormatter
import com.androidplot.xy.SimpleXYSeries
import com.androidplot.xy.StepMode
import com.androidplot.xy.XYGraphWidget
import com.androidplot.xy.XYPlot
import com.androidplot.xy.XYSeries
import kotlin.math.abs
import kotlin.math.log10
import kotlin.math.sqrt

object EisPlotHelper {

    fun redrawNyquist(
        plot: XYPlot,
        listReal: List<Float>,
        listImag: List<Float>,
        calculatedRct: String?,
        calculatedRs: String?,
    ) {
        plot.clear()
        if (listReal.isEmpty() || listImag.isEmpty()) {
            plot.domainTitle.text = "Zreal (\u03a9)"
            plot.rangeTitle.text = "Zimag (\u03a9)"
            plot.redraw()
            return
        }
        val nyquist: XYSeries = SimpleXYSeries(listReal, listImag, "Impedance Data")
        val format = LineAndPointFormatter(null, Color.BLACK, null, null)
        plot.addSeries(nyquist, format)

        if (calculatedRct != null && calculatedRs != null) {
            try {
                val rct = calculatedRct.toFloat()
                val rs = calculatedRs.toFloat()
                val calculatedZreal = ((rs + 1).toInt()..((rs + rct).toInt()) step abs(rct / 10).toInt().coerceAtLeast(1)).toList()
                val calculatedZimag = calculatedZreal.map {
                    sqrt(rct * rct / 4 - (it - rs - rct / 2) * (it - rs - rct / 2))
                }
                val interpolatedFormat = LineAndPointFormatter(Color.BLACK, null, null, null)
                val interpolatedNyquist: XYSeries = SimpleXYSeries(calculatedZreal, calculatedZimag, "Fitted Data")
                plot.addSeries(interpolatedNyquist, interpolatedFormat)
                Log.i("INTERP: ", calculatedZimag.toString())
            } catch (_: Exception) { /* ignore bad fit params */ }
        }

        format.vertexPaint.strokeWidth = 24f
        plot.domainTitle.text = "Zreal (\u03a9)"
        plot.rangeTitle.text = "Zimag (\u03a9)"
        plot.domainTitle.positionMetrics.xPositionMetric = plot.title.positionMetrics.xPositionMetric
        plot.domainTitle.anchor = Anchor.BOTTOM_MIDDLE
        plot.graph.getLineLabelStyle(XYGraphWidget.Edge.LEFT).paint.textSize = 22f
        plot.graph.getLineLabelStyle(XYGraphWidget.Edge.BOTTOM).paint.textSize = 22f
        plot.legend.isVisible = false
        plot.setDomainLowerBoundary(0.0, BoundaryMode.FIXED)
        plot.setRangeLowerBoundary(0.0, BoundaryMode.FIXED)
        plot.redraw()
    }

    fun redrawBode(
        plotMagnitude: XYPlot,
        plotPhase: XYPlot,
        listFreq: List<Float>,
        listPhase: List<Float>,
        listMagnitude: List<Float>,
    ) {
        plotMagnitude.clear()
        plotPhase.clear()
        if (listFreq.isEmpty()) {
            plotMagnitude.redraw()
            plotPhase.redraw()
            return
        }

        val bodeMagnitude: XYSeries = SimpleXYSeries(
            listFreq.map { log10(it) },
            listMagnitude,
            "Impedance Data",
        )
        val formatMagnitude = LineAndPointFormatter(Color.BLACK, Color.BLACK, null, null)
        plotMagnitude.addSeries(bodeMagnitude, formatMagnitude)

        val bodePhase: XYSeries = SimpleXYSeries(
            listFreq.map { log10(it) },
            listPhase,
            "Impedance Data",
        )
        val formatPhase = LineAndPointFormatter(Color.BLACK, Color.BLACK, null, null)
        plotPhase.addSeries(bodePhase, formatPhase)

        formatMagnitude.vertexPaint.strokeWidth = 24f
        plotMagnitude.domainTitle.text = "log(frequency)"
        plotMagnitude.domainTitle.positionMetrics.xPositionMetric = plotMagnitude.title.positionMetrics.xPositionMetric
        plotMagnitude.domainTitle.anchor = Anchor.BOTTOM_MIDDLE
        plotMagnitude.rangeTitle.text = "Magnitude (\u03a9)"
        plotMagnitude.layoutManager.remove(plotMagnitude.legend)
        plotMagnitude.graph.getLineLabelStyle(XYGraphWidget.Edge.LEFT).paint.textSize = 22f
        plotMagnitude.graph.getLineLabelStyle(XYGraphWidget.Edge.BOTTOM).paint.textSize = 32f
        plotMagnitude.domainStepMode = StepMode.INCREMENT_BY_VAL
        plotMagnitude.domainStepValue = 1.0
        plotMagnitude.setRangeLowerBoundary(0.0, BoundaryMode.FIXED)
        plotMagnitude.redraw()

        formatPhase.vertexPaint.strokeWidth = 24f
        plotPhase.domainTitle.text = "log(frequency)"
        plotPhase.domainTitle.positionMetrics.xPositionMetric = plotPhase.title.positionMetrics.xPositionMetric
        plotPhase.domainTitle.anchor = Anchor.BOTTOM_MIDDLE
        plotPhase.rangeTitle.text = "Phase (\u00b0)"
        plotPhase.layoutManager.remove(plotPhase.legend)
        plotPhase.graph.getLineLabelStyle(XYGraphWidget.Edge.LEFT).paint.textSize = 24f
        plotPhase.graph.getLineLabelStyle(XYGraphWidget.Edge.BOTTOM).paint.textSize = 32f
        plotPhase.domainStepMode = StepMode.INCREMENT_BY_VAL
        plotPhase.domainStepValue = 1.0
        plotPhase.redraw()
    }
}
