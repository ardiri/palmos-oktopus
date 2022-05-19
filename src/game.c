/*
 * @(#)game.c
 *
 * Copyright 1999-2000, Aaron Ardiri (mailto:aaron@ardiri.com)
 * All rights reserved.
 *
 * This file was generated as part of the "oktopus" program developed for 
 * the Palm Computing Platform designed by Palm: 
 *
 *   http://www.palm.com/ 
 *
 * The contents of this file is confidential and proprietrary in nature 
 * ("Confidential Information"). Redistribution or modification without 
 * prior consent of the original author is prohibited. 
 */

#include "palm.h"

// interface

static void GameAdjustLevel(PreferencesType *)    __GAME__;
static void GameIncrementScore(PreferencesType *) __GAME__;
static void GameMovePlayer(PreferencesType *)     __GAME__;
static void GameMoveTentacles(PreferencesType *)  __GAME__;

// global variable structure
typedef struct
{
  WinHandle winDigits;                      // scoring digits bitmaps
  WinHandle winLives;                       // the lives notification bitmaps

  WinHandle winDivers;                      // the divers bitmaps
  Boolean   diverChanged;                   // do we need to repaint diver?
  Boolean   diverOnScreen;                  // is diver bitmap on screen?
  UInt16    diverOldPosition;               // the *old* position of diver 

  WinHandle winDiverDeaths;                 // the diver death bitmaps

  WinHandle winTentacles;                   // the tentacle bitmaps
  Boolean   tentacleChanged[MAX_TENT];      // do we need to repaint tentacle
  Boolean   tentacleOnScreen[MAX_TENT];     // is tentacle bitmap on screen?
  UInt16    tentacleOnScreenPosition[MAX_TENT]; 
                                            // the *old* position of tentacle 

  UInt8     gameType;                       // the type of game active
  Boolean   playerDied;                     // has the player died?
  UInt8     moveDelayCount;                 // the delay between moves
  UInt8     moveLast;                       // the last move performed
  UInt8     moveNext;                       // the next desired move

  struct {

    Boolean    gamePadPresent;              // is the gamepad driver present
    UInt16     gamePadLibRef;               // library reference for gamepad

  } hardware;

} GameGlobals;

/**
 * Initialize the Game.
 *
 * @return true if game is initialized, false otherwise
 */  
Boolean   
GameInitialize()
{
  GameGlobals *gbls;
  Err         err;
  Boolean     result;

  // create the globals object, and register it
  gbls = (GameGlobals *)MemPtrNew(sizeof(GameGlobals));
  MemSet(gbls, sizeof(GameGlobals), 0);
  FtrSet(appCreator, ftrGameGlobals, (UInt32)gbls);

  // load the gamepad drvier if available
  {
    Err err;

    // attempt to load the library
    err = SysLibFind(GPD_LIB_NAME,&gbls->hardware.gamePadLibRef);
    if (err == sysErrLibNotFound)
      err = SysLibLoad('libr',GPD_LIB_CREATOR,&gbls->hardware.gamePadLibRef);

    // lets determine if it is available
    gbls->hardware.gamePadPresent = (err == errNone);

    // open the library if available
    if (gbls->hardware.gamePadPresent)
      GPDOpen(gbls->hardware.gamePadLibRef);
  }

  // initialize our "bitmap" windows
  err = errNone;
  {
    Int16 i;
    Err   e;

    gbls->winDigits = 
      WinCreateOffscreenWindow(70, 12, screenFormat, &e); err |= e;

    gbls->winLives = 
      WinCreateOffscreenWindow(48, 14, screenFormat, &e); err |= e;

    gbls->winDivers = 
      WinCreateOffscreenWindow(156, 78, screenFormat, &e); err |= e;
    gbls->diverChanged     = true;
    gbls->diverOnScreen    = false;
    gbls->diverOldPosition = 0;

    gbls->winDiverDeaths = 
      WinCreateOffscreenWindow(58, 29, screenFormat, &e); err |= e;

    gbls->winTentacles = 
      WinCreateOffscreenWindow(75, 60, screenFormat, &e); err |= e;
    for (i=0; i<MAX_TENT; i++) {
      gbls->tentacleChanged[i]          = true;
      gbls->tentacleOnScreen[i]         = false;
      gbls->tentacleOnScreenPosition[i] = 0;
    }
  }

  // no problems creating back buffers? load images.
  if (err == errNone) {
    WinHandle currWindow;
    MemHandle bitmapHandle;

    currWindow = WinGetDrawWindow();

    // digits
    WinSetDrawWindow(gbls->winDigits);
    bitmapHandle = DmGet1Resource('Tbmp', bitmapDigits);
    WinDrawBitmap((BitmapType *)MemHandleLock(bitmapHandle), 0, 0);
    MemHandleUnlock(bitmapHandle);
    DmReleaseResource(bitmapHandle);

    // lives
    WinSetDrawWindow(gbls->winLives);
    bitmapHandle = DmGet1Resource('Tbmp', bitmapLives);
    WinDrawBitmap((BitmapType *)MemHandleLock(bitmapHandle), 0, 0);
    MemHandleUnlock(bitmapHandle);
    DmReleaseResource(bitmapHandle);

    // divers
    WinSetDrawWindow(gbls->winDivers);
    bitmapHandle = DmGet1Resource('Tbmp', bitmapDivers);
    WinDrawBitmap((BitmapType *)MemHandleLock(bitmapHandle), 0, 0);
    MemHandleUnlock(bitmapHandle);
    DmReleaseResource(bitmapHandle);

    // diver deaths
    WinSetDrawWindow(gbls->winDiverDeaths);
    bitmapHandle = DmGet1Resource('Tbmp', bitmapDiverDeath);
    WinDrawBitmap((BitmapType *)MemHandleLock(bitmapHandle), 0, 0);
    MemHandleUnlock(bitmapHandle);
    DmReleaseResource(bitmapHandle);

    // tentacles
    WinSetDrawWindow(gbls->winTentacles);
    bitmapHandle = DmGet1Resource('Tbmp', bitmapTentacles);
    WinDrawBitmap((BitmapType *)MemHandleLock(bitmapHandle), 0, 0);
    MemHandleUnlock(bitmapHandle);
    DmReleaseResource(bitmapHandle);

    WinSetDrawWindow(currWindow);
  }

  result = (err == errNone);

  return result;
}

/**
 * Reset the Game.
 * 
 * @param prefs the global preference data.
 * @param gameType the type of game to configure for.
 */  
