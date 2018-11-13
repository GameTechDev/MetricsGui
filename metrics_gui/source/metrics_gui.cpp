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

#include "../../imgui/imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../../imgui/imgui_internal.h"
#include "../include/metrics_gui/metrics_gui.h"
#include "../../portable/countof.h"
#include "../../portable/snprintf.h"

#include <algorithm>
#include <assert.h>

namespace {

static float const HBAR_PADDING_TOP             =  2.f;
static float const HBAR_PADDING_BOTTOM          =  2.f;
static float const DESC_HBAR_PADDING            =  8.f;
static float const HBAR_VALUE_PADDING           =  8.f;
static float const PLOT_LEGEND_PADDING          =  8.f;
static float const LEGEND_TEXT_VERTICAL_SPACING =  2.f;

uint32_t gConstructedMetricIndex = 0;

int CreateQuantityLabel(
    char* memory,
    size_t memorySize,
    float quantity,
    char const* units,
    char const* prefix,
    bool useSiUnitPrefix)
{
    enum {
        NANO,
        MICRO,
        MILLI,
        NONE,
        KILO,
        MEGA,
        GIGA,
        TERA,
        NUM_SI_PREFIXES,
    };
    char const* siPrefixChar = "num kMGT";
    uint32_t siPrefix = NONE;
    double value = (double) quantity;

    // Adjust SI magnitude if requested
    if (useSiUnitPrefix) {
        if (units[0] != '\0' && (
            strcmp(units + 1, "Hz") == 0 ||
            strcmp(units + 1, "s") == 0)) {
            switch (units[0]) {
            case 'n': siPrefix = NANO; break;
            case 'u': siPrefix = MICRO; break;
            case 'm': siPrefix = MILLI; break;
            case 'k': siPrefix = KILO; break;
            case 'M': siPrefix = MEGA; break;
            case 'G': siPrefix = GIGA; break;
            case 'T': siPrefix = TERA; break;
            default: assert(false); break;
            }
            units = units + 1;
        }

        if (value == 0.0) { // If the value is zero, prevent 0 nUnits
            siPrefix = NONE;
        } else {
            auto sign = value < 0.0 ? -1.0 : 1.0;
            value *= sign;
            for (; value > 1000.0 && siPrefix < NUM_SI_PREFIXES - 1; ++siPrefix) value *= 0.001;
            for (; value < 1.0 && siPrefix > 0; --siPrefix) value *= 1000.0;
            value *= sign;
        }
    }

    // Convert value to 4 character long string
    //     XXX1234.123YYY => " XXX1234.123" (9+) => "XXX1234"
    //     234.123YYY     => " 234.123"     (8) => " 234"
    //     34.123YYY      => " 34.123"      (7) => "34.1"
    //     4.123YYY       => " 4.123"       (6) => "4.12"
    //     0.123YYY       => " 0.123"       (6) => ".123"
    char numberString[256];
    int n = snprintf(numberString, 256, " %.3lf", value);
    auto valueS = &numberString[1];

    if (n >= 8) {
        numberString[n - 4] = '\0';
        if (n == 8) valueS = &numberString[0];
    } else {
        if (numberString[1] == '0') {
            valueS = &numberString[1];

            // Special case: ".000" -> "   0"
            if (numberString[3] == '0' &&
                numberString[4] == '0' &&
                numberString[5] == '0') {
                numberString[1] = ' ';
                numberString[2] = ' ';
                numberString[3] = ' ';
            }
        }
        valueS[4] = '\0';
    }

    // Output final string
    char siPrefixS[] = {
        siPrefix == NONE ? '\0' : siPrefixChar[siPrefix],
        '\0'
    };
    return snprintf(memory, memorySize, "%s%s %s%s", prefix, valueS, siPrefixS, units);
}

void DrawQuantityLabel(
    float quantity,
    char const* units,
    char const* prefix,
    bool useSiUnitPrefix)
{
    char s[512] = {};
    CreateQuantityLabel(s, _countof(s), quantity, units, prefix, useSiUnitPrefix);
    ImGui::TextUnformatted(s);
}

} // anon namespace

