// gui_handlers.inline.h
// Definitions of event handlers for MyMainFrame GUI.
// These methods respond to user interactions like table selection, SQL execution,
// CSV export, and plot commands in the sqliteViewer ROOT application.
#ifndef GUI_HANDLERS_H
#define GUI_HANDLERS_H

// Responds to user selecting a table from the dropdown.
// Queries all rows from the selected table and loads them into the viewer.
void MyMainFrame::OnTableSelected(Int_t id) {
    if (!fTableDropdown->GetSelectedEntry()) return;

    const char* tableLabel = fTableDropdown->GetSelectedEntry()->GetTitle();
    TString tableName(tableLabel);

    if (tableName == "Custom") return;

    fDataView->Clear();
    TString query = Form("SELECT * FROM \"%s\"", tableName.Data());

    if (fLastQueryResult) {
        delete fLastQueryResult;
        fLastQueryResult = nullptr;
    }

    fLastQueryResult = fDB->Query(query);
    LoadQueryResultsToTable(fLastQueryResult);
    delete fLastQueryResult;
    fLastQueryResult = nullptr;
}

// Exports the currently displayed table data to a CSV file.
// Prompts the user for a filename and formats the output with quoted entries.
void MyMainFrame::OnExportCSVClicked() {
    if (fCurrentTableData.empty()) {
        printf("No displayed table data to export.\n");
        return;
    }

    static const char* filetypes[] = {"CSV files", "*.csv", "All files", "*", nullptr};
    TGFileInfo fi;
    fi.fFileTypes = filetypes;
    fi.fIniDir = StrDup(".");
    new TGFileDialog(gClient->GetRoot(), this, kFDSave, &fi);

    TString path = fi.fFilename;
    if (path.IsNull() || path == "") return;
    if (!path.EndsWith(".csv")) path += ".csv";

    std::ofstream out(path.Data());
    if (!out.is_open()) {
        printf("Failed to open file for writing: %s\n", path.Data());
        return;
    }

    for (size_t i = 0; i < fCurrentTableHeader.size(); ++i) {
        out << "\"" << fCurrentTableHeader[i] << "\"";
        if (i < fCurrentTableHeader.size() - 1) out << ",";
    }
    out << "\n";

    for (const auto& row : fCurrentTableData) {
        for (size_t i = 0; i < row.size(); ++i) {
            out << "\"" << row[i] << "\"";
            if (i < row.size() - 1) out << ",";
        }
        out << "\n";
    }

    out.close();
    printf("CSV export complete: %s\n", path.Data());
    fflush(stdout);
}

// Toggles visibility of the hint panel showing example SQL queries.
void MyMainFrame::OnToggleHints() {
    if (fHintsVisible) {
        this->HideFrame(fHintWrapper);
        fToggleHintsBtn->SetText("Show Examples");
    } else {
        this->ShowFrame(fHintWrapper);
        fToggleHintsBtn->SetText("Hide Examples");
    }
    fHintsVisible = !fHintsVisible;
    Layout();
    Resize(GetDefaultSize());
}

// Prompts the user to select a new SQLite database file.
// Connects to the selected DB, repopulates table list, and clears data view.
void MyMainFrame::OnChangeFile() {
    TGFileInfo fi;
    const char* filetypes[] = {"SQLite files", "*.sqlite", "All files", "*", nullptr};
    fi.fFileTypes = filetypes;
    fi.fIniDir = StrDup(".");
    new TGFileDialog(gClient->GetRoot(), this, kFDOpen, &fi);

    if (!fi.fFilename || strlen(fi.fFilename) == 0) return;

    if (fDB) {
        delete fDB;
        fDB = nullptr;
    }

    TString dbPath = fi.fFilename;
    fDB = TSQLServer::Connect(Form("sqlite://%s", dbPath.Data()), "", "");

    if (!fDB || fDB->IsZombie()) {
        fDataView->Clear();
        fDataView->AddLine("Failed to open selected database.");
        fDataView->Update();
        return;
    }

    if (fDBPathLabel) {
        fDBPathLabel->SetText(Form("Database: %s", dbPath.Data()));
    }

    fTableDropdown->RemoveEntries(0, fTableDropdown->GetNumberOfEntries());

    TSQLResult* tables = fDB->GetTables("");
    if (tables) {
        TSQLRow* row;
        int id = 1;
        while ((row = tables->Next())) {
            fTableDropdown->AddEntry(row->GetField(0), id++);
            delete row;
        }
        delete tables;
    }

    fDataView->Clear();
    fDataView->Update();
}

// Executes the SQL entered by the user if it's a SELECT query.
// Updates the result viewer and logs the query in the history panel.
void MyMainFrame::OnRunSQLClicked() {
    TString userQuery = fSQLBox->GetText()->AsString();
    TString lower = userQuery; lower.ToLower();

    fQueryHistory.push_back(userQuery);
    if (fQueryHistory.size() > kMaxQueryHistory)
        fQueryHistory.pop_front();

    fQueryHistoryView->Clear();
    for (const auto& q : fQueryHistory)
        fQueryHistoryView->AddLine(q);
    fQueryHistoryView->Update();

    if (!lower.BeginsWith("select")) {
        fDataView->Clear();
        fDataView->AddLine("Only SELECT queries are allowed.");
        fDataView->Update();
        return;
    }

    if (fLastQueryResult) {
        delete fLastQueryResult;
        fLastQueryResult = nullptr;
    }

    fLastQueryResult = fDB->Query(userQuery);
    if (!fLastQueryResult) {
        fDataView->Clear();
        fDataView->AddLine("Query failed or returned no results.");
        fDataView->Update();
        return;
    }

    LoadQueryResultsToTable(fLastQueryResult);
    delete fLastQueryResult;
    fLastQueryResult = nullptr;

    int customId = 99999;
    TGTextLBEntry* existing = dynamic_cast<TGTextLBEntry*>(
        fTableDropdown->GetListBox()->FindEntry("Custom")
    );

    if (existing) {
        fTableDropdown->RemoveEntry(customId);
    }

    fTableDropdown->AddEntry("Custom", customId);
    fTableDropdown->Select(customId);
    fTableDropdown->RemoveEntry(customId);

    fSQLBox->Clear();
}

// Enables/disables the Y-axis column selector based on plot dimensionality.
// If user selects 1D, disables the Y column.
void MyMainFrame::OnDimensionChanged(Int_t dim) {
    if (dim == 1) {
        fYColumnSelect->SetEnabled(kFALSE);
        fYColumnSelect->AddEntry("", -1);
        fYColumnSelect->Select(-1);
    } else {
        fYColumnSelect->SetEnabled(kTRUE);
        fYColumnSelect->RemoveEntry(-1);
    }

    fYColumnSelect->Layout();
}

// Gathers selected columns and plotting options from the GUI.
// Passes them to PlotSelectedData() for visualization.
void MyMainFrame::OnPlotButtonClicked() {
    int xIndex = fXColumnSelect->GetSelected() - 1;
    int yIndex = fYColumnSelect->GetSelected() - 1;
    int plotType = fPlotTypeBox->GetSelected();  // 1 = Histogram, 2 = Scatter

    if (xIndex < 0 || xIndex >= fCurrentTableHeader.size()) {
        printf("Invalid X column selection.\n");
        return;
    }

    PlotSelectedData(
        fCurrentTableData,
        fCurrentTableHeader,
        xIndex,
        yIndex,
        plotType,
        fCanvasQueue,
        kMaxCanvases,
        fLastHist,
        fLastHist2D
    );
}

#endif // GUI_HANDLERS_H
