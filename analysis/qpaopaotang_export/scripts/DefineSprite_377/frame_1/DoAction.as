stop();
this.onEnterFrame = function()
{
   var _loc3_ = int(_parent.getBytesLoaded() / _parent.getBytesTotal() * 100);
   this.gotoAndStop(int(_loc3_ / 10));
   if(_loc3_ >= 100)
   {
      delete this.onEnterFrame;
   }
};
