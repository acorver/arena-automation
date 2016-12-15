
library(rgl)
library(tidyr)
library(plyr)
library(twiddler)
library(data.table)
library(smoother)
library(zoo)
library(animation)

setwd('Z:/people/Abel/arena-automation/analysis/r')

# ========================================================================================
# Load data
# ========================================================================================

hm.all <- fread('../data/2016-11-14 14-09-32_Cortex.headmvmt.csv')

hm <- hm.all
hm <- hm[!is.na(hm.all$hml_x) & !is.na(hm.all$yfl_x),]
hm <- hm[hm$valid & hm_e_h < 4,]

# ========================================================================================
# Plot timeseries
# ========================================================================================

# 
hist(hm$hm_e_h)
hist(hm$hm_e_h[hm$valid])

# Smooth
fs <- rollmean(hm$frame, 200)

azm  <- hm$h_azm * (360/(2*3.1415))
azms <- rollmean(azm, 200)

alt  <- hm$h_alt * (360/(2*3.1415))
alts <- rollmean(alt, 200)

plot(fs[1:(200*120000)], alts[1:(200*120000)], type='l')
plot(fs[1:(200*1200)], azms[1:(200*1200)], type='l')

# Find a frame where head moved at least 20 degrees in 100 ms (20 frames)
isMvmtFrame <- alt[1:(length(alt)-20)] - alt[21:length(alt)]
frames <- hm$frame[abs(isMvmtFrame)>30]

#
#d <- hm[1e4:(1e4+200*60)]

fMvmtGUI <- function(iframe) {
  d <- hm[hm$frame >= frames[iframe] & hm$frame < (frames[iframe]+200),]
  plot(d$frame,d$h_alt * (360/(2*3.1415)), type='l')
  plot3d(d$hml_x,d$hml_y,d$hml_z,col='red')
  points3d(d$hmr_x,d$hmr_y,d$hmr_z,col='blue')
  segments3d(x=as.vector(t( cbind(d$hml_x, d$hmr_x) )),
             y=as.vector(t( cbind(d$hml_y, d$hmr_y) )),
             z=as.vector(t( cbind(d$hml_z, d$hmr_z) )), col='black') 
}
twiddle(fMvmtGUI(iframe), 
        iframe = knob(c(1, length(frames)),1), 
        auto = FALSE)

# Look at timecourse of angles prior to takeoff
# ...

# ========================================================================================
# Explore data
# ========================================================================================