MetricsGuiMetric::MetricsGuiMetric()
{
    auto c = ImColor::HSV(0.2f * gConstructedMetricIndex++, 0.8f, 0.8f);
    mColor[0] = c.Value.x;
    mColor[1] = c.Value.y;
    mColor[2] = c.Value.z;
    mColor[3] = c.Value.w;

    Initialize("", "", NONE);
}

MetricsGuiMetric::MetricsGuiMetric(
    char const* description,
    char const* units,
    uint32_t flags)
{
    auto c = ImColor::HSV(0.2f * gConstructedMetricIndex++, 0.8f, 0.8f);
    mColor[0] = c.Value.x;
    mColor[1] = c.Value.y;
    mColor[2] = c.Value.z;
    mColor[3] = c.Value.w;

    Initialize(description, units, flags);
}

void MetricsGuiMetric::Initialize(
    char const* description,
    char const* units,
    uint32_t flags)
{
    mDescription = description == nullptr ? "" : description;
    mUnits = units == nullptr ? "" : units;
    mTotalInHistory = 0.;
    mHistoryCount = 0;
    memset(mHistory, 0, NUM_HISTORY_SAMPLES * sizeof(float));
    mKnownMinValue = 0.f;
    mKnownMaxValue = 0.f;
    mFlags = flags;
    mSelected = false;
}

void MetricsGuiMetric::SetLastValue(
    float value,
    uint32_t prevIndex)
{
    assert(prevIndex < NUM_HISTORY_SAMPLES);
    auto p = &mHistory[NUM_HISTORY_SAMPLES - 1 - prevIndex];
    mTotalInHistory -= *p;
    *p = value;
    mTotalInHistory += value;
}

void MetricsGuiMetric::AddNewValue(
    float value)
{
    mTotalInHistory -= mHistory[0];
    memmove(
        mHistory,
        mHistory + 1,
        (NUM_HISTORY_SAMPLES - 1) * sizeof(mHistory[0]));
    mHistory[NUM_HISTORY_SAMPLES - 1] = value;
    mTotalInHistory += value;
    mHistoryCount = std::min((uint32_t) NUM_HISTORY_SAMPLES, mHistoryCount + 1);
}

float MetricsGuiMetric::GetLastValue(
    uint32_t prevIndex) const
{
    assert(prevIndex < NUM_HISTORY_SAMPLES);
    return mHistory[NUM_HISTORY_SAMPLES - 1 - prevIndex];
}

float MetricsGuiMetric::GetAverageValue() const
{
    return mHistoryCount == 0 ? 0.f : ((float) mTotalInHistory / mHistoryCount);
}

// Note: we defer computing the sizes because ImGui doesn't load the font until
// the first frame.

MetricsGuiPlot::WidthInfo::WidthInfo(
    MetricsGuiPlot* plot)
    : mLinkedPlots(1, plot)
    , mDescWidth(0.f)
    , mValueWidth(0.f)
    , mLegendWidth(0.f)
    , mInitialized(false)
{
}

void MetricsGuiPlot::WidthInfo::Initialize()
{
    if (mInitialized) {
        return;
    }

    auto prefixWidth = ImGui::CalcTextSize("XXX").x;
    auto sepWidth    = ImGui::CalcTextSize(": ").x;
    auto valueWidth  = ImGui::CalcTextSize("888. X").x;
    for (auto linkedPlot : mLinkedPlots) {
        for (auto metric : linkedPlot->mMetrics) {
            auto descWidth  = ImGui::CalcTextSize(metric->mDescription.c_str()).x;
            auto unitsWidth = ImGui::CalcTextSize(metric->mUnits.c_str()).x;
            auto quantWidth = valueWidth + unitsWidth;

            mDescWidth   = std::max(mDescWidth,   descWidth);
            mValueWidth  = std::max(mValueWidth,  quantWidth);
            mLegendWidth = std::max(mLegendWidth, std::max(descWidth, prefixWidth) + sepWidth + quantWidth);
        }
    }

    mInitialized = true;
}

