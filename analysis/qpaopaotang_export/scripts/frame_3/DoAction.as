stop();
if(bgMusic_sound == undefined)
{
   soundon = true;
   bgMusic_sound = new Sound(this);
   bgMusic_sound.attachSound("Bombit_-_mainscreen");
   bgMusic_sound.start();
   bgMusic_sound.onSoundComplete = function()
   {
      if(soundon)
      {
         this.start();
      }
   };
}
delete p1;
delete p2;
p1 = undefined;
p2 = undefined;