fGUI <- function(start1000, start, len, ps) {
  
  hmss <- hm[hm$frame>=(start1000*1000+start) & hm$frame<(start1000*1000+start+len),]
  
  uid <- rbind(1:nrow(hmss),1:nrow(hmss))
  plot3d(hmss$nj_x, hmss$nj_y, hmss$nj_z, col='purple')
  segments3d(x=as.vector(t( cbind(hmss$ayfl_x, hmss$ayfc_x) )),
             y=as.vector(t( cbind(hmss$ayfl_y, hmss$ayfc_y) )),
             z=as.vector(t( cbind(hmss$ayfl_z, hmss$ayfc_z) )), col=uid) 
  segments3d(x=as.vector(t( cbind(hmss$ayfc_x, hmss$ayfr_x) )),
             y=as.vector(t( cbind(hmss$ayfc_y, hmss$ayfr_y) )),
             z=as.vector(t( cbind(hmss$ayfc_z, hmss$ayfr_z) )), col=uid) 
  segments3d(x=as.vector(t( cbind(hmss$ayfr_x, hmss$ayfl_x) )),
             y=as.vector(t( cbind(hmss$ayfr_y, hmss$ayfl_y) )),
             z=as.vector(t( cbind(hmss$ayfr_z, hmss$ayfl_z) )), col=uid) 
  
  text3d(hmss$ayfl_x, hmss$ayfl_y, hmss$ayfl_z,'  L')
  text3d(hmss$ayfc_x, hmss$ayfc_y, hmss$ayfc_z,'  C')
  text3d(hmss$ayfr_x, hmss$ayfr_y, hmss$ayfr_z,'  R')
  
  next3d();
  #spheres3d(hmss$nj_x[1], hmss$nj_y[1], hmss$nj_z[1], 
  #          radius=hmss$head_rad, col='red', front='line', back='line', alpha=0.2)
  plot3d(hmss$hml_x, hmss$hml_y, hmss$hml_z, col='red', size=ps, 
         xlim=c(-5,5),ylim=c(-10,5),zlim=c(-5,5))
  points3d(hmss$hmr_x, hmss$hmr_y, hmss$hmr_z, col='blue', size=ps)
  #points3d(hmss$nj_x, hmss$nj_y, hmss$nj_z, col='purple', size=10)
  segments3d(x=as.vector(t( cbind(hmss$yfl_x, hmss$yfc_x) )),
             y=as.vector(t( cbind(hmss$yfl_y, hmss$yfc_y) )),
             z=as.vector(t( cbind(hmss$yfl_z, hmss$yfc_z) )), col=uid) 
  segments3d(x=as.vector(t( cbind(hmss$yfc_x, hmss$yfr_x) )),
             y=as.vector(t( cbind(hmss$yfc_y, hmss$yfr_y) )),
             z=as.vector(t( cbind(hmss$yfc_z, hmss$yfr_z) )), col=uid) 
  segments3d(x=as.vector(t( cbind(hmss$yfr_x, hmss$yfl_x) )),
             y=as.vector(t( cbind(hmss$yfr_y, hmss$yfl_y) )),
             z=as.vector(t( cbind(hmss$yfr_z, hmss$yfl_z) )), col=uid) 
  
  segments3d(x=as.vector(t( cbind(hmss$hml_x, hmss$hmr_x) )),
             y=as.vector(t( cbind(hmss$hml_y, hmss$hmr_y) )),
             z=as.vector(t( cbind(hmss$hml_z, hmss$hmr_z) )), col='black') 
  text3d((hmss$hmr_x+hmss$hml_x)/2, (hmss$hmr_y+hmss$hml_y)/2, (hmss$hmr_z+hmss$hml_z)/2, hmss$hm_e_h)
#  text3d(hmss$hml_x, hmss$hml_y, hmss$hml_z, hmss$hm_e_h)
  
  text3d(hmss$yfl_x, hmss$yfl_y, hmss$yfl_z,'  L')
  text3d(hmss$yfc_x, hmss$yfc_y, hmss$yfc_z,'  C')
  text3d(hmss$yfr_x, hmss$yfr_y, hmss$yfr_z,'  R')
  
  title3d(main=paste0("Error_h=",mean(hmss$hm_e_h, na.rm=TRUE)))
}
open3d(); mfrow3d(1,2)
twiddle(fGUI(start1000, start, len, ps), 
        start1000 = knob(c(1, nrow(hm)/1000),1), 
        start = knob(c(1, 1000),1), 
        len   = knob(c(0, 1000),1),
        ps    = knob(c(1, 10)),
        auto = FALSE)

# ========================================================================================
# 
# ========================================================================================

# Plot points w.r.t. Yframe
plot3d(hm$x, hm$y, hm$z)
segments3d(x=as.vector(t( cbind(hm$vx1, hm$vx2) )),
           y=as.vector(t( cbind(hm$vy1, hm$vy2) )),
           z=as.vector(t( cbind(hm$vz1, hm$vz2) )), col='blue') 
segments3d(x=as.vector(t( cbind(hm$vx2, hm$vx3) )),
           y=as.vector(t( cbind(hm$vy2, hm$vy3) )),
           z=as.vector(t( cbind(hm$vz2, hm$vz3) )), col='blue') 
segments3d(x=as.vector(t( cbind(hm$vx3, hm$vx1) )),
           y=as.vector(t( cbind(hm$vy3, hm$vy1) )),
           z=as.vector(t( cbind(hm$vz3, hm$vz1) )), col='blue') 

# 
hm.sep <- hm[ !duplicated(hm$frame), ]
hm.sep <- separate_rows(hm.sep, pairwise, sep=';', convert=TRUE)
hist(hm.sep$pairwise)

# Number of frames
length(unique(hm.sep$frame))

# 
pw <- hm.sep$pairwise
hist(pw[pw>5 & pw<6])
