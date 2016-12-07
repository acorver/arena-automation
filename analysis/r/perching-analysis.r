
# TODO: Create knitr report with all graphs, etc.
# TODO: Make this script auto-install all packages if not present, for portability
# TODO: Allow groups to be named, and file names to reflect name not group index
# 

# ================================================================
# == IMPORTS
# ================================================================

library(ggplot2)
library(rgl)
library(fields)
library(RColorBrewer)
library(hexbin)
library(parallel)
library(plyr)
library(data.table)
library(bit64)
library(compiler)
library(tcltk)
library(twiddler)
library(zoo)

# ================================================================
# == GLOBAL SETTINGS
# ================================================================

base.path <- 'Z:/people/Abel/perching-analysis/data/'

wdir <- 'Z:/people/Abel/perching-analysis/'

DEBUG <- FALSE

# These groups define what data gets aggregated and plotted together
groups = c(
  '(2016-10-(26).*)',
  '(2016-10-(25|22).*)',
  '(2016-10-(21).*)',
  '(2016-10-(20).*)',
  '(2016-10-(19).*)',
  '(2016-10-(18).*)',
  '(2016-10-(12).*)',
  '(2016-09-(10).*)|(2016-09-09 (1[7-9]|2[0-4]).*)',
  '(2016-09-09 1[4-6].*)',
  '(2016-09-09 1[0-1].*)',
  '(2016-10-(27).*)',
  '(2016-11-(04).*)',
  '(2016-11-(07).*)',
  '(2016-11-14.*)',
  '(2016-11-15 16.*)'
)

# ================================================================
# == READ DATA FUNCTIONS
# ================================================================

getDataList <- function(files, colNames, header=TRUE, fill=TRUE) {
  
  if (length(files)>1) {
    numCores <- detectCores() - 1;
    cl <- makeCluster(numCores);
    clusterEvalQ(cl, { library(data.table) })
    x <- parLapply(cl,files, 
      function(x, base.path, DEBUG){
        fread(paste(base.path, x, sep=''), header=header, fill=fill)
        #tryCatch(fread(paste(base.path, x, sep='')), 
        #  error=function(e) data.frame(matrix(nrow = 0, ncol = length(colNames))))
      }, base.path);
    stopCluster(cl)
    return(x);
  } else {
    return(.(fread(paste(base.path, files[1], sep=''))));
  }
}

loadData <- function(base.path, pattern, colNames=NULL) {
  
  files = list.files(base.path)
  files = files[grep(pattern, files, perl=TRUE)]
  
  if (length(files) == 0) {
    return(data.frame(matrix(nrow = 0, ncol = length(colNames))))
  }
  
  dl <- getDataList(files, colNames)
  data <- do.call(rbind, dl)
  
  if (!is.null(colNames)) {
    tryCatch({colnames(data) <- colNames});
  }
  
  # Replace Cortex's NaN values with R NaNs
  data[data==9999999] <- NA
  
  gc();
  
  return (data)
}

loadTakeoffs <- function(groupIdx) {
  takeoffs <- loadData(base.path, paste0(groups[groupIdx],'.takeoffs.csv'))
  takeoffs$upward <- (takeoffs$upward==1)
  takeoffs$timestamp <- as.double(takeoffs$timestamp)
  takeoffs$id <- 0:(nrow(takeoffs)-1)
  return(data.table(takeoffs))
}

loadFlysim <- function(groupIdx) {
  flysim <- data.table(loadData(base.path, paste0(groups[groupIdx],'.flysim.csv'), 
    c('trajectory', 'framestart', 'frameend', 'is_flysim', 'score_dir', 
      'score_r2', 'distanceFromYframe', 'distanceFromYframeSD', 'dist_ok', 
      'len_ok', 'dir_ok', 'std', 'stdX', 'stdY', 'stdZ')))
  return(flysim)
}

loadFlysimTracking <- function(groupIdx) {
  flysimTracking <- data.table(loadData(base.path, paste0(groups[groupIdx],'.flysim.tracking.csv')))
  return(flysimTracking)
}

loadTrials <- function(groupIdx) {
  trials <- data.table(loadData(base.path, pasteo(groups[groupIdx],'.trials.csv')))
  return(trials)
}