void   
GameReset(PreferencesType *prefs, Int8 gameType)
{
  GameGlobals *gbls;

  // get a globals reference
  FtrGet(appCreator, ftrGameGlobals, (UInt32 *)&gbls);

  // turn off all the "bitmaps"
  FrmDrawForm(FrmGetActiveForm());

  // turn on all the "bitmaps"
  {
    RectangleType rect    = { {   0,   0 }, {   0,   0 } };
    RectangleType scrRect = { {   0,   0 }, {   0,   0 } };
    UInt16        i, j, index;

    // 
    // draw the score
    //

    for (i=0; i<4; i++) {

      // what is the rectangle we need to copy?
      GameGetSpritePosition(spriteDigit, i,
                            &scrRect.topLeft.x, &scrRect.topLeft.y);
      scrRect.extent.x  = 7;
      scrRect.extent.y  = 12;
      rect.topLeft.x    = 8 * scrRect.extent.x;
      rect.topLeft.y    = 0;
      rect.extent.x     = scrRect.extent.x;
      rect.extent.y     = scrRect.extent.y;

      // draw the digit!
      WinCopyRectangle(gbls->winDigits, WinGetDrawWindow(),
                       &rect, scrRect.topLeft.x, scrRect.topLeft.y, winPaint);
    }

    // 
    // draw the lives
    //

    // what is the rectangle we need to copy?
    GameGetSpritePosition(spriteLife, 0,
                          &scrRect.topLeft.x, &scrRect.topLeft.y);
    scrRect.extent.x  = 24;
    scrRect.extent.y  = 14;
    rect.topLeft.x    = 0;
    rect.topLeft.y    = 0;
    rect.extent.x     = scrRect.extent.x;
    rect.extent.y     = scrRect.extent.y;

    // draw the life bitmap!
    WinCopyRectangle(gbls->winLives, WinGetDrawWindow(),
                     &rect, scrRect.topLeft.x, scrRect.topLeft.y, winOverlay);

    //
    // draw the tentacles
    //

    for (j=0; j<6; j++) {

      UInt8 offset[] = { 3, 4, 5, 4, 3, 1 };

      for (i=0; i<offset[j]; i++) {

        index = i + (j * 5);

        // what is the rectangle we need to copy?
        GameGetSpritePosition(spriteTentacle, index,
                              &scrRect.topLeft.x, &scrRect.topLeft.y);
        scrRect.extent.x  = 15;
        scrRect.extent.y  = 10;
        rect.topLeft.x    = i * scrRect.extent.x;
        rect.topLeft.y    = j * scrRect.extent.y;
        rect.extent.x     = scrRect.extent.x;
        rect.extent.y     = scrRect.extent.y;

        // draw the tentacle bitmap!
        WinCopyRectangle(gbls->winTentacles, WinGetDrawWindow(),
                         &rect, scrRect.topLeft.x, scrRect.topLeft.y, winOverlay);
      }
    }

    //
    // draw the diver death
    //

    for (i=0; i<2; i++) {

      // what is the rectangle we need to copy?
      GameGetSpritePosition(spriteDiverDeath, i,
                            &scrRect.topLeft.x, &scrRect.topLeft.y);
      scrRect.extent.x  = 29;
      scrRect.extent.y  = 29;
      rect.topLeft.x    = i * scrRect.extent.x;
      rect.topLeft.y    = 0;
      rect.extent.x     = scrRect.extent.x;
      rect.extent.y     = scrRect.extent.y;

      // draw the diver death bitmap!
      WinCopyRectangle(gbls->winDiverDeaths, WinGetDrawWindow(),
                       &rect, scrRect.topLeft.x, scrRect.topLeft.y, winOverlay);
    }

    //
    // draw the divers
    //

    for (i=0; i<6; i++) {

      // what is the rectangle we need to copy?
      GameGetSpritePosition(spriteDiver, i, 
                            &scrRect.topLeft.x, &scrRect.topLeft.y);
      scrRect.extent.x  = 26;
      scrRect.extent.y  = 26;
      rect.topLeft.x    = i * scrRect.extent.x; 
      rect.topLeft.y    = 0;
      rect.extent.x     = scrRect.extent.x;
      rect.extent.y     = scrRect.extent.y;

      // draw the diver bitmap
      WinCopyRectangle(gbls->winDivers, WinGetDrawWindow(),
                       &rect, scrRect.topLeft.x, scrRect.topLeft.y, winOverlay);
    }

    for (i=0; i<2; i++) {

      // what is the rectangle we need to copy?
      GameGetSpritePosition(spriteDiver, 7+i, 
                            &scrRect.topLeft.x, &scrRect.topLeft.y);
      scrRect.extent.x  = 26;
      scrRect.extent.y  = 26;
      rect.topLeft.x    = i * scrRect.extent.x; 
      rect.topLeft.y    = 1 * scrRect.extent.y;
      rect.extent.x     = scrRect.extent.x;
      rect.extent.y     = scrRect.extent.y;

      // draw the diver bitmap
      WinCopyRectangle(gbls->winDivers, WinGetDrawWindow(),
                       &rect, scrRect.topLeft.x, scrRect.topLeft.y, winOverlay);
    }

    // what is the rectangle we need to copy?
    GameGetSpritePosition(spriteDiver, 0, 
                          &scrRect.topLeft.x, &scrRect.topLeft.y);
    scrRect.extent.x  = 26;
    scrRect.extent.y  = 26;
    rect.topLeft.x    = 0;
    rect.topLeft.y    = 2 * scrRect.extent.y;
    rect.extent.x     = scrRect.extent.x;
    rect.extent.y     = scrRect.extent.y;

    // draw the diver bitmap
    WinCopyRectangle(gbls->winDivers, WinGetDrawWindow(),
                     &rect, scrRect.topLeft.x, scrRect.topLeft.y, winOverlay);
  }

  // wait a good two seconds :))
  SysTaskDelay(2 * SysTicksPerSecond());

  // turn off all the "bitmaps"
  FrmDrawForm(FrmGetActiveForm());

  // reset the preferences
  GameResetPreferences(prefs, gameType);
}

/**
 * Reset the Game preferences.
 * 
 * @param prefs the global preference data.
 * @param gameType the type of game to configure for.
 */  
