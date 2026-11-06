on(release){
   if(_parent.soundon)
   {
      _parent.soundon = false;
      _parent.bgMusic_sound.stop();
      nextFrame();
   }
   else
   {
      _parent.soundon = true;
      prevFrame();
      _parent.bgMusic_sound.start();
   }
}
