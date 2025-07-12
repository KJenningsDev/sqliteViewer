// sqliteViewer.C
// Main GUI class for analyzing data from SQLite databases using ROOT's TG framework.
// Provides support for SQL query execution, data visualization, and plot configuration.

// ROOT GUI
#include <TApplication.h>
#include <TGClient.h>
#include <TGFrame.h>
#include <TGListBox.h>
#include <TGTextView.h>
#include <TGTextEntry.h>
#include <TGTextEdit.h>
#include <TGButton.h>
#include <TGFileDialog.h>

// ROOT SQL
#include <TSQLServer.h>
#include <TSQLResult.h>
#include <TSQLRow.h>

// ROOT Utilities and STL
#include <RQ_OBJECT.h>
#include <deque>
#include <fstream>
#include <sstream>

#include "plot_utils.h"
#include <libgen.h>  // for dirname
#include <unistd.h>  // for readlink


// MyMainFrame
//  Inherits from TGMainFrame and serves as the main window.
//  Handles UI layout, user interaction, database querying, and data plotting.

class MyMainFrame : public TGMainFrame {
    RQ_OBJECT("MyMainFrame")

private:
    // UI Elements
    TGLabel *fDBPathLabel;
    TGComboBox *fTableDropdown;
    TGTextView *fDataView;
    TGTextEdit *fSQLBox;
    TGTextButton *fRunSQLBtn;
    TGGroupFrame *fHintBox;
    TGTextView *fHintText;
    TGVerticalFrame *fHintWrapper;
    TGTextButton *fToggleHintsBtn;
    bool fHintsVisible = false;


    // Database connection and query results
    TSQLServer *fDB;
    TSQLResult *fLastQueryResult = nullptr;

    // Cached query result table
    std::vector<std::vector<TString>> fCurrentTableData;
    std::vector<TString> fCurrentTableHeader;

    //Plot Controls and canvas history
    TGComboBox *fPlotTypeBox = nullptr;
    TGComboBox *fDimensionBox = nullptr;
    TGComboBox *fXColumnSelect = nullptr;
    TGComboBox *fYColumnSelect = nullptr;

    TH1* fLastHist = nullptr;
    TH2* fLastHist2D = nullptr;

    std::deque<TCanvas*> fCanvasQueue;
    static constexpr size_t kMaxCanvases = 3;

    // Query History Box
    TGTextView* fQueryHistoryView = nullptr;
    std::deque<TString> fQueryHistory;
    static constexpr size_t kMaxQueryHistory = 10;

    // Helper function to create a labeled TGComboBox with layout hints
    TGComboBox* AddComboRow(
        TGCompositeFrame* parent,
        const char* labelText,
        TGComboBox*& outputPtr,
        UInt_t labelHints, std::array<Int_t, 4> labelMargins,
        UInt_t boxHints,   std::array<Int_t, 4> boxMargins,
        UInt_t rowHints,   std::array<Int_t, 4> rowMargins,
        int width = 100)
    {
        TGHorizontalFrame* row = new TGHorizontalFrame(parent);

        TGLabel* label = new TGLabel(row, labelText);
        row->AddFrame(label, new TGLayoutHints(labelHints, labelMargins[0], labelMargins[1], labelMargins[2], labelMargins[3]));

        TGComboBox* combo = new TGComboBox(row);
        combo->Resize(width, 22);
        row->AddFrame(combo, new TGLayoutHints(boxHints, boxMargins[0], boxMargins[1], boxMargins[2], boxMargins[3]));

        parent->AddFrame(row, new TGLayoutHints(rowHints, rowMargins[0], rowMargins[1], rowMargins[2], rowMargins[3]));

        outputPtr = combo;
        return combo;
    }

