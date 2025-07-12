// plot_utils.h
// Utility functions for plotting SQLite query results using ROOT.
// Includes Freedman–Diaconis binning, statbox styling, and dynamic plot canvas handling.
// Used by the sqliteViewer application.
#ifndef PLOT_UTILS_H
#define PLOT_UTILS_H

#include <vector>
#include <TString.h>
#include <TH1.h>
#include <TMath.h>
#include <TPaveStats.h>
#include <TCanvas.h>
#include <algorithm>
#include <cmath>


// Computes an approximate quantile (e.g., Q1, Q3) from sorted data.
// Used in Freedman–Diaconis rule for determining bin width.
double GetQuartile(std::vector<double> data, double quartile) {
    if (data.empty()) return 0.0;

    std::sort(data.begin(), data.end());

    double idx = quartile * (data.size() - 1);
    size_t idx_below = static_cast<size_t>(std::floor(idx));
    size_t idx_above = static_cast<size_t>(std::ceil(idx));

    if (idx_below == idx_above) {
        return data[idx_below];
    } else {
        double fraction = idx - idx_below;
        return data[idx_below] * (1.0 - fraction) + data[idx_above] * fraction;
    }
}

// Rounds result of binwidth calculation to the nearest visually appealing value
double RoundToNiceValue(double value) {
    if (value <= 0) return 1.0;

    double exponent = std::floor(std::log10(value));
    double base = value / std::pow(10.0, exponent);

    double niceBase;
    if (base < 1.5)      niceBase = 1.0;
    else if (base < 3.0) niceBase = 2.0;
    else if (base < 7.0) niceBase = 5.0;
    else                 niceBase = 10.0;

    return niceBase * std::pow(10.0, exponent);
}

// Converts a column name like "energy__MeV" into "energy (MeV)"
// Assumes column names are in the format: label__unit
TString FormatAxisLabel(const TString& columnName) {
    Ssiz_t sep = columnName.Index("__");
    if (sep == kNPOS) return columnName;

    TString base = columnName(0, sep);
    TString unit = columnName(sep + 2, columnName.Length());

    base.ReplaceAll("_", " ");
    return Form("%s (%s)", base.Data(), unit.Data());
}

// Extracts the unit string (e.g., "MeV") from a column name like "energy__MeV"
TString ExtractUnit(const TString& columnName) {
    Ssiz_t sep = columnName.Index("__");
    if (sep == kNPOS) return "";
    return columnName(sep + 2, columnName.Length());
}

// Styles the statistics box for a histogram with transparent background,
// and moves it to the upper-right corner of the plot.
void StyleStatBox(TH1* hist) {
    gPad->Update();
    TPaveStats* stats = (TPaveStats*)hist->GetListOfFunctions()->FindObject("stats");

    if (stats) {
        stats->SetFillColor(0);
        stats->SetFillStyle(0);
        stats->SetLineColor(1);
        stats->SetTextSize(0.03);

        double right  = 1.0 - gPad->GetRightMargin();
        double top    = 1.0 - gPad->GetTopMargin();
        double frameWidth  = 1.0 - gPad->GetLeftMargin() - gPad->GetRightMargin();
        double frameHeight = 1.0 - gPad->GetTopMargin()  - gPad->GetBottomMargin();
        double width  = 0.25 * frameWidth;
        double height = 0.25 * frameHeight;

        stats->SetX2NDC(right);
        stats->SetX1NDC(right - width);
        stats->SetY2NDC(top);
        stats->SetY1NDC(top - height);

        stats->Draw();
        gPad->Modified();
    }
}

