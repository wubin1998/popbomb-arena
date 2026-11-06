function timeInit()
{
   c1 = c2 = c3 = c4 = "0";
   t = 180;
   upDate();
   t0 = getTimer() / 1000;
   pausing = false;
}
function timePause()
{
   if(pausing)
   {
      pausing = false;
   }
   else
   {
      pausing = true;
   }
}
function timeStart()
{
   timerInterval = setInterval(function()
   {
      var _loc2_ = getTimer() / 1000;
      if(pausing == false)
      {
         t -= _loc2_ - t0;
         if(t < 1)
         {
            t = 0;
            timeStop();
            _parent.game.gameover("timeup");
         }
         upDate();
      }
      t0 = _loc2_;
   }
   ,500);
}
function upDate()
{
   var _loc1_ = String(int(t) % 60);
   var _loc2_ = String(int((t - _loc1_) / 60));
   if(_loc1_.length < 2)
   {
      c3 = "0";
      c4 = _loc1_;
   }
   else
   {
      c3 = _loc1_.charAt(0);
      c4 = _loc1_.charAt(1);
   }
   if(_loc2_.length < 2)
   {
      c1 = "0";
      c2 = _loc2_;
   }
   else
   {
      c1 = _loc2_.charAt(0);
      c2 = _loc2_.charAt(1);
   }
}
function timeStop()
{
   clearInterval(timerInterval);
}