MetricsGuiPlot::MetricsGuiPlot()
    : mMetrics()
    , mMetricRange()
    , mWidthInfo(new MetricsGuiPlot::WidthInfo(this))
    , mMinValue(0.f)
    , mMaxValue(0.f)
    , mRangeInitialized(false)
    , mBarRounding(0.f)
    , mRangeDampening(0.95f)
    , mInlinePlotRowCount(2)
    , mPlotRowCount(5)
    , mVBarMinWidth(6)
    , mVBarGapWidth(1)
    , mShowAverage(false)
    , mShowInlineGraphs(false)
    , mShowOnlyIfSelected(false)
    , mShowLegendDesc(true)
    , mShowLegendColor(true)
    , mShowLegendUnits(true)
    , mShowLegendAverage(false)
    , mShowLegendMin(true)
    , mShowLegendMax(true)
    , mBarGraph(false)
    , mStacked(false)
    , mSharedAxis(false)
    , mFilterHistory(true)
{
}

MetricsGuiPlot::MetricsGuiPlot(
    MetricsGuiPlot const& copy)
    : mMetrics(copy.mMetrics)
    , mMetricRange(copy.mMetricRange)
    , mWidthInfo(copy.mWidthInfo)
    , mMinValue(copy.mMinValue)
    , mMaxValue(copy.mMaxValue)
    , mRangeInitialized(copy.mRangeInitialized)
    , mBarRounding(copy.mBarRounding)
    , mRangeDampening(copy.mRangeDampening)
    , mInlinePlotRowCount(copy.mInlinePlotRowCount)
    , mPlotRowCount(copy.mPlotRowCount)
    , mVBarMinWidth(copy.mVBarMinWidth)
    , mVBarGapWidth(copy.mVBarGapWidth)
    , mShowAverage(copy.mShowAverage)
    , mShowInlineGraphs(copy.mShowInlineGraphs)
    , mShowOnlyIfSelected(copy.mShowOnlyIfSelected)
    , mShowLegendDesc(copy.mShowLegendDesc)
    , mShowLegendColor(copy.mShowLegendColor)
    , mShowLegendUnits(copy.mShowLegendUnits)
    , mShowLegendAverage(copy.mShowLegendAverage)
    , mShowLegendMin(copy.mShowLegendMin)
    , mShowLegendMax(copy.mShowLegendMax)
    , mBarGraph(copy.mBarGraph)
    , mStacked(copy.mStacked)
    , mSharedAxis(copy.mSharedAxis)
    , mFilterHistory(copy.mFilterHistory)
{
    mWidthInfo->mLinkedPlots.emplace_back(this);
}

MetricsGuiPlot::~MetricsGuiPlot()
{
    auto it = std::find(mWidthInfo->mLinkedPlots.begin(), mWidthInfo->mLinkedPlots.end(), this);
    assert(it != mWidthInfo->mLinkedPlots.end());
    mWidthInfo->mLinkedPlots.erase(it);
    if (mWidthInfo->mLinkedPlots.empty()) {
        delete mWidthInfo;
    }
}

