function mov(p, dx)
{
   if(p.setDown)
   {
      return 0;
   }
   var _loc3_;
   if(totalPlayer == 2)
   {
      _loc3_ = this["p" + (3 - p.id)];
      do
      {
         p.heroId += dx;
         if(p.heroId <= 0)
         {
            p.heroId = 4;
         }
         else if(p.heroId > 4)
         {
            p.heroId = 1;
         }
      }
      while(p.heroId == _loc3_.heroId);
   }
   else
   {
      p.heroId += dx;
      if(p.heroId <= 0)
      {
         p.heroId = 4;
      }
      else if(p.heroId > 4)
      {
         p.heroId = 1;
      }
   }
   var _loc5_ = this["p_pointer" + p.id];
   _loc5_.delayTime = 2;
   _loc5_.heroId = p.heroId;
   _loc5_.p.gotoAndStop((p.id - 1) * 4 + p.heroId);
   _loc5_.arrow.gotoAndStop(p.heroId);
   _loc5_._x = this["hero" + p.heroId]._x;
}
var heroNum = 1;
while(heroNum <= 4)
{
   this["heroObj" + heroNum] = {heroId:0,life:1,totalBomb:1,totalScore:0,score:0,killScore:0,timeScore:0,toolScore:0,brickScore:0,bonus:0};
   var hero = this["hero" + heroNum];
   if(heroNum <= totalPlayer)
   {
      var p = this["p" + heroNum];
      var p_pointer = this.attachMovie("p_pointer","p_pointer" + heroNum,heroNum,{heroId:0,delayTime:2,_xscale:200,_yscale:200,_y:hero._y});
      p_pointer.onEnterFrame = function()
      {
         if(--this.delayTime <= 0)
         {
            this.delayTime = 100;
            this._parent["眼皮" + this.heroId].gotoAndPlay(2);
         }
      };
   }
   heroNum++;
}
Key.addListener(this);
totalSetDown = 0;
if(totalPlayer == 1)
{
   heroObj1.life = 1;
   p1 = {setDown:false,id:1,heroId:0,key0:37,key1:38,key2:39,key3:40,fireKey:32};
   mov(p1,1);
   this.onKeyUp = function()
   {
      switch(Key.getCode())
      {
         case p1.key0:
         case p1.key1:
            mov(p1,-1);
            break;
         case p1.key2:
         case p1.key3:
            mov(p1,1);
            break;
         case p1.fireKey:
            gotoAndStop("game");
         default:
            return;
      }
   };
}
else
{
   heroObj1.life = 1;
   heroObj2.life = 1;
   p1 = {setDown:false,id:1,heroId:0,key0:65,key1:87,key2:68,key3:83,fireKey:32};
   p2 = {setDown:false,id:2,heroId:0,key0:37,key1:38,key2:39,key3:40,fireKey:13};
   mov(p1,1);
   mov(p2,1);
   this.onKeyUp = function()
   {
      var _loc0_;
      switch(Key.getCode())
      {
         case p1.key0:
         case p1.key1:
            mov(p1,-1);
            break;
         case p1.key2:
         case p1.key3:
            mov(p1,1);
            break;
         case p1.fireKey:
            if(p1.setDown == false)
            {
               p1.setDown = true;
               p_pointer1.gotoAndStop(1);
               totalSetDown = _loc0_ = totalSetDown + 1;
               if(_loc0_ == totalPlayer)
               {
                  gotoAndStop("game");
               }
            }
            break;
         case p2.key0:
         case p2.key1:
            mov(p2,-1);
            break;
         case p2.key2:
         case p2.key3:
            mov(p2,1);
            break;
         case p2.fireKey:
            if(p2.setDown == false)
            {
               p2.setDown = true;
               p_pointer2.gotoAndStop(1);
               totalSetDown = _loc0_ = totalSetDown + 1;
               if(_loc0_ == totalPlayer)
               {
                  gotoAndStop("game");
               }
            }
         default:
            return;
      }
   };
}
specialToolName_array = ["火箭","地雷","手榴弹","汽弹","冰箭","火焰箭"];
specialToolId = 6;
bgMusic_array = ["Bombit_-_level_1","Bombit_-_level_2","Bombit_-_level_3","Bombit_-_level_4"];
bgMusicId = 4;