    // Converts a TSQLResult into an internal table (headers and data),
    // populates the fDataView for text display,
    // and refreshes column selectors for plotting.
    void LoadQueryResultsToTable(TSQLResult* result) {
        if (!result) return;

        fCurrentTableHeader.clear();
        fCurrentTableData.clear();
        fDataView->Clear();

        // Extract and format headers
        Int_t nFields = result->GetFieldCount();
        TString header;
        for (int i = 0; i < nFields; ++i) {
            TString col = result->GetFieldName(i);
            header += TString::Format("%-15s", col.Data());
            fCurrentTableHeader.push_back(col);
        }

        fDataView->AddLine(header);
        fDataView->AddLine(" ");

        // Read all rows
        while (TSQLRow* row = result->Next()) {
            TString line;
            std::vector<TString> rowData;

            for (int i = 0; i < nFields; ++i) {
                TString val = row->GetField(i);
                line += TString::Format("%-15s", val.Data());
                rowData.push_back(val);
            }

            fDataView->AddLine(line);
            fCurrentTableData.push_back(rowData);
            delete row;
        }

        fDataView->Update();

        // Populate column selectors
        fXColumnSelect->RemoveEntries(0, fXColumnSelect->GetNumberOfEntries());
        fYColumnSelect->RemoveEntries(0, fYColumnSelect->GetNumberOfEntries());

        fXColumnSelect->AddEntry(" ", -1);
        fYColumnSelect->AddEntry(" ", -1);
        fXColumnSelect->Select(-1);
        fYColumnSelect->Select(-1);
        fXColumnSelect->RemoveEntry(-1);
        fYColumnSelect->RemoveEntry(-1);

        for (const auto& col : fCurrentTableHeader) {
            int entryId = fXColumnSelect->GetNumberOfEntries() + 1;
            fXColumnSelect->AddEntry(col, entryId);
            fYColumnSelect->AddEntry(col, entryId);
        }
    }


public:
    // GUI event handlers (defined in gui_handlers.inline.h)
    void OnTableSelected(Int_t id);
    void OnExportCSVClicked();
    void OnToggleHints();
    void OnChangeFile();
    void OnRunSQLClicked();
    void OnDimensionChanged(Int_t dim);
    void OnPlotButtonClicked();

    // Main constructor: sets up the GUI layout, prompts user to select database,
    // initializes widgets and connects signals to handlers.
    MyMainFrame(const TGWindow *p, UInt_t w, UInt_t h) : TGMainFrame(p, w, h), fDB(nullptr) {

        // Horizontal frame for data view panel and plot controls panel
        TGHorizontalFrame *hFrame = new TGHorizontalFrame(this, w, h);

        // Plot controls panel
        TGVerticalFrame *plotPanel = new TGVerticalFrame(hFrame, 200, 300);
        TGLabel *plotLabel = new TGLabel(plotPanel, "Plot Controls");
        plotPanel->AddFrame(plotLabel, new TGLayoutHints(kLHintsCenterX, 5, 5, 5, 5));

        // Plot type selection
        TGHorizontalFrame *plotTypeRow = new TGHorizontalFrame(plotPanel);
        TGLabel *plotTypeLabel = new TGLabel(plotTypeRow, "Plot Type:");
        plotTypeRow->AddFrame(plotTypeLabel, new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 2, 5, 2, 2));

        fPlotTypeBox = AddComboRow(plotPanel, "Plot Type:", fPlotTypeBox,
            kLHintsLeft | kLHintsCenterY, {2, 5, 2, 2},
            kLHintsRight,                 {5, 2, 2, 2},
            kLHintsExpandX,               {0, 0, 5, 0});
        fPlotTypeBox->AddEntry("Histogram", 1);
        fPlotTypeBox->AddEntry("Scatter", 2);


        // Dimension selection
        fDimensionBox = AddComboRow(plotPanel, "Dimensions:", fDimensionBox,
            kLHintsLeft | kLHintsCenterY, {2, 5, 2, 2},
            kLHintsRight,                 {5, 2, 2, 2},
            kLHintsExpandX,               {0, 0, 5, 0});
        fDimensionBox->AddEntry("1D", 1);
        fDimensionBox->AddEntry("2D", 2);
        fDimensionBox->Connect("Selected(Int_t)", "MyMainFrame", this, "OnDimensionChanged(Int_t)");

        // X Column Selector
        fXColumnSelect = AddComboRow(plotPanel, "X Column:", fXColumnSelect,
            kLHintsLeft | kLHintsCenterY, {2, 5, 2, 2},
            kLHintsRight,                 {5, 2, 2, 2},
            kLHintsExpandX,               {0, 0, 5, 0});

