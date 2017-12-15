/*
Copyright 2017 Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef METRICS_GUI_H
#define METRICS_GUI_H

#include <stdint.h>
#include <string>
#include <vector>

struct MetricsGuiMetric {
    enum Flags {
        NONE                    = 0,
        USE_SI_UNIT_PREFIX      = 1u << 1,
        KNOWN_MIN_VALUE         = 1u << 2,
        KNOWN_MAX_VALUE         = 1u << 3,
    };

    enum { NUM_HISTORY_SAMPLES = 256 };

    std::string mDescription;
    std::string mUnits;
    double mTotalInHistory; // needs to be double for precision reasons (accumulating small deltas)
    uint32_t mHistoryCount;
    float mColor[4];
    float mHistory[NUM_HISTORY_SAMPLES];    // Don't forget to update mTotalInHistory if you modify this outside of AddNewValue()
    float mKnownMinValue;
    float mKnownMaxValue;
    uint32_t mFlags;
    bool mSelected;

    MetricsGuiMetric();
    MetricsGuiMetric(char const* description, char const* units, uint32_t flags);
    void Initialize(char const* description, char const* units, uint32_t flags);

    void AddNewValue(float value);
    float GetAverageValue() const;

    // Get and set values in the history buffer.  prevIndex==0 gets/sets last
    // value added, prevIndex==NUM_HISTORY_SAMPLES-1 gets/sets the oldest
    // stored value.
    void SetLastValue(float value, uint32_t prevIndex = 0);
    float GetLastValue(uint32_t prevIndex = 0) const;
};

struct MetricsGuiPlot {
    struct WidthInfo {
        std::vector<MetricsGuiPlot*> mLinkedPlots;
        float mDescWidth;
        float mValueWidth;
        float mLegendWidth;
        bool mInitialized;
        explicit WidthInfo(MetricsGuiPlot* plot);
        void Initialize();
    };

    std::vector<MetricsGuiMetric*> mMetrics;
    std::vector<std::pair<float, float> > mMetricRange;
    WidthInfo* mWidthInfo;
    float mMinValue;
    float mMaxValue;
    bool mRangeInitialized;

    // Draw/update options:
    float mBarRounding;             // amount of rounding on bars
    float mRangeDampening;          // weight of historic range on axis range [0,1]
    uint32_t mInlinePlotRowCount;   // height of DrawList() inline plots, in text rows
    uint32_t mPlotRowCount;         // height of DrawHistory() plots, in text rows
    uint32_t mVBarMinWidth;         // min width of bar graph bar in pixels
    uint32_t mVBarGapWidth;         // width of bar graph inter-bar gap in pixels
    bool mShowAverage;              // draw horizontal line at series average
    bool mShowInlineGraphs;         // show history plot in DrawList()
    bool mShowOnlyIfSelected;       // draw show selected metrics
    bool mShowLegendDesc;           // show series description in legend
    bool mShowLegendColor;          // use series color in legend
    bool mShowLegendUnits;          // show units in legend values
    bool mShowLegendAverage;        // show series average in legend
    bool mShowLegendMin;            // show plot y-axis minimum in legend
    bool mShowLegendMax;            // show plot y-axis maximum in legend
    bool mBarGraph;                 // use bars to draw history
    bool mStacked;                  // stack series when drawing history
    bool mSharedAxis;               // use first series' axis range
    bool mFilterHistory;            // allow single plot point to represent more than on history value

    MetricsGuiPlot();
    MetricsGuiPlot(MetricsGuiPlot const& copy);
    ~MetricsGuiPlot();

    void AddMetric(MetricsGuiMetric* metric);
    void AddMetrics(MetricsGuiMetric* metrics, size_t metricCount);

    void SortMetricsByName();

    // Linking legends of multiple plots makes their legend widths the same.
    void LinkLegends(MetricsGuiPlot* plot);

    void UpdateAxes();

    // -----------------------------------------------------------------
    // | description | padding | bar........ | padding | quanity units |
    // -----------------------------------------------------------------
    // | inline plot.............................| Max: quantity units |
    // | ........................................| Min: quantity units |
    // -----------------------------------------------------------------
    void DrawList();

    // -----------------------------------------------------------------
    // | plot....................................| Description         |
    // | ........................................| Max: quantity units |
    // | ........................................| Cur: quantity units |
    // | ........................................| Min: quantity units |
    // -----------------------------------------------------------------
    void DrawHistory();
};

#endif // ifndef METRICS_GUI_H
