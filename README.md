# sqliteViewer
ROOT GUI Tool for Browsing, Querying, and Plotting SQLite Databases

---

## Table of Contents
1. Overview
2. Requirements
3. Setup
4. Usage
   4.1 Launching the Viewer
   4.2 Loading and Exploring Data
   4.3 Writing SQL Queries
   4.4 Plotting Histograms and Scatter Plots
   4.5 Exporting to CSV
   4.6 Viewing Query History and Hints
5. Notes on SQL Compatibility
6. Troubleshooting
7. Credits

---

## 1. Overview

`sqliteViewer` is a graphical ROOT application for interactive inspection and plotting of data from SQLite databases. It is designed for quick data browsing, simple plotting, and review of detector-related tables and results without writing custom scripts.

### Key Features:
- GUI-based `.sqlite` browser
- Read-only SQL query execution (`SELECT` only)
- Table and column dropdowns for quick access
- Plotting:
  - 1D Histograms (auto bin width via Freedman–Diaconis rule)
  - 2D Histograms and Scatter Plots
- CSV export
- Query history viewer
- Toggleable SQL hint box

---

## 2. Requirements

- ROOT (v6 recommended)
- A SQLite database (.sqlite) file with numeric data
- C++17-compatible environment (ROOT handles this internally)

---

## 3. Setup

Clone or download this repository. Ensure the following files are present:

- `sqliteViewerTESTING.C` – Entry point and GUI logic
- `gui_handlers.inline.h` – Button and menu callback methods
- `plot_utils.h` – Plotting utilities and histogram styling
- `sql_hints.txt` – Optional query examples for GUI hint panel

Place them in the same directory when launching.

---

## 4. Usage

### 4.1 Launching the Viewer

From your terminal:

```bash
root
.x sqliteViewerTESTING.C
```

A file dialog will prompt you to select a `.sqlite` database.

---

### 4.2 Loading and Exploring Data

- Use the **Table** dropdown to preview the contents of any table.
- The table preview area will show a formatted dump of the table.
- Table headers automatically populate the X/Y column selectors.

---

### 4.3 Writing SQL Queries

- Only `SELECT` queries are allowed.
- Results will populate the table preview.
- Column names in query results are used for plotting.

#### Example:
```sql
SELECT Detector_ID, CR FROM PMT_Data WHERE CR > 0.95;
```

- Query history is shown in a box below the result window.

---

### 4.4 Plotting Histograms and Scatter Plots

- Choose **Plot Type**: Histogram or Scatter
- Choose **Dimensions**: 1D or 2D
- Select X and (optionally) Y columns
- Click **Plot Data**

Histograms use automatic binning and label formatting.

---

### 4.5 Exporting to CSV

- Click **Save as CSV** to export the currently displayed table or query result.
- Choose a filename and location when prompted.

---

### 4.6 Viewing Query History and Hints

- The bottom of the window shows a scrollable history of the last 10 queries.
- Click **Show Examples** to reveal a hint box loaded from `sql_hints.txt`.

---

## 5. Notes on SQL Compatibility

### Allowed:
- `SELECT`, `WHERE`, `ORDER BY`, `GROUP BY`, `HAVING`
- `JOIN`, `LEFT JOIN`, `AS` (aliasing)
- Aggregates: `AVG()`, `COUNT()`, `MAX()`, etc.

### Disallowed:
- Any write operations: `INSERT`, `UPDATE`, `DELETE`
- Schema modification: `ALTER`, `DROP`, `CREATE`

---

## 6. Troubleshooting

- **Database fails to open:** Check that the file is accessible and not locked by another program.
- **Plot fails or crashes:** Ensure the columns you select contain numeric values.
- **Nothing plots:** Try switching from 2D to 1D, or recheck your SQL query.

---

## 7. Credits

Developed by Karsten Jennings
Idaho State University – Department of Physics
Contact: karstenjennings@isu.edu
