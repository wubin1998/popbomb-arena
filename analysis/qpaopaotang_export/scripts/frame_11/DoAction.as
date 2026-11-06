nameOutput = "";
scoreOutput = "";
var L = highScore_arr.length;
var i = 0;
while(i < L)
{
   var j = i;
   while(j < L)
   {
      if(highScore_arr[i][1] < highScore_arr[j][1])
      {
         var temp = highScore_arr[i][0];
         highScore_arr[i][0] = highScore_arr[j][0];
         highScore_arr[j][0] = temp;
         var temp = highScore_arr[i][1];
         highScore_arr[i][1] = highScore_arr[j][1];
         highScore_arr[j][1] = temp;
      }
      j++;
   }
   i++;
}
if(L > 10)
{
   L = 10;
}
var i = 0;
while(i < L)
{
   var playerName = highScore_arr[i][0];
   var highScore = highScore_arr[i][1];
   this["name" + (i + 1)] = playerName;
   this["score" + (i + 1)] = highScore;
   var j = 0;
   while(j <= totalPlayer)
   {
      if(playerName == this["playerName" + j] && highScore == this["heroObj" + j].totalScore)
      {
         break;
      }
      j++;
   }
   if(j > totalPlayer)
   {
      this["new" + i]._visible = false;
   }
   else
   {
      this["new" + i]._visible = true;
   }
   i++;
}
gameSave();