void   
GameResetPreferences(PreferencesType *prefs, Int8 gameType)
{
  GameGlobals *gbls;
  Int16       i;

  // get a globals reference
  FtrGet(appCreator, ftrGameGlobals, (UInt32 *)&gbls);

  // now we are playing
  prefs->game.gamePlaying              = true;
  prefs->game.gamePaused               = false;
  prefs->game.gameWait                 = true;
  prefs->game.gameAnimationCount       = 0;

  // reset score and lives
  prefs->game.gameScore                = 0;
  prefs->game.gameLives                = 3;

  // reset oktopus specific things
  prefs->game.oktopus.gameType         = gameType;
  prefs->game.oktopus.gameLevel        = 1;
  prefs->game.oktopus.bonusAvailable   = true;
  prefs->game.oktopus.bonusScoring     = false;

  prefs->game.oktopus.goldInBag        = 0;

  prefs->game.oktopus.diverCount       = 0;
  prefs->game.oktopus.diverPosition    = 0;
  prefs->game.oktopus.diverNewPosition = 0;

  MemSet(&prefs->game.oktopus.tentacleWait[0], sizeof(UInt16) * MAX_TENT, 0);
  MemSet(&prefs->game.oktopus.tentaclePosition[0], sizeof(UInt16) * MAX_TENT, 0);
  MemSet(&prefs->game.oktopus.tentacleDirection[0], sizeof(Int16) * MAX_TENT, 1);

  // reset the "backup" and "onscreen" flags
  gbls->diverChanged                   = true;
  for (i=0; i<MAX_TENT; i++) {
    gbls->tentacleChanged[i]           = true;
    gbls->tentacleOnScreen[i]          = false;
  }

  gbls->gameType                       = gameType;
  gbls->playerDied                     = false;
  gbls->moveDelayCount                 = 0;
  gbls->moveLast                       = moveNone;
  gbls->moveNext                       = moveNone;
}

/**
 * Process key input from the user.
 * 
 * @param prefs the global preference data.
 * @param keyStatus the current key state.
 */  
void   
GameProcessKeyInput(PreferencesType *prefs, UInt32 keyStatus)
{
  GameGlobals *gbls;

  // get a globals reference
  FtrGet(appCreator, ftrGameGlobals, (UInt32 *)&gbls);

  keyStatus &= (prefs->config.ctlKeyLeft  |
                prefs->config.ctlKeyRight);

  // additional checks here
  if (gbls->hardware.gamePadPresent) {

    UInt8 gamePadKeyStatus;
    Err   err;

    // read the state of the gamepad
    err = GPDReadInstant(gbls->hardware.gamePadLibRef, &gamePadKeyStatus);
    if (err == errNone) {

      // process
      if (((gamePadKeyStatus & GAMEPAD_LEFT)      != 0) ||
          ((gamePadKeyStatus & GAMEPAD_LEFTFIRE)  != 0))
        keyStatus |= prefs->config.ctlKeyLeft;
      if (((gamePadKeyStatus & GAMEPAD_RIGHT)     != 0) ||
          ((gamePadKeyStatus & GAMEPAD_RIGHTFIRE) != 0))
        keyStatus |= prefs->config.ctlKeyRight;

      // special purpose :)
      if  ((gamePadKeyStatus & GAMEPAD_SELECT)    != 0) {

        // wait until they let it go :)
        do {
          GPDReadInstant(gbls->hardware.gamePadLibRef, &gamePadKeyStatus);
        } while ((gamePadKeyStatus & GAMEPAD_SELECT) != 0);

        keyStatus = 0;
        prefs->game.gamePaused = !prefs->game.gamePaused;
      }
      if  ((gamePadKeyStatus & GAMEPAD_START)     != 0) {

        // wait until they let it go :)
        do {
          GPDReadInstant(gbls->hardware.gamePadLibRef, &gamePadKeyStatus);
        } while ((gamePadKeyStatus & GAMEPAD_START) != 0);

        keyStatus = 0;
        GameReset(prefs, prefs->game.oktopus.gameType);
      }
    }
  }

  // did they press at least one of the game keys?
  if (keyStatus != 0) {

    // if they were waiting, we should reset the game animation count
    if (prefs->game.gameWait) { 
      prefs->game.gameAnimationCount = 0;
    }

    // great! they wanna play
    prefs->game.gamePaused = false;
    prefs->game.gameWait   = false;
  }

  // move left
  if (
      ((keyStatus &  prefs->config.ctlKeyLeft) != 0) &&
      (
       (gbls->moveDelayCount == 0) || 
       (gbls->moveLast       != moveLeft)
      )
     ) {

    // adjust the position if possible
    if (prefs->game.oktopus.diverPosition > 0) {
      prefs->game.oktopus.diverNewPosition = 
        prefs->game.oktopus.diverPosition - 1;
    }
  }

  // move right
  else
  if (
      ((keyStatus & prefs->config.ctlKeyRight) != 0) &&
      (
       (gbls->moveDelayCount == 0) || 
       (gbls->moveLast       != moveRight)
      )
     ) {

    // adjust the position if possible
    if (prefs->game.oktopus.diverPosition < 6) {
      prefs->game.oktopus.diverNewPosition = 
        prefs->game.oktopus.diverPosition + 1;
    }
  }
}
  
/**
 * Process stylus input from the user.
 * 
 * @param prefs the global preference data.
 * @param x the x co-ordinate of the stylus event.
 * @param y the y co-ordinate of the stylus event.
 */  
void   
GameProcessStylusInput(PreferencesType *prefs, Coord x, Coord y)
{
  RectangleType rect;
  Int16         i;

  // lets take a look at all the possible "positions"
  for (i=0; i<7; i++) {

    // get the bounding box of the position
    GameGetSpritePosition(spriteDiver, i,
                          &rect.topLeft.x, &rect.topLeft.y);
    rect.extent.x = 26;
    rect.extent.y = 26;

    // did they tap inside this rectangle?
    if (RctPtInRectangle(x, y, &rect)) {

      // ok, this is where we are going to go :)
      prefs->game.oktopus.diverNewPosition = i;

      // if they were waiting, we should reset the game animation count
      if (prefs->game.gameWait) {
        prefs->game.gameAnimationCount = 0;
        prefs->game.gameWait           = false;
      }

      // great! they wanna play
      prefs->game.gamePaused = false;
      break;                                        // stop looking
    }
  }
}

/**
 * Process the object movement in the game.
 * 
 * @param prefs the global preference data.
 */  