        // Y Column Selector (for 2D)
        fYColumnSelect = AddComboRow(plotPanel, "Y Column:", fYColumnSelect,
            kLHintsLeft | kLHintsCenterY, {2, 5, 2, 2},
            kLHintsRight,                 {5, 2, 2, 2},
            kLHintsExpandX,               {0, 0, 5, 0});
        fYColumnSelect->SetEnabled(kFALSE);

        // Plot Button
        TGTextButton *plotBtn = new TGTextButton(plotPanel, "Plot Data");
        plotBtn->Connect("Clicked()", "MyMainFrame", this, "OnPlotButtonClicked()");
        plotPanel->AddFrame(plotBtn, new TGLayoutHints(kLHintsCenterX, 5, 5, 10, 10));

        //Attach left Panel to main horizontal
        hFrame->AddFrame(plotPanel, new TGLayoutHints(kLHintsTop | kLHintsRight | kLHintsExpandY, 5, 5, 10, 10));

        // Prompt for .sqlite at start
        TGFileInfo fi;
        const char *filetypes[] = {"SQLite files", "*.sqlite", "All files", "*", nullptr};
        fi.fFileTypes = filetypes;
        fi.fIniDir = StrDup(".");
        new TGFileDialog(gClient->GetRoot(), this, kFDOpen, &fi);

        if (!fi.fFilename || strlen(fi.fFilename) == 0) {
            printf("No database file selected.\n");
            return;
        }

        TString dbPath = fi.fFilename;
        fDB = TSQLServer::Connect(Form("sqlite://%s", dbPath.Data()), "", "");
        if (!fDB || fDB->IsZombie()) {
            printf("Failed to connect to database: %s\n", dbPath.Data());
            return;
        }

        // Display file path and change file button
        TGHorizontalFrame *fileRow = new TGHorizontalFrame(this);