.loadDfTracking <- function(groupIdx) {

  # Load tracking
  tracking <- data.table(loadData(base.path, paste0(groups[groupIdx],'(?<!flysim).tracking.csv'))
    , c('frame','relframe','trajectory','timestamp','time','takeoffTraj','x','y','z'))
  tracking$timestamp <- as.double(tracking$timestamp)
  
  # Load flysim
  flysim <- loadFlysim( groupIdx )
  flysim$frame <- flysim$framestart
  
  # Do left outer join to add flysim info
  setkey(tracking, frame)
  setkey(flysim  , frame)
  tracking$flysimStart <- flysim[tracking]$framestart
  tracking$flysimStart <- na.locf(tracking$flysimStart)
  tracking$afterFlysim <- tracking$frame - tracking$flysimStart
  
  # Join with takeoff data
  takeoffs <- loadTakeoffs( groupIdx )
  takeoffs$takeoffTraj <- takeoffs$id
  tracking <- merge(tracking, takeoffs[,c('takeoffTraj','upward','bboxsize',
    'p1','p2','p3','r2'),with=FALSE], by="takeoffTraj", all.x=TRUE)
  
  # Trajectories that have NA as upward had no perching prior to movement, e.g. came flying into the FOV
  tracking$upward[is.na(tracking$upward)] <- FALSE
  
  # Clean up
  gc()
  
  return(tracking)
}

loadDfTracking <- cmpfun(.loadDfTracking)

# ================================================================
# == PLOT 3D TRAJECTORIES
# ================================================================

plotTrajectories <- function( groupIdx ) {
  
  filePattern <- paste0( groups[groupIdx], '.tracking.csv' )
  
  # Load data
  t1 <- loadData( base.path, filePattern )
  #    c('frame','relframe','trajectory','timestamp','time','takeoffTraj','x','y','z'))
  #t1 <- t1[t1$body=="b'Yframe'",]
  
  # Remove NAs
  t1 <- t1[!is.na(t1$x * t1$y * t1$z),]
  
  # Remove noise
  t1 <- t1[t1$z > -100,]
  
  # Plot in 3D
  open3d()
  par3d(windowRect = c(0, 0, 1600, 800))
  mfrow3d(1,2)
  
  next3d(); plot3d(t1$x,t1$y,t1$z,col='black',size=1)
  next3d(); plot3d(t1$x,t1$y,t1$z,col='black',size=1)

  U <- par3d("userMatrix")
  U.bottom <- rotate3d(U, -pi/3, 1,0,0 )
  
  subsceneIDs = subsceneList()
  useSubscene3d(subsceneIDs[2])
  par3d(userMatrix = U.bottom)
  
  dstDir <- file.path(base.path, 
     '../output/traj/', groupIdx, '/')
  
  dir.create(dstDir, showWarnings = FALSE, recursive=TRUE)

  cmd <- paste0(
    "ffmpeg -framerate 10 -i ",
    "\"",wdir,"/output/traj/",groupIdx,"/movie%%03d.png\" ", 
    "-c:v libx264 -r 30 -pix_fmt yuv420p -crf 15 ",
    "\"",wdir,"/output/trajectory_",groupIdx,".mp4\"")
    
  movie3d( function(time){
    for (ss in subsceneIDs) {
      useSubscene3d(ss)
      U <- par3d()$userMatrix
      par3d(userMatrix = rotate3d(U, 2 * pi / 360, 0,0,1))
    }
  }, duration = 18, dir=dstDir, fps=10, convert=cmd)
  
  # clean up
  rm(t1)
  gc()
  rgl.close()
}

animateTrajectories <- function( d ) {
  
  # Remove noise
  d <- d[d$z > -100,]
  
  # Plot in 3D
  open3d()
  par3d(windowRect = c(0, 0, 1600, 800))
  mfrow3d(1,2)
  
  next3d(); plot3d(d$x,d$y,d$z,col='black',size=1)
  next3d(); plot3d(d$x,d$y,d$z,col='black',size=1)
  
  U <- par3d("userMatrix")
  U.bottom <- rotate3d(U, -pi/3, 1,0,0 )
  
  subsceneIDs = subsceneList()
  useSubscene3d(subsceneIDs[2])
  par3d(userMatrix = U.bottom)
  
  dstDir <- file.path(base.path, 
                      '../output/traj/', groupIdx, '/')
  
  dir.create(dstDir, showWarnings = FALSE, recursive=TRUE)
  
  cmd <- paste0(
    "ffmpeg -framerate 10 -i ",
    "\"",wdir,"/output/traj/",groupIdx,"/movie%%03d.png\" ", 
    "-c:v libx264 -r 30 -pix_fmt yuv420p -crf 15 ",
    "\"",wdir,"/output/trajectory_",groupIdx,".mp4\"")
  
  movie3d( function(time){
    for (ss in subsceneIDs) {
      useSubscene3d(ss)
      U <- par3d()$userMatrix
      par3d(userMatrix = rotate3d(U, 2 * pi / 360, 0,0,1))
    }
  }, duration = 18, dir=dstDir, fps=10, convert=cmd)
  
  # clean up
  rm(t1)
  gc()
  rgl.close()
}