void   
GameMovement(PreferencesType *prefs)
{
  const CustomPatternType erase = { 0,0,0,0,0,0,0,0 };
  const RectangleType     rect  = {{   0,  16 }, { 160, 16 }};

  GameGlobals    *gbls;
  SndCommandType deathSnd = {sndCmdFreqDurationAmp,0, 512,50,sndMaxAmp};

  // get a globals reference
  FtrGet(appCreator, ftrGameGlobals, (UInt32 *)&gbls);

  //
  // the game is NOT paused.
  //

  if (!prefs->game.gamePaused) {

    // we must make sure the user is ready for playing 
    if (!prefs->game.gameWait) {

      // we cannot be dead yet :)
      gbls->playerDied = false;

      // are we in bonus mode?
      if ((prefs->game.oktopus.bonusScoring) &&
          (prefs->game.gameAnimationCount % GAME_FPS) < (GAME_FPS >> 1)) {

        Char   str[32];
        FontID currFont = FntGetFont();

        StrCopy(str, "    * BONUS PLAY *    ");
        FntSetFont(boldFont);
        WinDrawChars(str, StrLen(str), 
                     80 - (FntCharsWidth(str, StrLen(str)) >> 1), 19);
        FntSetFont(currFont);
      }
      else {

        // erase the status area
        WinSetPattern(&erase);
        WinFillRectangle(&rect, 0);
      }

      // player gets first move
      GameMovePlayer(prefs);
      GameMoveTentacles(prefs);

      // is it time to upgrade the game?
      if (prefs->game.gameAnimationCount >= 
           ((gbls->gameType == GAME_A) ? 0x7ff : 0x3ff)) {

        prefs->game.gameAnimationCount = 0;
        prefs->game.oktopus.gameLevel++;

        // upgrading of difficulty?
        if (
            (gbls->gameType                == GAME_A) &&
            (prefs->game.oktopus.gameLevel > 4)
           ) {

          gbls->gameType                 = GAME_B;
          prefs->game.oktopus.gameLevel -= 2;  // give em a break :)
        }
      } 

      // has the player died in this frame?
      if (gbls->playerDied) {

        UInt16        i, index;
        RectangleType rect    = { {   0,   0 }, {   0,   0 } };
        RectangleType scrRect = { {   0,   0 }, {   0,   0 } };

        // play the beep sound
        DevicePlaySound(&deathSnd);
        SysTaskDelay(50);

        // we need to make some changes (to the oktopus)
        prefs->game.oktopus.tentaclePosition[2] = 2;
        gbls->tentacleChanged[2] = true;
        prefs->game.oktopus.tentaclePosition[3] = 2;
        gbls->tentacleChanged[3] = true;
        GameDraw(prefs);

        if (gbls->diverOnScreen) {

          index = gbls->diverOldPosition;

          // what is the rectangle we need to copy?
          GameGetSpritePosition(spriteDiver, index, 
                                &scrRect.topLeft.x, &scrRect.topLeft.y);
          scrRect.extent.x  = 26;
          scrRect.extent.y  = 26;
          rect.topLeft.x    = index * scrRect.extent.x; 
          rect.topLeft.y    = 0;
          rect.extent.x     = scrRect.extent.x;
          rect.extent.y     = scrRect.extent.y;

          // invert the old diver bitmap
          WinCopyRectangle(gbls->winDivers, WinGetDrawWindow(),
                           &rect, scrRect.topLeft.x, scrRect.topLeft.y, winMask);
          gbls->diverOnScreen    = false;
        }

        // what is the rectangle we need to copy?
        GameGetSpritePosition(spriteTentacle, 25,
                              &scrRect.topLeft.x, &scrRect.topLeft.y);
        scrRect.extent.x  = 15;
        scrRect.extent.y  = 10;
        rect.topLeft.x    = 0;
        rect.topLeft.y    = 5 * scrRect.extent.y;
        rect.extent.x     = scrRect.extent.x;
        rect.extent.y     = scrRect.extent.y;

        // draw the tentacle bitmap!
        WinCopyRectangle(gbls->winTentacles, WinGetDrawWindow(),
                         &rect, scrRect.topLeft.x, scrRect.topLeft.y, winInvert);

        // play death sound and flash the player
        for (i=0; i<4; i++) {

          index = i % 2;

          // what is the rectangle we need to copy?
          GameGetSpritePosition(spriteDiverDeath, index,
                                &scrRect.topLeft.x, &scrRect.topLeft.y);
          scrRect.extent.x  = 29;
          scrRect.extent.y  = 29;
          rect.topLeft.x    = index * scrRect.extent.x;
          rect.topLeft.y    = 0;
          rect.extent.x     = scrRect.extent.x;
          rect.extent.y     = scrRect.extent.y;

          // draw the diver death bitmap
          WinCopyRectangle(gbls->winDiverDeaths, WinGetDrawWindow(),
                           &rect, scrRect.topLeft.x, scrRect.topLeft.y, winOverlay);

          // play the beep sound
          DevicePlaySound(&deathSnd);
          SysTaskDelay(50);

          // invert the diver death bitmap
          WinCopyRectangle(gbls->winDiverDeaths, WinGetDrawWindow(),
                           &rect, scrRect.topLeft.x, scrRect.topLeft.y, winMask);
        }

        // what is the rectangle we need to copy?
        GameGetSpritePosition(spriteTentacle, 25,
                              &scrRect.topLeft.x, &scrRect.topLeft.y);
        scrRect.extent.x  = 15;
        scrRect.extent.y  = 10;
        rect.topLeft.x    = 0;
        rect.topLeft.y    = 5 * scrRect.extent.y;
        rect.extent.x     = scrRect.extent.x;
        rect.extent.y     = scrRect.extent.y;

        // draw the tentacle bitmap!
        WinCopyRectangle(gbls->winTentacles, WinGetDrawWindow(),
                         &rect, scrRect.topLeft.x, scrRect.topLeft.y, winInvert);

        // lose a life :(
        prefs->game.gameLives--;
        {  
          index = 0;  // all the lives shown

          // what is the rectangle we need to copy?
          GameGetSpritePosition(spriteLife, 0, 
                                &scrRect.topLeft.x, &scrRect.topLeft.y);
          scrRect.extent.x  = 24;
          scrRect.extent.y  = 14;
          rect.topLeft.x    = index * scrRect.extent.x;
          rect.topLeft.y    = 0;
          rect.extent.x     = scrRect.extent.x;
          rect.extent.y     = scrRect.extent.y;
      
          // invert the three miss bitmap!
          WinCopyRectangle(gbls->winLives, WinGetDrawWindow(),
                           &rect, scrRect.topLeft.x, scrRect.topLeft.y, winMask);
        }

        // no more lives left: GAME OVER!
        if (prefs->game.gameLives == 0) {

          EventType event;

          // GAME OVER - return to main screen
          MemSet(&event, sizeof(EventType), 0);
          event.eType            = menuEvent;
          event.data.menu.itemID = gameMenuItemExit;
          EvtAddEventToQueue(&event);

          prefs->game.gamePlaying = false;
        }

        // continue game
        else {
          GameAdjustLevel(prefs);
          prefs->game.oktopus.bonusScoring = false;
          prefs->game.gameWait             = true;
        }
      }
    }

    // we have to display "GET READY!"
    else {

      // flash on:
      if ((prefs->game.gameAnimationCount % GAME_FPS) < (GAME_FPS >> 1)) {

        Char   str[32];
        FontID currFont = FntGetFont();

        StrCopy(str, "    * GET READY *    ");
        FntSetFont(boldFont);
        WinDrawChars(str, StrLen(str), 
                     80 - (FntCharsWidth(str, StrLen(str)) >> 1), 19);
        FntSetFont(currFont);
      }

      // flash off:
      else {

        // erase the status area
        WinSetPattern(&erase);
        WinFillRectangle(&rect, 0);
      }
    }

    // update the animation counter
    prefs->game.gameAnimationCount++;
  }

  //
  // the game is paused.
  //

  else {

    Char   str[32];
    FontID currFont = FntGetFont();

    StrCopy(str, "    *  PAUSED  *    ");
    FntSetFont(boldFont);
    WinDrawChars(str, StrLen(str), 
                 80 - (FntCharsWidth(str, StrLen(str)) >> 1), 19);
    FntSetFont(currFont);
  }
}

