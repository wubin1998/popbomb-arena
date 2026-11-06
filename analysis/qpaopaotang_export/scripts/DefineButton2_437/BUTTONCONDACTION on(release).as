on(release){
   if(++_parent.qualityNum > 3)
   {
      _parent.qualityNum = 1;
   }
   gotoAndStop(_parent.qualityNum);
}