# ================================================================
# == PLOT TAKEOFFS
# ================================================================

plotTakeoffs <- function( groupIdx ) {
  
  tracking <- loadDfTracking( groupIdx )
  fs <- loadFlysim( groupIdx )
  fsTracking <- loadFlysimTracking(groupIdx)
  colnames(fsTracking) <- c('flysimTraj','frame','flysim.x','flysim.y','flysim.z')
  #colnames(fs) <- c('trajectory', 'framestart', 'frameend', 
  #                  'dirscore', 'r2', 'dstFromYframe', 'dstFromYframeSD')
  
  # ----------------------------------------------------
  # Plot all trajectories (movie)
  # ----------------------------------------------------
  
  t <- tracking
  plot3d(t$x, t$y, t$z, col=t$trajectory,size=1)
  
  # ----------------------------------------------------
  # Plot upward trajectories (movie)
  # ----------------------------------------------------
  
  t <- tracking[tracking$upward,]
  plot3d(t$x, t$y, t$z, col=t$trajectory,size=1)
  
  open3d()
  par3d(windowRect = c(0, 0, 1600, 800))
  mfrow3d(1,2)
  
  next3d(); plot3d(t1$x,t1$y,t1$z,col='black',size=1)
  next3d(); plot3d(t1$x,t1$y,t1$z,col='black',size=1)
  
  U <- par3d("userMatrix")
  U.bottom <- rotate3d(U, -pi/3, 1,0,0 )
  
  subsceneIDs = subsceneList()
  useSubscene3d(subsceneIDs[2])
  par3d(userMatrix = U.bottom)
  
  dstDir <- file.path(base.path, 
                      '../output/traj/', groupIdx, '/')
  
  dir.create(dstDir, showWarnings = FALSE, recursive=TRUE)
  
  cmd <- paste0(
    "ffmpeg -framerate 10 -i ",
    "\"",wdir,"/output/traj/",groupIdx,"/movie%%03d.png\" ", 
    "-c:v libx264 -r 30 -pix_fmt yuv420p -crf 15 ",
    "\"",wdir,"/output/trajectory_",groupIdx,".mp4\"")
  
  movie3d( function(time){
    for (ss in subsceneIDs) {
      useSubscene3d(ss)
      U <- par3d()$userMatrix
      par3d(userMatrix = rotate3d(U, 2 * pi / 360, 0,0,1))
    }
  }, duration = 18, dir=dstDir, fps=10, convert=cmd)
  
  # clean up
  rm(t1)
  gc()
  rgl.close()
  
  # ----------------------------------------------------
  # Plot not upward (landing/horizontal) trajectories (movie)
  # ----------------------------------------------------
  
  t <- tracking[!tracking$upward,]
  plot3d(t$x, t$y, t$z, col=t$trajectory,size=1)
  
  # ----------------------------------------------------
  # Plot flysim
  # ----------------------------------------------------
  
  fsIDs <- unique(fsTracking$flysimTraj)
  length(fsIDs)
  
  fs$flysimTraj <- fs$trajectory
  fsTracking <- merge(fsTracking,fs, by="flysimTraj")
  
  fFS <- function(idx) {
    t <- fsTracking[fsTracking$flysimTraj==fsIDs[idx],]
    
    t <- ddply(t, c("frame","flysimTraj"), function(X) data.frame(X, mx=mean(X$flysim.x)))
    t <- ddply(t, c("frame","flysimTraj"), function(X) data.frame(X, my=mean(X$flysim.y)))
    t <- ddply(t, c("frame","flysimTraj"), function(X) data.frame(X, mz=mean(X$flysim.z)))
    
    #next3d();
    rgl.clear()
    #plot3d(t$flysim.x-t$mx, t$flysim.y-t$my, t$flysim.z-t$mz, col='red', size=1)
    #next3d();
    plot3d(t$flysim.x, t$flysim.y, t$flysim.z, col='red', size=1)
    
    #par(mfrow=c(2,2))
    #hist(t$flysim.x-t$mx)
    #hist(t$flysim.y-t$my)
    #hist(t$flysim.z-t$mz)
    
    #print("-------")
    #print(shapiro.test(t$flysim.x-t$mx)$p.value)
    #print(shapiro.test(t$flysim.y-t$my)$p.value)
    #print(shapiro.test(t$flysim.z-t$mz)$p.value)
    
    print(sd(t$flysim.x-t$mx))
    print(sd(t$flysim.y-t$my))
    print(sd(t$flysim.z-t$mz))
    
    print(t$std)
    
    print(paste0(mean(t$dstFromYframe),',',mean(t$r2)))
  }
  open3d();mfrow3d(1,2)
  twiddle(fFS(idx), idx = knob(c(13, length(fsIDs)),1), auto = FALSE)
  
  # ----------------------------------------------------
  # Plot flysim trajectories
  # ----------------------------------------------------
  
  t <- tracking[tracking$afterFlysim > -100 & tracking$afterFlysim < 200,] # & tracking$upward,]
  t <- t[grep('(2016-11-14 14:41:.*)',t$time),]
  t <- t[grep('(2016-11-14 19:(25|26|27):.*)',t$time),]
  t <- merge(t,fsTracking,by="frame",all.x=TRUE)
  
  t <- tracking[tracking$r2 > 0.8,]
  hist(t$p1)
  hist(t$p2)
  hist(t$p3)
  
  t <- tracking[tracking$p2 < -7 & tracking$r2 > 0.98 & tracking$upward,]
  t <- merge(t,fsTracking,by="frame",all.x=TRUE)
  
  plot3d  (t$x       , t$y       , t$z       , col='black',size=1)
  points3d(t$flysim.x, t$flysim.y, t$flysim.z, col='red'  ,size=1)
  
  
  fAll <- function(idxTakeoff, frameRangeAfter, frameRangeBefore) {
    a <- t[t$trajectory==unique(t$trajectory)[idxTakeoff],]
    a <- a[1:frameRangeAfter,]
    rgl.clear()
    
    plot3d  (a$x       , a$y       , a$z       , col='black',size=1, 
       xlim=c(-500, 500),
       ylim=c(-500, 500),
       zlim=c(-100, 800),
    )
    points3d(a$flysim.x, a$flysim.y, a$flysim.z, col='red'  ,size=1)
    
    print(unique(a$time))
  }
  open3d()
  twiddle(
    fAll(idxTakeoff, frameRangeAfter, frameRangeBefore),
    idxTakeoff = knob(c(1, length(unique(t$trajectory))),1),
    frameRangeAfter  = knob(c(1, 2000), 1),
    frameRangeBefore = knob(c(1, 1000), 1),
    auto = FALSE)
  
}