/**
 * Draw the game on the screen.
 * 
 * @param prefs the global preference data.
 */
void   
GameDraw(PreferencesType *prefs)
{
  GameGlobals   *gbls;
  Int16         i, j, index;
  RectangleType rect    = { {   0,   0 }, {   0,   0 } };
  RectangleType scrRect = { {   0,   0 }, {   0,   0 } };

  // get a globals reference
  FtrGet(appCreator, ftrGameGlobals, (UInt32 *)&gbls);

  // 
  // DRAW INFORMATION/BITMAPS ON SCREEN
  //

  // draw the score
  {
    Int16 base;
 
    base = 1000;  // max score (4 digits)
    for (i=0; i<4; i++) {

      index = (prefs->game.gameScore / base) % 10;

      // what is the rectangle we need to copy?
      GameGetSpritePosition(spriteDigit, i, 
                            &scrRect.topLeft.x, &scrRect.topLeft.y);
      scrRect.extent.x  = 7;
      scrRect.extent.y  = 12;
      rect.topLeft.x    = index * scrRect.extent.x;
      rect.topLeft.y    = 0;
      rect.extent.x     = scrRect.extent.x;
      rect.extent.y     = scrRect.extent.y;

      // draw the digit!
      WinCopyRectangle(gbls->winDigits, WinGetDrawWindow(),
                       &rect, scrRect.topLeft.x, scrRect.topLeft.y, winPaint);
      base /= 10;
    }
  }

  // draw the life that have occurred :( 
  if (prefs->game.gameLives > 1) {
  
    index = 3 - prefs->game.gameLives;

    // what is the rectangle we need to copy?
    GameGetSpritePosition(spriteLife, 0, 
                          &scrRect.topLeft.x, &scrRect.topLeft.y);
    scrRect.extent.x  = 24;
    scrRect.extent.y  = 14;
    rect.topLeft.x    = index * scrRect.extent.x;
    rect.topLeft.y    = 0;
    rect.extent.x     = scrRect.extent.x;
    rect.extent.y     = scrRect.extent.y;

    // draw the life bitmap!
    WinCopyRectangle(gbls->winLives, WinGetDrawWindow(),
                     &rect, scrRect.topLeft.x, scrRect.topLeft.y, winOverlay);
  }

  // no lives left, make sure none are shown
  else {
  
    index = 0;  // all the lives shown

    // what is the rectangle we need to copy?
    GameGetSpritePosition(spriteLife, 0, 
                          &scrRect.topLeft.x, &scrRect.topLeft.y);
    scrRect.extent.x  = 24;
    scrRect.extent.y  = 14;
    rect.topLeft.x    = index * scrRect.extent.x;
    rect.topLeft.y    = 0;
    rect.extent.x     = scrRect.extent.x;
    rect.extent.y     = scrRect.extent.y;

    // invert the three miss bitmap!
    WinCopyRectangle(gbls->winLives, WinGetDrawWindow(),
                     &rect, scrRect.topLeft.x, scrRect.topLeft.y, winMask);
  }

  // draw the tentacles
  for (i=0; i<5; i++) {

    // draw the tentacles on the screen (only if it has changed)
    if (gbls->tentacleChanged[i]) {

      //
      // erase the previous tentacle (if any)
      //

      if (gbls->tentacleOnScreen[i]) {

        index = (i * 5);

        for (j=0; j<gbls->tentacleOnScreenPosition[i]; j++) {

          // what is the rectangle we need to copy?
          GameGetSpritePosition(spriteTentacle, index,
                                &scrRect.topLeft.x, &scrRect.topLeft.y);
          scrRect.extent.x  = 15;
          scrRect.extent.y  = 10;
          rect.topLeft.x    = (index % 5) * scrRect.extent.x;
          rect.topLeft.y    = i * scrRect.extent.y;
          rect.extent.x     = scrRect.extent.x;
          rect.extent.y     = scrRect.extent.y;

          // invert the old tentacle bitmap!
          WinCopyRectangle(gbls->winTentacles, WinGetDrawWindow(),
                           &rect, scrRect.topLeft.x, scrRect.topLeft.y, winMask);
          index++;
        }

        gbls->tentacleOnScreen[i] = false;
      }

      //
      // draw the tentacle
      //

      if (prefs->game.oktopus.tentaclePosition[i] > 0) {

        index = (i * 5);

        for (j=0; j<prefs->game.oktopus.tentaclePosition[i]; j++) {

          // what is the rectangle we need to copy?
          GameGetSpritePosition(spriteTentacle, index,
                                &scrRect.topLeft.x, &scrRect.topLeft.y);
          scrRect.extent.x  = 15;
          scrRect.extent.y  = 10;
          rect.topLeft.x    = (index % 5) * scrRect.extent.x;
          rect.topLeft.y    = i * scrRect.extent.y;
          rect.extent.x     = scrRect.extent.x;
          rect.extent.y     = scrRect.extent.y;

          // draw the new tentacle bitmap!
          WinCopyRectangle(gbls->winTentacles, WinGetDrawWindow(),
                           &rect, scrRect.topLeft.x, scrRect.topLeft.y, winOverlay);
          index++;
        }

        // save this location, record tentacle is onscreen
        gbls->tentacleOnScreen[i]         = true;
        gbls->tentacleOnScreenPosition[i] = prefs->game.oktopus.tentaclePosition[i];
      }

      // dont draw until we need to
      gbls->tentacleChanged[i] = false;
    }
  }

  // draw diver (only if it has changed)
  if (gbls->diverChanged) {

    // 
    // erase the previous diver
    // 

    if (gbls->diverOnScreen) {

      index = gbls->diverOldPosition;

      // what is the rectangle we need to copy?
      GameGetSpritePosition(spriteDiver, index, 
                            &scrRect.topLeft.x, &scrRect.topLeft.y);
      scrRect.extent.x  = 26;
      scrRect.extent.y  = 26;
      rect.topLeft.x    = index * scrRect.extent.x; 
      rect.topLeft.y    = 0;
      rect.extent.x     = scrRect.extent.x;
      rect.extent.y     = scrRect.extent.y;

      // invert the old diver bitmap
      WinCopyRectangle(gbls->winDivers, WinGetDrawWindow(),
                       &rect, scrRect.topLeft.x, scrRect.topLeft.y, winMask);
      gbls->diverOnScreen    = false;
    }

    // 
    // draw diver at the new position
    // 

    index = prefs->game.oktopus.diverPosition;

    // what is the rectangle we need to copy?
    GameGetSpritePosition(spriteDiver, index, 
                          &scrRect.topLeft.x, &scrRect.topLeft.y);
    scrRect.extent.x  = 26;
    scrRect.extent.y  = 26;
    rect.topLeft.x    = index * scrRect.extent.x; 
    rect.topLeft.y    = 0;
    rect.extent.x     = scrRect.extent.x;
    rect.extent.y     = scrRect.extent.y;

    // save this location, record diver is onscreen
    gbls->diverOnScreen    = true;
    gbls->diverOldPosition = index;

    // draw the diver bitmap!
    WinCopyRectangle(gbls->winDivers, WinGetDrawWindow(),
                     &rect, scrRect.topLeft.x, scrRect.topLeft.y, winOverlay);

    // dont draw until we need to
    gbls->diverChanged = false;
  }
}

