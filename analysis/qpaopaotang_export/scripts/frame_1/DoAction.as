function gameSave()
{
   trace("saved");
   var _loc1_ = SharedObject.getLocal("bomb_it");
   var _loc2_ = {playerName1:playerName1,playerName2:playerName2,highScore_arr:highScore_arr};
   _loc1_.data.myObj = _loc2_;
}
function upDateStageNum(stageNum)
{
   stageNum_str = "";
   if(stageNum < 10)
   {
      stageNum_str += "0";
   }
   stageNum_str += stageNum;
}
fscommand("allowscale",false);
fscommand("showmenu",false);
qualityNum = 1;
life0 = 1;
var totalPlayer = 0;
myLSO = SharedObject.getLocal("bomb_it");
if(myLSO.data.myObj == undefined)
{
   trace("first play");
   playerName1 = "";
   playerName2 = "";
   highScore_arr = [["cpu",100],["cpu",137728],["cpu",50000],["cpu",40000],["cpu",30000],["cpu",20000],["cpu",10000],["cpu",5000],["cpu",3000],["cpu",1000]];
   gameSave();
}
else
{
   trace("loaded");
   var myObj = myLSO.data.myObj;
   playerName1 = myObj.playerName1;
   playerName2 = myObj.playerName2;
   highScore_arr = myObj.highScore_arr.slice();
}