        fDBPathLabel = new TGLabel(fileRow, Form("Database: %s", dbPath.Data()));
        fileRow->AddFrame(fDBPathLabel, new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 5, 5, 5, 5));

        TGTextButton *changeFileBtn = new TGTextButton(fileRow, "Change File");
        changeFileBtn->Connect("Clicked()", "MyMainFrame", this, "OnChangeFile()");
        fileRow->AddFrame(changeFileBtn, new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 5, 10, 5, 5));

        AddFrame(fileRow, new TGLayoutHints(kLHintsExpandX));

        // Table selection dropdown
        TGHorizontalFrame *tableRow = new TGHorizontalFrame(this);

        TGLabel *tableLabel = new TGLabel(tableRow, "Table:");
        tableRow->AddFrame(tableLabel, new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 5, 5, 5, 5));

        fTableDropdown = new TGComboBox(tableRow, 1);
        fTableDropdown->Resize(200, 22);
        fTableDropdown->SetEditable(kFALSE);
        fTableDropdown->Connect("Selected(Int_t)", "MyMainFrame", this, "OnTableSelected(Int_t)");
        tableRow->AddFrame(fTableDropdown, new TGLayoutHints(kLHintsLeft, 5, 10, 5, 5));

        AddFrame(tableRow, new TGLayoutHints(kLHintsExpandX));

        // Panel for Data View and Export Button
        TGVerticalFrame *resultPanel = new TGVerticalFrame(hFrame);

        fDataView = new TGTextView(resultPanel, 400, 300);
        resultPanel->AddFrame(fDataView, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 5, 10, 10, 5));

        TGHorizontalFrame *exportRow = new TGHorizontalFrame(resultPanel);
        TGTextButton *fExportBtn = new TGTextButton(exportRow, "Save as CSV");
        fExportBtn->Connect("Clicked()", "MyMainFrame", this, "OnExportCSVClicked()");
        exportRow->AddFrame(fExportBtn, new TGLayoutHints(kLHintsRight | kLHintsBottom, 10, 10, 5, 5));
        resultPanel->AddFrame(exportRow, new TGLayoutHints(kLHintsExpandX));

        hFrame->AddFrame(resultPanel, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

        // Add the above panels to main window
        AddFrame(hFrame, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

        // query history box
        TGGroupFrame* historyFrame = new TGGroupFrame(this, "Recent Queries");
        fQueryHistoryView = new TGTextView(historyFrame, 600, 60);
        fQueryHistoryView->SetEditable(kFALSE);
        historyFrame->AddFrame(fQueryHistoryView, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 5, 5, 5, 5));
        AddFrame(historyFrame, new TGLayoutHints(kLHintsExpandX, 10, 10, 10, 5));


        // SQL query input
        TGLabel *sqlLabel = new TGLabel(this, "Execute SQL:");
        AddFrame(sqlLabel, new TGLayoutHints(kLHintsLeft, 10, 10, 5, 0));

        TGHorizontalFrame *sqlRow = new TGHorizontalFrame(this);

        fSQLBox = new TGTextEdit(sqlRow, 500, 80);  // width, height in pixels
        fSQLBox->SetEditable(kTRUE);
        sqlRow->AddFrame(fSQLBox, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 5, 5, 5, 5));

        fRunSQLBtn = new TGTextButton(sqlRow, "Run");
        fRunSQLBtn->Connect("Clicked()", "MyMainFrame", this, "OnRunSQLClicked()");
        sqlRow->AddFrame(fRunSQLBtn, new TGLayoutHints(kLHintsLeft | kLHintsTop, 5, 5, 5, 5));

        AddFrame(sqlRow, new TGLayoutHints(kLHintsExpandX));

        // Toggle Hints button
        fToggleHintsBtn = new TGTextButton(this, "Show Examples");
        fToggleHintsBtn->Connect("Clicked()", "MyMainFrame", this, "OnToggleHints()");
        AddFrame(fToggleHintsBtn, new TGLayoutHints(kLHintsLeft, 10, 10, 2, 0));
        TGFrame *spacer = new TGFrame(this, 1, 10);  // width is ignored; height = 10px
        AddFrame(spacer, new TGLayoutHints(kLHintsExpandX, 0, 0, 0, 0));

        // Wrapper to fully hide hints
        fHintWrapper = new TGVerticalFrame(this);

        // Group frame inside wrapper
        fHintBox = new TGGroupFrame(fHintWrapper, "Example SQL Queries");
        fHintText = new TGTextView(fHintBox, 600, 80);
        fHintText->SetEditable(kFALSE);

        // Get directory where this source file resides
        char sourcePath[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", sourcePath, sizeof(sourcePath)-1);
        sourcePath[len] = '\0';

        TString macroDir = gSystem->DirName(__FILE__);  // Use macro’s file location
        TString fullHintPath = Form("%s/sql_hints.txt", macroDir.Data());

        std::ifstream hintFile(fullHintPath.Data());

        if (!hintFile.is_open()) {
            fHintText->AddLine(Form("Failed to load %s", fullHintPath.Data()));
        } else {
            std::string line;
            while (std::getline(hintFile, line)) {
                fHintText->AddLine(line.c_str());
            }
        }

        fHintBox->AddFrame(fHintText, new TGLayoutHints(kLHintsExpandX, 5, 5, 5, 5));
        fHintWrapper->AddFrame(fHintBox, new TGLayoutHints(kLHintsExpandX));
        AddFrame(fHintWrapper, new TGLayoutHints(kLHintsExpandX));

        // Populate table dropdown
        if (fDB) {
            TSQLResult *res = fDB->GetTables("");
            while (TSQLRow *row = res->Next()) {
                TString tableName = row->GetField(0);
                fTableDropdown->AddEntry(tableName, fTableDropdown->GetNumberOfEntries() + 1);

                delete row;
            }
            delete res;
        }

        // Final Layout
        MapSubwindows();
        fHintText->Update();
        this->HideFrame(fHintWrapper);  // Actually hide it
        fHintsVisible = false;          // Explicitly reflect that state

        Resize(GetDefaultSize());
        MapWindow();
    }

    // Destructor: cleans up database connection and TG resources.
    virtual ~MyMainFrame() {
        Cleanup();
        delete fDB;
    }
};

#include "gui_handlers.inline.h"

// Entry point: creates and displays the main window.
void sqliteViewer() {
    new MyMainFrame(gClient->GetRoot(), 800, 500);
}