# ================================================================
# == PLOT TAKEOFFS AFTER FLYSIM
# ================================================================

plotTakeoffsFlysim <- function( groupIdx ) {
  
  flysim <- loadFlysim( groupIdx )
  
}

# ================================================================
# == PLOT TAKEOFFS AFTER FLYSIM
# ================================================================

plotTakeoffsFlysim <- function( groupIdx ) {
  
  
}

# ================================================================
# == PLOT 2D PERCHING LOCATIONS
# ================================================================

getPerchingData <- function( groupIdx ) {
  
  filePattern <- paste0( groups[groupIdx], '.perches.csv' )
  
  # Load dF perching locations
  perching.data <- loadData(
    base.path, filePattern, 
    c('file',
      'avg.x','avg.y','avg.z',
      'min.x','min.y','min.z',
      'max.x','max.y','max.z',
      'frame.start','frame.end',
      'num.frames'))
  
  if (nrow(perching.data) > 0) {
    
    # Bin the data  
    perching.data$approx.x <- as.numeric(as.character(
      cut(perching.data$avg.x, breaks= 15 * (-50:50),
          labels= 15 * ((-50:49)+.5))))
    
    perching.data$approx.y <- as.numeric(as.character(
      cut(perching.data$avg.y, breaks= 15 * (-50:50),
          labels= 15 * ((-50:49)+.5))))
    
    perching.data$approx.z <- as.numeric(as.character(
      cut(perching.data$avg.z, breaks= 5 * (-10:120),
          labels=  5 * ((-10:119)+.5))))
  }
  
  return(perching.data)
}

