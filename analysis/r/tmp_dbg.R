
hm.sep <- aggregate(hm, by=list(hm$frame), FUN=function(X){
  return(X[1,])
})
