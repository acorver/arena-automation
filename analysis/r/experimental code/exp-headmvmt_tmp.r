
library(rgl)
library(tidyr)
library(twiddler)
library(data.table)
library(smoother)
library(zoo)
library(animation)

setwd('Z:/people/Abel/perching-analysis/r')

hm.all <- fread('../data/2016-11-14 14-09-32_Cortex.headmvmt.csv')

hm <- hm.all
hm <- hm[!is.na(hm.all$hml_x) & !is.na(hm.all$yfl_x),]
#hm <- hm[1e5:((1e5)+100000)]

# Plot head mvmt
hist(hm$h_azm * (360/(2*3.1415)))
hist(hm$h_alt)
hist(hm$h_roll)

azm <- hm$h_azm * (360/(2*3.1415))
plot(azm, type='l')
abline(h=mean(azm))

a <- azm[abs(azm-mean(azm)) > 10]

# ...
azm  <- hm$h_azm * (360/(2*3.1415))
azmm <- rollmean(azm, 100)

plot(azm, type='l')
lines(azmm, col='red')

# ========================================================================================
#
# ========================================================================================

fGUI <- function(offset, render) {
  c <- (offset*1000):(offset*1000+200*5)
  plot(azm[c], type='l',ylim=c(20, 160))
  lines(azmm[c], col='red')
  
  f <- function(t){
    print(t)
    hmss <- hm[(offset*1000+t*100):(offset*1000+t*100+1),]
    plot3d(hmss$hmr_x, hmss$hmr_y, hmss$hmr_z, col='blue', size=5,
           xlim=c(-5,5),ylim=c(-5,5),zlim=c(-5,5))
    points3d(hmss$hml_x, hmss$hml_y, hmss$hml_z, col='red', size=5)
    segments3d(x=as.vector(t( cbind(hmss$hml_x, hmss$hmr_x) )),
               y=as.vector(t( cbind(hmss$hml_y, hmss$hmr_y) )),
               z=as.vector(t( cbind(hmss$hml_z, hmss$hmr_z) )), col='black') 
    text3d(hmss$hmr_x, hmss$hmr_y, hmss$hmr_z,'  L')
    text3d(hmss$hml_x, hmss$hml_y, hmss$hml_z,'  R')
  }
  
  f2 <- function() {
    hmss <- hm[(offset*1000):(offset*1000+1000),]
    #plot3d(hmss$hmr_x, hmss$hmr_y, hmss$hmr_z, col='blue', size=5,
    #       xlim=c(-5,5),ylim=c(-5,5),zlim=c(-5,5))
    #points3d(hmss$hml_x, hmss$hml_y, hmss$hml_z, col='red', size=5)
    segments3d(x=as.vector(t( cbind(hmss$hml_x, hmss$hmr_x) )),
               y=as.vector(t( cbind(hmss$hml_y, hmss$hmr_y) )),
               z=as.vector(t( cbind(hmss$hml_z, hmss$hmr_z) )), col='black') 
    text3d(hmss$hmr_x, hmss$hmr_y, hmss$hmr_z,'  L')
    text3d(hmss$hml_x, hmss$hml_y, hmss$hml_z,'  R')
  }
  
  cmd <- paste0(
    "ffmpeg -framerate 10 -i ",
    "\"Z:/people/Abel/perching-analysis/r/headmvmt_vid/movie%%03d.png\" ", 
    "-c:v libx264 -r 30 -pix_fmt yuv420p -crf 15 ",
    "out.mp4\"")
  if(render>0){movie3d(f, duration=10, dir='headmvmt_vid', convert=cmd)}else{f2()}
}
x11(); open3d(); 
grid3d('x');grid3d('y');grid3d('z');
twiddle(fGUI(offset, render), 
        offset = knob(c(1, 1e3),1), 
        render = knob(c(0,1),1),
        auto = FALSE)

# ========================================================================================
#
# ========================================================================================

# Plot head fit characteristics
hist(hm$head_res)
hist(hm$head_res[hm$head_res>-10])
min(hm$head_res)
max(hm$head_res)

# Fit sphere to head


#
hm <- hm.all[abs(hm.all$head_res_l)<5,]
plot3d(hm$hml_x, hm$hml_y, hm$hml_z, col='red', size=1)
points3d(hm$hmr_x, hm$hmr_y, hm$hmr_z, col='blue', size=1)
points3d(hm$nj_x, hm$nj_y, hm$nj_z, col='black', size=10)

segments3d(x=as.vector(t( cbind(hm$yfl_x, hm$yfc_x) )),
           y=as.vector(t( cbind(hm$yfl_y, hm$yfc_y) )),
           z=as.vector(t( cbind(hm$yfl_z, hm$yfc_z) )), col='blue') 
segments3d(x=as.vector(t( cbind(hm$yfc_x, hm$yfr_x) )),
           y=as.vector(t( cbind(hm$yfc_y, hm$yfr_y) )),
           z=as.vector(t( cbind(hm$yfc_z, hm$yfr_z) )), col='blue') 
segments3d(x=as.vector(t( cbind(hm$yfr_x, hm$yfl_x) )),
           y=as.vector(t( cbind(hm$yfr_y, hm$yfl_y) )),
           z=as.vector(t( cbind(hm$yfr_z, hm$yfl_z) )), col='blue') 




segments3d(x=as.vector(t( cbind(hm$ayfl_x, hm$ayfc_x) )),
           y=as.vector(t( cbind(hm$ayfl_y, hm$ayfc_y) )),
           z=as.vector(t( cbind(hm$ayfl_z, hm$ayfc_z) )), col='blue') 
segments3d(x=as.vector(t( cbind(hm$ayfc_x, hm$ayfr_x) )),
           y=as.vector(t( cbind(hm$ayfc_y, hm$ayfr_y) )),
           z=as.vector(t( cbind(hm$ayfc_z, hm$ayfr_z) )), col='blue') 
segments3d(x=as.vector(t( cbind(hm$ayfr_x, hm$ayfl_x) )),
           y=as.vector(t( cbind(hm$ayfr_y, hm$ayfl_y) )),
           z=as.vector(t( cbind(hm$ayfr_z, hm$ayfl_z) )), col='blue') 

d <- cbind(
  hm$yfr_x-hm$yfl_x,
  hm$yfr_y-hm$yfl_y,
  hm$yfr_z-hm$yfl_z)
hist(d[d[,1]<7,1])
hist(d[,2])
hist(d[,3])

hist(hm$hm_e_l)
hist(hm$hm_e_h)

# ========================================================================================
# GUI
# ========================================================================================

#hm <- hm[sample(nrow(hm)),]
hm <- hm.all


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