plotPerching2D <- function( groupIdx ) {

  filePatternPerch <- paste0( groups[groupIdx], '.perch_objects.csv' )
  
  # Load dF perching locations
  perching.data <- getPerchingData( groupIdx )
  
  if (nrow(perching.data) == 0) { return(); }
  
  # Load perch locations
  perch.object.data <- loadData(
    base.path, filePatternPerch,
    c('file','body','frame','marker.id','x','y','z')
  )

  perch.object.data <- aggregate(perch.object.data, by=
     list(body=perch.object.data$body,
          marker.id=perch.object.data$marker.id), FUN=mean, na.rm=TRUE)
  perch.object.data <- perch.object.data[!grepl('Yframe|YFrame',perch.object.data$body),]

  # Subset perching data by minimum of 5 seconds perching time
  dpl <- perching.data[perching.data$num.frames > 1000,]
  
  # Heat map of number of returns
  ggplot(dpl, aes(avg.x, avg.y)) +
    xlim(-500, 500) +
    ylim(-500, 500) +
    stat_bin2d() +
    geom_point(data=perch.object.data, aes(x=x,y=y, color=body))
  
  ggsave( paste0(base.path, '../output/numreturns_', groupIdx, '.png'),
      width = 10, height = 8)
  
  # Heat map of total length of stay
  dpl.agg <- aggregate( list(num.frames=dpl$num.frames), by=list(
    approx.x = dpl$approx.x,
    approx.y = dpl$approx.y
  ), FUN=sum, na.rm=TRUE)
  
  ggplot(dpl.agg, aes(approx.x, approx.y)) +
    xlim(-500, 500) +
    ylim(-500, 500) +
    geom_tile(aes(fill = log10(num.frames)), colour = "white") +
    scale_fill_gradient(low = "white",high = "steelblue") +
    geom_point(data=perch.object.data, aes(x=x,y=y, color=body))

  ggsave( paste0(base.path, '../output/totalperch_', groupIdx, '.png'),
          width = 10, height = 8)
  
  # clean up
  rm(perching.data)
  gc()
}

# ================================================================
# == PLOT 3D PERCHING LOCATIONS
# ================================================================

plotPerching3D <- function(groupIdx) {

  # Load dF perching locations
  perching.data <- getPerchingData( groupIdx )
  
  if (nrow(perching.data) == 0) { return(); }
  
  # Perching locations
  p1 <- perching.data[perching.data$num.frames > 1000,]
  
  # Plot in 3D
  open3d()
  par3d(windowRect = c(0, 0, 1600, 800))
  mfrow3d(1,2)
  
  next3d(); plot3d(p1$avg.x,p1$avg.y,p1$avg.z,col='black',size=3)
  next3d(); plot3d(p1$avg.x,p1$avg.y,p1$avg.z,col='black',size=3)
  
  U <- par3d("userMatrix")
  U.bottom <- rotate3d(U, -pi/3, 1,0,0 )
  
  subsceneIDs = subsceneList()
  useSubscene3d(subsceneIDs[2])
  par3d(userMatrix = U.bottom)
  
  dstDir <- file.path(base.path, '../output/perch3d/', groupIdx, '/')
  
  dir.create(dstDir, showWarnings = FALSE, recursive=TRUE)
  
  cmd <- paste0(
    "ffmpeg -framerate 10 -i ",
    "\"",wdir,"/output/perch3d/",groupIdx,"/movie%%03d.png\" ", 
    "-c:v libx264 -r 30 -pix_fmt yuv420p -crf 15 ",
    "\"",wdir,"/output/perching_",groupIdx,".mp4\"")
  
  movie3d( function(time){
    for (ss in subsceneIDs) {
      useSubscene3d(ss)
      U <- par3d()$userMatrix
      par3d(userMatrix = rotate3d(U, 2 * pi / 360, 0,0,1))
    }
  }, duration = 18, dir=dstDir, fps=10, convert=cmd)
  
  # Clean
  rgl.close()
  rm(p1)
  gc()
}

