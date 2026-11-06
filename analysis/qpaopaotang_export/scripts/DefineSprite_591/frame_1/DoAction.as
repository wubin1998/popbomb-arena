function f()
{
   hitArea.enabled = false;
   if(_parent.stageClear)
   {
      continue_mc.gotoAndStop(2);
   }
   else
   {
      continue_mc.gotoAndStop(1);
   }
   var _loc3_ = 1;
   var _loc4_;
   while(_loc3_ <= _parent.totalPlayer)
   {
      _loc4_ = _parent["heroObj" + _loc3_];
      this["killScore" + _loc3_] = _loc4_.killScore;
      this["destruction" + _loc3_] = _loc4_.brickScore + _loc4_.toolScore;
      this["timeScore" + _loc3_] = _loc4_.timeScore;
      this["bonus" + _loc3_] = _loc4_.bonus;
      this["score" + _loc3_] = _loc4_.score;
      this["totalScore" + _loc3_] = _loc4_.totalScore;
      _loc3_ = _loc3_ + 1;
   }
   Key.addListener(this);
   this.onKeyUp = function()
   {
      if(_parent.stageClear)
      {
         _parent.game.gotoAndPlay(1);
      }
      else
      {
         _parent.game.gotoAndPlay(2);
      }
      gotoAndStop(1);
   };
}
stop();
Key.removeListener(this);