/**
 * Get the position of a particular sprite on the screen.
 *
 * @param spriteType the type of sprite.
 * @param index the index required in the sprite position list.
 * @param x the x co-ordinate of the position
 * @param y the y co-ordinate of the position
 */
void
GameGetSpritePosition(UInt8 spriteType, 
                      UInt8 index, 
                      Coord *x, 
                      Coord *y)
{
  switch (spriteType) 
  {
    case spriteDigit: 
         {
           *x = 120 + (index * 9);
           *y = 38;
         }
         break;

    case spriteLife: 
         {
           *x = 36;
           *y = 41;
         }
         break;

    case spriteDiver: 
         {
           Coord positions[][2] = {
                                   {  15,  41 },
                                   {  13,  69 },
                                   {  19,  96 },
                                   {  43, 114 },
                                   {  71, 114 },
                                   { 100, 117 },
                                   { 128, 112 },
                                   { 100, 117 },
                                   { 100, 117 }
                                 };

           *x = positions[index][0];
           *y = positions[index][1];
         }
         break;

    case spriteDiverDeath: 
         {
           Coord positions[][2] = {
                                   {  77,  86 },
                                   {  77,  86 }
                                 };

           *x = positions[index][0];
           *y = positions[index][1];
         }
         break;

    case spriteTentacle: 
         {
           Coord positions[][2] = {
                                   {  51,  76 },   // tentacle one
                                   {  40,  77 }, 
                                   {  31,  73 }, 
                                   {   0,   0 }, 
                                   {   0,   0 }, 
                                   {  51,  76 },   // tentacle two
                                   {  44,  82 }, 
                                   {  40,  88 }, 
                                   {  36,  97 }, 
                                   {   0,   0 }, 
                                   {  68,  85 },   // tentacle three
                                   {  66,  91 }, 
                                   {  65,  97 }, 
                                   {  64, 105 }, 
                                   {  60, 112 }, 
                                   {  87,  94 },   // tentacle four
                                   {  88, 101 },
                                   {  89, 106 },
                                   {  89, 114 },
                                   {   0,   0 }, 
                                   { 116, 102 },   // tentacle five
                                   { 120, 110 },
                                   { 123, 115 },
                                   {   0,   0 }, 
                                   {   0,   0 }, 
                                   {  72,  98 }    // death hold
                                  };

           *x = positions[index][0];
           *y = positions[index][1];
         }
         break;

    default:
         break;
  }
}

/**
 * Terminate the game.
 */
void   
GameTerminate()
{
  GameGlobals *gbls;

  // get a globals reference
  FtrGet(appCreator, ftrGameGlobals, (UInt32 *)&gbls);

  // unlock the gamepad driver (if available)
  if (gbls->hardware.gamePadPresent) {

    Err    err;
    UInt32 gamePadUserCount;

    err = GPDClose(gbls->hardware.gamePadLibRef, &gamePadUserCount);
    if (gamePadUserCount == 0)
      SysLibRemove(gbls->hardware.gamePadLibRef);
  }

  // clean up windows/memory
  if (gbls->winDigits != NULL)
    WinDeleteWindow(gbls->winDigits,      false);
  if (gbls->winLives != NULL)
    WinDeleteWindow(gbls->winLives,       false);
  if (gbls->winDivers != NULL)
    WinDeleteWindow(gbls->winDivers,      false);
  if (gbls->winDiverDeaths != NULL)
    WinDeleteWindow(gbls->winDiverDeaths, false);
  if (gbls->winTentacles != NULL)
    WinDeleteWindow(gbls->winTentacles,   false);
  MemPtrFree(gbls);

  // unregister global data
  FtrUnregister(appCreator, ftrGameGlobals);
}

/**
 * Adjust the level (remove birds that are too close and reset positions)
 *
 * @param prefs the global preference data.
 */
static void 
GameAdjustLevel(PreferencesType *prefs)
{
  GameGlobals *gbls;

  // get a globals reference
  FtrGet(appCreator, ftrGameGlobals, (UInt32 *)&gbls);

  // reset the "gold" count
  prefs->game.oktopus.goldInBag        = 0;

  // player should go back to the "starting" position
  prefs->game.oktopus.diverCount       = 0;
  prefs->game.oktopus.diverPosition    = 0;
  prefs->game.oktopus.diverNewPosition = prefs->game.oktopus.diverPosition;
  gbls->diverChanged                   = true;

  // player is not dead
  gbls->playerDied                     = false;
}

/**
 * Increment the players score. 
 *
 * @param prefs the global preference data.
 */
