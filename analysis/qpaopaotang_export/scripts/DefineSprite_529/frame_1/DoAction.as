function init()
{
   var _loc4_ = 1;
   var _loc5_;
   var _loc3_;
   while(_loc4_ <= 4)
   {
      _loc5_ = _parent["heroObj" + _loc4_];
      _loc3_ = this["pan" + _loc4_];
      _loc3_.gotoAndStop(_loc5_.heroId);
      _loc3_.lifeMc.txt = _loc5_.life;
      _loc3_.shield.tool_mc.gotoAndStop(4);
      _loc3_.shield._visible = false;
      _loc3_.shoe.tool_mc.gotoAndStop(6);
      _loc3_.shoe._visible = false;
      _loc3_.glove.tool_mc.gotoAndStop(2);
      _loc3_.glove._visible = false;
      if(_loc5_.life > 0)
      {
         _loc3_.pan_mask.gotoAndStop(1);
      }
      else
      {
         _loc3_.pan_mask.gotoAndStop(2);
      }
      _loc4_ = _loc4_ + 1;
   }
}
function upDate()
{
   var _loc5_ = 1;
   var _loc3_;
   var _loc4_;
   while(_loc5_ <= 4)
   {
      _loc3_ = _parent["heroObj" + _loc5_];
      _loc4_ = this["pan" + _loc5_];
      _loc4_.lifeMc.txt = _loc3_.life;
      if(_loc3_.life == 0)
      {
         _loc4_.pan_mask.gotoAndStop(2);
      }
      _loc4_.score = _loc3_.score;
      _loc4_.totalBombMc.txt = _loc3_.totalBomb;
      _loc5_ = _loc5_ + 1;
   }
}
function showMc(mcName, heroNum, value)
{
   this["pan" + heroNum][mcName]._visible = value;
}
function mcBlink(mcName, heroNum)
{
   var _loc2_ = this["pan" + heroNum][mcName];
   _loc2_.delayTime = 35;
   _loc2_.onEnterFrame = function()
   {
      if(--this.delayTime <= 0)
      {
         delete this.onEnterFrame;
      }
      else if(this.delayTime % 10 < 5)
      {
         this._visible = true;
      }
      else
      {
         this._visible = false;
      }
   };
}
