# Copyright (c) 2026 Fulda University of Applied Sciences. SPDX-License-Identifier: BSD-3-Clause
# Run from this directory with: gnuplot job_bar_charts_all_clusters_epslatex.gp
# Two-column (figure*) version: full \textwidth in IEEEtran (17.5cm) instead of \columnwidth.
set terminal epslatex color size 17.5cm,4.cm font ",8"
set output "job_bar_charts_all_clusters.tex"
load "paper_style.gp"
# Font controls for this figure.
GP_JOB_TITLE_FONT_SIZE = "9.0pt"
GP_JOB_TITLE_FONT_BASELINE = "9.0pt"
GP_JOB_PANEL_TITLE_FONT_SIZE = "8.0pt"
GP_JOB_PANEL_TITLE_FONT_BASELINE = "8.0pt"
GP_JOB_TIC_FONT_SIZE = "7.5pt"
GP_JOB_TIC_FONT_BASELINE = "7.5pt"
GP_JOB_AXIS_FONT_SIZE = "7.5pt"
GP_JOB_AXIS_FONT_BASELINE = "7.5pt"
GP_JOB_LEGEND_FONT_SIZE = "7.5pt"
GP_JOB_LEGEND_FONT_BASELINE = "7.5pt"
set boxwidth 0.200 absolute
set bars 0.8
set pointintervalbox 0
set xrange [-0.58:4.58]
set xtics (GP_LATEX_TEXT(GP_JOB_TIC_FONT_SIZE, GP_JOB_TIC_FONT_BASELINE, "m0") 0, GP_LATEX_TEXT(GP_JOB_TIC_FONT_SIZE, GP_JOB_TIC_FONT_BASELINE, "m25") 1, GP_LATEX_TEXT(GP_JOB_TIC_FONT_SIZE, GP_JOB_TIC_FONT_BASELINE, "m50") 2, GP_LATEX_TEXT(GP_JOB_TIC_FONT_SIZE, GP_JOB_TIC_FONT_BASELINE, "m75") 3, GP_LATEX_TEXT(GP_JOB_TIC_FONT_SIZE, GP_JOB_TIC_FONT_BASELINE, "m100") 4) scale 0 offset 0,0.5
set grid y lw 0.4 lc rgb GP_GRID
set border lw 0.7
set tics out
set format y GP_TIC_FORMAT("%.1f", GP_JOB_TIC_FONT_SIZE, GP_JOB_TIC_FONT_BASELINE)
set ytics offset 0.8,0.25
set yrange [0:*]
unset key
# set label 900 GP_LATEX_TEXT(GP_JOB_TITLE_FONT_SIZE, GP_JOB_TITLE_FONT_BASELINE, "MSA job turnaround time decomposition") at screen 0.50,0.975 center front
set object 901 rect from screen 0.335,0.9190 to screen 0.346,0.9548 fc rgb GP_F0 fs solid 0.92 border rgb GP_BORDER front
set label 901 GP_LATEX_TEXT(GP_JOB_LEGEND_FONT_SIZE, GP_JOB_LEGEND_FONT_BASELINE, "F0") at screen 0.351,0.9369 left front
set object 902 rect from screen 0.397,0.9190 to screen 0.408,0.9548 fc rgb GP_F10 fs solid 0.92 border rgb GP_BORDER front
set label 902 GP_LATEX_TEXT(GP_JOB_LEGEND_FONT_SIZE, GP_JOB_LEGEND_FONT_BASELINE, "F10") at screen 0.413,0.9369 left front
set object 903 rect from screen 0.459,0.9190 to screen 0.470,0.9548 fc rgb GP_F30 fs solid 0.92 border rgb GP_BORDER front
set label 903 GP_LATEX_TEXT(GP_JOB_LEGEND_FONT_SIZE, GP_JOB_LEGEND_FONT_BASELINE, "F30") at screen 0.475,0.9369 left front
set object 904 rect from screen 0.521,0.9190 to screen 0.532,0.9548 fc rgb GP_NEUTRAL fs pattern 2 border rgb GP_BORDER front
set label 904 GP_LATEX_TEXT(GP_JOB_LEGEND_FONT_SIZE, GP_JOB_LEGEND_FONT_BASELINE, "wait") at screen 0.537,0.9369 left front
set object 905 rect from screen 0.583,0.9190 to screen 0.594,0.9548 fc rgb GP_NEUTRAL fs solid 0.92 border rgb GP_BORDER front
set label 905 GP_LATEX_TEXT(GP_JOB_LEGEND_FONT_SIZE, GP_JOB_LEGEND_FONT_BASELINE, "runtime") at screen 0.599,0.9369 left front
set multiplot layout 1,3 margins 0.055,0.995,0.1048,0.7917 spacing 0.045,0.00
set ylabel GP_LATEX_TEXT(GP_JOB_AXIS_FONT_SIZE, GP_JOB_AXIS_FONT_BASELINE, "mean time [h]") offset 14.7,0
set title GP_LATEX_TEXT(GP_JOB_PANEL_TITLE_FONT_SIZE, GP_JOB_PANEL_TITLE_FONT_BASELINE, "Haswell") offset 0,-0.8
set yrange [0:*]
plot "data/cori_haswell_turnaround_mean_h.csv" every ::1 using ($1-0.24):3 with boxes fs solid 0.18 lc rgb GP_F0 notitle, \
     "data/cori_haswell_turnaround_mean_h.csv" every ::1 using ($1+0.00):5 with boxes fs solid 0.18 lc rgb GP_F10 notitle, \
     "data/cori_haswell_turnaround_mean_h.csv" every ::1 using ($1+0.24):4 with boxes fs solid 0.18 lc rgb GP_F30 notitle, \
     "data/cori_haswell_turnaround_mean_h.csv" every ::1 using ($1-0.24):3 with boxes fs pattern 2 lc rgb GP_F0 notitle, \
     "data/cori_haswell_turnaround_mean_h.csv" every ::1 using ($1+0.00):5 with boxes fs pattern 2 lc rgb GP_F10 notitle, \
     "data/cori_haswell_turnaround_mean_h.csv" every ::1 using ($1+0.24):4 with boxes fs pattern 2 lc rgb GP_F30 notitle, \
     "data/cori_haswell_runtime_mean_h.csv" every ::1 using ($1-0.24):3 with boxes fs solid 0.92 lc rgb GP_F0 notitle, \
     "data/cori_haswell_runtime_mean_h.csv" every ::1 using ($1+0.00):5 with boxes fs solid 0.92 lc rgb GP_F10 notitle, \
     "data/cori_haswell_runtime_mean_h.csv" every ::1 using ($1+0.24):4 with boxes fs solid 0.92 lc rgb GP_F30 notitle, \
     "data/cori_haswell_turnaround_range_h.csv" every ::1 using ($1-0.24):(($2+$3)/2):2:3 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle, \
     "data/cori_haswell_turnaround_range_h.csv" every ::1 using ($1+0.00):(($4+$5)/2):4:5 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle, \
     "data/cori_haswell_turnaround_range_h.csv" every ::1 using ($1+0.24):(($6+$7)/2):6:7 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle
unset ylabel
set title GP_LATEX_TEXT(GP_JOB_PANEL_TITLE_FONT_SIZE, GP_JOB_PANEL_TITLE_FONT_BASELINE, "KNL") offset 0,-0.8
set yrange [0:*]
plot "data/cori_knl_turnaround_mean_h.csv" every ::1 using ($1-0.24):3 with boxes fs solid 0.18 lc rgb GP_F0 notitle, \
     "data/cori_knl_turnaround_mean_h.csv" every ::1 using ($1+0.00):5 with boxes fs solid 0.18 lc rgb GP_F10 notitle, \
     "data/cori_knl_turnaround_mean_h.csv" every ::1 using ($1+0.24):4 with boxes fs solid 0.18 lc rgb GP_F30 notitle, \
     "data/cori_knl_turnaround_mean_h.csv" every ::1 using ($1-0.24):3 with boxes fs pattern 2 lc rgb GP_F0 notitle, \
     "data/cori_knl_turnaround_mean_h.csv" every ::1 using ($1+0.00):5 with boxes fs pattern 2 lc rgb GP_F10 notitle, \
     "data/cori_knl_turnaround_mean_h.csv" every ::1 using ($1+0.24):4 with boxes fs pattern 2 lc rgb GP_F30 notitle, \
     "data/cori_knl_runtime_mean_h.csv" every ::1 using ($1-0.24):3 with boxes fs solid 0.92 lc rgb GP_F0 notitle, \
     "data/cori_knl_runtime_mean_h.csv" every ::1 using ($1+0.00):5 with boxes fs solid 0.92 lc rgb GP_F10 notitle, \
     "data/cori_knl_runtime_mean_h.csv" every ::1 using ($1+0.24):4 with boxes fs solid 0.92 lc rgb GP_F30 notitle, \
     "data/cori_knl_turnaround_range_h.csv" every ::1 using ($1-0.24):(($2+$3)/2):2:3 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle, \
     "data/cori_knl_turnaround_range_h.csv" every ::1 using ($1+0.00):(($4+$5)/2):4:5 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle, \
     "data/cori_knl_turnaround_range_h.csv" every ::1 using ($1+0.24):(($6+$7)/2):6:7 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle
unset ylabel
set title GP_LATEX_TEXT(GP_JOB_PANEL_TITLE_FONT_SIZE, GP_JOB_PANEL_TITLE_FONT_BASELINE, "Theta") offset 0,-0.8
set yrange [0:*]
plot "data/theta_turnaround_mean_h.csv" every ::1 using ($1-0.24):3 with boxes fs solid 0.18 lc rgb GP_F0 notitle, \
     "data/theta_turnaround_mean_h.csv" every ::1 using ($1+0.00):5 with boxes fs solid 0.18 lc rgb GP_F10 notitle, \
     "data/theta_turnaround_mean_h.csv" every ::1 using ($1+0.24):4 with boxes fs solid 0.18 lc rgb GP_F30 notitle, \
     "data/theta_turnaround_mean_h.csv" every ::1 using ($1-0.24):3 with boxes fs pattern 2 lc rgb GP_F0 notitle, \
     "data/theta_turnaround_mean_h.csv" every ::1 using ($1+0.00):5 with boxes fs pattern 2 lc rgb GP_F10 notitle, \
     "data/theta_turnaround_mean_h.csv" every ::1 using ($1+0.24):4 with boxes fs pattern 2 lc rgb GP_F30 notitle, \
     "data/theta_runtime_mean_h.csv" every ::1 using ($1-0.24):3 with boxes fs solid 0.92 lc rgb GP_F0 notitle, \
     "data/theta_runtime_mean_h.csv" every ::1 using ($1+0.00):5 with boxes fs solid 0.92 lc rgb GP_F10 notitle, \
     "data/theta_runtime_mean_h.csv" every ::1 using ($1+0.24):4 with boxes fs solid 0.92 lc rgb GP_F30 notitle, \
     "data/theta_turnaround_range_h.csv" every ::1 using ($1-0.24):(($2+$3)/2):2:3 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle, \
     "data/theta_turnaround_range_h.csv" every ::1 using ($1+0.00):(($4+$5)/2):4:5 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle, \
     "data/theta_turnaround_range_h.csv" every ::1 using ($1+0.24):(($6+$7)/2):6:7 with yerrorbars lc rgb "#222222" lw 1.1 pt -1 notitle
unset multiplot