static void 
GameIncrementScore(PreferencesType *prefs)
{
  GameGlobals    *gbls;
  Int16          i, index;
  RectangleType  rect     = { {   0,   0 }, {   0,   0 } };
  RectangleType  scrRect  = { {   0,   0 }, {   0,   0 } };
  SndCommandType scoreSnd = {sndCmdFreqDurationAmp,0,1024, 5,sndMaxAmp};

  // get a globals reference
  FtrGet(appCreator, ftrGameGlobals, (UInt32 *)&gbls);

  // adjust accordingly
  prefs->game.gameScore += prefs->game.oktopus.bonusScoring ? 2 : 1;

  // redraw score bitmap
  {
    Int16 base;
 
    base = 1000;  // max score (4 digits)
    for (i=0; i<4; i++) {

      index = (prefs->game.gameScore / base) % 10;

      // what is the rectangle we need to copy?
      GameGetSpritePosition(spriteDigit, i, 
                            &scrRect.topLeft.x, &scrRect.topLeft.y);
      scrRect.extent.x  = 7;
      scrRect.extent.y  = 12;
      rect.topLeft.x    = index * scrRect.extent.x;
      rect.topLeft.y    = 0;
      rect.extent.x     = scrRect.extent.x;
      rect.extent.y     = scrRect.extent.y;

      // draw the digit!
      WinCopyRectangle(gbls->winDigits, WinGetDrawWindow(),
                       &rect, scrRect.topLeft.x, scrRect.topLeft.y, winPaint);
      base /= 10;
    }
  }

  // play the sound
  DevicePlaySound(&scoreSnd);
  SysTaskDelay(5);

  // is it time for a bonus?
  if (
      (prefs->game.gameScore >= 300) &&
      (prefs->game.oktopus.bonusAvailable)
     ) {

    SndCommandType snd = {sndCmdFreqDurationAmp,0,0,5,sndMaxAmp};

    // give a little fan-fare sound
    for (i=0; i<15; i++) {
      snd.param1 += 256 + (1 << i);  // frequency
      DevicePlaySound(&snd);

      SysTaskDelay(2); // small deley 
    }

    // apply the bonus!
    if (prefs->game.gameLives == 3) 
      prefs->game.oktopus.bonusScoring = true;
    else
      prefs->game.gameLives = 3;

    prefs->game.oktopus.bonusAvailable = false;
  }

#ifdef PROTECTION_ON
  // is it time to say 'bye-bye' to our freeware guys?
  if (
      (prefs->game.gameScore >= 50) && 
      (!CHECK_SIGNATURE(prefs))
     ) {

    EventType event;

    // "please register" dialog :)
    ApplicationDisplayDialog(rbugForm);

    // GAME OVER - return to main screen
    MemSet(&event, sizeof(EventType), 0);
    event.eType            = menuEvent;
    event.data.menu.itemID = gameMenuItemExit;
    EvtAddEventToQueue(&event);

    // stop the game in its tracks
    prefs->game.gamePlaying = false;
  }
#endif
}

/**
 * Move the player.
 *
 * @param prefs the global preference data.
 */
static void
GameMovePlayer(PreferencesType *prefs) 
{
  GameGlobals    *gbls;
  SndCommandType plymvSnd = {sndCmdFreqDurationAmp,0, 768, 5,sndMaxAmp};

  // get a globals reference
  FtrGet(appCreator, ftrGameGlobals, (UInt32 *)&gbls);

  // 
  // collecting bonus points for returning to the boat?
  //

  if (
      (prefs->game.oktopus.diverPosition == 0) &&
      (prefs->game.oktopus.goldInBag     != 0)
     ) {

    RectangleType rect    = { {   0,   0 }, {   0,   0 } };
    RectangleType scrRect = { {   0,   0 }, {   0,   0 } };
    UInt16        i;

    if (gbls->diverOnScreen) {

      // what is the rectangle we need to copy?
      GameGetSpritePosition(spriteDiver, 0, 
                            &scrRect.topLeft.x, &scrRect.topLeft.y);
      scrRect.extent.x  = 26;
      scrRect.extent.y  = 26;
      rect.topLeft.x    = 0;
      rect.topLeft.y    = 0;
      rect.extent.x     = scrRect.extent.x;
      rect.extent.y     = scrRect.extent.y;

      // invert the old diver bitmap
      WinCopyRectangle(gbls->winDivers, WinGetDrawWindow(),
                       &rect, scrRect.topLeft.x, scrRect.topLeft.y, winMask);
    }

    // what is the rectangle we need to copy?
    GameGetSpritePosition(spriteDiver, 0, 
                          &scrRect.topLeft.x, &scrRect.topLeft.y);
    scrRect.extent.x  = 26;
    scrRect.extent.y  = 26;
    rect.topLeft.x    = 0;
    rect.topLeft.y    = 2 * scrRect.extent.y;
    rect.extent.x     = scrRect.extent.x;
    rect.extent.y     = scrRect.extent.y;

    // draw the "collecting" diver bitmap
    WinCopyRectangle(gbls->winDivers, WinGetDrawWindow(),
                     &rect, scrRect.topLeft.x, scrRect.topLeft.y, winOverlay);

    // this is worth 3 points
    for (i=0; i<3; i++) {
      GameIncrementScore(prefs);
    }

    // erase the "collecting" diver bitmap
    WinCopyRectangle(gbls->winDivers, WinGetDrawWindow(),
                     &rect, scrRect.topLeft.x, scrRect.topLeft.y, winMask);

    if (gbls->diverOnScreen) {

      // what is the rectangle we need to copy?
      GameGetSpritePosition(spriteDiver, 0, 
                            &scrRect.topLeft.x, &scrRect.topLeft.y);
      scrRect.extent.x  = 26;
      scrRect.extent.y  = 26;
      rect.topLeft.x    = 0;
      rect.topLeft.y    = 0;
      rect.extent.x     = scrRect.extent.x;
      rect.extent.y     = scrRect.extent.y;

      // draw the old diver bitmap
      WinCopyRectangle(gbls->winDivers, WinGetDrawWindow(),
                       &rect, scrRect.topLeft.x, scrRect.topLeft.y, winOverlay);
    }

    // reset elementary information
    prefs->game.oktopus.goldInBag  = 0;
    prefs->game.oktopus.diverCount = 0;
  }

  // 
  // do we "boot" the diver out from the boat?
  //

  if (
      (prefs->game.oktopus.diverPosition == 0) &&
      ((prefs->game.oktopus.diverCount / GAME_FPS) == 5)  // 5 seconds
     ) {
    prefs->game.oktopus.diverNewPosition = 1;
  }

  //
  // where does diver want to go today?
  //

  // current position differs from new position?
  if (prefs->game.oktopus.diverPosition != 
      prefs->game.oktopus.diverNewPosition) {

    // need to move left
    if (prefs->game.oktopus.diverPosition > 
        prefs->game.oktopus.diverNewPosition) {

      gbls->moveNext = moveLeft;
    }

    // need to move right
    else
    if (prefs->game.oktopus.diverPosition < 
        prefs->game.oktopus.diverNewPosition) {

      gbls->moveNext = moveRight;
    }
  }

  // lets make sure they are allowed to do the move
  if (
      (gbls->moveDelayCount == 0) || 
      (gbls->moveLast != gbls->moveNext) 
     ) {
    gbls->moveDelayCount = 
     ((gbls->gameType == GAME_A) ? 4 : 3);
  }
  else {
    gbls->moveDelayCount--;
    gbls->moveNext = moveNone;
  }

  // update counter
  prefs->game.oktopus.diverCount++;

  // which direction do they wish to move?
  switch (gbls->moveNext)
  {
    case moveLeft:
         {
           // lets make sure it is valid
           if (
               (prefs->game.oktopus.diverPosition != 1) ||
               (prefs->game.oktopus.goldInBag      > 0)
              ) {
             prefs->game.oktopus.diverPosition--;
             gbls->diverChanged = true;
           }
         }
         break;

    case moveRight:
         {
           // collecting "gold"
           if (prefs->game.oktopus.diverPosition == 5)  {

             // max 5 gold pieces in the bag at once
             if (prefs->game.oktopus.goldInBag < 5) {

               RectangleType rect    = { {   0,   0 }, {   0,   0 } };
               RectangleType scrRect = { {   0,   0 }, {   0,   0 } };
               UInt16        i, index;

               // erase the "old" diver
               if (gbls->diverOnScreen) {

                 index = gbls->diverOldPosition;

                 // what is the rectangle we need to copy?
                 GameGetSpritePosition(spriteDiver, index, 
                                       &scrRect.topLeft.x, &scrRect.topLeft.y);
                 scrRect.extent.x  = 26;
                 scrRect.extent.y  = 26;
                 rect.topLeft.x    = index * scrRect.extent.x; 
                 rect.topLeft.y    = 0;
                 rect.extent.x     = scrRect.extent.x;
                 rect.extent.y     = scrRect.extent.y;

                 // invert the old diver bitmap
                 WinCopyRectangle(gbls->winDivers, WinGetDrawWindow(),
                                  &rect, scrRect.topLeft.x, scrRect.topLeft.y, winMask);
               }

               for (i=0; i<2; i++) {

                 // what is the rectangle we need to copy?
                 GameGetSpritePosition(spriteDiver, 7+i, 
                                       &scrRect.topLeft.x, &scrRect.topLeft.y);
                 scrRect.extent.x  = 26;
                 scrRect.extent.y  = 26;
                 rect.topLeft.x    = i * scrRect.extent.x; 
                 rect.topLeft.y    = 1 * scrRect.extent.y;
                 rect.extent.x     = scrRect.extent.x;
                 rect.extent.y     = scrRect.extent.y;
 
                 // draw the diver bitmap
                 WinCopyRectangle(gbls->winDivers, WinGetDrawWindow(),
                                  &rect, scrRect.topLeft.x, scrRect.topLeft.y, winOverlay);

                 // wait one frame
                 SysTaskDelay(SysTicksPerSecond() / GAME_FPS);

                 // invert the diver bitmap
                 WinCopyRectangle(gbls->winDivers, WinGetDrawWindow(),
                                  &rect, scrRect.topLeft.x, scrRect.topLeft.y, winInvert);
               }

               // ok! one more gold coin collected :)
               GameIncrementScore(prefs);
               prefs->game.oktopus.goldInBag++;  // on more gold :))

               // draw the "old" diver
               if (gbls->diverOnScreen) {

                 index = gbls->diverOldPosition;

                 // what is the rectangle we need to copy?
                 GameGetSpritePosition(spriteDiver, index, 
                                       &scrRect.topLeft.x, &scrRect.topLeft.y);
                 scrRect.extent.x  = 26;
                 scrRect.extent.y  = 26;
                 rect.topLeft.x    = index * scrRect.extent.x; 
                 rect.topLeft.y    = 0;
                 rect.extent.x     = scrRect.extent.x;
                 rect.extent.y     = scrRect.extent.y;

                 // draw the old diver bitmap
                 WinCopyRectangle(gbls->winDivers, WinGetDrawWindow(),
                                  &rect, scrRect.topLeft.x, scrRect.topLeft.y, winOverlay);
               }
             }

             // stay put :)
             prefs->game.oktopus.diverNewPosition = 
               prefs->game.oktopus.diverPosition;
           }

           else {
             prefs->game.oktopus.diverPosition++;
             gbls->diverChanged = true;
           }
         }
         break;

    default:
         break;
  }

  gbls->moveLast = gbls->moveNext;
  gbls->moveNext = moveNone;

  // do we need to play a movement sound? 
  if (gbls->diverChanged)  
    DevicePlaySound(&plymvSnd);
}