# ================================================================
# == PERCHING ORIENTATIONS
# ================================================================

rad2deg <- function(rad) {(rad * 180) / (pi)}

plotOrientations2D <- function( groupIdx ) {

  filePattern <- paste0( groups[groupIdx], '.angles.csv' )

  orientation.data <- loadData(
    base.path, filePattern,
    c('file','body','frame','perch.range.idx',
      'pos.x','pos.y','pos.z',
      'axis1.x','axis1.y','axis1.z',
      'axis2.x','axis2.y','axis2.z',
      'angle.alt','angle.azi',
      'perch.frame.start','perch.frame.end'
    )
  )

  orientation.data$num.frames <-
    orientation.data$perch.frame.end -
    orientation.data$perch.frame.start

  orientation.data$angle.alt.deg <-
    rad2deg(orientation.data$angle.alt)
  
  orientation.data[grep(setup.1,orientation.data$file),]$exp.group <- 'setup.1'

  dpo <- orientation.data[
    !is.nan(orientation.data$angle.alt) &
    !is.nan(orientation.data$angle.azi),]
  
  dpo$approx.x <- as.numeric(as.character(cut(dpo$pos.x, breaks= 25 * (-25:25), labels= 25 * ((-25:24)+.5) )))
  dpo$approx.y <- as.numeric(as.character(cut(dpo$pos.y, breaks= 25 * (-25:25), labels= 25 * ((-25:24)+.5) )))
  
  # Look only at stationary landings
  dpo <- dpo[dpo$num.frames > 200 & dpo$pos.z < 50,]
  
  dpo <- aggregate(dpo, by=list(
    perch.range.idx = dpo$perch.range.idx,
    body = dpo$body,
    approx.x = dpo$approx.x,
    approx.y = dpo$approx.y
  ), FUN=mean, na.rm=TRUE)
  
  # Azimuth plot
  ggplot(data=d, aes(x=approx.x, y=approx.y)) +
    xlim(-500, 500) + 
    ylim(-500, 500) + 
    geom_segment(aes(xend=x+dx, yend=y+dy),
      arrow = arrow(length = unit(0.3,"cm")))

  ggsave( paste0(base.path, '../output/azimuths_', groupIdx, '.png'),
          width = 10, height = 8)
  
  # clean up
  rm(orientation.data, dpo)
  gc()
}


# ================================================================
# == PLOT FORAGING AS FUNCTION OF TIME OF DAY
# ================================================================

## Plot frequency of *any* flight type as function of time of day



## Plot frequency of capture flight types as function of time of day


# ================================================================
# == OUTPUT TEXT REPORT WITH DAILY FLIGHT STATISTICS
# ================================================================

## Start of experiment

## End of experiment

## Number of takeoffs

## Number of capture flights

## 

# ================================================================
# == PROCESS A GROUP
# ================================================================

processGroup <- function(groupIdx) {

  print(paste0("Processing ", groupIdx))
  
  # Testing this function:
  plotTakeoffs(groupIdx);
  
  return();
  
  tryCatch({
    print(paste0("Processing orientations", groupIdx))
    plotOrientations( groupIdx );
  }, error=function(x){print(x);traceback();});

  gc(); rm(list = ls()); # Clean up memory
  
  tryCatch({
    print(paste0("Processing trajectories", groupIdx))
    plotTrajectories( groupIdx );
  }, error=function(x){print(x);traceback();});

  gc(); rm(list = ls()); # Clean up memory
  
  tryCatch({
    print(paste0("Processing perching 3d", groupIdx))
    plotPerching3D( groupIdx );
  }, error=function(x){print(x);traceback();});

  gc(); rm(list = ls()); # Clean up memory
  
  tryCatch({
    print(paste0("Processing perching 2d", groupIdx))
    plotPerching2D( groupIdx );
  }, error=function(x){print(x);traceback();});

  gc(); rm(list = ls()); # Clean up memory
}

# ================================================================
# == ENTRY POINT
# ================================================================

for (groupIdx in c(14)) { #1:length(groups)) {
  
  # Check if this group has already been processed
  if(!file.exists(paste0(base.path, '../output/numreturns_', groupIdx, '.png'))) {
    processGroup(groupIdx)
  }else{
    print(paste0("Skipping group ",groupIdx));
  }
}
