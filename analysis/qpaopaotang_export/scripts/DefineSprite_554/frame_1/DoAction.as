function gameover(msg)
{
   delete this.onKeyUp;
   Key.removeListener(this);
   bgMusic_sound.stop();
   youWin.varWin = "";
   _parent.timer.timeStop();
   delete game.onEnterFrame;
   pausing = true;
   _parent.pause_btn.enabled = false;
   _parent.music_btn.enabled = false;
   var _loc12_ = false;
   trace(msg);
   var _loc8_;
   var _loc7_;
   var _loc5_;
   var _loc6_;
   var _loc4_;
   var _loc3_;
   switch(msg)
   {
      case "timeup":
         youWin.varWin = "timeup";
         _loc8_ = 0;
         _loc7_ = 0;
         _loc5_ = 0;
         while(_loc5_ <= 4)
         {
            _loc6_ = brickArea["hero" + _loc5_];
            if(_loc6_ != undefined)
            {
               _loc4_ = _parent["heroObj" + _loc5_].score;
               if(_loc5_ <= totalPlayer)
               {
                  if(_loc8_ < _loc4_)
                  {
                     _loc8_ = _loc4_;
                  }
               }
               else if(_loc7_ < _loc4_)
               {
                  _loc7_ = _loc4_;
               }
            }
            _loc5_ = _loc5_ + 1;
         }
         if(_loc8_ > _loc7_)
         {
            youWin.varWin += "win";
            _loc5_ = 1;
            while(_loc5_ <= totalPlayer)
            {
               _loc6_ = brickArea["hero" + _loc5_];
               if(_loc6_ != undefined)
               {
                  _loc3_ = _parent["heroObj" + _loc6_.heroNum];
                  _loc3_.bonus += 1000;
                  _loc3_.score += 1000;
                  _loc3_.totalScore += 1000;
               }
               _loc5_ = _loc5_ + 1;
            }
            _parent.allPan.upDate();
            if(_parent.stageNum == _parent.totalStage)
            {
               pauseInterval = setInterval(function()
               {
                  _parent.stageNum = _parent.stageNum + 1;
                  clearInterval(pauseInterval);
                  bgMusic_sound.stop();
                  _parent.gotoAndStop("score");
               }
               ,5500);
               this.onKeyUp = function()
               {
                  youWin.stop();
                  _parent.stageNum = _parent.stageNum + 1;
                  clearInterval(pauseInterval);
                  bgMusic_sound.stop();
                  _parent.gotoAndStop("score");
               };
            }
            else
            {
               _parent.stageClear = true;
               pauseInterval = setInterval(function()
               {
                  clearInterval(pauseInterval);
                  bgMusic_sound.stop();
                  _parent.stageScore.gotoAndStop(totalPlayer + 1);
               }
               ,5500);
               this.onKeyUp = function()
               {
                  youWin.stop();
                  clearInterval(pauseInterval);
                  bgMusic_sound.stop();
                  _parent.stageScore.gotoAndStop(totalPlayer + 1);
               };
            }
            bgMusic_sound.attachSound("Bombit_-_level_complete");
            break;
         }
         _loc5_ = 1;
         while(_loc5_ <= totalPlayer)
         {
            _loc6_ = brickArea["hero" + _loc5_];
            if(_loc6_ != undefined)
            {
               _parent["heroObj" + _loc5_].life--;
               brickArea.attachMovie("heroExp","heroExp" + _loc6_.heroNum,_loc6_.getDepth(),{heroId:_loc6_.heroId,_x:_loc6_._x,_y:_loc6_._y});
            }
            _loc5_ = _loc5_ + 1;
         }
         _parent.allPan.upDate();
      case "lose":
         youWin.varWin += "lose";
         _loc5_ = 1;
         while(_loc5_ <= totalPlayer)
         {
            if(_parent["heroObj" + _loc5_].life > 0)
            {
               break;
            }
            _loc5_ = _loc5_ + 1;
         }
         if(_loc5_ <= totalPlayer)
         {
            _parent.stageClear = false;
            pauseInterval = setInterval(function()
            {
               clearInterval(pauseInterval);
               bgMusic_sound.stop();
               _parent.stageScore.gotoAndStop(totalPlayer + 1);
            }
            ,5500);
            this.onKeyUp = function()
            {
               youWin.stop();
               clearInterval(pauseInterval);
               bgMusic_sound.stop();
               _parent.stageScore.gotoAndStop(totalPlayer + 1);
            };
         }
         else
         {
            _loc12_ = true;
            pauseInterval = setInterval(function()
            {
               clearInterval(pauseInterval);
               bgMusic_sound.stop();
               _parent.gotoAndStop("score");
            }
            ,5500);
            this.onKeyUp = function()
            {
               youWin.stop();
               clearInterval(pauseInterval);
               bgMusic_sound.stop();
               _parent.gotoAndStop("score");
            };
         }
         bgMusic_sound.attachSound("Bombit_-_mission_failed");
         break;
      case "win":
         _loc5_ = 1;
         while(_loc5_ <= totalPlayer)
         {
            _loc6_ = brickArea["hero" + _loc5_];
            if(_loc6_ != undefined)
            {
               _loc3_ = _parent["heroObj" + _loc6_.heroNum];
               _loc3_.timeScore += int(_parent.timer.t * 10);
               _loc3_.bonus += 1000;
               _loc3_.score += _loc3_.bonus + _loc3_.timeScore;
               _loc3_.totalScore += _loc3_.bonus + _loc3_.timeScore;
            }
            _loc5_ = _loc5_ + 1;
         }
         _parent.allPan.upDate();
         youWin.varWin = "win";
         if(_parent.stageNum == _parent.totalStage)
         {
            pauseInterval = setInterval(function()
            {
               _parent.stageNum = _parent.stageNum + 1;
               clearInterval(pauseInterval);
               bgMusic_sound.stop();
               _parent.gotoAndStop("score");
            }
            ,5500);
            this.onKeyUp = function()
            {
               youWin.stop();
               clearInterval(pauseInterval);
               bgMusic_sound.stop();
               _parent.gotoAndStop("score");
            };
         }
         else
         {
            _parent.stageClear = true;
            pauseInterval = setInterval(function()
            {
               clearInterval(pauseInterval);
               bgMusic_sound.stop();
               _parent.stageScore.gotoAndStop(totalPlayer + 1);
            }
            ,5500);
            this.onKeyUp = function()
            {
               youWin.stop();
               clearInterval(pauseInterval);
               bgMusic_sound.stop();
               _parent.stageScore.gotoAndStop(totalPlayer + 1);
            };
         }
         bgMusic_sound.attachSound("Bombit_-_level_complete");
   }
   youWin.play();
   if(_parent.soundon)
   {
      bgMusic_sound.start();
   }
   var _loc0_;
   if(!_loc12_)
   {
      _loc5_ = 1;
      while(_loc5_ <= 4)
      {
         _loc3_ = _parent["heroObj" + _loc5_];
         if(_loc3_.life == 0)
         {
            _loc3_.life = 1;
            _loc3_.totalScore -= 1000;
            if(_loc0_ < 0)
            {
               _loc3_.totalScore = 0;
            }
         }
         _loc5_ = _loc5_ + 1;
      }
   }
}
function putBomb(hero)
{
   if(hero.totalBomb <= 0 || hero.puttAble == false)
   {
      return false;
   }
   var _loc4_ = hero.y + "_" + hero.x;
   if(bgArea["bg" + _loc4_].bomb != "" || brickArea["grenade" + _loc4_] != undefined || brickArea["rocket" + _loc4_] != undefined || brickArea["mine" + _loc4_] != undefined || brickArea["gasBomb" + _loc4_] != undefined)
   {
      return false;
   }
   if(_parent.soundon)
   {
      vo_sound.attachSound("putBomb.wav");
      vo_sound.start();
   }
   _parent["heroObj" + hero.heroNum].totalBomb = --hero.totalBomb;
   _parent.allPan.upDate();
   var bg = bgArea["bg" + hero.y + "_" + hero.x];
   bg.bomb = "bomb" + hero.y + "_" + hero.x;
   var _loc5_ = brickArea.attachMovie("bomb",bg.bomb,calDepth(hero.x,hero.y,brickArea.margin) - 5,{heroNum:hero.heroNum,heroId:hero.heroId,bg:bg,bombLevel:hero.bombLevel,delayTime:60,x:hero.x,y:hero.y,_x:hero.x * d,_y:hero.y * d});
   _loc5_.onEnterFrame = function()
   {
      if(pausing)
      {
         return 0;
      }
      if(--this.delayTime <= 0 || this.bg.expEnergy > 0)
      {
         bombExp(this.bg);
      }
   };
   return true;
}
function addBrickExp(x, y, heroNum)
{
   var bg = bgArea["bg" + y + "_" + x];
   var _loc4_ = brickArea.attachMovie("brickExp","brickExp" + y + "_" + x,calDepth(x,y,brickArea.margin),{delayTime:10,bg:bg,_x:x * d,_y:y * d});
   _loc4_.onEnterFrame = function()
   {
      if(--this.delayTime <= 0)
      {
         this.bg.expEnergy--;
         this.removeMovieClip();
      }
   };
   bg.brick = "";
   bg.expEnergy++;
   bg.expHeroNum = heroNum;
}
function putSpecial(hero)
{
   if(hero.totalSpecial <= 0 || hero.puttAble == false || hero.moving)
   {
      return false;
   }
   var x = hero.x;
   var y = hero.y;
   var bg = bgArea["bg" + y + "_" + x];
   if(bg.bomb != "")
   {
      return false;
   }
   var id = hero._currentframe - 1;
   var dx = xy[id][0];
   var dy = xy[id][1];
   var _loc12_;
   var _loc16_;
   var _loc9_;
   var _loc8_;
   var _loc10_;
   var _loc14_;
   var _loc11_;
   var _loc6_;
   var _loc5_;
   switch(hero.specialToolName)
   {
      case "火箭":
         if(brickArea["rocket" + y + "_" + x] != undefined)
         {
            return 0;
         }
         if(_parent.soundon)
         {
            vo_sound.attachSound("rocket.mp3");
            vo_sound.start();
         }
         _loc12_ = brickArea.attachMovie("rocket","rocket" + y + "_" + x,calDepth(x,y,brickArea.margin) - 5,{hero:hero,heroNum:hero.heroNum,_x:x * d,_y:y * d,dx:dx,dy:dy,vx:0,vy:0});
         _loc12_.gotoAndStop(id + 1);
         _loc12_.onEnterFrame = function()
         {
            if(pausing)
            {
               return 0;
            }
            this.vx += this.dx;
            this.vy += this.dy;
            this._x += this.vx;
            this._y += this.vy;
            var _loc6_ = Math.round(this._x / d);
            var _loc5_ = Math.round(this._y / d);
            var _loc8_ = bgArea.getNextHighestDepth();
            var _loc9_ = bgArea.attachMovie("rocketSmoke","rocketSmoke" + _loc8_,_loc8_,{_x:this._x,_y:this._y});
            _loc9_.gotoAndStop(this._currentframe);
            var _loc3_ = 1;
            var _loc4_;
            while(_loc3_ <= 4)
            {
               if(_loc3_ != this.heroNum)
               {
                  _loc4_ = brickArea["hero" + _loc3_];
                  if(_loc4_ != undefined)
                  {
                     var dx = _loc4_._x - this._x;
                     var dy = _loc4_._y - this._y;
                     if(dx * dx + dy * dy < d * d / 4)
                     {
                        addBrickExp(_loc6_,_loc5_,this.heroNum);
                        if(_parent.soundon)
                        {
                           vo_sound.attachSound("exp.wav");
                           vo_sound.start();
                        }
                        this.removeMovieClip();
                        return 0;
                     }
                  }
               }
               _loc3_ = _loc3_ + 1;
            }
            var _loc7_ = bgArea["bg" + _loc5_ + "_" + _loc6_];
            if(_loc7_.brick == undefined || _loc7_.brick == "unbreakAble")
            {
               addBrickExp(_loc6_ - this.dx,_loc5_ - this.dy,this.heroNum);
               if(_parent.soundon)
               {
                  vo_sound.attachSound("exp.wav");
                  vo_sound.start();
               }
               this.removeMovieClip();
               return 0;
            }
            if(_loc7_.bomb != "" || _loc7_.brick != "")
            {
               addBrickExp(_loc6_,_loc5_,this.heroNum);
               this.removeMovieClip();
               return 0;
            }
            this.swapDepths(calDepth(_loc6_,_loc5_,brickArea.margin) - 5);
         };
         break;
      case "地雷":
         if(brickArea["mine" + y + "_" + x] != undefined)
         {
            return 0;
         }
         if(_parent.soundon)
         {
            vo_sound.attachSound("bibi.wav");
            vo_sound.start();
         }
         _loc16_ = brickArea.attachMovie("mine","mine" + y + "_" + x,calDepth(x,y,brickArea.margin) - 5,{heroNum:hero.heroNum,bg:bg,x:x,y:y,_x:x * d,_y:y * d});
         _loc16_.onEnterFrame = function()
         {
            if(pausing)
            {
               return 0;
            }
            var _loc3_ = 1;
            var _loc4_;
            var _loc6_;
            var _loc5_;
            while(_loc3_ <= 4)
            {
               _loc4_ = brickArea["hero" + _loc3_];
               if(_loc4_ != undefined)
               {
                  _loc6_ = _loc4_._x - this._x;
                  _loc5_ = _loc4_._y - this._y;
                  if(_loc3_ != this.heroNum)
                  {
                     if(_loc6_ * _loc6_ + _loc5_ * _loc5_ < d * d / 4)
                     {
                        addBrickExp(this.x,this.y,this.heroNum);
                        if(_parent.soundon)
                        {
                           vo_sound.attachSound("exp.wav");
                           vo_sound.start();
                        }
                        this.removeMovieClip();
                        return 0;
                     }
                  }
               }
               _loc3_ = _loc3_ + 1;
            }
            if(this.bg.expEnergy > 0)
            {
               addBrickExp(this.x,this.y,this.heroNum);
               if(_parent.soundon)
               {
                  vo_sound.attachSound("exp.wav");
                  vo_sound.start();
               }
               this.removeMovieClip();
               return 0;
            }
         };
         break;
      case "手榴弹":
         if(brickArea["grenade" + y + "_" + x] != undefined)
         {
            return 0;
         }
         _loc9_ = x + dx * 2;
         _loc8_ = y + dy * 2;
         var bg = bgArea["bg" + _loc8_ + "_" + _loc9_];
         if(bg.brick == undefined || bg.brick == "unbreakAble")
         {
            return false;
         }
         if(_parent.soundon)
         {
            vo_sound.attachSound("grenade.mp3");
            vo_sound.start();
         }
         _loc10_ = brickArea.attachMovie("grenade","grenade" + y + "_" + x,calDepth(x,y,brickArea.margin) - 5,{hero:hero,heroNum:hero.heroNum,bg:bg,x:_loc9_,y:_loc8_,_x:x * d,_y:y * d});
         _loc10_.gotoAndStop(id + 1);
         _loc10_.onEnterFrame = function()
         {
            if(pausing)
            {
               return 0;
            }
            if(this.body._currentframe == int(this.body._totalframes / 2))
            {
               this.swapDepths(calDepth(this.x,this.y,brickArea.margin) - 5);
            }
            var _loc3_;
            var _loc4_;
            if(this.body._currentframe == this.body._totalframes)
            {
               addBrickExp(this.x,this.y,this.heroNum);
               _loc3_ = 0;
               while(_loc3_ < 4)
               {
                  var x = this.x + xy[_loc3_][0];
                  var y = this.y + xy[_loc3_][1];
                  _loc4_ = bgArea["bg" + y + "_" + x];
                  if(_loc4_.brick != undefined && _loc4_.brick != "unbreakAble")
                  {
                     addBrickExp(x,y,this.heroNum);
                  }
                  _loc3_ = _loc3_ + 1;
               }
               if(_parent.soundon)
               {
                  vo_sound.attachSound("exp.wav");
                  vo_sound.start();
               }
               this.removeMovieClip();
               return 0;
            }
         };
         break;
      case "汽弹":
         if(brickArea["gasBomb" + y + "_" + x] != undefined)
         {
            return 0;
         }
         if(!_parent.soundon)
         {
         }
         _loc14_ = brickArea.attachMovie("gasBomb","gasBomb" + y + "_" + x,calDepth(x,y,brickArea.margin) - 5,{heroNum:hero.heroNum,bg:bg,x:x,y:y,_x:x * d,_y:y * d});
         _loc14_.onEnterFrame = function()
         {
            if(pausing)
            {
               return 0;
            }
            var _loc3_ = 1;
            var _loc2_;
            var _loc5_;
            var _loc4_;
            while(_loc3_ <= 4)
            {
               _loc2_ = brickArea["hero" + _loc3_];
               if(_loc2_ != undefined)
               {
                  _loc5_ = _loc2_._x - this._x;
                  _loc4_ = _loc2_._y - this._y;
                  if(_loc3_ != this.heroNum)
                  {
                     if(_loc5_ * _loc5_ + _loc4_ * _loc4_ == 0)
                     {
                        _loc2_.slowVLevel = 20;
                        _loc2_.slowTime = 600;
                        delete this.onEnterFrame;
                        this.removeMovieClip();
                        return 0;
                     }
                  }
               }
               _loc3_ = _loc3_ + 1;
            }
            if(this.bg.expEnergy > 0)
            {
               this.removeMovieClip();
               return 0;
            }
         };
         break;
      case "冰箭":
         var bg = bgArea["bg" + (y + dy) + "_" + (x + dx)];
         if(bg.brick != "" || bg.bomb != "")
         {
            return false;
         }
         _loc11_ = bgArea.getNextHighestDepth();
         _loc6_ = bgArea.createEmptyMovieClip("iceArea" + _loc11_,_loc11_);
         _loc6_.x = x;
         _loc6_.y = y;
         _loc6_.dx = dx;
         _loc6_.dy = dy;
         _loc6_.id = 0;
         _loc6_.offset = 0;
         _loc6_.expXy = new Array();
         _loc6_.onEnterFrame = function()
         {
            if(pausing)
            {
               return 0;
            }
            var _loc6_;
            var _loc5_;
            var _loc8_;
            var _loc9_;
            var _loc7_;
            var _loc2_;
            if(++this.id <= 12)
            {
               if(this.id % 4 == 0)
               {
                  this.offset = this.offset + 1;
                  _loc6_ = this.x + this.dx * this.offset;
                  _loc5_ = this.y + this.dy * this.offset;
                  _loc8_ = bgArea["bg" + _loc5_ + "_" + _loc6_];
                  if(_loc8_.brick == "" && _loc8_.bomb == "")
                  {
                     _loc8_.brick = "brick" + _loc5_ + "_" + _loc6_;
                     this.expXy.push([_loc6_,_loc5_]);
                     _loc9_ = brickArea.attachMovie("freezeEffect",_loc8_.brick,calDepth(_loc6_,_loc5_,brickArea.margin),{_x:_loc6_ * d,_y:_loc5_ * d});
                     _loc9_.onEnterFrame = function()
                     {
                        var _loc3_ = 1;
                        var _loc2_;
                        var _loc5_;
                        var _loc4_;
                        while(_loc3_ <= 4)
                        {
                           _loc2_ = brickArea["hero" + _loc3_];
                           _loc5_ = _loc2_._x - this._x;
                           _loc4_ = _loc2_._y - this._y;
                           if(_loc5_ * _loc5_ + _loc4_ * _loc4_ < 100)
                           {
                              _loc2_.stopTime = 300;
                           }
                           _loc3_ = _loc3_ + 1;
                        }
                     };
                  }
                  else
                  {
                     this.id = 12;
                  }
               }
            }
            else if(this.id > 48)
            {
               _loc7_ = this.expXy.length;
               _loc2_ = 0;
               while(_loc2_ < _loc7_)
               {
                  _loc6_ = this.expXy[_loc2_][0];
                  _loc5_ = this.expXy[_loc2_][1];
                  bgArea["bg" + _loc5_ + "_" + _loc6_].brick = "";
                  _loc2_ = _loc2_ + 1;
               }
               this.removeMovieClip();
            }
         };
         break;
      case "火焰箭":
         var bg = bgArea["bg" + (y + dy) + "_" + (x + dx)];
         if(bg.brick == undefined || bg.brick == "unbreakAble")
         {
            return false;
         }
         _loc11_ = bgArea.getNextHighestDepth();
         _loc5_ = bgArea.createEmptyMovieClip("fireArea" + _loc11_,_loc11_);
         _loc5_.x = x;
         _loc5_.y = y;
         _loc5_.dx = dx;
         _loc5_.dy = dy;
         _loc5_.id = 0;
         _loc5_.offset = 0;
         _loc5_.heroNum = hero.heroNum;
         _loc5_.expXy = new Array();
         _loc5_.onEnterFrame = function()
         {
            if(pausing)
            {
               return 0;
            }
            var _loc5_;
            var _loc4_;
            var _loc6_;
            var _loc7_;
            var _loc3_;
            if(++this.id <= 12)
            {
               if(this.id % 4 == 0)
               {
                  this.offset = this.offset + 1;
                  _loc5_ = this.x + this.dx * this.offset;
                  _loc4_ = this.y + this.dy * this.offset;
                  _loc6_ = bgArea["bg" + _loc4_ + "_" + _loc5_];
                  if(_loc6_.brick != undefined && _loc6_.brick != "unbreakAble")
                  {
                     if(_loc6_.bomb != "")
                     {
                        bombExp(_loc6_);
                     }
                     if(_loc6_.brick != "")
                     {
                        brickBreak(_loc6_,this.heroNum);
                        if(_parent.soundon)
                        {
                           vo_sound.attachSound("exp.wav");
                           vo_sound.start();
                        }
                     }
                     this.expXy.push([_loc5_,_loc4_]);
                     _loc6_.expEnergy = _loc6_.expEnergy + 1;
                     _loc6_.expHeroNum = this.heroNum;
                     brickArea.attachMovie("fireEffect","fireEffect" + _loc4_ + "_" + _loc5_,calDepth(_loc5_,_loc4_,brickArea.margin) - 7,{_x:_loc5_ * d,_y:_loc4_ * d});
                  }
                  else
                  {
                     this.id = 12;
                  }
               }
            }
            else if(this.id > 48)
            {
               _loc7_ = this.expXy.length;
               _loc3_ = 0;
               while(_loc3_ < _loc7_)
               {
                  _loc5_ = this.expXy[_loc3_][0];
                  _loc4_ = this.expXy[_loc3_][1];
                  bgArea["bg" + _loc4_ + "_" + _loc5_].expEnergy--;
                  _loc3_ = _loc3_ + 1;
               }
               this.removeMovieClip();
            }
         };
   }
   var _loc0_;
   hero.totalSpecial = _loc0_ = hero.totalSpecial - 1;
   if(_loc0_ <= 0)
   {
      hero.specialToolViewer.removeMovieClip();
   }
   if(hero.heroNum <= totalPlayer)
   {
      hero.puttAble = false;
   }
   return true;
}
function hurtHero(hero)
{
   var _loc3_;
   if(hero.varShield <= 0)
   {
      if(hero.shield)
      {
         hero.shield = false;
         _parent.allPan.showMc("shield",hero.heroNum,false);
         hero.varShield = 60;
      }
      else
      {
         _loc3_ = brickArea.attachMovie("heroDieSmoke","heroDieSmoke" + hero.heroNum,10000 + random(10000),{_x:hero._x,_y:hero._y});
         _parent["heroObj" + hero.heroNum].life--;
         _parent.allPan.upDate();
         if(--hero.life <= 0 || hero.heroNum <= totalPlayer)
         {
            brickArea.attachMovie("heroExp","heroExp" + hero.heroNum,hero.getDepth(),{heroId:hero.heroId,_x:hero._x,_y:hero._y});
         }
         else if(hero.heroNum > totalPlayer)
         {
            _loc3_.gotoAndPlay(20);
            hero.varShield = 60;
         }
      }
      return true;
   }
   return false;
}
function bombExp(bg)
{
   var _loc12_ = brickArea[bg.bomb];
   if(_loc12_ == undefined || _loc12_.x != _loc12_._x / d || _loc12_.y != _loc12_._y / d)
   {
      return false;
   }
   if(_parent.soundon)
   {
      vo_sound.attachSound("exp.wav");
      vo_sound.start();
   }
   var _loc16_ = brickArea["hero" + _loc12_.heroNum];
   _parent["heroObj" + _loc16_.heroNum].totalBomb = ++_loc16_.totalBomb;
   _parent.allPan.upDate();
   var depth = bgArea.getNextHighestDepth();
   var _loc3_ = bgArea.createEmptyMovieClip("expArea" + _loc12_.y + "_" + _loc12_.x,depth);
   _loc3_._x = _loc3_._y = d / 2;
   _loc3_.x0 = _loc12_.x;
   _loc3_.y0 = _loc12_.y;
   _loc3_.heroNum = _loc12_.heroNum;
   _loc3_.heroId = _loc12_.heroId;
   _loc3_.bombLevel = _loc12_.bombLevel;
   _loc3_.expPos = 0;
   _loc3_.depth = 0;
   _loc3_.expXy = new Array();
   _loc12_.removeMovieClip();
   bg.bomb = "";
   var _loc5_ = _loc3_.attachMovie("exp","exp" + _loc3_.y0 + "_" + _loc3_.x0,_loc3_.depth,{_x:_loc3_.x0 * d,_y:_loc3_.y0 * d});
   bg.expEnergy = bg.expEnergy + 1;
   bg.expHeroNum = _loc3_.heroNum;
   _loc3_.expXy.push([_loc3_.x0,_loc3_.y0]);
   _loc5_.gotoAndStop(_loc3_.heroId);
   _loc3_.ltrb = [true,true,true,true];
   var _loc4_ = 0;
   var _loc11_;
   var _loc10_;
   while(_loc4_ < 4)
   {
      _loc11_ = _loc3_.x0 + xy[_loc4_][0];
      _loc10_ = _loc3_.y0 + xy[_loc4_][1];
      bg = bgArea["bg" + _loc10_ + "_" + _loc11_];
      if(bg == undefined || bg.brick == "unbreakAble")
      {
         _loc3_.ltrb[_loc4_] = false;
      }
      _loc4_ = _loc4_ + 1;
   }
   _loc3_.onEnterFrame = function()
   {
      if(pausing)
      {
         return 0;
      }
      this.expPos = this.expPos + 1;
      var _loc2_ = 0;
      var _loc5_;
      var _loc4_;
      var _loc9_;
      var _loc8_;
      var _loc3_;
      var _loc7_;
      var _loc6_;
      while(_loc2_ < 4)
      {
         if(this.ltrb[_loc2_])
         {
            _loc5_ = xy[_loc2_][0];
            _loc4_ = xy[_loc2_][1];
            _loc9_ = this.x0 + _loc5_ * this.expPos;
            _loc8_ = this.y0 + _loc4_ * this.expPos;
            this.depth = this.depth + 1;
            _loc3_ = this.attachMovie("exp","exp" + _loc8_ + "_" + _loc9_,this.depth,{_x:_loc9_ * d,_y:_loc8_ * d});
            this.expXy.push([_loc9_,_loc8_]);
            _loc3_._rotation = (_loc2_ + 2) * 90;
            _loc7_ = bgArea["bg" + _loc8_ + "_" + _loc9_];
            _loc7_.expEnergy = _loc7_.expEnergy + 1;
            _loc7_.expHeroNum = this.heroNum;
            if(_loc7_.brick != "")
            {
               this.ltrb[_loc2_] = false;
               brickBreak(_loc7_,this.heroNum);
            }
            _loc6_ = bgArea["bg" + (_loc8_ + _loc4_) + "_" + (_loc9_ + _loc5_)];
            if(_loc6_ == undefined || _loc6_.brick == "unbreakAble" || this.expPos >= this.bombLevel)
            {
               this.ltrb[_loc2_] = false;
            }
            if(this.ltrb[_loc2_])
            {
               _loc3_.gotoAndStop(this.heroId + 4);
            }
            else
            {
               _loc3_.gotoAndStop(this.heroId + 8);
            }
         }
         _loc2_ = _loc2_ + 1;
      }
      if(this.expPos >= this.bombLevel)
      {
         delete this.onEnterFrame;
         this.delayTime = 10;
         this.onEnterFrame = function()
         {
            if(pausing)
            {
               return 0;
            }
            var _loc6_;
            var _loc2_;
            var _loc5_;
            var _loc4_;
            var _loc3_;
            if(--this.delayTime <= 0)
            {
               _loc6_ = this.expXy.length;
               _loc2_ = 0;
               while(_loc2_ < _loc6_)
               {
                  _loc5_ = this.expXy[_loc2_][0];
                  _loc4_ = this.expXy[_loc2_][1];
                  _loc3_ = bgArea["bg" + _loc4_ + "_" + _loc5_];
                  _loc3_.expEnergy = _loc3_.expEnergy - 1;
                  _loc2_ = _loc2_ + 1;
               }
               delete this.onEnterFrame;
               this.onEnterFrame = function()
               {
                  if(pausing)
                  {
                     return 0;
                  }
                  if((this._alpha -= 20) < 5)
                  {
                     this.removeMovieClip();
                  }
               };
            }
         };
      }
   };
   return true;
}
function brickBreak(bg, heroNum)
{
   var _loc4_ = bg.x;
   var _loc3_ = bg.y;
   brickArea.attachMovie("brickExp","brickExp" + _loc3_ + "_" + _loc4_,calDepth(_loc4_,_loc3_,brickArea.margin),{_x:_loc4_ * d,_y:_loc3_ * d});
   var _loc6_;
   if(brickArea["hero" + heroNum] != undefined)
   {
      _loc6_ = _parent["heroObj" + heroNum];
      _loc6_.brickScore += brickScore;
      _loc6_.score += brickScore;
      _loc6_.totalScore += brickScore;
      _parent.allPan.upDate();
   }
   bg.brick = "";
   var _loc2_;
   var _loc5_;
   if(random(100) < 65)
   {
      _loc2_ = brickArea.attachMovie("tool","tool" + _loc3_ + "_" + _loc4_,calDepth(_loc4_,_loc3_,brickArea.margin) - 6,{_x:_loc4_ * d,_y:_loc3_ * d});
      if(random(100) < 80)
      {
         _loc5_ = random(100);
         if(_loc5_ < 1)
         {
            _loc2_.toolName = "生命加1";
         }
         else if(_loc5_ < 15)
         {
            _loc2_.toolName = "抢手";
         }
         else if(_loc5_ < 40)
         {
            _loc2_.toolName = "炸弹糖";
         }
         else if(_loc5_ < 50)
         {
            _loc2_.toolName = "盾牌";
         }
         else if(_loc5_ < 75)
         {
            _loc2_.toolName = "威力药";
         }
         else
         {
            _loc2_.toolName = "飞鞋";
         }
      }
      else
      {
         _loc2_.toolName = _parent.specialToolName_array[_parent.specialToolId + random(2)];
      }
      _loc2_.tool_mc.gotoAndStop(_loc2_.toolName);
      bg.tool = _loc2_._name;
   }
}
function pushBomb(bg, dx, dy)
{
   var _loc3_ = brickArea[bg.bomb];
   var _loc4_;
   if(_loc3_ != undefined && bg.x == _loc3_.x && bg.y == _loc3_.y && _loc3_.x == _loc3_._x / d && _loc3_.y == _loc3_._y / d)
   {
      _loc3_.movId = 0;
      _loc3_.dx = dx;
      _loc3_.dy = dy;
      _loc4_ = bgArea["bg" + (bg.y + dy) + "_" + (bg.x + dx)];
      if(_loc4_.bomb == "" && _loc4_.brick == "")
      {
         bg.bomb = "";
         var x = _loc3_.x + dx;
         var y = _loc3_.y + dy;
         _loc3_._name = "bomb" + y + "_" + x;
         _loc3_.swapDepths(calDepth(x,y,brickArea.margin) - 5);
         _loc4_.bomb = _loc3_._name;
         if(_parent.soundon)
         {
            vo_sound.attachSound("pushBomb.mp3");
            vo_sound.start();
         }
         _loc3_.onEnterFrame = function()
         {
            if(pausing)
            {
               return 0;
            }
            var _loc3_;
            var _loc2_;
            if(++this.movId < 2)
            {
               _loc3_ = this.movId / 2;
               this._x = (this.x + this.dx * _loc3_) * d;
               this._y = (this.y + this.dy * _loc3_) * d;
            }
            else
            {
               this.x += this.dx;
               this.y += this.dy;
               this._x = this.x * d;
               this._y = this.y * d;
               this.bg = bgArea["bg" + this.y + "_" + this.x];
               _loc2_ = bgArea["bg" + (this.y + this.dy) + "_" + (this.x + this.dx)];
               if(_loc2_.bomb == "" && _loc2_.brick == "")
               {
                  this.bg.bomb = "";
                  var x = this.x + this.dx;
                  var y = this.y + this.dy;
                  this._name = "bomb" + y + "_" + x;
                  this.swapDepths(calDepth(x,y,brickArea.margin) - 5);
                  _loc2_.bomb = this._name;
                  this.movId = 0;
               }
               else
               {
                  bombExp(this.bg);
               }
            }
         };
         return true;
      }
   }
   return false;
}
function movHero(hero, xyId)
{
   hero.dx = xy[xyId][0];
   hero.dy = xy[xyId][1];
   var bg = bgArea["bg" + (hero.y + hero.dy) + "_" + (hero.x + hero.dx)];
   hero.gotoAndStop(xyId + 1);
   hero.body.stop();
   if(bg.brick == "" && (bg.bomb == "" || hero.pushBombAble))
   {
      if(bg.bomb != "")
      {
         if(pushBomb(bg,hero.dx,hero.dy) == false)
         {
            return false;
         }
      }
      hero.moving = true;
      hero.movId = 0;
      hero.bg = bg;
      hero.body.play();
      hero.onEnterFrame = function()
      {
         if(pausing)
         {
            return false;
         }
         var _loc8_ = this.slowTime != -1 ? this.slowVLevel : this.vLevel;
         var _loc9_;
         var _loc3_;
         var _loc4_;
         var _loc7_;
         var _loc10_;
         var _loc0_;
         var _loc5_;
         if(++this.movId < _loc8_)
         {
            _loc9_ = this.movId / _loc8_;
            this._x = (this.x + this.dx * _loc9_) * d;
            this._y = (this.y + this.dy * _loc9_) * d;
         }
         else
         {
            this.moving = false;
            delete this.onEnterFrame;
            this.body.stop();
            this.x += this.dx;
            this.y += this.dy;
            this._x = this.x * d;
            this._y = this.y * d;
            this.swapDepths(calDepth(this.x,this.y,brickArea.margin) - this.heroNum);
            if(this.bg.tool != "")
            {
               if(_parent.varHard == "easy" && this.heroNum > totalPlayer)
               {
                  return false;
               }
               _loc3_ = brickArea[this.bg.tool].toolName;
               _loc4_ = true;
               switch(_loc3_)
               {
                  case "火箭":
                     if(_parent.varHard != "hard" && this.heroNum > totalPlayer)
                     {
                        _loc4_ = false;
                     }
                     else
                     {
                        this.totalSpecial = 2;
                        this.attachMovie("specialToolViewer","specialToolViewer",0);
                        this.specialToolViewer.tool.gotoAndStop(_loc3_);
                        this.specialToolName = _loc3_;
                     }
                     break;
                  case "地雷":
                     if(_parent.varHard != "hard" && this.heroNum > totalPlayer)
                     {
                        _loc4_ = false;
                     }
                     else
                     {
                        this.totalSpecial = 2;
                        this.attachMovie("specialToolViewer","specialToolViewer",0);
                        this.specialToolViewer.tool.gotoAndStop(_loc3_);
                        this.specialToolName = _loc3_;
                     }
                     break;
                  case "手榴弹":
                     if(_parent.varHard != "hard" && this.heroNum > totalPlayer)
                     {
                        _loc4_ = false;
                     }
                     else
                     {
                        _loc7_ = bgArea.getNextHighestDepth();
                        _loc10_ = bgArea.attachMovie("grenade_x","grenade_x" + _loc7_,_loc7_,{hero:this,_x:-1000});
                        _loc10_.onEnterFrame = function()
                        {
                           if(pausing)
                           {
                              return 0;
                           }
                           var _loc2_;
                           var _loc4_;
                           var _loc3_;
                           var _loc5_;
                           if(this.hero.specialToolName != "手榴弹")
                           {
                              this.removeMovieClip();
                           }
                           else if(this.hero.totalSpecial > 0)
                           {
                              if(this.hero.moving)
                              {
                                 this._x = -1000;
                              }
                              else
                              {
                                 _loc2_ = this.hero._currentframe - 1;
                                 _loc4_ = this.hero.x + xy[_loc2_][0] * 2;
                                 _loc3_ = this.hero.y + xy[_loc2_][1] * 2;
                                 _loc5_ = bgArea["bg" + _loc3_ + "_" + _loc4_];
                                 if(_loc5_.brick == "")
                                 {
                                    this._x = _loc4_ * d;
                                    this._y = _loc3_ * d;
                                 }
                                 else
                                 {
                                    this._x = -1000;
                                 }
                              }
                           }
                           else
                           {
                              this.removeMovieClip();
                           }
                        };
                        this.totalSpecial = 2;
                        this.attachMovie("specialToolViewer","specialToolViewer",0);
                        this.specialToolViewer.tool.gotoAndStop(_loc3_);
                        this.specialToolName = _loc3_;
                     }
                     break;
                  case "汽弹":
                     if(_parent.varHard != "hard" && this.heroNum > totalPlayer)
                     {
                        _loc4_ = false;
                     }
                     else
                     {
                        this.totalSpecial = 2;
                        this.attachMovie("specialToolViewer","specialToolViewer",0);
                        this.specialToolViewer.tool.gotoAndStop(_loc3_);
                        this.specialToolName = _loc3_;
                     }
                     break;
                  case "冰箭":
                     if(_parent.varHard != "hard" && this.heroNum > totalPlayer)
                     {
                        _loc4_ = false;
                     }
                     else
                     {
                        this.totalSpecial = 1;
                        this.attachMovie("specialToolViewer","specialToolViewer",0);
                        this.specialToolViewer.tool.gotoAndStop(_loc3_);
                        this.specialToolName = _loc3_;
                     }
                     break;
                  case "火焰箭":
                     if(_parent.varHard != "hard" && this.heroNum > totalPlayer)
                     {
                        _loc4_ = false;
                     }
                     else
                     {
                        this.totalSpecial = 2;
                        this.attachMovie("specialToolViewer","specialToolViewer",0);
                        this.specialToolViewer.tool.gotoAndStop(_loc3_);
                        this.specialToolName = _loc3_;
                     }
                     break;
                  case "生命加1":
                     _parent["heroObj" + this.heroNum].life++;
                     _parent.allPan.mcBlink("lifeMc",this.heroNum);
                     break;
                  case "抢手":
                     this.pushBombAble = true;
                     _parent.allPan.showMc("glove",this.heroNum,true);
                     break;
                  case "炸弹糖":
                     var _temp_2 = _parent["heroObj" + this.heroNum];
                     var _temp_1 = "totalBomb";
                     this.totalBomb = _loc0_ = this.totalBomb + 1;
                     _temp_2[_temp_1] = _loc0_;
                     _parent.allPan.mcBlink("totalBombMc",this.heroNum);
                     break;
                  case "盾牌":
                     this.shield = true;
                     _parent.allPan.showMc("shield",this.heroNum,true);
                     break;
                  case "威力药":
                     if(this.bombLevel < 10)
                     {
                        this.bombLevel = this.bombLevel + 1;
                        _parent.allPan.mcBlink("bomb",this.heroNum);
                     }
                     break;
                  case "飞鞋":
                     if(this.heroNum > totalPlayer && _parent.varHard != "hard")
                     {
                        _loc4_ = false;
                     }
                     else if(this.vLevel > 3)
                     {
                        this.vLevel = this.vLevel - 1;
                        _parent.allPan.mcBlink("shoe",this.heroNum);
                     }
               }
               if(_loc4_)
               {
                  brickArea[this.bg.tool].gotoAndPlay("remove");
                  this.bg.tool = "";
                  _loc5_ = _parent["heroObj" + this.heroNum];
                  _loc5_.toolScore += 100;
                  _loc5_.score += 100;
                  _loc5_.totalScore += 100;
                  _parent.allPan.upDate();
                  if(_parent.soundon)
                  {
                     vo_sound.attachSound("tool.wav");
                     vo_sound.start();
                  }
               }
            }
         }
      };
      return true;
   }
   return false;
}
function getBgStyle(sceneStyle)
{
   switch(sceneStyle)
   {
      case 0:
         return random(3) + 1;
      case 1:
         return random(2) + 4;
      case 2:
         return random(2) + 6;
      case 3:
         return random(3) + 8;
      case 4:
         return random(2) + 11;
      default:
         return;
   }
}
function getBigBrickStyle(sceneStyle)
{
   switch(sceneStyle)
   {
      case 0:
         return 52;
      case 1:
         return random(2) + 53;
      case 2:
         return random(2) + 55;
      case 3:
         return 57;
      case 4:
         return 58;
      default:
         return;
   }
}
function calDepth(x, y, margin)
{
   return (y * w + (w - x)) * margin;
}
function pause()
{
   if(pausing)
   {
      pausing = false;
      _parent.music_btn.enabled = true;
   }
   else
   {
      pausing = true;
      _parent.music_btn.enabled = false;
   }
}
function isSave(x0, y0)
{
   var _loc1_ = bgArea["bg" + y0 + "_" + x0];
   if(_loc1_ == undefined || _loc1_.expEnergy > 0 || _loc1_.bomb != "" || _loc1_.brick != "" || bgArea["expArea" + y0 + "_" + x0] != undefined)
   {
      return false;
   }
   var _loc6_ = 0;
   var _loc8_;
   var _loc7_;
   var _loc5_;
   var _loc3_;
   var _loc2_;
   var _loc4_;
   for(; _loc6_ < 4; _loc6_ = _loc6_ + 1)
   {
      _loc8_ = xy[_loc6_][0];
      _loc7_ = xy[_loc6_][1];
      _loc5_ = 1;
      _loc3_ = x0 + _loc8_;
      _loc2_ = y0 + _loc7_;
      _loc1_ = bgArea["bg" + _loc2_ + "_" + _loc3_];
      while(!(_loc1_ == undefined || _loc1_.brick != ""))
      {
         _loc4_ = bgArea["expArea" + _loc2_ + "_" + _loc3_];
         if(_loc1_.bomb != "" && brickArea[_loc1_.bomb].bombLevel >= _loc5_ || _loc4_ != undefined && _loc4_.bombLevel >= _loc5_)
         {
            return false;
         }
         _loc3_ += _loc8_;
         _loc2_ += _loc7_;
         _loc1_ = bgArea["bg" + _loc2_ + "_" + _loc3_];
         _loc5_ = _loc5_ + 1;
         continue;
         return false;
      }
   }
   return true;
}
function getScore(x0, y0)
{
   if(!isSave(x0,y0))
   {
      return -1;
   }
   var _loc2_ = 0;
   var _loc1_ = 0;
   while(_loc1_ < 4)
   {
      if(isSave(x0 + xy[_loc1_][0],y0 + xy[_loc1_][1]))
      {
         _loc2_ = _loc2_ + 1;
      }
      _loc1_ = _loc1_ + 1;
   }
   return _loc2_;
}
function disorder(arr)
{
   var _loc4_ = arr.length;
   var _loc6_ = arr[_loc4_ - 1];
   var _loc1_ = 0;
   var _loc3_;
   var _loc5_;
   while(_loc1_ < _loc4_)
   {
      _loc3_ = random(_loc4_);
      _loc5_ = arr[_loc1_];
      arr[_loc1_] = arr[_loc3_];
      arr[_loc3_] = _loc5_;
      _loc1_ = _loc1_ + 1;
   }
   if(_loc6_ == arr[0])
   {
      _loc5_ = arr[_loc4_ - 1];
      arr[_loc4_ - 1] = arr[0];
      arr[0] = _loc5_;
   }
}
game = this;
for(§each§ in brickArea)
{
   brickArea[eval("each")].removeMovieClip();
}
for(§each§ in bgArea)
{
   bgArea[eval("each")].removeMovieClip();
}
switch(_parent.varHard)
{
   case "easy":
      escapeRate = 20;
      brickScore = 5;
      break;
   case "normal":
      escapeRate = 60;
      brickScore = 10;
      break;
   case "hard":
      escapeRate = 100;
      brickScore = 15;
}
w = 15;
h = 13;
d = 30;
xy = [[-1,0],[0,-1],[1,0],[0,1]];
heroXy = [[3,3],[w - 4,3],[3,h - 4],[w - 4,h - 4]];
if(_parent.specialToolId >= 4)
{
   _parent.specialToolId = 0;
   disorder(_parent.specialToolName_array);
   trace("specialToolName_array=" + _parent.specialToolName_array);
}
else
{
   _parent.specialToolId += 2;
}
if(_parent.bgMusicId >= 3)
{
   _parent.bgMusicId = 0;
   disorder(_parent.bgMusic_array);
   trace("bgMusic_array=" + _parent.bgMusic_array);
}
else
{
   _parent.bgMusicId++;
}
trace(_parent.specialToolName_array[_parent.specialToolId] + "," + _parent.specialToolName_array[_parent.specialToolId + 1]);
totalPlayer = _parent.totalPlayer;
var heroNum = 1;
while(heroNum <= totalPlayer)
{
   game["p" + heroNum] = _parent["p" + heroNum];
   _parent["heroObj" + heroNum].heroId = game["p" + heroNum].heroId;
   heroNum++;
}
var heroNum = totalPlayer + 1;
while(heroNum <= 4)
{
   var j = 1;
   while(j <= 4)
   {
      var k = 1;
      while(k <= heroNum)
      {
         if(_parent["heroObj" + k].heroId == j)
         {
            break;
         }
         k++;
      }
      if(k >= heroNum)
      {
         _parent["heroObj" + heroNum].heroId = j;
         break;
      }
      j++;
   }
   heroNum++;
}
var _loc0_;
_parent.stageNum = _loc0_ = _parent.stageNum + 1;
_parent.upDateStageNum(_loc0_);
game.createEmptyMovieClip("bgMusic_mc",100);
bgMusic_sound = new Sound(bgMusic_mc);
bgMusic_sound.onSoundComplete = function()
{
   this.start();
};
game.createEmptyMovieClip("vo_mc",101);
vo_sound = new Sound(vo_mc);
soundPause = false;
_parent.allPan.init();
pausing = false;
sceneStyle = random(5);
switch(sceneStyle)
{
   case 0:
      brickStyle = [1,2,3,4,5,6];
      unbreakAbleStyle = [7,8,9];
      break;
   case 1:
      brickStyle = [10,11,12];
      unbreakAbleStyle = [13,14,15,16,17,18,19];
      break;
   case 2:
      brickStyle = [20,21,22,23];
      unbreakAbleStyle = [24,25,26,27,28,29,30,31];
      break;
   case 3:
      brickStyle = [32,33,34,35,36,37,38];
      unbreakAbleStyle = [39,40,41];
      break;
   case 4:
      brickStyle = [42,43,44,45,46,47];
      unbreakAbleStyle = [48,49,50,51];
}
disorder(brickStyle);
disorder(unbreakAbleStyle);
bgArea.margin = 1;
var bgStyle = getBgStyle(sceneStyle);
var y = 0;
while(y < h)
{
   var x = 0;
   while(x < w)
   {
      var bg = bgArea.attachMovie("bg","bg" + y + "_" + x,calDepth(x,y,bgArea.margin),{brick:"",bomb:"",tool:"",expEnergy:0,expHeroNum:0,x:x,y:y,_x:x * d,_y:y * d});
      bg.gotoAndStop(bgStyle);
      x++;
   }
   y++;
}
brickArea.margin = 10;
var x0 = int(w / 2) - 1;
var y0 = int(h / 2) - 1;
var y = y0;
while(y <= y0 + 1)
{
   var x = x0;
   while(x <= x0 + 1)
   {
      bgArea["bg" + y + "_" + x].brick = "unbreakAble";
      x++;
   }
   y++;
}
brick = brickArea.attachMovie("brick","brick" + y0 + "_" + x0,calDepth(x0,y0 + 1,brickArea.margin),{unbreakAble:true,_x:x0 * d,_y:y0 * d});
brick.gotoAndStop(getBigBrickStyle(sceneStyle));
var ranMap = new Array();
var y = 0;
while(y < h)
{
   var x = 0;
   while(x < w)
   {
      var i = 0;
      while(i < 4)
      {
         var heroX = heroXy[i][0];
         var heroY = heroXy[i][1];
         if(x == heroX && y == heroY)
         {
            break;
         }
         var j = 0;
         while(j < 4)
         {
            if(x == heroX + xy[j][0] && y == heroY + xy[j][1])
            {
               break;
            }
            j++;
         }
         if(j < 4)
         {
            break;
         }
         i++;
      }
      if(i >= 4)
      {
         ranMap.push([x,y]);
      }
      x++;
   }
   y++;
}
var ranMapL = ranMap.length;
var i = 0;
while(i < ranMapL)
{
   var ran = random(ranMapL);
   var temp = ranMap[i];
   ranMap[i] = ranMap[ran];
   ranMap[ran] = temp;
   i++;
}
var aroundXy = [[-1,0],[-1,-1],[0,-1],[1,-1],[1,0],[1,1],[0,1],[-1,1]];
var i = 0;
while(i < 4)
{
   var heroX = heroXy[i][0];
   var heroY = heroXy[i][1];
   var j = 1;
   while(j < 8)
   {
      ranMap.unshift([heroX + aroundXy[j][0],heroY + aroundXy[j][1]]);
      j += 2;
   }
   i++;
}
var i = 0;
while(i < ranMapL)
{
   var x0 = ranMap[i][0];
   var y0 = ranMap[i][1];
   if(bgArea["bg" + y0 + "_" + x0].brick == "")
   {
      var j = 0;
      while(j < 8)
      {
         var k1 = (j + 1) % 8;
         var k2 = (j + 2) % 8;
         if(bgArea["bg" + (y0 + aroundXy[j][1]) + "_" + (x0 + aroundXy[j][0])].brick != "" && bgArea["bg" + (y0 + aroundXy[k1][1]) + "_" + (x0 + aroundXy[k1][0])].brick != "" && bgArea["bg" + (y0 + aroundXy[k2][1]) + "_" + (x0 + aroundXy[k2][0])].brick != "")
         {
            break;
         }
         j += 2;
      }
      if(j >= 8)
      {
         if(bgArea["bg" + y0 + "_" + (x0 - 1)].brick == "")
         {
            var empty = true;
            var totalEmpty = 1;
            var totalBrick = 0;
         }
         else
         {
            var empty = false;
            var totalEmpty = 0;
            var totalBrick = 1;
         }
         var j = 1;
         while(j < 8)
         {
            var brick = bgArea["bg" + (y0 + aroundXy[j][1]) + "_" + (x0 + aroundXy[j][0])].brick;
            if(empty)
            {
               if(brick != "")
               {
                  empty = false;
                  totalBrick++;
               }
            }
            else if(brick == "")
            {
               if(j % 2 == 0)
               {
                  empty = true;
                  totalEmpty++;
               }
               else
               {
                  var k = (j + 1) % 8;
                  if(bgArea["bg" + (y0 + aroundXy[k][1]) + "_" + (x0 + aroundXy[k][0])].brick == "")
                  {
                     empty = true;
                     totalEmpty++;
                  }
                  else
                  {
                     j += 2;
                  }
               }
            }
            j++;
         }
         if(totalEmpty + totalBrick <= 3)
         {
            bgArea["bg" + y0 + "_" + x0].brick = "unbreakAble";
            var brick = brickArea.attachMovie("brick","brick" + y0 + "_" + x0,calDepth(x0,y0,brickArea.margin),{unbreakAble:true,_x:x0 * d,_y:y0 * d});
            brick.gotoAndStop(unbreakAbleStyle[random(2)]);
         }
      }
   }
   i++;
}
delete ranMap;
delete aroundXy;
Key.addListener(this);
