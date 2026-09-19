# Copyright (c) 2026 Fulda University of Applied Sciences. SPDX-License-Identifier: BSD-3-Clause
# Run from this directory with: gnuplot shutdown_policy_matrices_by_cluster_epslatex.gp
set terminal epslatex color size 8.5cm,4.0cm font ",8"
set output "shutdown_policy_matrices_by_cluster.tex"
load "paper_style.gp"
# Font controls for this figure.
# GP_SHUTDOWN_HEADER_FONT_SIZE = "9.0pt"
# GP_SHUTDOWN_HEADER_FONT_BASELINE = "9.0pt"
# GP_SHUTDOWN_NOTE_FONT_SIZE = "7.5pt"
# GP_SHUTDOWN_NOTE_FONT_BASELINE = "7.5pt"
GP_SHUTDOWN_PANEL_TITLE_FONT_SIZE = "8.0pt"
GP_SHUTDOWN_PANEL_TITLE_FONT_BASELINE = "8.0pt"
GP_SHUTDOWN_TIC_FONT_SIZE = "7.5pt"
GP_SHUTDOWN_TIC_FONT_BASELINE = "7.5pt"
GP_SHUTDOWN_CELL_FONT_SIZE = "7.5pt"
GP_SHUTDOWN_CELL_FONT_BASELINE = "7.5pt"
# set label 1000 GP_LATEX_TEXT(GP_SHUTDOWN_HEADER_FONT_SIZE, GP_SHUTDOWN_HEADER_FONT_BASELINE, "F30 shutdown success metrics by cluster and scheduler") at screen 0.5, screen 0.968 center front
# set label 1001 GP_LATEX_TEXT(GP_SHUTDOWN_NOTE_FONT_SIZE, GP_SHUTDOWN_NOTE_FONT_BASELINE, "Color: green=closer to row target, red=larger deviation") at screen 0.5, screen 0.888 center front
set multiplot layout 1,2 margins 0.165,0.98,0.156,0.87 spacing 0.06,0.00
set xrange [0.5:3.5]
set yrange [0.5:2.5]
set xtics (GP_LATEX_TEXT(GP_SHUTDOWN_TIC_FONT_SIZE, GP_SHUTDOWN_TIC_FONT_BASELINE, "Haswell") 1, GP_LATEX_TEXT(GP_SHUTDOWN_TIC_FONT_SIZE, GP_SHUTDOWN_TIC_FONT_BASELINE, "KNL") 2, GP_LATEX_TEXT(GP_SHUTDOWN_TIC_FONT_SIZE, GP_SHUTDOWN_TIC_FONT_BASELINE, "Theta") 3) scale 0 offset 0,0.3
set ytics (GP_LATEX_TEXT(GP_SHUTDOWN_TIC_FONT_SIZE, GP_SHUTDOWN_TIC_FONT_BASELINE, "\\shortstack[r]{mean\\\\on-window\\\\utilization}") 2, GP_LATEX_TEXT(GP_SHUTDOWN_TIC_FONT_SIZE, GP_SHUTDOWN_TIC_FONT_BASELINE, "\\shortstack[r]{normalized\\\\node overrun}") 1) scale 0
set border lw 0.75
set key off
set style fill solid 1.0 border rgb GP_BORDER
unset colorbox
set title GP_LATEX_TEXT(GP_SHUTDOWN_PANEL_TITLE_FONT_SIZE, GP_SHUTDOWN_PANEL_TITLE_FONT_BASELINE, "F30 -- EBF") offset 0,-0.8
plot "data/shutdown_policy_cluster_F30_ebf.csv" every ::1 using 1:2:3:4:5:6:7 with boxxyerrorbars lc rgb variable notitle, \
     "data/shutdown_policy_cluster_F30_ebf.csv" every ::1 using 1:2:(GP_LATEX_LABEL(GP_SHUTDOWN_CELL_FONT_SIZE, GP_SHUTDOWN_CELL_FONT_BASELINE, ($2 == 2 ? sprintf("%.1f\\\\char37{}", $11) : sprintf("%.2fh", $11)))) with labels center tc rgb GP_TEXT notitle
# unset label 1000
# unset label 1001
unset ytics
set title GP_LATEX_TEXT(GP_SHUTDOWN_PANEL_TITLE_FONT_SIZE, GP_SHUTDOWN_PANEL_TITLE_FONT_BASELINE, "F30 -- MSA") offset 0,-0.8
plot "data/shutdown_policy_cluster_F30_msa.csv" every ::1 using 1:2:3:4:5:6:7 with boxxyerrorbars lc rgb variable notitle, \
     "data/shutdown_policy_cluster_F30_msa.csv" every ::1 using 1:2:(GP_LATEX_LABEL(GP_SHUTDOWN_CELL_FONT_SIZE, GP_SHUTDOWN_CELL_FONT_BASELINE, ($2 == 2 ? sprintf("%.1f\\\\char37{}", $11) : sprintf("%.2fh", $11)))) with labels center tc rgb GP_TEXT notitle
unset multiplot