/**
 * Move the tentacles.
 *
 * @param prefs the global preference data.
 */
static void
GameMoveTentacles(PreferencesType *prefs)
{
  GameGlobals *gbls;

  // get a globals reference
  FtrGet(appCreator, ftrGameGlobals, (UInt32 *)&gbls);

  // only do this if the player is still alive
  if (!gbls->playerDied) {

    SndCommandType grdmvSnd = {sndCmdFreqDurationAmp,0, 384, 5,sndMaxAmp};
    UInt16         tentacleLength[] = { 3, 4, 5, 4, 3 };
    UInt16         i;

    // process each "tentacle"
    for (i=0; i<5; i++) {

      // is it time to process it?
      if (prefs->game.oktopus.tentacleWait[i] == 0) {

        // start moving the tentacle?
        if (prefs->game.oktopus.tentaclePosition[i] == 0) {

          Boolean ok;
          UInt8   birthFactor = (gbls->gameType == GAME_A) ? 4 : 2;

          ok = !(
                 (
                  (i == 0) && 
                  ((prefs->game.oktopus.tentaclePosition[1] != 0)  ||
                   (prefs->game.oktopus.diverPosition        > 2))
                 ) ||
                 (
                  (i == 1) && 
                  (prefs->game.oktopus.tentaclePosition[0] != 0)
                 )
                );
          ok &= ((SysRandom(0) % birthFactor) == 0);

          // are we going to add?
          if (ok) {

            // its moving :)
            prefs->game.oktopus.tentaclePosition[i]  = 1;
            prefs->game.oktopus.tentacleDirection[i] = 1;
            prefs->game.oktopus.tentacleWait[i]      =
              (gbls->gameType == GAME_A) 
                ? MAX(4, 6 - (prefs->game.oktopus.gameLevel >> 1))
                : MAX(3, 5 - (prefs->game.oktopus.gameLevel >> 2));

            gbls->tentacleChanged[i] = true;
          }
        }

        // its on it's way.. cycle through
        else {

          prefs->game.oktopus.tentaclePosition[i] += 
            prefs->game.oktopus.tentacleDirection[i];
          prefs->game.oktopus.tentacleWait[i]      =
            (gbls->gameType == GAME_A) 
              ? MAX(4, 6 - (prefs->game.oktopus.gameLevel >> 1))
              : MAX(3, 5 - (prefs->game.oktopus.gameLevel >> 2));
          gbls->tentacleChanged[i] = true;

          // has the tentacle reached the end of its path?
          if (prefs->game.oktopus.tentaclePosition[i] == tentacleLength[i])
            prefs->game.oktopus.tentacleDirection[i] = -1;

          DevicePlaySound(&grdmvSnd);
        }
      }

      // continue the animations
      else { 

        prefs->game.oktopus.tentacleWait[i]--;

        // has the player been caught?
        gbls->playerDied |=
          (
           ((i+1) == prefs->game.oktopus.diverPosition) &&
           (prefs->game.oktopus.tentaclePosition[i] == tentacleLength[i])
          );
      }
    }
  }
}
