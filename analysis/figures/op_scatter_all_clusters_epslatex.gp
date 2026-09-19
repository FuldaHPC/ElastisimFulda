# Copyright (c) 2026 Fulda University of Applied Sciences. SPDX-License-Identifier: BSD-3-Clause
# Run from this directory with: gnuplot op_scatter_all_clusters_epslatex.gp
# Two-column (figure*) version: full \textwidth in IEEEtran (17.5cm) instead of \columnwidth.
# 6 panels in the style of the turnaround time decomposition: bars + black seed whiskers.
# Rows = turnaround time reduction / daily cost saving (both relative to EBF F0 at m0,
# so bars extend up or down from the zero line), columns = clusters, x = malleability.
# Bar = seed mean, whisker = min--max over the three seeds.
# MSA; policies F0 (green), F10 (red), F30 (blue), same palette as the other figures.
# CSV columns: 1=mal_idx 2=mal 3=policy 4=ta_mean 5=ta_min 6=ta_max 7=cost_mean 8=cost_min 9=cost_max
set terminal epslatex color size 17.5cm,6.0cm font ",8"
set output "op_scatter_all_clusters.tex"
load "paper_style.gp"
# Font controls for this figure.
GP_OP_TITLE_FONT_SIZE = "9.0pt"
GP_OP_TITLE_FONT_BASELINE = "9.0pt"
GP_OP_PANEL_TITLE_FONT_SIZE = "8.0pt"
GP_OP_PANEL_TITLE_FONT_BASELINE = "8.0pt"
GP_OP_TIC_FONT_SIZE = "7.5pt"
GP_OP_TIC_FONT_BASELINE = "7.5pt"
GP_OP_AXIS_FONT_SIZE = "6.5pt"
GP_OP_AXIS_FONT_BASELINE = "6.5pt"
GP_OP_LEGEND_FONT_SIZE = "7.5pt"
GP_OP_LEGEND_FONT_BASELINE = "7.5pt"
# set label 900 GP_LATEX_TEXT(GP_OP_TITLE_FONT_SIZE, GP_OP_TITLE_FONT_BASELINE, "MSA daily cost saving and turnaround time reduction relative to EBF F0 at m0") at screen 0.50,0.982 center front
set object 901 rect from screen 0.430,0.9438 to screen 0.441,0.9686 fc rgb GP_F0 fs solid 0.92 border rgb GP_BORDER front
set label 901 GP_LATEX_TEXT(GP_OP_LEGEND_FONT_SIZE, GP_OP_LEGEND_FONT_BASELINE, "F0") at screen 0.446,0.9562 left front
set object 902 rect from screen 0.492,0.9438 to screen 0.503,0.9686 fc rgb GP_F10 fs solid 0.92 border rgb GP_BORDER front
set label 902 GP_LATEX_TEXT(GP_OP_LEGEND_FONT_SIZE, GP_OP_LEGEND_FONT_BASELINE, "F10") at screen 0.508,0.9562 left front
set object 903 rect from screen 0.560,0.9438 to screen 0.571,0.9686 fc rgb GP_F30 fs solid 0.92 border rgb GP_BORDER front
set label 903 GP_LATEX_TEXT(GP_OP_LEGEND_FONT_SIZE, GP_OP_LEGEND_FONT_BASELINE, "F30") at screen 0.576,0.9562 left front
set grid y lw 0.4 lc rgb GP_GRID
set border lw 0.7
set tics out
set boxwidth 0.200 absolute
set bars 0.8
set pointintervalbox 0
set xrange [-0.58:4.58]
set xzeroaxis lt 1 lc rgb GP_BORDER lw 0.9
set multiplot layout 2,3 margins 0.055,0.995,0.101818182,0.867575758 spacing 0.045,0.072121212

