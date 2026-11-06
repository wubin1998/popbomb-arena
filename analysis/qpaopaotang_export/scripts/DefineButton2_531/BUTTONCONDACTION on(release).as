on(release){
   if(++qualityNum > 3)
   {
      qualityNum = 1;
   }
   quality_mc.gotoAndStop(qualityNum);
}