void MetricsGuiPlot::LinkLegends(
    MetricsGuiPlot* plot)
{
    // Already linked?
    auto otherWidthInfo = plot->mWidthInfo;
    if (mWidthInfo == otherWidthInfo) {
        return;
    }

    // Ensure either neither or both axes are initialized
    if (mWidthInfo->mInitialized || otherWidthInfo->mInitialized) {
        mWidthInfo->Initialize();
        otherWidthInfo->Initialize();
        mWidthInfo->mDescWidth   = std::max(mWidthInfo->mDescWidth,   otherWidthInfo->mDescWidth  );
        mWidthInfo->mValueWidth  = std::max(mWidthInfo->mValueWidth,  otherWidthInfo->mValueWidth );
        mWidthInfo->mLegendWidth = std::max(mWidthInfo->mLegendWidth, otherWidthInfo->mLegendWidth);
    }

    // Move plot's linked plots to this
    do {
        plot = otherWidthInfo->mLinkedPlots.back();
        otherWidthInfo->mLinkedPlots.pop_back();

        plot->mWidthInfo = mWidthInfo;
        mWidthInfo->mLinkedPlots.emplace_back(plot);
    } while (!otherWidthInfo->mLinkedPlots.empty());

    delete otherWidthInfo;
}

void MetricsGuiPlot::UpdateAxes()
{
    float oldWeight;
    if (mRangeInitialized) {
        oldWeight = std::min(1.f, std::max(0.f, mRangeDampening));
    } else {
        oldWeight = 0.f;
        mRangeInitialized = true;
    }
    auto newWeight = 1.f - oldWeight;

    float minPlotValue = FLT_MAX;
    float maxPlotValue = FLT_MIN;
    for (size_t i = 0, N = mMetrics.size(); i < N; ++i) {
        auto metric = mMetrics[i];
        auto metricRange = &mMetricRange[i];

        auto knownMinValue = 0 != (metric->mFlags & MetricsGuiMetric::KNOWN_MIN_VALUE);
        auto knownMaxValue = 0 != (metric->mFlags & MetricsGuiMetric::KNOWN_MAX_VALUE);
        auto historyRange = std::make_pair(
            knownMinValue ? &metric->mKnownMinValue : std::min_element(metric->mHistory, metric->mHistory + MetricsGuiMetric::NUM_HISTORY_SAMPLES),
            knownMaxValue ? &metric->mKnownMaxValue : std::max_element(metric->mHistory, metric->mHistory + MetricsGuiMetric::NUM_HISTORY_SAMPLES));
        metricRange->first  = metricRange->first  * oldWeight + *historyRange.first  * newWeight;
        metricRange->second = metricRange->second * oldWeight + *historyRange.second * newWeight;

        minPlotValue = std::min(minPlotValue, *historyRange.first);
        maxPlotValue = std::max(maxPlotValue, *historyRange.second);
    }

    if (mSharedAxis) {
        minPlotValue = mMetricRange[0].first;
        maxPlotValue = mMetricRange[0].second;
    } else if (mStacked) {
        maxPlotValue = FLT_MIN;
        for (size_t i = 0; i < MetricsGuiMetric::NUM_HISTORY_SAMPLES; ++i) {
            float stackedValue = 0.f;
            for (auto metric : mMetrics) {
                stackedValue += metric->mHistory[i];
            }
            maxPlotValue = std::max(maxPlotValue, stackedValue);
        }
    }

    mMinValue = mMinValue * oldWeight + minPlotValue * newWeight;
    mMaxValue = mMaxValue * oldWeight + maxPlotValue * newWeight;
}

void MetricsGuiPlot::AddMetric(
    MetricsGuiMetric* metric)
{
    mMetrics.emplace_back(metric);
    mMetricRange.emplace_back(FLT_MAX, FLT_MIN);
}

void MetricsGuiPlot::AddMetrics(
    MetricsGuiMetric* metrics,
    size_t metricCount)
{
    mMetrics.reserve(mMetrics.size() + metricCount);
    mMetricRange.reserve(mMetrics.size());
    for (size_t i = 0; i < metricCount; ++i) {
        AddMetric(&metrics[i]);
    }
}

void MetricsGuiPlot::SortMetricsByName()
{
    std::sort(mMetrics.begin(), mMetrics.end(), [](MetricsGuiMetric* a, MetricsGuiMetric* b) {
        return a->mDescription.compare(b->mDescription) < 0;
    });
}

