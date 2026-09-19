# Copyright (c) 2026 Fulda University of Applied Sciences. SPDX-License-Identifier: BSD-3-Clause
# Shared paper-export Gnuplot helpers.
# Plot-specific font sizes, margins, and labels remain in each *_epslatex.gp file.
set datafile separator ","
GP_GOOD = "#1a9850"
GP_MID = "#f7f7f7"
GP_BAD = "#b2182b"
GP_TEXT = "#111111"
GP_GRID = "#d0d0d0"
GP_BORDER = "#777777"
GP_CELL_BORDER = "#666666"
GP_NEUTRAL = "#666666"
# Policy colors (F0/F10/F30) shared by all bar figures
# diverging value palette (GP_GOOD/GP_BAD) so policy identity never implies a rating.
GP_F0  = "#55A868"
GP_F10 = "#C44E52"
GP_F30 = "#4C72B0"
GP_LATEX_TEXT(size, baseline, text) = sprintf("%cfontsize{%s}{%s}%cselectfont %s", 92, size, baseline, 92, text)
GP_LATEX_LABEL(size, baseline, text) = sprintf("%c%cfontsize{%s}{%s}%c%cselectfont %s", 92, 92, size, baseline, 92, 92, text)
GP_TIC_FORMAT(fmt, size, baseline) = sprintf("%cfontsize{%s}{%s}%cselectfont %s", 92, size, baseline, 92, fmt)
