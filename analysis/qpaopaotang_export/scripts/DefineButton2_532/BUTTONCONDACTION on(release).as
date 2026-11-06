on(release){
   if(soundon)
   {
      game.bgMusic_sound.stop();
      soundon = false;
      soundon_mc.nextFrame();
   }
   else
   {
      game.bgMusic_sound.start();
      soundon = true;
      soundon_mc.prevFrame();
   }
}