namespace {

bool DrawPrefix(
    MetricsGuiPlot* plot)
{
    plot->mWidthInfo->Initialize();

    auto window = ImGui::GetCurrentWindow();
    if (window->SkipItems) {
        return false;
    }

    return true;
}

void DrawMetrics(
    MetricsGuiPlot* plot,
    std::vector<MetricsGuiMetric*> const& metrics,
    uint32_t plotRowCount,
    float plotMinValue,
    float plotMaxValue)
{
    auto window = ImGui::GetCurrentWindow();
    auto const& style = GImGui->Style;

    auto textHeight = ImGui::GetTextLineHeight();

    auto plotWidth = std::max(0.f,
        ImGui::GetContentRegionAvailWidth() -
        window->WindowPadding.x -
        plot->mWidthInfo->mLegendWidth -
        PLOT_LEGEND_PADDING);
    auto plotHeight = std::max(0.f, (textHeight + LEGEND_TEXT_VERTICAL_SPACING) * plotRowCount);

    ImRect frame_bb(
        window->DC.CursorPos,
        window->DC.CursorPos + ImVec2(plotWidth, plotHeight));
    ImRect inner_bb(
        frame_bb.Min + style.FramePadding,
        frame_bb.Max - style.FramePadding);

    ImGui::ItemSize(frame_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(frame_bb, NULL)) {
        return;
    }

    ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

    plotWidth = inner_bb.GetWidth();
    plotHeight = inner_bb.GetHeight();

    size_t pointCount = MetricsGuiMetric::NUM_HISTORY_SAMPLES;
    size_t maxBarCount = (size_t) (plotWidth / (plot->mVBarMinWidth + plot->mVBarGapWidth));

    if (plotMaxValue == plotMinValue) {
        pointCount = 0;
    }

    bool useFilterPath = plot->mFilterHistory || (maxBarCount > pointCount);
    if (!useFilterPath) {
        pointCount = maxBarCount;
    } else if (plot->mBarGraph) {
        pointCount = std::min(pointCount, maxBarCount);
    } else {
        pointCount = std::min(pointCount, (size_t) (plotWidth));
    }
    if (pointCount > 0) {
        std::vector<float> baseValue(pointCount, 0.f);
        auto hScale = plotWidth / (float) (plot->mBarGraph ? pointCount : (pointCount - 1));
        auto vScale = plotHeight / (plotMaxValue - plotMinValue);
        for (auto metric : metrics) {
            if (plot->mShowOnlyIfSelected && !metric->mSelected) {
                continue;
            }

            auto color = ImGui::ColorConvertFloat4ToU32(*(ImVec4*) &metric->mColor);

            size_t historyBeginIdx = useFilterPath
                ? 0
                : (MetricsGuiMetric::NUM_HISTORY_SAMPLES - pointCount);
            ImVec2 p;
            float prevB = 0.f;
            for (size_t i = 0; i < pointCount; ++i) {
                size_t historyEndIdx = useFilterPath
                    ? ((i + 1) * MetricsGuiMetric::NUM_HISTORY_SAMPLES / pointCount)
                    : (historyBeginIdx + 1);
                size_t N = historyEndIdx - historyBeginIdx;
                float v = 0.f;
                if (N > 0) {
                    do {
                        v += metric->mHistory[historyBeginIdx];
                        ++historyBeginIdx;
                    } while (historyBeginIdx < historyEndIdx);
                    v = v / (float) N;
                }
                float b = baseValue[i];
                v += b;

                ImVec2 pn(
                    inner_bb.Min.x + hScale * i,
                    inner_bb.Max.y - vScale * (v - plotMinValue));

                if (i > 0) {
                    if (plot->mBarGraph) {
                        ImVec2 p1(
                            pn.x - plot->mVBarGapWidth,
                            inner_bb.Max.y - vScale * (prevB - plotMinValue));
                        p  = ImClamp(p,  inner_bb.Min, inner_bb.Max);
                        p1 = ImClamp(p1, inner_bb.Min, inner_bb.Max);
                        window->DrawList->AddRectFilled(p, p1, color, plot->mBarRounding);
                    } else {
                        pn = ImClamp(pn, inner_bb.Min, inner_bb.Max);
                        window->DrawList->AddLine(p, pn, color);
                    }
                }

                p = pn;
                prevB = b;
                if (plot->mStacked) {
                    baseValue[i] = v;
                }
            }

            if (plot->mBarGraph) {
                ImVec2 p1(
                    inner_bb.Max.x - plot->mVBarGapWidth,
                    inner_bb.Max.y - vScale * (prevB - plotMinValue));
                p  = ImClamp(p,  inner_bb.Min, inner_bb.Max);
                p1 = ImClamp(p1, inner_bb.Min, inner_bb.Max);
                window->DrawList->AddRectFilled(p, p1, color, plot->mBarRounding);
            }

            if (plot->mShowAverage) {
                auto avgValue = metric->GetAverageValue();
                auto y = inner_bb.Max.y - vScale * (avgValue - plotMinValue);
                y = ImClamp(y, inner_bb.Min.y, inner_bb.Max.y);
                window->DrawList->AddLine(
                    ImVec2(inner_bb.Min.x, y),
                    ImVec2(inner_bb.Max.x, y),
                    color);
            }
        }
    }

    ImGui::SameLine();

    auto useSiUnitPrefix = false;
    auto units = "";
    if (plot->mShowLegendUnits) {
        useSiUnitPrefix = (metrics[0]->mFlags & MetricsGuiMetric::USE_SI_UNIT_PREFIX) != 0;
        units = metrics[0]->mUnits.c_str();
    }


    // Draw legend, special case for single metric
    //
    // TODO: if nothing, enlarge plot
    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, LEGEND_TEXT_VERTICAL_SPACING));

    if (metrics.size() == 1) {
        // ---| Desc
        //    | Max: xxx
        //    | Avg: xxx
        //    | Min: xxx
        //    |
        // ---|
        if (plot->mShowLegendColor) {
            ImGui::PushStyleColor(ImGuiCol_Text, *(ImVec4*) &metrics[0]->mColor);
        }
        if (plot->mShowLegendDesc) {
            ImGui::TextUnformatted(metrics[0]->mDescription.c_str());
        }
        if (plot->mShowLegendMax) {
            DrawQuantityLabel(plotMaxValue, units, "Max: ", useSiUnitPrefix);
        }
        if (plot->mShowLegendAverage) {
            for (auto metric : metrics) {
                auto plotAvgValue = metric->GetAverageValue();
                DrawQuantityLabel(plotAvgValue, units, "Avg: ", useSiUnitPrefix);
            }
        }
        if (plot->mShowLegendMin) {
            DrawQuantityLabel(plotMinValue, units, "Min: ", useSiUnitPrefix);
        }
        if (plot->mShowLegendColor) {
            ImGui::PopStyleColor();
        }
    } else {
        // ---| Max: xxx
        //    | Desc
        //    | Avg: xxx
        //    |
        //    |
        // ---| Min: xxx
        if (plot->mShowLegendMax) {
            DrawQuantityLabel(plotMaxValue, units, "Max: ", useSiUnitPrefix);
        }
        if (plot->mShowLegendDesc || plot->mShowLegendAverage) {
            // Order series based on value and/or stack order
            std::vector<MetricsGuiMetric*> ordered(metrics.begin(), metrics.end());
            if (plot->mStacked) {
                std::reverse(ordered.begin(), ordered.end());
            } else {
                std::sort(ordered.begin(), ordered.end(), [](MetricsGuiMetric* a, MetricsGuiMetric* b) {
                    return b->GetAverageValue() < a->GetAverageValue();
                });
            }
            for (auto metric : ordered) {
                if (plot->mShowLegendColor) {
                    ImGui::PushStyleColor(ImGuiCol_Text, *(ImVec4*) &metric->mColor);
                }
                if (plot->mShowLegendDesc) {
                    if (plot->mShowLegendAverage) {
                        char prefix[128];
                        snprintf(prefix, _countof(prefix), "%s ", metric->mDescription.c_str());
                        auto plotAvgValue = metric->GetAverageValue();
                        DrawQuantityLabel(plotAvgValue, units, prefix, useSiUnitPrefix);
                    } else {
                        ImGui::TextUnformatted(metric->mDescription.c_str());
                    }
                } else {
                    auto plotAvgValue = metric->GetAverageValue();
                    DrawQuantityLabel(plotAvgValue, units, "Avg: ", useSiUnitPrefix);
                }
                if (plot->mShowLegendColor) {
                    ImGui::PopStyleColor();
                }
            }
        }
        if (plot->mShowLegendMin) {
            auto cy = window->DC.CursorPos.y;
            auto ty = frame_bb.Max.y - textHeight;
            if (cy < ty) {
                ImGui::ItemSize(ImVec2(0.f, ty - cy));
            }
            DrawQuantityLabel(plotMinValue, units, "Min: ", useSiUnitPrefix);
        }
    }


    ImGui::PopStyleVar(1);
    ImGui::EndGroup();
}

}