# ---- row 1: turnaround time reduction ----
set yrange [-43:67]
set ytics 20
set format y GP_TIC_FORMAT("%g", GP_OP_TIC_FONT_SIZE, GP_OP_TIC_FONT_BASELINE)
set ytics offset 0.8,0
set xtics (GP_LATEX_TEXT(GP_OP_TIC_FONT_SIZE, GP_OP_TIC_FONT_BASELINE, "m0") 0, GP_LATEX_TEXT(GP_OP_TIC_FONT_SIZE, GP_OP_TIC_FONT_BASELINE, "m25") 1, GP_LATEX_TEXT(GP_OP_TIC_FONT_SIZE, GP_OP_TIC_FONT_BASELINE, "m50") 2, GP_LATEX_TEXT(GP_OP_TIC_FONT_SIZE, GP_OP_TIC_FONT_BASELINE, "m75") 3, GP_LATEX_TEXT(GP_OP_TIC_FONT_SIZE, GP_OP_TIC_FONT_BASELINE, "m100") 4) scale 0 offset 0,0.5
set ylabel GP_LATEX_TEXT(GP_OP_AXIS_FONT_SIZE, GP_OP_AXIS_FONT_BASELINE, "turnaround time reduction [\\char37{}]") offset 14.7,0
set title GP_LATEX_TEXT(GP_OP_PANEL_TITLE_FONT_SIZE, GP_OP_PANEL_TITLE_FONT_BASELINE, "Haswell") offset 0,-0.8
plot "data/op_scatter_cori_haswell.csv" every ::1 using ($1-0.24):(strcol(3) eq "F0"  ? $4 : 1/0) with boxes fs solid 0.92 lc rgb GP_F0 notitle, \
     "data/op_scatter_cori_haswell.csv" every ::1 using ($1+0.00):(strcol(3) eq "F10" ? $4 : 1/0) with boxes fs solid 0.92 lc rgb GP_F10 notitle, \
     "data/op_scatter_cori_haswell.csv" every ::1 using ($1+0.24):(strcol(3) eq "F30" ? $4 : 1/0) with boxes fs solid 0.92 lc rgb GP_F30 notitle, \
     "data/op_scatter_cori_haswell.csv" every ::1 using ($1-0.24):(strcol(3) eq "F0"  ? ($5+$6)/2 : 1/0):5:6 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle, \
     "data/op_scatter_cori_haswell.csv" every ::1 using ($1+0.00):(strcol(3) eq "F10" ? ($5+$6)/2 : 1/0):5:6 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle, \
     "data/op_scatter_cori_haswell.csv" every ::1 using ($1+0.24):(strcol(3) eq "F30" ? ($5+$6)/2 : 1/0):5:6 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle
unset ylabel
set title GP_LATEX_TEXT(GP_OP_PANEL_TITLE_FONT_SIZE, GP_OP_PANEL_TITLE_FONT_BASELINE, "KNL") offset 0,-0.8
plot "data/op_scatter_cori_knl.csv" every ::1 using ($1-0.24):(strcol(3) eq "F0"  ? $4 : 1/0) with boxes fs solid 0.92 lc rgb GP_F0 notitle, \
     "data/op_scatter_cori_knl.csv" every ::1 using ($1+0.00):(strcol(3) eq "F10" ? $4 : 1/0) with boxes fs solid 0.92 lc rgb GP_F10 notitle, \
     "data/op_scatter_cori_knl.csv" every ::1 using ($1+0.24):(strcol(3) eq "F30" ? $4 : 1/0) with boxes fs solid 0.92 lc rgb GP_F30 notitle, \
     "data/op_scatter_cori_knl.csv" every ::1 using ($1-0.24):(strcol(3) eq "F0"  ? ($5+$6)/2 : 1/0):5:6 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle, \
     "data/op_scatter_cori_knl.csv" every ::1 using ($1+0.00):(strcol(3) eq "F10" ? ($5+$6)/2 : 1/0):5:6 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle, \
     "data/op_scatter_cori_knl.csv" every ::1 using ($1+0.24):(strcol(3) eq "F30" ? ($5+$6)/2 : 1/0):5:6 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle
set title GP_LATEX_TEXT(GP_OP_PANEL_TITLE_FONT_SIZE, GP_OP_PANEL_TITLE_FONT_BASELINE, "Theta") offset 0,-0.8
plot "data/op_scatter_theta.csv" every ::1 using ($1-0.24):(strcol(3) eq "F0"  ? $4 : 1/0) with boxes fs solid 0.92 lc rgb GP_F0 notitle, \
     "data/op_scatter_theta.csv" every ::1 using ($1+0.00):(strcol(3) eq "F10" ? $4 : 1/0) with boxes fs solid 0.92 lc rgb GP_F10 notitle, \
     "data/op_scatter_theta.csv" every ::1 using ($1+0.24):(strcol(3) eq "F30" ? $4 : 1/0) with boxes fs solid 0.92 lc rgb GP_F30 notitle, \
     "data/op_scatter_theta.csv" every ::1 using ($1-0.24):(strcol(3) eq "F0"  ? ($5+$6)/2 : 1/0):5:6 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle, \
     "data/op_scatter_theta.csv" every ::1 using ($1+0.00):(strcol(3) eq "F10" ? ($5+$6)/2 : 1/0):5:6 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle, \
     "data/op_scatter_theta.csv" every ::1 using ($1+0.24):(strcol(3) eq "F30" ? ($5+$6)/2 : 1/0):5:6 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle

# ---- row 2: daily cost saving ----
unset title
set yrange [-11:15]
set ytics 5
set format y GP_TIC_FORMAT("%g", GP_OP_TIC_FONT_SIZE, GP_OP_TIC_FONT_BASELINE)
set xtics (GP_LATEX_TEXT(GP_OP_TIC_FONT_SIZE, GP_OP_TIC_FONT_BASELINE, "m0") 0, GP_LATEX_TEXT(GP_OP_TIC_FONT_SIZE, GP_OP_TIC_FONT_BASELINE, "m25") 1, GP_LATEX_TEXT(GP_OP_TIC_FONT_SIZE, GP_OP_TIC_FONT_BASELINE, "m50") 2, GP_LATEX_TEXT(GP_OP_TIC_FONT_SIZE, GP_OP_TIC_FONT_BASELINE, "m75") 3, GP_LATEX_TEXT(GP_OP_TIC_FONT_SIZE, GP_OP_TIC_FONT_BASELINE, "m100") 4) scale 0 offset 0,0.5
set ylabel GP_LATEX_TEXT(GP_OP_AXIS_FONT_SIZE, GP_OP_AXIS_FONT_BASELINE, "daily cost saving [\\char37{}]") offset 14.7,0
plot "data/op_scatter_cori_haswell.csv" every ::1 using ($1-0.24):(strcol(3) eq "F0"  ? $7 : 1/0) with boxes fs solid 0.92 lc rgb GP_F0 notitle, \
     "data/op_scatter_cori_haswell.csv" every ::1 using ($1+0.00):(strcol(3) eq "F10" ? $7 : 1/0) with boxes fs solid 0.92 lc rgb GP_F10 notitle, \
     "data/op_scatter_cori_haswell.csv" every ::1 using ($1+0.24):(strcol(3) eq "F30" ? $7 : 1/0) with boxes fs solid 0.92 lc rgb GP_F30 notitle, \
     "data/op_scatter_cori_haswell.csv" every ::1 using ($1-0.24):(strcol(3) eq "F0"  ? ($8+$9)/2 : 1/0):8:9 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle, \
     "data/op_scatter_cori_haswell.csv" every ::1 using ($1+0.00):(strcol(3) eq "F10" ? ($8+$9)/2 : 1/0):8:9 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle, \
     "data/op_scatter_cori_haswell.csv" every ::1 using ($1+0.24):(strcol(3) eq "F30" ? ($8+$9)/2 : 1/0):8:9 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle
unset ylabel
plot "data/op_scatter_cori_knl.csv" every ::1 using ($1-0.24):(strcol(3) eq "F0"  ? $7 : 1/0) with boxes fs solid 0.92 lc rgb GP_F0 notitle, \
     "data/op_scatter_cori_knl.csv" every ::1 using ($1+0.00):(strcol(3) eq "F10" ? $7 : 1/0) with boxes fs solid 0.92 lc rgb GP_F10 notitle, \
     "data/op_scatter_cori_knl.csv" every ::1 using ($1+0.24):(strcol(3) eq "F30" ? $7 : 1/0) with boxes fs solid 0.92 lc rgb GP_F30 notitle, \
     "data/op_scatter_cori_knl.csv" every ::1 using ($1-0.24):(strcol(3) eq "F0"  ? ($8+$9)/2 : 1/0):8:9 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle, \
     "data/op_scatter_cori_knl.csv" every ::1 using ($1+0.00):(strcol(3) eq "F10" ? ($8+$9)/2 : 1/0):8:9 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle, \
     "data/op_scatter_cori_knl.csv" every ::1 using ($1+0.24):(strcol(3) eq "F30" ? ($8+$9)/2 : 1/0):8:9 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle
plot "data/op_scatter_theta.csv" every ::1 using ($1-0.24):(strcol(3) eq "F0"  ? $7 : 1/0) with boxes fs solid 0.92 lc rgb GP_F0 notitle, \
     "data/op_scatter_theta.csv" every ::1 using ($1+0.00):(strcol(3) eq "F10" ? $7 : 1/0) with boxes fs solid 0.92 lc rgb GP_F10 notitle, \
     "data/op_scatter_theta.csv" every ::1 using ($1+0.24):(strcol(3) eq "F30" ? $7 : 1/0) with boxes fs solid 0.92 lc rgb GP_F30 notitle, \
     "data/op_scatter_theta.csv" every ::1 using ($1-0.24):(strcol(3) eq "F0"  ? ($8+$9)/2 : 1/0):8:9 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle, \
     "data/op_scatter_theta.csv" every ::1 using ($1+0.00):(strcol(3) eq "F10" ? ($8+$9)/2 : 1/0):8:9 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle, \
     "data/op_scatter_theta.csv" every ::1 using ($1+0.24):(strcol(3) eq "F30" ? ($8+$9)/2 : 1/0):8:9 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle
unset multiplot