// Main entry point for plotting selected columns from query results.
// Handles:
//  - 1D histograms using Freedman–Diaconis binning
//  - 2D histograms with adaptive bin widths
//  - 1D or 2D scatter plots
// Supports rotation through a limited number of TCanvas windows.
void PlotSelectedData(
    const std::vector<std::vector<TString>>& tableData,
    const std::vector<TString>& tableHeader,
    int xIndex, int yIndex,
    int plotType,
    std::deque<TCanvas*>& canvasQueue,
    size_t maxCanvases,
    TH1*& fLastHist,
    TH2*& fLastHist2D
) {
    std::vector<double> xData;
    std::vector<double> yData;

    for (const auto& row : tableData) {
        bool xValid = row.size() > xIndex && !row[xIndex].IsNull();
        bool yValid = row.size() > yIndex && !row[yIndex].IsNull();

        if (xValid) xData.push_back(row[xIndex].Atof());
        if (yIndex >= 0 && yValid) yData.push_back(row[yIndex].Atof());
    }

    if (fLastHist)   { delete fLastHist; fLastHist = nullptr; }
    if (fLastHist2D) { delete fLastHist2D; fLastHist2D = nullptr; }

    if (canvasQueue.size() >= maxCanvases) {
        TCanvas* oldCanvas = canvasQueue.front();
        oldCanvas->Close(); delete oldCanvas;
        canvasQueue.pop_front();
    }

    TCanvas* newCanvas = new TCanvas(Form("canvas_%u", gRandom->Integer(1e9)), "Plot", 800, 600);
    canvasQueue.push_back(newCanvas);
    newCanvas->cd();

    gStyle->SetOptStat(1110);
    gStyle->SetPalette(55);

    if (plotType == 1) {
        if (yIndex < 0 || yData.size() != xData.size()) {
            double q1 = GetQuartile(xData, 0.25);
            double q3 = GetQuartile(xData, 0.75);
            double iqr = q3 - q1;
            double rawBinWidth = 2 * iqr / std::cbrt(xData.size());
            double binWidth = RoundToNiceValue(rawBinWidth);
            if (binWidth <= 0) binWidth = 1;

            double minVal = TMath::MinElement(xData.size(), xData.data());
            double maxVal = TMath::MaxElement(xData.size(), xData.data());
            int nBins = (int)((maxVal - minVal) / binWidth);
            if (nBins < 1) nBins = 10;

            TH1D* h1 = new TH1D("h1", FormatAxisLabel(tableHeader[xIndex]), nBins, minVal, maxVal);
            for (auto val : xData) h1->Fill(val);

            h1->GetXaxis()->SetTitle(FormatAxisLabel(tableHeader[xIndex]));
            TString unit = ExtractUnit(tableHeader[xIndex]);
            h1->GetYaxis()->SetTitle(unit.IsNull() ? "Entries" : Form("Entries / %g %s", binWidth, unit.Data()));

            h1->SetStats(true);
            h1->Draw();
            StyleStatBox(h1);

            fLastHist = h1;
        } else {
            double q1x = GetQuartile(xData, 0.25), q3x = GetQuartile(xData, 0.75);
            double iqrx = q3x - q1x;
            double bwx = RoundToNiceValue(2 * iqrx / std::cbrt(xData.size()));

            double q1y = GetQuartile(yData, 0.25), q3y = GetQuartile(yData, 0.75);
            double iqry = q3y - q1y;
            double bwy = RoundToNiceValue(2 * iqry / std::cbrt(yData.size()));

            if (bwx <= 0) bwx = 1;
            if (bwy <= 0) bwy = 1;

            double minX = TMath::MinElement(xData.size(), xData.data());
            double maxX = TMath::MaxElement(xData.size(), xData.data());
            double minY = TMath::MinElement(yData.size(), yData.data());
            double maxY = TMath::MaxElement(yData.size(), yData.data());

            int nBinsX = (int)((maxX - minX) / bwx);
            int nBinsY = (int)((maxY - minY) / bwy);
            if (nBinsX < 1) nBinsX = 10;
            if (nBinsY < 1) nBinsY = 10;

            TH2D* h2 = new TH2D("h2",
                Form("2D Histogram of %s vs %s",
                     FormatAxisLabel(tableHeader[xIndex]).Data(),
                     FormatAxisLabel(tableHeader[yIndex]).Data()),
                nBinsX, minX, maxX,
                nBinsY, minY, maxY
            );

            for (size_t i = 0; i < xData.size(); ++i)
                h2->Fill(xData[i], yData[i]);

            h2->GetXaxis()->SetTitle(FormatAxisLabel(tableHeader[xIndex]));
            h2->GetYaxis()->SetTitle(FormatAxisLabel(tableHeader[yIndex]));

            h2->SetStats(true);
            h2->Draw("COLZ");
            StyleStatBox(h2);

            fLastHist2D = h2;
        }

    } else if (plotType == 2) {
        TGraph* g = new TGraph(xData.size());

        if (yIndex < 0 || yData.size() != xData.size()) {
            for (int i = 0; i < xData.size(); ++i)
                g->SetPoint(i, i, xData[i]);
            g->SetTitle(Form("%s;Index;%s", tableHeader[xIndex].Data(), tableHeader[xIndex].Data()));
            g->SetMarkerColor(kBlue);

        } else {
            for (int i = 0; i < xData.size(); ++i)
                g->SetPoint(i, xData[i], yData[i]);
            g->SetTitle(Form("2D Scatter Plot;%s;%s",
                             tableHeader[xIndex].Data(),
                             tableHeader[yIndex].Data()));
            g->SetMarkerColor(kRed);
        }

        g->SetMarkerStyle(20);
        g->Draw("AP");
    }

    newCanvas->Update();
}

#endif
