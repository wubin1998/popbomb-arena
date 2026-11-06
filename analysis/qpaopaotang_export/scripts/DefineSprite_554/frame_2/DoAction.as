this.onKeyUp = function()
{
   var _loc2_ = Key.getCode();
   var _loc1_ = 1;
   while(_loc1_ <= totalPlayer)
   {
      if(_loc2_ == game["p" + _loc1_].fireKey)
      {
         brickArea["hero" + _loc1_].puttAble = true;
      }
      _loc1_ = _loc1_ + 1;
   }
};
_parent.timer.timeInit();
_parent.timer.timeStart();
_parent.allPan.init();
pausing = false;
_parent.pause_btn.enabled = true;
_parent.music_btn.enabled = true;
bgMusic_sound.attachSound(_parent.bgMusic_array[_parent.bgMusicId]);
if(_parent.soundon)
{
   bgMusic_sound.start();
}
for(§each§ in bgArea)
{
   var mc = bgArea[eval("each")];
   if(mc.getDepth() > w * h)
   {
      mc.removeMovieClip();
   }
   else
   {
      mc.expEnergy = 0;
   }
}
for(§each§ in brickArea)
{
   var mc = brickArea[eval("each")];
   if(mc.unbreakAble != true)
   {
      mc.removeMovieClip();
   }
}
var styleL = 4 >= brickStyle.length ? brickStyle.length : 4;
var y = 0;
while(y < h)
{
   var x = 0;
   while(x < w)
   {
      var bg = bgArea["bg" + y + "_" + x];
      if(bg.brick != "unbreakAble")
      {
         bg.brick = "";
         bg.bomb = "";
         bg.tool = "";
         bg.expHeroNum = 0;
         bg.expEnergy = 0;
         if(random(100) < 60)
         {
            var brick = brickArea.attachMovie("brick","brick" + y + "_" + x,calDepth(x,y,brickArea.margin),{_x:x * d,_y:y * d});
            brick.gotoAndStop(brickStyle[random(styleL)]);
            bg.brick = "brick" + y + "_" + x;
         }
      }
      x++;
   }
   y++;
}
heroId = 0;
var heroNum = 1;
while(heroNum <= 4)
{
   var heroObj = _parent["heroObj" + heroNum];
   heroObj.totalBomb = 1;
   heroObj.score = 0;
   heroObj.killScore = 0;
   heroObj.timeScore = 0;
   heroObj.toolScore = 0;
   heroObj.brickScore = 0;
   heroObj.bonus = 0;
   var heroId = heroObj.heroId;
   x0 = heroXy[heroId - 1][0];
   y0 = heroXy[heroId - 1][1];
   var life = heroObj.life;
   if(life > 0)
   {
      var bg = bgArea["bg" + y0 + "_" + x0];
      brickArea[bg.brick].removeMovieClip();
      bg.brick = "";
      var hero = brickArea.attachMovie("hero" + heroId,"hero" + heroNum,calDepth(x0,y0,brickArea.margin) - heroNum,{puttAble:true,slowVLevel:0,stopTime:0,slowTime:-1,heroId:heroId,heroNum:heroNum,varShield:60 + random(4),life:life,prevFire:false,pushBombAble:false,shield:false,vLevel:6,bombLevel:1,totalBomb:1,moving:false,x:x0,y:y0,_x:x0 * d,_y:y0 * d});
      if(heroNum <= totalPlayer)
      {
         hero.p = game["p" + heroNum];
         if(_parent.varHard == "easy")
         {
            hero.shield = true;
            _parent.allPan.showMc("shield",heroNum,true);
         }
         var p_pointer = hero.attachMovie("p_pointer","p_pointer",0,{delayTime:150});
         p_pointer.p.gotoAndStop(heroId + (heroNum - 1) * 4);
         p_pointer.arrow.gotoAndStop(heroId);
         p_pointer.onEnterFrame = function()
         {
            if(pausing)
            {
               return 0;
            }
            if(--this.delayTime <= 0)
            {
               this.removeMovieClip();
            }
         };
      }
      hero.gotoAndStop("down");
      hero.body.stop();
      var i = 0;
      while(i < 4)
      {
         var x = x0 + xy[i][0];
         var y = y0 + xy[i][1];
         bgArea["bg" + y + "_" + x].brick = "";
         brickArea["brick" + y + "_" + x].removeMovieClip();
         i++;
      }
   }
   heroNum++;
}
_parent.allPan.upDate();
game.onEnterFrame = function()
{
   if(pausing)
   {
      return 0;
   }
   var _loc15_ = 0;
   var _loc16_ = 0;
   var _loc6_ = 1;
   var _loc2_;
   var _loc13_;
   var _loc11_;
   var _loc12_;
   var _loc4_;
   var _loc10_;
   var _loc7_;
   var _loc14_;
   var _loc3_;
   var _loc9_;
   var _loc8_;
   var _loc5_;
   while(_loc6_ <= 4)
   {
      _loc2_ = brickArea["hero" + _loc6_];
      if(_loc2_ != undefined)
      {
         if(_loc2_.varShield > 0)
         {
            if(--_loc2_.varShield % 2)
            {
               _loc2_._alpha = 30;
            }
            else
            {
               _loc2_._alpha = 100;
            }
         }
         else
         {
            _loc13_ = bgArea["bg" + Math.round(_loc2_._y / d) + "_" + Math.round(_loc2_._x / d)];
            if(_loc13_.expEnergy > 0)
            {
               if(_loc13_.expHeroNum != _loc2_.heroNum)
               {
                  _loc11_ = _parent["heroObj" + _loc13_.expHeroNum];
                  _loc11_.killScore += 1000;
                  _loc11_.score += 1000;
                  _loc11_.totalScore += 1000;
                  _parent.allPan.upDate();
               }
               hurtHero(_loc2_);
            }
         }
         if(_loc2_.slowTime > 0)
         {
            _loc2_.slowTime = _loc2_.slowTime - 1;
         }
         if(_loc6_ <= totalPlayer)
         {
            _loc15_ = _loc15_ + 1;
            if(_loc2_.moving)
            {
               if(Key.isDown(game["p" + _loc6_].fireKey))
               {
                  _loc2_.prevFire = true;
               }
            }
            else
            {
               if(_loc2_.slowTime == 0)
               {
                  trace("恢复");
                  _loc2_.slowTime = -1;
               }
               if(_loc2_.stopTime > 0)
               {
                  _loc2_.stopTime = _loc2_.stopTime - 1;
               }
               else
               {
                  _loc12_ = game["p" + _loc6_];
                  _loc4_ = 0;
                  while(_loc4_ < 4)
                  {
                     if(Key.isDown(_loc12_["key" + _loc4_]))
                     {
                        movHero(_loc2_,_loc4_);
                        break;
                     }
                     _loc4_ = _loc4_ + 1;
                  }
                  if(Key.isDown(_loc12_.fireKey) || _loc2_.prevFire)
                  {
                     _loc2_.prevFire = false;
                     if(_loc2_.totalSpecial > 0)
                     {
                        putSpecial(_loc2_);
                     }
                     else
                     {
                        putBomb(_loc2_);
                     }
                  }
               }
            }
         }
         else
         {
            _loc16_ = _loc16_ + 1;
            if(_loc2_.moving == false)
            {
               if(_loc2_.slowTime == 0)
               {
                  trace("恢复");
                  _loc2_.slowTime = -1;
               }
               if(_loc2_.stopTime > 0)
               {
                  _loc2_.stopTime = _loc2_.stopTime - 1;
               }
               else if(isSave(_loc2_.x,_loc2_.y))
               {
                  if(random(100) < 20)
                  {
                     _loc10_ = 0;
                     _loc7_ = -1;
                     _loc14_ = random(4);
                     _loc4_ = 0;
                     while(_loc4_ < 4)
                     {
                        _loc3_ = (_loc14_ + _loc4_) % 4;
                        _loc9_ = xy[_loc3_][0];
                        _loc8_ = xy[_loc3_][1];
                        _loc5_ = getScore(_loc2_.x + _loc9_,_loc2_.y + _loc8_);
                        if(_loc5_ > 0)
                        {
                           if(bgArea["bg" + (_loc2_.y + _loc8_) + "_" + (_loc2_.x + _loc9_)].tool != "")
                           {
                              _loc5_ += 5;
                           }
                           _loc5_ += random(3);
                           if(_loc5_ >= _loc10_)
                           {
                              _loc10_ = _loc5_;
                              _loc7_ = _loc3_;
                           }
                        }
                        _loc4_ = _loc4_ + 1;
                     }
                     if(_loc7_ >= 0)
                     {
                        movHero(_loc2_,_loc7_);
                     }
                  }
                  else if(random(100) < 5)
                  {
                     _loc10_ = 0;
                     _loc7_ = -1;
                     _loc14_ = random(4);
                     _loc4_ = 0;
                     while(_loc4_ < 4)
                     {
                        _loc3_ = (_loc14_ + _loc4_) % 4;
                        _loc9_ = xy[_loc3_][0];
                        _loc8_ = xy[_loc3_][1];
                        _loc5_ = getScore(_loc2_.x + _loc9_,_loc2_.y + _loc8_);
                        if(_loc5_ > 0)
                        {
                           if(_loc5_ >= _loc10_)
                           {
                              _loc10_ = _loc5_;
                              _loc7_ = _loc3_;
                           }
                        }
                        _loc4_ = _loc4_ + 1;
                     }
                     if(_loc7_ >= 0 && _loc10_ >= 2)
                     {
                        if(_loc2_.totalSpecial > 0)
                        {
                           putSpecial(_loc2_);
                        }
                        else
                        {
                           putBomb(_loc2_);
                        }
                        movHero(_loc2_,_loc7_);
                     }
                  }
               }
               else if(random(100) < escapeRate)
               {
                  _loc10_ = 0;
                  _loc7_ = -1;
                  _loc14_ = random(4);
                  _loc4_ = 0;
                  while(_loc4_ < 4)
                  {
                     _loc3_ = (_loc14_ + _loc4_) % 4;
                     _loc5_ = getScore(_loc2_.x + xy[_loc3_][0],_loc2_.y + xy[_loc3_][1]);
                     if(_loc5_ >= _loc10_)
                     {
                        _loc10_ = _loc5_;
                        _loc7_ = _loc3_;
                     }
                     _loc4_ = _loc4_ + 1;
                  }
                  if(_loc7_ < 0)
                  {
                     _loc7_ = random(4);
                  }
                  movHero(_loc2_,_loc7_);
               }
            }
         }
      }
      _loc6_ = _loc6_ + 1;
   }
   if(_loc15_ <= 0)
   {
      gameover("lose");
   }
   else if(_loc16_ <= 0)
   {
      gameover("win");
   }
};