void MetricsGuiPlot::DrawList()
{
    if (!DrawPrefix(this)) {
        return;
    }

    auto window    = ImGui::GetCurrentWindow();
    auto height    = ImGui::GetTextLineHeight();
    auto valueX    = ImGui::GetContentRegionAvailWidth() - window->WindowPadding.x - mWidthInfo->mValueWidth;
    auto barStartX = mWidthInfo->mDescWidth + DESC_HBAR_PADDING;
    auto barEndX   = valueX - HBAR_VALUE_PADDING;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 0));

    for (size_t i = 0, N = mMetrics.size(); i < N; ++i) {
        auto metric = mMetrics[i];
        auto const& metricRange = mMetricRange[i];

        // Draw description and value
        auto x = window->DC.CursorPos.x;
        auto y = window->DC.CursorPos.y;
        ImGui::Selectable(metric->mDescription.c_str(), &metric->mSelected, ImGuiSelectableFlags_DrawFillAvailWidth);
        if (valueX >= barStartX) {
            auto useSiUnitPrefix  = 0 != (metric->mFlags & MetricsGuiMetric::USE_SI_UNIT_PREFIX);
            auto lastValue = metric->GetLastValue();
            ImGui::SameLine(x + valueX - (window->Pos.x - window->Scroll.x));

            DrawQuantityLabel(lastValue, metric->mUnits.c_str(), "", useSiUnitPrefix);

            // Draw bar
            if (barEndX > barStartX) {
                auto normalizedValue = metricRange.second > metricRange.first
                    ? ImSaturate((lastValue - metricRange.first) / (metricRange.second - metricRange.first))
                    : (lastValue == 0.f ? 0.f : 1.f);
                window->DrawList->AddRectFilled(
                    ImVec2(
                        x + barStartX,
                        y + HBAR_PADDING_TOP),
                    ImVec2(
                        x + barStartX + normalizedValue * (barEndX - barStartX),
                        y + height - HBAR_PADDING_BOTTOM),
                    ImGui::GetColorU32(*(ImVec4*) &metric->mColor),
                    mBarRounding);
            }
        }

        if (mShowInlineGraphs &&
            (!mShowOnlyIfSelected || metric->mSelected)) {
            std::vector<MetricsGuiMetric*> m(1, metric);
            DrawMetrics(this, m, mInlinePlotRowCount, metricRange.first, metricRange.second);
        }
    }

    ImGui::PopStyleVar();
}

void MetricsGuiPlot::DrawHistory()
{
    if (!DrawPrefix(this)) {
        return;
    }

    DrawMetrics(this, mMetrics, mPlotRowCount, mMinValue, mMaxValue);
}

