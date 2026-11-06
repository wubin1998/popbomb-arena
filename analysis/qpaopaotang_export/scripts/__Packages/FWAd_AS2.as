class FWAd_AS2 extends MovieClip
{
   var _FWAd;
   var adValues;
   var loadingBar;
   var loadingBar_bar;
   var mcl;
   var onEnterFrame;
   static var canNotShowAd;
   static var frameworkInitFinished;
   static var frameworkLoader;
   static var fwAd;
   static var hei;
   static var id;
   static var ifIsLoadingAdShowStartBtn;
   static var lc;
   static var lc_connName;
   static var lc_sendName;
   static var loadFrameworkFailed;
   static var loadFrameworkSuccess;
   static var loadGameCompleted;
   static var newsLoader;
   static var newsMcl;
   static var news_lc;
   static var noShowForbidden;
   static var noShowLogo;
   static var onClickStartBtn;
   static var restLoadFrameworkTimes;
   static var wid;
   static var xx;
   static var yy;
   static var ifLoadFrameworkErrorCanPlayGame = true;
   static var loadFrameworkTimeoutTime = 8;
   static var loadFrameworkTimes = 2;
   static var ifAdFailCanPlayGame = true;
   static var ifAddCallBackErrorCanPlayGame = false;
   static var swfDomain = "";
   static var swfFolder = "http://" + swfDomain + "/";
   static var frameworkSWFPath = swfFolder + "";
   static var newsSWFPath = swfFolder + "News.swf";
   static var AD_TYPE_LOADING = "loading";
   static var AD_TYPE_CONTAINER = "container";
   static var adType = AD_TYPE_LOADING;
   static var newsValues = {x:0,y:0};
   static var checkNewsFirstInitIntervalId = -1;
   var timeoutId = -1;
   function FWAd_AS2()
   {
      super();
      xx = this.adValues.x;
      yy = this.adValues.y;
      wid = this.adValues.wid;
      hei = this.adValues.hei;
      id = this.adValues.id;
      adType = this.adValues.adType;
      ifIsLoadingAdShowStartBtn = this.adValues.ifIsLoadingAdShowStartBtn;
      noShowLogo = this.adValues.noShowLogo;
      noShowForbidden = this.adValues.noShowForbidden;
      if(this.adValues.onClickCloseBtn)
      {
         onClickStartBtn = this.adValues.onClickCloseBtn;
      }
      else
      {
         onClickStartBtn = this.adValues.onClickStartBtn;
      }
      fwAd = this;
      this._FWAd = FWAd_AS2;
      frameworkLoader = null;
      lc = null;
      loadFrameworkSuccess = false;
      loadFrameworkFailed = false;
      frameworkInitFinished = false;
      System.security.allowDomain(swfDomain);
      System.security.allowInsecureDomain(swfDomain);
      fscommand("showMenu","false");
      var my_cm = new ContextMenu();
      my_cm.hideBuiltInItems();
      _root.menu = my_cm;
      fscommand("trapallkeys","true");
      loadGameCompleted = _root.getBytesLoaded() == _root.getBytesTotal();
      frameworkLoader = this.createEmptyMovieClip("frameworkLoader",this.getNextHighestDepth());
      frameworkLoader.createEmptyMovieClip("content",frameworkLoader.getNextHighestDepth());
      this.mcl = new MovieClipLoader();
      this.mcl.onLoadInit = function()
      {
         fwAd.loadFrameworkComplete();
      };
      this.mcl.onLoadError = function()
      {
         fwAd.loadFrameworkError();
      };
      restLoadFrameworkTimes = loadFrameworkTimes;
      this.loadFramework();
      var txt_sp = this.createEmptyMovieClip("txt_sp",this.getNextHighestDepth());
      var DropShadowFilterClassName = "flash.filters.DropShadowFilter";
      var DropShadowFilterClass = eval(DropShadowFilterClassName);
      if(DropShadowFilterClass)
      {
         txt_sp.filters = [new DropShadowFilterClass(0,0,0,1,2,2,4)];
      }
      var loadingBar_hei = 8;
      var loadingBar_x = 10;
      var loadingBar_wid = wid - 2 * loadingBar_x;
      this.loadingBar = this.createEmptyMovieClip("loadingBar",this.getNextHighestDepth());
      this.loadingBar._x = loadingBar_x;
      this.loadingBar._y = hei - loadingBar_hei - 6;
      var loadingBar_bottom = this.loadingBar.createEmptyMovieClip("loadingBar_bottom",this.loadingBar.getNextHighestDepth());
      this.loadingBar_bar = this.loadingBar.createEmptyMovieClip("loadingBar_bar",this.loadingBar.getNextHighestDepth());
      var loadingBar_line = this.loadingBar.createEmptyMovieClip("loadingBar_line",this.loadingBar.getNextHighestDepth());
      loadingBar_bottom.clear();
      loadingBar_bottom.beginFill(3355443);
      loadingBar_bottom.moveTo(0,0);
      loadingBar_bottom.lineTo(loadingBar_wid,0);
      loadingBar_bottom.lineTo(loadingBar_wid,loadingBar_hei);
      loadingBar_bottom.lineTo(0,loadingBar_hei);
      loadingBar_bottom.lineTo(0,0);
      loadingBar_bottom.endFill();
      this.loadingBar_bar.clear();
      var MatrixClassName = "flash.geom.Matrix";
      var MatrixClass = eval(MatrixClassName);
      if(MatrixClass)
      {
         this.loadingBar_bar.beginGradientFill("linear",[16777215,16750950],[100,100],[63,255],new MatrixClass(0.006,0,0,1,0,0));
      }
      else
      {
         this.loadingBar_bar.beginFill(16750950);
      }
      this.loadingBar_bar._rotation = 90;
      this.loadingBar_bar.moveTo(0,- loadingBar_wid);
      this.loadingBar_bar.lineTo(loadingBar_hei,- loadingBar_wid);
      this.loadingBar_bar.lineTo(loadingBar_hei,0);
      this.loadingBar_bar.lineTo(0,0);
      this.loadingBar_bar.lineTo(0,- loadingBar_wid);
      this.loadingBar_bar.endFill();
      loadingBar_line.clear();
      loadingBar_line.lineStyle(1,6710886);
      loadingBar_line.moveTo(0,0);
      loadingBar_line.lineTo(loadingBar_wid,0);
      loadingBar_line.lineTo(loadingBar_wid,loadingBar_hei);
      loadingBar_line.lineTo(0,loadingBar_hei);
      loadingBar_line.lineTo(0,0);
      loadingBar_line.endFill();
      this.loadingBar_bar._width = 1;
      this.loadingBar_bar._xscale = 100;
      var _loc0_;
      if((_loc0_ = adType) !== AD_TYPE_LOADING)
      {
         this.loadingBar._visible = false;
      }
      if(loadGameCompleted)
      {
         this.loadingBar._visible = false;
      }
      else
      {
         this.loadingBar.onEnterFrame = function()
         {
            fwAd.loadGameProgress();
         };
      }
   }
   static function showNews(_newsValues)
   {
      clearInterval(checkNewsFirstInitIntervalId);
      newsValues.wid = wid;
      for(var _loc3_ in _newsValues)
      {
         newsValues[_loc3_] = _newsValues[_loc3_];
      }
      if(!newsValues.id)
      {
         newsValues.id = id + "_news";
      }
      var _loc4_ = newsValues.container || _root;
      if(_loc4_)
      {
         newsLoader = _loc4_.createEmptyMovieClip("newsLoader",_loc4_.getNextHighestDepth());
         newsLoader.createEmptyMovieClip("content",newsLoader.getNextHighestDepth());
         newsLoader.x = newsValues.x;
         newsLoader.y = newsValues.y;
         newsMcl = new MovieClipLoader();
         newsMcl.onLoadInit = function()
         {
            loadNewsComplete();
         };
         newsMcl.loadClip(newsSWFPath,newsLoader.content);
      }
      else
      {
         trace("未指定容器,请改成例如: FWAd_AS2.showNews({container:_root}");
      }
      news_lc = new LocalConnection();
   }
   static function loadNewsComplete()
   {
      clearInterval(checkNewsFirstInitIntervalId);
      checkNewsFirstInitIntervalId = setInterval(checkNewsFirstInit,100);
   }
   static function checkNewsFirstInit()
   {
      var _loc1_ = Math.round(newsLoader.getBounds(newsLoader).xMin / 10);
      if(_loc1_ < -1000)
      {
         clearInterval(checkNewsFirstInitIntervalId);
         news_lc.send("_News" + _loc1_,"that2this","newsValues",newsValues);
      }
   }
   static function showAd(_adValues)
   {
      var _loc6_ = {x:xx,y:yy,wid:wid,hei:hei,id:id,adType:AD_TYPE_CONTAINER};
      for(var _loc3_ in _adValues)
      {
         _loc6_[_loc3_] = _adValues[_loc3_];
      }
      var _loc4_ = _loc6_.container || _root;
      if(_loc4_)
      {
         _loc4_.attachMovie("FWAd_AS2","fwAd",_loc4_.getNextHighestDepth(),{adValues:_loc6_});
      }
      else
      {
         trace("未指定容器,请改成例如: FWAd_AS2.showAd({container:_root}");
      }
   }
   function loadFramework()
   {
      this.mcl.loadClip(frameworkSWFPath,frameworkLoader.content);
      _global.clearTimeout(this.timeoutId);
      this.timeoutId = _global.setTimeout(this.loadFrameworkTimeout,loadFrameworkTimeoutTime * 1000);
   }
   function loadGameProgress()
   {
      var _loc0_;
      var _loc3_;
      if((_loc0_ = adType) !== AD_TYPE_LOADING)
      {
         this.loadingBar.onEnterFrame = null;
      }
      else
      {
         _loc3_ = _root.getBytesLoaded() / _root.getBytesTotal();
         this.loadingBar_bar._xscale = 100;
         this.loadingBar_bar._yscale = _loc3_ * 100;
         if(_loc3_ >= 1)
         {
            this.loadingBar.onEnterFrame = null;
            this.loadGameComplete();
         }
      }
   }
   function loadGameComplete()
   {
      loadGameCompleted = true;
      this.loadingBar._visible = false;
      this.checkLoadGameCompleteAndTryStart();
      if(frameworkInitFinished)
      {
         lc.send(lc_sendName,"that2this","loadGameCompleted");
      }
   }
   function loadFrameworkComplete()
   {
      this.onEnterFrame = this.checkFrameworkFirstInit;
   }
   function checkFrameworkFirstInit()
   {
      var _loc4_ = Math.round(frameworkLoader.getBounds(frameworkLoader).xMin / 10);
      if(_loc4_ < -1000)
      {
         this.onEnterFrame = null;
         lc_connName = "_FWAd" + _loc4_;
         lc_sendName = "_Framework" + _loc4_;
         lc = new LocalConnection();
         lc.allowDomain = function()
         {
            return true;
         };
         lc.allowInsecureDomain = function()
         {
            return true;
         };
         lc.onStatus = function()
         {
            var _loc2_ = arguments[0];
            var _loc0_;
            if((_loc0_ = _loc2_.level) === "error")
            {
               trace("lc error");
            }
         };
         lc.connect(lc_connName);
         lc.that2this = function()
         {
            fwAd.Framework2FWAd.apply(fwAd,arguments);
         };
         _global.clearTimeout(this.timeoutId);
         loadFrameworkSuccess = true;
         this.checkLoadGameCompleteAndTryStart();
      }
   }
   function loadFrameworkTimeout()
   {
      trace("loadFrameworkTimeout");
      fwAd.loadFrameworkError();
   }
   function loadFrameworkError()
   {
      trace("剩余尝试加载次数:" + restLoadFrameworkTimes);
      _global.clearTimeout(this.timeoutId);
      if(--restLoadFrameworkTimes < 0)
      {
         loadFrameworkFailed = true;
         this.checkLoadGameCompleteAndTryStart();
      }
      else
      {
         this.mcl.uploadClip();
         this.loadFramework();
      }
   }
   function Framework2FWAd()
   {
      switch(arguments[0])
      {
         case "initFinished":
            frameworkInitFinished = true;
            lc.send(lc_sendName,"that2this","confirmInitFinished",this.getFWAdValuesByNameArr(arguments.slice(1)));
            break;
         case "clickStartBtn":
            this.clickStartBtn();
            break;
         case "canNotShowAd":
         case "noId":
            canNotShowAd = true;
            this.checkLoadGameCompleteAndTryStart();
         default:
            return;
      }
   }
   function getFWAdValuesByNameArr(nameArr)
   {
      var _loc3_ = new Object();
      var _loc1_;
      for(var _loc4_ in nameArr)
      {
         _loc1_ = nameArr[_loc4_];
         _loc3_[_loc1_] = FWAd_AS2[_loc1_];
      }
      return _loc3_;
   }
   function checkLoadGameCompleteAndTryStart()
   {
      if(loadGameCompleted)
      {
         if(loadFrameworkFailed)
         {
            trace("加载 framework 失败");
            if(ifLoadFrameworkErrorCanPlayGame || this.checkIsOurDomain())
            {
               trace("自动跳到游戏");
               this.clickStartBtn();
            }
            else
            {
               trace("不自动跳到游戏");
            }
         }
         else if(canNotShowAd)
         {
            if(this.checkIsOurDomain())
            {
               this.clickStartBtn();
            }
         }
      }
   }
   function checkIsOurDomain()
   {
      var _loc2_;
      if(_root._url)
      {
         _loc2_ = _root._url.toLowerCase();
         return _loc2_.indexOf(".youyouwin.com/") > 0 || _loc2_.indexOf(".2144.cn/") > 0 || _loc2_.indexOf(".flash8.net/") > 0 || _loc2_.indexOf("file:///") == 0;
      }
      return true;
   }
   function clickStartBtn()
   {
      if(onClickStartBtn != null)
      {
         onClickStartBtn();
         onClickStartBtn = null;
      }
      this.removeMovieClip();
      var _loc4_ = new Array();
      var _loc3_;
      var _loc2_;
      for(var _loc5_ in this._parent)
      {
         _loc3_ = this._parent[_loc5_];
         if(_loc3_ instanceof MovieClip)
         {
            _loc2_ = _loc3_.getDepth();
            if(_loc2_ >= 0)
            {
               _loc4_[_loc2_] = true;
            }
         }
      }
      _loc2_ = 65535;
      while(_loc2_ >= 0)
      {
         if(!_loc4_[_loc2_])
         {
            this.swapDepths(_loc2_);
            break;
         }
         _loc2_ = _loc2_ - 1;
      }
      this.removeMovieClip();
      this._visible = false;
      this._x = -10000;
   }
}
