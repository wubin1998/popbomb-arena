function submit_f()
{
   var _loc3_ = 1;
   while(_loc3_ <= _parent.totalPlayer)
   {
      _parent.highScore_arr.push([_parent["playerName" + _loc3_],this["totalScore" + _loc3_]]);
      _loc3_ = _loc3_ + 1;
   }
   _parent.gameSave();
}
totalScore1 = _parent.heroObj1.totalScore;
totalScore2 = _parent.heroObj2.totalScore;
if(_parent.stageNum == _parent.totalStage + 1)
{
   gameover_welldone.gotoAndStop(_parent.varHard);
   switch(_parent.varHard)
   {
      case "easy":
         _parent.varHard = "normal";
         break;
      case "normal":
         _parent.varHard = "hard";
   }
}
else
{
   gameover_welldone.stop();
}
gotoAndStop(_parent.totalPlayer);
