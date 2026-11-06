stageNum = 0;
var heroNum = 1;
while(heroNum <= 4)
{
   var heroObj = this["heroObj" + heroNum];
   heroObj.totalScore = 0;
   if(heroNum <= totalPlayer)
   {
      heroObj.life = 1;
   }
   else
   {
      heroObj.life = 1;
   }
   heroNum++;
}
p_pointer1.removeMovieClip();
p_pointer2.removeMovieClip();
bgMusic_sound.stop();
Key.removeListener(this);
quality_mc.gotoAndStop(qualityNum);
if(soundon)
{
   soundon_mc.stop();
}
else
{
   soundon_mc.nextFrame();
}
switch(varHard)
{
   case "easy":
      totalStage = 15;
      break;
   case "normal":
      totalStage = 20;
      break;
   case "hard":
      totalStage = 25;
}
trace("varHard=" + varHard + ",totalStage=" + totalStage);
